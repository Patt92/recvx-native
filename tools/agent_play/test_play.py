#!/usr/bin/env python3
"""Controller tests use temporary telemetry/control files, never the game or user saves."""
from contextlib import contextmanager, redirect_stdout
from copy import deepcopy
import json
import io
import os
from pathlib import Path
import tempfile
import threading
import time
import unittest
import socket
import subprocess
import sys

from nav import owner, wait
from pathfind import GridCache
from play import Play, PlayError, compact, request


def state():
    walls = []
    for i, (x, z, ex, ez) in enumerate([(-20,-20,1,40), (20,-20,1,40), (-20,-20,40,1), (-20,20,40,1)]):
        walls.append(dict(i=i, fl=1, ty=0, attr=0, floor=0, p=[x,0,z], e=[ex,30,ez]))
    return dict(room='RM_TEST.RDX', actor=[0,0,0], yaw=0, hp=100, player_floor=0,
                control=dict(steer=False, pending=0, result='arrived'), weapon=9,
                items=[9<<16, (9<<16)|10], walls=walls, interactions=[], enemies=[], room_items=[])


class PlayTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.directory = Path(self.tmp.name)
        self.play = Play(self.directory)
        self.d = state()
        self.write(self.d)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, d):
        tmp = self.directory/'auto.tmp'
        tmp.write_text(json.dumps(d))
        tmp.replace(self.directory/'auto.json')

    def controls(self):
        path = self.directory/'live.txt'
        return path.read_text() if path.exists() else ''

    @contextmanager
    def runtime(self, phases):
        """Tick a fixture, then switch phases relative to the first dispatched input."""
        stop = threading.Event()
        def tick():
            began = None
            while not stop.is_set():
                if self.controls() and began is None:
                    began = time.monotonic()
                elapsed = -1 if began is None else time.monotonic()-began
                d = self.d
                for delay, value in phases:
                    if elapsed >= delay:
                        d = value
                self.write(d)
                stop.wait(.025)
        thread = threading.Thread(target=tick)
        thread.start()
        try:
            yield
        finally:
            stop.set()
            thread.join()

    def test_all_modal_owners_are_not_free(self):
        d = state(); d.update(computer=None, stairs=None, typewriter=None)
        self.assertEqual(owner(d), 'FREE')
        for key in ('inventory_open', 'files_open', 'box_open', 'debug_open', 'title_active', 'settings_active'):
            d = state(); d[key] = True
            self.assertNotEqual(owner(d), 'FREE', key)
        for key in ('typewriter', 'computer', 'stairs'):
            d = state(); d[key] = dict(active=True)
            self.assertNotEqual(owner(d), 'FREE', key)

    def test_output_bounds_geometry_and_preserves_dialogue(self):
        d = state()
        d['dialog_open'] = True
        d['dialog_text'] = 'An entire decision page\nincluding its choices.'
        for i in range(100):
            d['interactions'].append(dict(index=i, flags=1, floor=0, type=1, attribute=0,
                                         position=[i,0,0], extent=[1,1,1], parameters=[0,0]))
        result = compact(d, 0)
        self.assertEqual(len(result['zones']), 5)
        self.assertEqual(result['other_zones'], 95)
        self.assertEqual(result['dialog']['text'], d['dialog_text'])
        self.assertEqual(result['inv'], [[1,9,10]])
        self.assertNotIn('walls', result)

    def test_stale_file_never_receives_input(self):
        os.utime(self.play.auto, (time.time()-10, time.time()-10))
        result = request(self.play, dict(op='act', action=dict(op='press', key='confirm')))
        self.assertIn('stale', result['error'])
        self.assertEqual(self.controls(), '')

    def test_recent_final_file_is_not_mistaken_for_running_game(self):
        result = request(self.play, dict(op='act', action=dict(op='press', key='confirm'), seconds=.1))
        self.assertIn('not advancing', result['error'])
        self.assertEqual(self.controls(), '')

    def test_busy_input_is_not_duplicated(self):
        self.d['control']['steer'] = True; self.write(self.d)
        result = request(self.play, dict(op='act', action=dict(op='press', key='confirm')))
        self.assertIn('already pending', result['error'])
        self.assertEqual(self.controls(), '')

    def test_second_controller_is_rejected(self):
        with self.play.exclusive():
            result = request(Play(self.directory), dict(op='act', action=dict(op='press', key='confirm')))
            self.assertIn('another action', result['error'])

    def test_blocked_destination_is_not_silently_shifted(self):
        with self.assertRaisesRegex(PlayError, 'target blocked'):
            self.play.compile(self.d, dict(op='move', to=[20,0]))
        self.assertEqual(self.controls(), '')

    def test_exact_final_waypoint_and_tolerance(self):
        line = self.play.compile(self.d, dict(op='move', to=[8.2,8.3], face=[9,9]))[0][1]
        self.assertIn('8.2000 8.3000 stop', line.replace(' run', ''))
        self.assertIn('stop 0.35 danger 15 face 9.0000 9.0000', line)

    def test_grab_owner_accepts_shake_but_not_movement(self):
        self.d['cinematic'] = True
        self.d['player_work'] = dict(mode=[4, 0, 0, 0])
        self.assertEqual(owner(self.d), 'GRAB')
        commands = self.play.compile(self.d, dict(op='shake', presses=4))
        self.assertEqual([c[1] for c in commands], ['keys left', 'keys right', 'keys left', 'keys right', 'keys none'])
        with self.assertRaisesRegex(PlayError, 'FREE'):
            self.play.compile(self.d, dict(op='move', to=[1, 2]))
        del self.d['player_work']
        with self.assertRaisesRegex(PlayError, 'GRAB'):
            self.play.compile(self.d, dict(op='shake'))

    def test_zone_approach_lands_probe_in_zone_and_routes(self):
        # Table wall in the room; the item zone overlaps it, only its free edge is reachable.
        self.d['walls'].append(dict(i=9, fl=1, ty=0, attr=0, floor=0, p=[-5, 0, 5], e=[10, 10, 6]))
        self.d['interactions'] = [dict(index=2, flags=1, type=4, attribute=0, floor=0, position=[-2, 0, 2],
                                       extent=[4, 0, 7], parameters=[0, 0, 0, 0])]
        a = self.d['interactions'][0]
        approach = self.play.approaches(self.d, a)
        self.assertTrue(approach)
        for ap in approach:
            (x, z), (fx, fz) = ap['to'], ap['face']
            self.assertAlmostEqual(((fx - x) ** 2 + (fz - z) ** 2) ** .5, 2.0, places=1)
            self.assertTrue(-2 <= fx <= 2 and 2 <= fz <= 9)
        line = self.play.compile(self.d, dict(op='interact', zone=2, to=approach[0]['to'], face=approach[0]['face']))[0][1]
        self.assertTrue(line.endswith('then confirm'))

    def test_compact_labels_title_menu_and_items(self):
        self.d.update(title_active=True, title_mode=9, title_cursor=1, title_stage='game_mode', title_entry='NEW GAME',
                      title_entries=['LOAD GAME', 'NEW GAME', 'OPTION', 'QUIT'], item_names=['Handgun', 'Handgun'],
                      inventory_open=True, menu_selected=1, menu_item=9, menu_item_name='Handgun',
                      menu_commands_open=True, menu_commands=['Equip', 'Examine', 'Combine', 'Use'], menu_command=1)
        r = compact(self.d, 0)
        self.assertEqual(r['title']['entry'], 'NEW GAME')
        self.assertEqual(r['menu']['item'], 'Handgun')
        self.assertEqual(r['menu']['command'], 'Examine')
        self.assertEqual(r['inv'][0], [1, 9, 10, 'Handgun'])

    def test_fire_uses_equipped_item_not_drawn_weapon_number(self):
        self.d['items'] = [1, (9 << 16) | 12]
        self.d['weapon'] = 4
        self.d['item_names'] = ['', 'Handgun']
        self.assertTrue(self.play.compile(self.d, dict(op='fire', shots=1)))
        self.assertEqual(compact(self.d, 0)['equipped'], ['Handgun', 12])
        self.d.update(inventory_open=True, menu_selected=1)
        self.assertTrue(compact(self.d, 0)['menu']['equipped'])
        self.d['items'] = [0, (9 << 16) | 12]
        with self.assertRaisesRegex(PlayError, 'handgun'):
            self.play.compile(dict(self.d, inventory_open=False), dict(op='fire', shots=1))

    def test_danger_only_for_active_enemies_with_player_input(self):
        near = deepcopy(self.d)
        near['enemies'] = [dict(index=0, hp=50, position=[4, 0, 4], work=dict(mode=[5, 0, 0, 0]))]
        self.assertNotEqual(Play.reason(self.d, near, waiting=True), 'danger')  # scripted (mode 5)
        near['enemies'][0]['work']['mode'][0] = 1
        near['cinematic'] = True
        self.assertNotEqual(Play.reason(self.d, near, waiting=True), 'danger')
        near['cinematic'] = False
        self.assertEqual(Play.reason(self.d, near, waiting=True), 'danger')
        self.assertTrue(compact(near, 0)['enemies'][0]['reach'])

    def test_route_options_danger_and_near(self):
        self.d['interactions'] = [dict(index=2, flags=1, type=4, attribute=0, floor=0, position=[-2, 0, 6],
                                       extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        line = self.play.compile(self.d, dict(op='interact', zone=2, to=[0, 6], face=[0, 8]))[0][1]
        self.assertNotIn('danger', line)  # final approach under 8 units is never cut
        self.assertIn('near 1', line)
        self.assertEqual(line.split(' run')[0], 'route 0.0000 6.0000')  # free straight segment, no detour
        self.assertIn('danger 15', self.play.compile(self.d, dict(op='move', to=[12, 12]))[0][1])
        line = self.play.compile(self.d, dict(op='move', to=[12, 12], danger=False))[0][1]
        self.assertNotIn('danger', line)

    def test_fire_turns_to_nearest_threat_and_shake_then_moves(self):
        self.d['items'] = [1, (9 << 16) | 12]
        self.d['enemies'] = [dict(index=3, hp=50, position=[10, 0, 0], work=dict(mode=[1, 0, 0, 0])),
                             dict(index=4, hp=50, position=[2, 0, 2], work=dict(mode=[5, 0, 0, 0]))]
        self.assertEqual(self.play.compile(self.d, dict(op='fire', shots=2)), [(1, 'face 10.0000 0.0000 then fire 2')])
        self.d['enemies'] = []
        self.assertEqual(self.play.compile(self.d, dict(op='fire', shots=1, low=True)), [(1, 'fire 1 low')])
        self.d.update(cinematic=True, player_work=dict(mode=[4, 0, 0, 0]))
        commands = self.play.compile(self.d, dict(op='shake', presses=2, then=dict(op='move', to=[-12, -12])))
        self.assertEqual(commands[-1][0], 8)
        self.assertTrue(commands[-1][1].startswith('route ') and 'danger' not in commands[-1][1])

    def test_shake_runs_even_when_its_escape_is_unusable(self):
        # Live: 44 shakes were refused for an extra "danger" in the escape and 19 for a blocked target,
        # each a lost decision while grabbed.
        self.d.update(cinematic=True, player_work=dict(mode=[4, 0, 0, 0]))
        commands = self.play.compile(self.d, dict(op='shake', presses=2, then=dict(op='move', to=[-12, -12], danger=False)))
        self.assertTrue(commands[-1][1].startswith('route '))
        self.assertIsNone(self.play.shake_note)
        commands = self.play.compile(self.d, dict(op='shake', presses=2, then=dict(op='move', to=[90, 90])))
        self.assertEqual([c[1] for c in commands], ['keys left', 'keys right', 'keys none'])
        self.assertIn('shook free without the escape move', self.play.shake_note)
        commands = self.play.compile(self.d, dict(op='shake', presses=2, then=dict(op='fire', shots=2)))
        self.assertEqual(commands[-1][1], 'keys none')

    def test_interact_on_stairs_reports_the_floor_change(self):
        stairs = dict(index=3, flags=1, floor=0, type=1, attribute=0, position=[-2, 0, -6], extent=[4, 0, 4],
                      parameters=[0, 1, 4, 255])
        self.d['interactions'] = [stairs]
        self.write(self.d)
        up = deepcopy(self.d); up['actor'] = [0, 3.0, -4]
        top = deepcopy(self.d); top['actor'] = [0, 8.0, -8]; top['player_floor'] = 2
        with self.runtime([(0, self.d), (.2, up), (.5, top)]):
            out = self.play.act(dict(op='interact', zone=3, to=[0.0, -1.0], face=[0.0, -5.0]), 2)
        self.assertEqual(out['result'], 'floor')
        self.assertEqual(out['floor'], [0, 2])

    def test_door_approach_prefers_centre_of_door_face(self):
        # Door zone in front of a wall at x >= 44; centre line z = 69.5.
        self.d['walls'] = [dict(i=0, fl=1, ty=0, attr=0, floor=0, p=[44, 0, 40], e=[2, 30, 60]),
                           dict(i=1, fl=1, ty=0, attr=0, floor=0, p=[20, 0, 40], e=[24, 30, 2]),
                           dict(i=2, fl=1, ty=0, attr=0, floor=0, p=[20, 0, 98], e=[24, 30, 2]),
                           dict(i=3, fl=1, ty=0, attr=0, floor=0, p=[18, 0, 40], e=[2, 30, 60])]
        self.d['actor'] = [30, 0, 60]
        self.d['interactions'] = [dict(index=0, flags=1, type=0, attribute=0, floor=0, position=[35, 0, 61],
                                       extent=[9, 0, 17], parameters=[0, 1, 0, 0])]
        best = self.play.approaches(self.d, self.d['interactions'][0])[0]
        self.assertLess(abs(best['face'][1] - 69.5), 1.0)

    def test_overlapping_lower_zone_drops_approach_higher_zone_ranks_last(self):
        self.d['interactions'] = [dict(index=4, flags=1, type=4, attribute=0, floor=0, position=[-2, 0, 6],
                                       extent=[4, 0, 4], parameters=[0, 0, 0, 0]),
                                  dict(index=12, flags=1, type=3, attribute=0, floor=0, position=[-6, 0, 8],
                                       extent=[12, 0, 6], parameters=[0, 0, 0, 0])]
        item = self.play.approaches(self.d, self.d['interactions'][0], limit=10)
        self.assertTrue(item)
        self.assertNotIn('overlap', item[0])
        text = self.play.approaches(self.d, self.d['interactions'][1], limit=50)
        for ap in text:  # zone 4 (lower index) would answer at these points
            (x, z), (fx, fz) = ap['to'], ap['face']
            ix, iz = x + (fx - x) * 2 / 6, z + (fz - z) * 2 / 6
            self.assertFalse(-2 <= ix <= 2 and 6 <= iz <= 10)

    def test_round4_near_voice_notes_stairs(self):
        door = dict(index=0, flags=1, type=0, attribute=0, floor=0, position=[6, 0, -44], extent=[8, 0, 7],
                    parameters=[0, 1, 0, 0])
        self.assertEqual(Play.near_for(door, (10, -36), (10, -40.5)), 2.8)
        self.d.update(dialog_open=True, dialog_text='', dialog_index=7)
        self.assertEqual(compact(self.d, 0)['dialog']['voice'], True)
        self.d['dialog_open'] = False
        blocked = dict(door, index=1, type=3, attribute=0x3c00, position=[-2, 0, 4], extent=[4, 0, 4])
        stair = dict(index=2, flags=1, type=1, attribute=0, floor=0, position=[-3, 0, 10], extent=[6, 0, 6],
                     parameters=[0, 1, 4, 255])
        upper = dict(stair, index=3, floor=2, position=[-3, 8, 16], parameters=[1, 3, 4, 255])
        self.d['interactions'] = [blocked, stair, upper]
        self.write(self.d)
        self.assertIn('all facings', self.play.describe_zone(1)['approach_note'])
        ap = self.play.approaches(self.d, stair)
        self.assertTrue(ap)
        self.assertGreater(ap[0]['face'][1] - ap[0]['to'][1], .5)  # faces along the stair (+z)

    def test_invalid_actions_do_not_become_control_text(self):
        for action in [dict(op='position', to=[1,2]), dict(op='press', key='confirm\n1 skip'),
                       dict(op='move', to=[float('nan'),2]), dict(op='move', to=[1,2], typo=True)]:
            with self.assertRaises(PlayError):
                self.play.compile(self.d, action)
        self.assertEqual(self.controls(), '')

    def test_move_cannot_run_under_menu_owner(self):
        self.d['inventory_open'] = True
        with self.assertRaisesRegex(PlayError, 'FREE'):
            self.play.compile(self.d, dict(op='move', to=[1,2]))

    def test_dialogue_not_ready_is_not_confirmed(self):
        self.d['dialog_open'] = True
        with self.assertRaisesRegex(PlayError, 'still printing'):
            self.play.compile(self.d, dict(op='press', key='confirm'))

    def test_no_implicit_dialogue_acknowledgement(self):
        dialog = deepcopy(self.d)
        dialog.update(dialog_open=True, dialog_closable=True, dialog_text='Read this before choosing the next action.')
        with self.runtime([(0, dialog)]):
            result = self.play.act(dict(op='press', key='inventory'), seconds=1)
        self.assertEqual(result['result'], 'dialog')
        self.assertEqual(result['dialog']['text'], dialog['dialog_text'])
        self.assertNotIn('confirm', self.controls())

    def test_pickup_transition_does_not_report_free(self):
        menu = deepcopy(self.d); menu['inventory_open'] = True
        printing = deepcopy(menu); printing.update(dialog_open=True, dialog_text='Question still printing')
        choice = deepcopy(printing); choice.update(dialog_choosing=True, dialog_text='Take it? Yes No')
        with self.runtime([(0, menu), (.08, printing), (.2, choice)]):
            result = self.play.act(dict(op='press', key='confirm'), seconds=1)
        self.assertEqual(result['result'], 'choice')
        self.assertIn('INVENTORY', result['owner'])
        self.assertEqual(self.controls().count('confirm'), 1)

    def test_route_stall_even_if_previous_route_also_stalled(self):
        self.d['control']['result'] = 'stalled'; self.write(self.d)
        moving = deepcopy(self.d); moving['control'] = dict(result='moving', steer=True, pending=0)
        stopped = deepcopy(self.d)
        with self.runtime([(0, moving), (.15, stopped)]):
            result = self.play.act(dict(op='move', to=[8,8]), seconds=1)
        self.assertEqual(result['result'], 'stalled')

    def test_auto_hold_is_kept_and_held_result_waits_for_new_tick(self):
        self.d['control'].update(held=True, run='auto', tick=100); self.write(self.d)
        stale_beat = deepcopy(self.d)
        held_after = deepcopy(self.d); held_after['control']['tick'] = 112
        with self.runtime([(0, stale_beat), (.2, held_after)]):
            result = self.play.act(dict(op='keys', keys=['left'], ticks=2), seconds=1)
        self.assertEqual(result['result'], 'held')
        self.assertTrue(result['held'])
        self.assertGreater(result['elapsed_ms'], 150)
        self.assertNotIn('run on', self.controls())

    def test_wait_ticks_writes_noop_and_run_mode_action(self):
        with self.runtime([(0, self.d)]):
            self.play.act(dict(op='wait', ticks=30), seconds=.3)
        self.assertIn('30 tick', self.controls())
        self.assertEqual(self.play.compile(self.d, dict(op='run', mode='auto', dist=18)), [(1, 'run auto 18')])

    def test_enemy_entering_danger_radius_returns_danger(self):
        near = deepcopy(self.d)
        near['enemies'] = [dict(index=0, hp=50, position=[5, 0, 5], work=dict(mode=[1, 0, 0, 0]))]
        self.assertEqual(Play.reason(self.d, near, waiting=True), 'danger')
        self.assertNotEqual(Play.reason(near, near, waiting=True), 'danger')  # only on entering

    def test_room_change_is_returned_without_waiting_full_budget(self):
        changed = deepcopy(self.d); changed['room'] = 'RM_NEXT.RDX'
        with self.runtime([(0, changed)]):
            result = self.play.act(dict(op='press', key='confirm'), seconds=1)
        self.assertEqual(result['result'], 'room')
        self.assertLess(result['elapsed_ms'], 500)

    def test_context_change_before_dispatch_does_not_receive_input(self):
        changed = deepcopy(self.d); changed['inventory_open'] = True
        def switch():
            time.sleep(.03); self.write(changed)
        thread = threading.Thread(target=switch); thread.start()
        result = request(self.play, dict(op='act', action=dict(op='move', to=[8,8])))
        thread.join()
        self.assertIn('context changed', result['error'])
        self.assertEqual(self.controls(), '')

    def test_wait_does_not_write_and_has_short_budget(self):
        result = self.play.act(dict(op='wait'), seconds=.1)
        self.assertEqual(result['result'], 'no_update')
        self.assertLess(result['elapsed_ms'], 500)
        self.assertEqual(self.controls(), '')

    def test_legacy_wait_does_not_acknowledge_a_finished_page(self):
        self.d.update(dialog_open=True, dialog_closable=True, dialog_text='Read this page')
        self.write(self.d)
        output = io.StringIO()
        with self.runtime([]), redirect_stdout(output):
            wait(self.d, str(self.play.auto), str(self.play.live), .5, [])
        self.assertIn('DIALOG:', output.getvalue())
        self.assertEqual(self.controls(), '')

    def test_legacy_wait_does_not_report_open_inventory_free(self):
        self.d['inventory_open'] = True; self.write(self.d)
        output = io.StringIO()
        with self.runtime([]), redirect_stdout(output):
            wait(self.d, str(self.play.auto), str(self.play.live), .5, [])
        self.assertIn('OWNER INVENTORY', output.getvalue())
        self.assertNotIn('free after', output.getvalue())

    def test_moving_wall_invalidates_cache_with_identical_flags(self):
        cache = GridCache(); first = cache.get(self.d)
        self.d['walls'][0]['p'][0] += 1
        second = cache.get(self.d)
        self.assertIsNot(first, second)
        self.assertIs(second, cache.get(self.d))

    def test_actor_movement_inside_same_geometry_reuses_cache(self):
        cache = GridCache(); first = cache.get(self.d)
        self.d['actor'][0] += 1
        self.assertIs(first, cache.get(self.d))
        self.d['player_floor'] = 1
        self.assertIsNot(first, cache.get(self.d))

    def test_outside_negative_coordinate_is_not_cell_zero(self):
        g = GridCache().get(self.d)
        self.assertEqual(g.idx(g.x0-.1, g.z0-.1), (-1,-1))

    def test_persistent_socket_observation_without_runtime_inputs(self):
        path = self.directory/'test.sock'
        proc = subprocess.Popen([sys.executable, str(Path(__file__).with_name('play.py')),
                                 '--directory', str(self.directory), '--socket', str(path), 'serve'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            line = proc.stdout.readline()
            if not line:
                raise RuntimeError(proc.stderr.read())
            ready = json.loads(line)
            self.assertEqual(ready['ready'], str(path))
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.connect(str(path)); client.sendall(b'{"op":"state"}\n')
                with client.makefile() as f:
                    result = json.loads(f.readline())
            self.assertEqual(result['room'], 'RM_TEST')
            self.assertEqual(self.controls(), '')
        finally:
            proc.terminate()
            stdout, stderr = proc.communicate(timeout=3)
        self.assertEqual(proc.returncode, 0, stderr)
        self.assertFalse(path.exists())

    def test_stream_returns_only_new_bounded_events(self):
        stream = self.directory/'stream.log'
        stream.write_text('old\nfirst\nsecond\nthird\nfourth\n')
        self.assertEqual(self.play.observe()['events'], ['second','third','fourth'])
        self.assertNotIn('events', self.play.observe())
        with open(stream, 'a') as f: f.write('next\n')
        self.assertEqual(self.play.observe()['events'], ['next'])


if __name__ == '__main__':
    unittest.main()
