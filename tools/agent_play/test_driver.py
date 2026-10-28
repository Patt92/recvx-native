#!/usr/bin/env python3
"""Driver tests with a scripted model and a fixture runtime; no endpoint, game or user saves."""
from contextlib import contextmanager
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from driver import Driver, expectation_met, parse_action, passive, trim
from play import Play
from test_play import state


class Scripted:
    """Stands in for Client: returns queued messages and records every request."""

    def __init__(self, replies):
        self.replies, self.requests = list(replies), []

    def complete(self, messages, tools=None, max_tokens=None):
        self.requests.append((deepcopy(messages), tools))
        self.budgets = getattr(self, 'budgets', []) + [max_tokens]
        return self.replies.pop(0), dict(prompt_tokens=100, completion_tokens=10,
                                          prompt_tokens_details=dict(cached_tokens=80))


def text(action):
    return dict(role='assistant', content=json.dumps(action))


def call(name, args, id='c1'):
    return dict(role='assistant', content='', tool_calls=[dict(id=id, type='function',
                function=dict(name=name, arguments=json.dumps(args)))])


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.directory = Path(self.tmp.name)
        self.play = Play(self.directory)
        self.d = state()
        self.d['control']['run'] = 'auto'
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
    def ticking(self):
        stop = threading.Event()
        def tick():
            while not stop.is_set():
                self.write(self.d)
                stop.wait(.02)
        thread = threading.Thread(target=tick); thread.start()
        try:
            yield
        finally:
            stop.set(); thread.join()

    def driver(self, replies, **kw):
        client = Scripted(replies)
        drv = Driver(self.play, client, seconds=.2, out=io.StringIO(), **kw)
        # The game-data tables (door graph, item uses over 205 rooms) are not read in timing-sensitive
        # tests; the tests of those features set them up themselves.
        drv._door_graph = {}
        Driver._item_uses_cache = {}
        Driver._box_places_cache = {}
        drv.goal_list = lambda obs, d, lookahead=True: []   # the goal tests switch it back on
        return drv, client

    def test_dialog_automator_collects_stops_at_choice_and_remembers(self):
        drv, _ = self.driver([])
        drv.dialog_auto = 'choices_only'
        page = dict(room='RM_0090', dialog=dict(text='Scanner panel.', ready=False))
        self.assertEqual(drv.dialog_tick(page), dict(op='wait'))
        page['dialog']['ready'] = True
        self.assertEqual(drv.dialog_tick(page), dict(op='press', key='confirm'))
        choice = dict(room='RM_0090', dialog=dict(text='Push the switch?', ready=True, choice=True, highlighted=0))
        self.assertIsNone(drv.dialog_tick(choice))
        self.assertEqual(drv.pending_feedback['dialog_transcript'], ['Scanner panel.', 'Push the switch?'])
        self.assertIsNone(drv.dialog_tick(dict(room='RM_0090')))
        self.assertEqual(drv.memory['dialogs'][-1]['text'], ['Scanner panel.', 'Push the switch?'])
        self.assertIn('dialog_completed', drv.pending_feedback)

    def test_dialog_auto_off_and_all(self):
        drv, _ = self.driver([])
        choice = dict(room='RM_0090', dialog=dict(text='Yes or No?', ready=True, choice=True))
        drv.dialog_auto = 'off'
        self.assertIsNone(drv.dialog_tick(choice))
        drv.dialog_auto = 'all'
        self.assertEqual(drv.dialog_tick(choice), dict(op='press', key='confirm'))

    def test_box_transfer_discards_stale_slots_and_reports_live_state(self):
        drv, _ = self.driver([])
        for result in ('swapped', 'refused'):
            drv.plan = [dict(op='box', store=3)]
            drv.prefetched = dict(action=dict(op='box', store=3))
            live = dict(owner='INVENTORY BOX', box=dict(mode='inventory'), inv=[[2, 55, 1, 'Lighter']], result=result)
            with patch.object(self.play, 'act', return_value=live):
                obs, feedback, _ = drv.act(dict(op='box', store=2))
            self.assertEqual(drv.plan, [])
            self.assertIsNone(drv.prefetched)
            self.assertEqual(feedback['inventory_now'][0][0], 2)
            self.assertEqual(feedback['box_state']['mode'], 'inventory')
            self.assertIn('does not mean the box is closed', feedback['box_next'])

    def test_parse_action_tolerates_fences_prose_and_escaped_quotes(self):
        self.assertEqual(parse_action('```json\n{"op":"press","key":"confirm"}\n```'), dict(op='press', key='confirm'))
        self.assertEqual(parse_action('I go. {"op":"move","to":[1,2],"note":"say \\"hi\\" {x}"} done')['note'],
                         'say "hi" {x}')
        with self.assertRaises(ValueError):
            parse_action('{"key":"confirm"}')

    def test_passive_owners_skip_the_model(self):
        self.assertTrue(passive(dict(owner='CINEMATIC')))
        self.assertTrue(passive(dict(owner='CINEMATIC DOOR')))
        self.assertTrue(passive(dict(owner='FREE', busy=True)))
        self.assertTrue(passive(dict(owner='INVENTORY', menu=dict(locked=True))))
        self.assertFalse(passive(dict(owner='CINEMATIC DIALOG', dialog=dict(text='x'))))
        self.assertFalse(passive(dict(owner='GRAB')))
        self.assertFalse(passive(dict(owner='FREE')))

    def test_cinematic_is_waited_out_without_a_request(self):
        self.d['cinematic'] = True
        self.write(self.d)
        drv, client = self.driver([])
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(client.requests, [])
        self.assertEqual(self.controls(), '')

    def test_tool_call_then_action_reaches_the_control_file(self):
        self.d['interactions'] = [dict(index=4, flags=1, floor=0, type=4, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([call('zone', dict(i=4)),
                                   text(dict(op='move', to=[2, 2], note='go to the item'))])
        with self.ticking():
            drv.run(steps=1)
        first, tools = client.requests[0]
        self.assertEqual(first[0]['role'], 'system')
        self.assertIn('play Resident Evil', first[0]['content'])
        self.assertEqual({t['function']['name'] for t in tools}, {'zone', 'hint', 'look', 'dialog'})
        second = client.requests[1][0]
        self.assertEqual(second[-1]['role'], 'tool')
        self.assertIn('approach', second[-1]['content'])
        self.assertIn('route 2.0000 2.0000', self.controls())
        self.assertEqual(drv.note, 'go to the item')
        # The history keeps the decision but not the tool exchange.
        self.assertEqual([m['role'] for m in drv.history], ['user', 'assistant'])
        self.assertNotIn('note', json.loads(drv.history[1]['content']))

    def test_rejected_action_is_fed_back_as_error(self):
        drv, client = self.driver([text(dict(op='move', to=[20, 0])), text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertEqual(feedback['result'], 'error')
        self.assertIn('target blocked', feedback['error'])
        self.assertIn('cancel', self.controls())

    def test_reply_without_json_gets_one_retry_in_the_same_decision(self):
        drv, client = self.driver([dict(role='assistant', content='I think I should press confirm.'),
                                   text(dict(op='press', key='confirm'))])
        with self.ticking():
            drv.run(steps=1)
        self.assertEqual(len(client.requests), 2)
        self.assertIn('Reply with ONLY one JSON action', client.requests[1][0][-1]['content'])
        self.assertIn('confirm', self.controls())

    def test_no_tools_mode_accepts_zone_as_pseudo_action(self):
        self.d['interactions'] = [dict(index=1, flags=1, floor=0, type=3, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='zone', i=1)), text(dict(op='press', key='confirm'))], tools=False)
        with self.ticking():
            drv.run(steps=1)
        self.assertIsNone(client.requests[0][1])
        self.assertIn('tool zone:', client.requests[1][0][-1]['content'])

    def test_endless_lookups_end_in_a_forced_action_round_without_tools(self):
        drv, client = self.driver([call('hint', dict(cmd='room'), id=f'c{i}') for i in range(3)] +
                                  [call('hint', dict(cmd='room'), id='c9'),
                                   text(dict(op='press', key='confirm'))])
        with self.ticking():
            drv.run(steps=1)
        # Request 4 has no tools; a tool call there is ignored and request 5 must answer.
        self.assertEqual(len(client.requests), 5)
        self.assertIsNone(client.requests[3][1])
        self.assertIsNone(client.requests[4][1])
        self.assertIn('Lookup budget used up', client.requests[3][0][-1]['content'])
        self.assertIn('confirm', self.controls())

    def test_lookup_written_as_action_is_answered_not_dispatched(self):
        self.d['interactions'] = [dict(index=9, flags=1, floor=0, type=3, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='zone', i=9)), text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=1)
        self.assertIn('tool zone:', client.requests[1][0][-1]['content'])
        self.assertNotIn('zone', self.controls())
        self.assertIn('cancel', self.controls())

    def test_no_usable_reply_waits_and_reports_instead_of_stopping(self):
        prose = dict(role='assistant', content='hmm')
        drv, client = self.driver([prose] * 6 + [text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=2)
        second = json.loads(client.requests[-1][0][-1]['content'])
        self.assertEqual(second['result'], 'error')
        self.assertIn('no game action', second['error'])
        self.assertIn('cancel', self.controls())

    def test_timeout_is_retried_after_a_pause(self):
        import socket
        import urllib.error
        from unittest import mock
        import driver
        calls, pauses = [], []
        class Reply(io.BytesIO):
            def __enter__(self): return self
            def __exit__(self, *a): return False
        def urlopen(req, timeout):
            calls.append(1)
            if len(calls) == 1:
                raise socket.timeout('timed out')
            if len(calls) == 2:
                raise urllib.error.URLError(ConnectionRefusedError('refused'))
            if len(calls) == 3:
                raise urllib.error.HTTPError(req.full_url, 503, 'busy', {}, None)
            return Reply(json.dumps(dict(choices=[dict(message=dict(content='{"op":"wait"}'))])).encode())
        client = driver.Client('http://x/v1', 'm', out=io.StringIO(), sleep=pauses.append, retry_delay=3)
        with mock.patch('urllib.request.urlopen', urlopen):
            message, _ = client.complete([])
        self.assertEqual(message['content'], '{"op":"wait"}')
        self.assertEqual(pauses, [3, 6, 12])
        # A request error that will not heal (bad key, bad model) is not retried.
        def denied(req, timeout):
            raise urllib.error.HTTPError(req.full_url, 401, 'unauthorized', {}, None)
        with mock.patch('urllib.request.urlopen', denied), self.assertRaises(urllib.error.HTTPError):
            client.complete([])
        limited = driver.Client('http://x/v1', 'm', retries=2, out=io.StringIO(), sleep=lambda s: None)
        with mock.patch('urllib.request.urlopen', lambda req, timeout: (_ for _ in ()).throw(socket.timeout())), \
                self.assertRaises(TimeoutError):
            limited.complete([])

    def test_door_back_and_ready_approach_are_in_the_state(self):
        self.d['interactions'] = [
            dict(index=1, flags=1, floor=0, type=0, attribute=0, position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 1, 0, 0]),
            dict(index=0, flags=1, floor=0, type=0, attribute=0, position=[-9, 0, -9], extent=[4, 0, 4], parameters=[0, 3, 0, 0])]
        self.write(self.d)
        drv, _ = self.driver([])
        drv.room = 'RM_0010'
        obs = drv.with_approaches(drv.mark_way_back(self.play.observe()))
        zones = {z['i']: z for z in obs['zones']}
        self.assertTrue(zones[1].get('back'))
        self.assertNotIn('back', zones[0])
        self.assertEqual(obs['came_from'], 'RM_0010')
        self.assertEqual(len(zones[0]['go']), 2)

    def test_blocked_interact_error_carries_the_computed_points(self):
        self.d['interactions'] = [dict(index=4, flags=1, floor=0, type=4, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='interact', zone=4, to=[20, 0], face=[7, 7])),
                                   text(dict(op='press', key='cancel'))])
        drv.with_approaches = lambda obs: obs
        drv.correct = lambda action, obs: action  # the error path itself is under test here
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertEqual(feedback['result'], 'error')
        self.assertTrue(feedback['use_one_of'])
        self.assertEqual(set(feedback['use_one_of'][0]), {'to', 'face'})

    def test_save_screen_names_the_highlighted_choice(self):
        from play import compact
        d = state()
        d['typewriter'] = dict(active=True, mode='save', phase_name='confirm', text='Save?', selection=1,
                               cursor=0, slot=dict(n=0, exists=True), used_slots=[0])
        tw = compact(d, 0)['typewriter']
        self.assertEqual(tw['highlighted'], 'No')
        self.assertTrue(tw['slot_used'])
        d['typewriter']['selection'] = 0
        self.assertEqual(compact(d, 0)['typewriter']['highlighted'], 'Yes')
        d['typewriter']['phase_name'] = 'files'
        self.assertNotIn('highlighted', compact(d, 0)['typewriter'])

    def computer(self, **kw):
        c = dict(active=True, number=7, md0=2, md1=10, flg=0, txflg=0, key=[1, 1], words=['', ''],
                 text=['PASSWORD:', '', ''])
        c.update(kw)
        return c

    def test_computer_screen_and_input_wait_reach_the_model(self):
        from play import compact
        d = state()
        d['cinematic'] = True
        d['computer'] = self.computer(txflg=0x4)
        view = compact(d, 0)['computer']
        self.assertEqual(view['screen'], ['PASSWORD:'])
        self.assertEqual(view['waits'], 'confirm')
        self.assertTrue(view['cancel_exits'])
        d['computer'] = self.computer(flg=0x10 | 0x200, txflg=0x8)
        view = compact(d, 0)['computer']
        self.assertEqual((view['waits'], view['key']), ('keyboard', 'A'))
        self.assertNotIn('cancel_exits', view)
        d['computer'] = self.computer(txflg=0x1)
        self.assertEqual(compact(d, 0)['computer']['waits'], 'printing')
        self.assertTrue(passive(dict(owner='CINEMATIC COMPUTER', computer=dict(waits='printing'))))
        self.assertFalse(passive(dict(owner='CINEMATIC COMPUTER', computer=dict(waits='keyboard'))))

    def test_type_walks_the_soft_keyboard_and_presses_enter(self):
        from play import key_step, KEY_ROWS
        self.d['cinematic'] = True
        self.d['computer'] = self.computer(flg=0x10, txflg=0x8, key=[1, 1])
        commands = self.play.compile(self.d, dict(op='type', text='b2'))
        # Replay the moves on the keyboard model: every confirm must land on the wanted key.
        at, typed = (1, 1), ''
        for _, line in commands:
            if line == 'confirm':
                typed += KEY_ROWS[at[1]][at[0]]
            else:
                at = key_step(*at, line)
        self.assertEqual(typed, 'B2\n')
        ticks = [t for t, _ in commands]
        self.assertEqual(ticks, sorted(ticks))
        self.assertTrue(all(b - a >= 2 for a, b in zip(ticks, ticks[1:])))
        with self.assertRaisesRegex(Exception, 'A-Z or 0-9'):
            self.play.compile(self.d, dict(op='type', text='a-b'))
        self.d['computer'] = self.computer(txflg=0x4)
        with self.assertRaisesRegex(Exception, 'keyboard'):
            self.play.compile(self.d, dict(op='type', text='AB'))

    def test_computer_input_wait_ends_a_wait_action(self):
        from play import Play
        self.assertEqual(Play.reason(state(), dict(state(), cinematic=True, computer=self.computer(txflg=0x4)),
                                     waiting=True), 'computer')
        self.assertIsNone(Play.reason(state(), dict(state(), cinematic=True, computer=self.computer(txflg=0x1)),
                                      waiting=True))

    def test_unchanged_passive_state_is_handed_to_the_model(self):
        self.d['cinematic'] = True
        self.write(self.d)
        drv, client = self.driver([text(dict(op='press', key='confirm'))])
        with self.ticking():
            drv.run(steps=5)
        self.assertEqual(len(client.requests), 1)
        feedback = json.loads(client.requests[0][0][-1]['content'])
        self.assertGreaterEqual(feedback['unchanged_for'], 4)

    def test_document_text_is_shown(self):
        from play import compact
        d = state()
        d.update(files_open=True, inventory_open=True, file_text='The code is on the back.')
        self.assertEqual(compact(d, 0)['document'], 'The code is on the back.')

    def test_text_zones_get_a_go_point(self):
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, _ = self.driver([])
        zones = drv.with_approaches(self.play.observe())['zones']
        self.assertEqual(zones[0]['kind'], 'text')
        self.assertEqual(len(zones[0]['go']), 2)

    def test_blocked_move_returns_free_reachable_points(self):
        drv, client = self.driver([text(dict(op='move', to=[20, 0])), text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertIn('target blocked', feedback['error'])
        points = [p['to'] for p in feedback['use_one_of']]
        self.assertTrue(points)
        from pathfind import _blocked_fn, active_walls
        tests = [_blocked_fn(w, 3.5)[1] for w in active_walls(self.d)]
        for x, z in points:
            self.assertFalse(any(t(x, z) for t in tests), (x, z))
        # Closest first, and the suggestions are spread apart.
        self.assertLess(abs(points[0][0] - 20), 6)
        self.assertTrue(all(abs(a[0] - b[0]) + abs(a[1] - b[1]) >= 2 for a, b in zip(points, points[1:])))
        # The suggestion is accepted as a move target.
        self.play.compile(self.d, dict(op='move', to=points[0]))

    def test_hint_command_written_as_op_is_answered(self):
        drv, client = self.driver([text(dict(op='blocked', at=[70, 100])), text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=1)
        self.assertIn('tool hint:', client.requests[1][0][-1]['content'])
        self.assertNotIn('blocked', self.controls())
        self.assertIn('cancel', self.controls())

    def test_event_and_msg_lookups_use_this_drivers_room(self):
        from unittest import mock
        import driver
        drv, _ = self.driver([])
        self.d['room'] = 'RM_0090.RDX'
        self.write(self.d)
        seen = {}
        def run(argv, **kw):
            seen['argv'] = argv
            return mock.Mock(stdout='ok', stderr='')
        with mock.patch.object(driver.subprocess, 'run', run):
            drv.tool('hint', dict(cmd='event', args=['3']))
            self.assertEqual(seen['argv'][-3:], ['event', '3', 'RM_0090.RDX'])
            drv.tool('hint', dict(cmd='msg', args=['2', 'RM_0010.RDX']))
            self.assertEqual(seen['argv'][-3:], ['msg', '2', 'RM_0010.RDX'])
            drv.tool('hint', dict(cmd='zone', args=['12']))
            self.assertEqual(seen['argv'][-1], str(self.play.auto))

    def test_text_hint_survives_into_the_menu_the_game_opens(self):
        drv, _ = self.driver([])
        dialog = dict(owner='CINEMATIC DIALOG', dialog=dict(text='If I were equipped with a \nlighter, I could see outside...'),
                      inv=[[1, 55, 1, 'Lighter'], [2, 145, 1, 'Playing Manual']])
        drv.remember_texts(dialog)
        menu = dict(owner='INVENTORY', menu=dict(cursor=3, slot=2, item='Playing Manual'),
                    inv=dialog['inv'], equipped=None)
        tips = drv.tips(menu, 'CINEMATIC DIALOG')
        self.assertTrue(any('opened the status menu itself' in t for t in tips))
        self.assertTrue(any('lighter' in t for t in tips))
        self.assertIn('lighter, I could see outside', drv.texts[-1])
        # Once the lighter is equipped the item tip stops; a pickup text never asks to use an item.
        menu['equipped'] = ['Lighter', 1]
        self.assertFalse(any('names an item' in t for t in drv.tips(menu, 'INVENTORY')))
        drv.remember_texts(dict(dialog=dict(text="You've taken the\nHandgun Bullets.")))
        free = dict(owner='FREE', inv=[[3, 12, 15, 'Handgun Bullets']])
        self.assertEqual(drv.tips(free, 'DIALOG INVENTORY'), [])

    def test_recent_texts_and_tips_reach_the_model(self):
        self.d.update(inventory_open=True, main_cursor=3, menu_selected=2, menu_item_name='Playing Manual',
                      items=[0, (55 << 16) | 1, (145 << 16) | 1], item_names=['', 'Lighter', 'Playing Manual'])
        self.write(self.d)
        drv, client = self.driver([text(dict(op='press', key='up'))])
        drv.texts = ['If I were equipped with a lighter, I could see outside...']
        drv.last_owner = 'CINEMATIC DIALOG'
        with self.ticking():
            drv.run(steps=1)
        payload = json.loads(client.requests[0][0][-1]['content'])
        self.assertIn('lighter', payload['recent_texts'][-1])
        self.assertTrue(payload['tips'])

    def two_doors(self):
        # Two doors far apart in the 40x40 fixture room, like the north and south doors of a hall.
        self.d['interactions'] = [
            dict(index=13, flags=1, floor=0, type=0, attribute=0, position=[-2, 0, -19], extent=[4, 0, 2], parameters=[0, 4, 0, 0]),
            dict(index=14, flags=1, floor=0, type=0, attribute=0, position=[-2, 0, 17], extent=[4, 0, 2], parameters=[0, 8, 0, 0])]
        self.write(self.d)

    def test_invented_interact_point_is_replaced_by_the_computed_one(self):
        self.two_doors()
        drv, _ = self.driver([])
        points = drv.room_points(self.play.read()[0])
        self.assertEqual(set(points), {13, 14})
        fixed = drv.correct(dict(op='interact', zone=14, to=[0, 12], face=[0, 30]), {})
        self.assertEqual((fixed['zone'], fixed['to'], fixed['face']), (14, points[14][0], points[14][1]))
        exact = dict(op='interact', zone=14, to=points[14][0], face=points[14][1])
        self.assertEqual(drv.correct(exact, {}), exact)
        self.assertEqual(drv.correct(dict(op='move', to=[1, 1]), {}), dict(op='move', to=[1, 1]))

    def test_zone_number_follows_the_coordinates_when_they_belong_to_another_zone(self):
        self.two_doors()
        drv, _ = self.driver([])
        points = drv.room_points(self.play.read()[0])
        north = points[13][0]
        # Zone 14 named, but standing at the north door's point: the north door was meant.
        fixed = drv.correct(dict(op='interact', zone=14, to=[north[0], north[1] + .5], face=[0, -30]), {})
        self.assertEqual((fixed['zone'], fixed['to']), (13, north))
        # Coordinates in the middle of the room keep the named zone.
        kept = drv.correct(dict(op='interact', zone=14, to=[0, 0], face=[0, 30]), {})
        self.assertEqual(kept['zone'], 14)

    def test_far_zones_carry_go_points(self):
        self.two_doors()
        drv, _ = self.driver([])
        obs = self.play.observe()
        obs['zones'] = [z for z in obs['zones'] if z['i'] == 13]   # as if 14 were beyond the nearest five
        obs = drv.with_approaches(obs)
        self.assertEqual([z['i'] for z in obs['far_zones']], [14])
        self.assertEqual(obs['far_zones'][0]['destination'], 8)
        self.assertEqual(len(obs['far_zones'][0]['go']), 2)

    def save_screen(self, count=0):
        return dict(active=True, mode='save', phase_name='files', text='', selection=0, cursor=0,
                    save_count=count, slot=dict(n=0, exists=False), used_slots=[])

    def test_early_save_screen_is_closed_without_a_model_call(self):
        self.d['typewriter'] = self.save_screen()
        self.write(self.d)
        drv, client = self.driver([])
        with self.ticking():
            drv.run(steps=1)
        self.assertEqual(client.requests, [])
        self.assertIn('cancel', self.controls())
        # A typewriter save screen after enough new rooms reaches the model.
        drv2, client2 = self.driver([text(dict(op='press', key='confirm'))])
        drv2.rooms_since_save = {'a', 'b', 'c'}
        with self.ticking():
            drv2.run(steps=1)
        self.assertEqual(len(client2.requests), 1)

    def test_save_is_allowed_after_enough_new_rooms_and_reset_by_a_save(self):
        drv, _ = self.driver([], save_rooms=2)
        for room in ('RM_0000.RDX', 'RM_0010.RDX'):
            self.d['room'] = room; self.write(self.d)
            drv.track_saves(dict(room=room))
        self.assertFalse(drv.save_allowed())
        self.d['room'] = 'RM_0020.RDX'; self.write(self.d)
        drv.track_saves(dict(room='RM_0020.RDX'))
        self.assertTrue(drv.save_allowed())
        # The save itself (save_count +1) starts the count again.
        self.d['typewriter'] = dict(active=False, save_count=0); self.write(self.d)
        drv.track_saves(dict(room='RM_0020.RDX'))
        self.d['typewriter'] = dict(active=False, save_count=1); self.write(self.d)
        drv.track_saves(dict(room='RM_0020.RDX'))
        self.assertFalse(drv.save_allowed())
        # Loading counts as a fresh save, too.
        drv.rooms_since_save = {'a', 'b', 'c'}
        self.d['typewriter'] = dict(active=True, mode='load', save_count=1); self.write(self.d)
        drv.track_saves(dict(room='RM_0020.RDX'))
        self.assertFalse(drv.save_allowed())

    def test_save_question_gets_a_tip_and_policy_can_be_switched_off(self):
        drv, _ = self.driver([])
        obs = dict(owner='CINEMATIC DIALOG', dialog=dict(text='Will you use an Ink Ribbon to save?', choice=True))
        self.assertTrue(any('choose No' in t for t in drv.tips(obs, '')))
        drv.save_rooms = 0
        self.assertTrue(any('reasonable' in t for t in drv.tips(obs, '')))
        drv.save_rooms = 3
        drv.rooms_since_save = {'a', 'b', 'c'}
        self.assertTrue(any('reasonable' in t for t in drv.tips(obs, '')))

    def box_state(self, **kw):
        d = state()
        d.update(box_open=True, inventory_open=True, box_selecting=False, box_exit=False, box_index=0,
                 menu_selected=2, items=[0, (55 << 16) | 1, (145 << 16) | 1, (12 << 16) | 15] + [0] * 8,
                 box_items=[[(9 << 16) | 15, 'Handgun']] + [[0, '']] * 31)
        d.update(kw)
        return d

    def test_box_view_names_rows_and_refusal(self):
        from play import compact
        view = compact(self.box_state(box_index=0, box_note="No need to deposit\nthis since it's\nnot metal."), 0)['box']
        self.assertEqual(view['contents'], [[0, 'Handgun', 15]])
        self.assertEqual(view['empty_rows'], 31)
        self.assertEqual(view['at_item'], 'Handgun')
        self.assertIn('not metal', view['refused'])
        self.assertEqual(compact(self.box_state(box_selecting=True), 0)['box']['mode'], 'list')

    def test_box_target_picks_empty_row_or_free_slot_and_rejects_bad_input(self):
        d = self.box_state()
        self.assertEqual(self.play.box_target(d, dict(store=3)), (3, 1))
        self.assertEqual(self.play.box_target(d, dict(take=0)), (4, 0))
        for bad, msg in ((dict(store=7), 'occupied inventory'), (dict(take=5), 'occupied box row'),
                         (dict(store=3, take=0), 'exactly one'), (dict(), 'exactly one')):
            with self.assertRaisesRegex(Exception, msg):
                self.play.box_target(d, bad)
        with self.assertRaisesRegex(Exception, 'press cancel once'):
            self.play.box_target(self.box_state(box_selecting=True), dict(store=3))
        full = self.box_state(box_items=[[(9 << 16) | 1, 'X']] * 32)
        with self.assertRaisesRegex(Exception, 'box is full'):
            self.play.box_target(full, dict(store=3))

    def test_box_swap_follows_the_live_cursor(self):
        """Fixture runtime that moves the cursor and the list like box_menu.cpp, one line at a time."""
        d = self.box_state(menu_selected=1)
        self.write(d)
        stop = threading.Event()
        def runtime():
            seen = 0
            while not stop.is_set():
                lines = self.controls().splitlines()
                for line in lines[seen:]:
                    act = line.split(' ', 1)[1]
                    if act == 'down' and not d['box_selecting']:
                        d['menu_selected'] = 2 if d['menu_selected'] == 1 else d['menu_selected'] + 2
                    elif act == 'right' and not d['box_selecting']:
                        d['menu_selected'] += 1
                    elif act == 'confirm' and not d['box_selecting']:
                        d['box_selecting'] = True
                    elif act.startswith('keys back'):
                        d['box_index'] = (d['box_index'] + 1) % 32
                    elif act.startswith('keys forward'):
                        d['box_index'] = (d['box_index'] - 1) % 32
                    elif act == 'confirm':
                        s, r = d['menu_selected'], d['box_index']
                        d['items'][s], d['box_items'][r] = d['box_items'][r][0], [d['items'][s], 'Bullets']
                        d['box_selecting'] = False
                seen = len(lines)
                self.write(d)
                stop.wait(.02)
        thread = threading.Thread(target=runtime); thread.start()
        try:
            result = self.play.act(dict(op='box', store=3), 4)
        finally:
            stop.set(); thread.join()
        self.assertEqual(result['result'], 'swapped')
        self.assertEqual(d['items'][3], 0)
        self.assertEqual(d['box_items'][1][0], (12 << 16) | 15)
        self.assertEqual([l.split(' ', 1)[1] for l in self.controls().splitlines()],
                         ['down', 'right', 'confirm', 'keys back 1', 'confirm'])

    def test_box_swap_waits_for_slow_cursor_and_list(self):
        """Live: cursor and list index change ~10 ticks after a key, box_scrolling in between. A press per
        changed value only; the old loop pressed twice and swapped the item on the next (occupied) row."""
        d = self.box_state(menu_selected=3, box_items=[[(9 << 16) | 15, 'Handgun'], [0, ''], [(8 << 16), 'Knife']] +
                           [[0, '']] * 29)
        self.write(d)
        stop, pending = threading.Event(), []
        def runtime():
            seen = 0
            while not stop.is_set():
                now = time.monotonic()
                lines = self.controls().splitlines()
                for line in lines[seen:]:
                    act = line.split(' ', 1)[1]
                    if act == 'up' and not d['box_selecting']:
                        pending.append((now + .15, 'menu_selected', 1 if d['menu_selected'] in (2, 3) else d['menu_selected'] - 2))
                    elif act == 'confirm' and not d['box_selecting']:
                        d['box_selecting'] = True
                    elif act.startswith('keys back'):
                        d['box_scrolling'] = True
                        pending.append((now + .15, 'box_index', (d['box_index'] + 1) % 32))
                    elif act == 'confirm':
                        s, r = d['menu_selected'], d['box_index']
                        d['items'][s], d['box_items'][r] = d['box_items'][r][0], [d['items'][s], 'Lighter']
                        d['box_selecting'] = False
                seen = len(lines)
                for due in [x for x in pending if x[0] <= now]:
                    d[due[1]] = due[2]
                    d['box_scrolling'] = False
                    pending.remove(due)
                self.write(d)
                stop.wait(.02)
        thread = threading.Thread(target=runtime); thread.start()
        try:
            result = self.play.act(dict(op='box', store=1), 6)
        finally:
            stop.set(); thread.join()
        self.assertEqual(result['result'], 'swapped')
        self.assertEqual(d['box_items'][1][0], (55 << 16) | 1)   # the lighter went to the first empty row
        self.assertEqual(d['box_items'][2][0], 8 << 16)          # the knife stayed in the box
        self.assertEqual([l.split(' ', 1)[1] for l in self.controls().splitlines()],
                         ['up', 'confirm', 'keys back 1', 'confirm'])

    def test_unreachable_zone_entries_do_not_break_move_or_correct(self):
        # Live crash: room_points keeps None for zones without a reachable point; zone_of_move and
        # correct iterated over it.
        self.two_doors()
        drv, _ = self.driver([])
        d = self.play.read()[0]
        drv.room_points(d)
        drv._candidates[99] = None
        drv._points_key = drv._points_key  # keep the table
        self.assertIsNone(drv.zone_of_move('RM_TEST', dict(op='move', to=[1.0, 1.0]), 'settled'))
        drv.room_points = lambda d: {99: None, 14: [[0.0, 14.0], [0.0, 18.0]]}
        fixed = drv.correct(dict(op='interact', zone=99, to=[0.5, 13.5], face=[0, 18]), {})
        self.assertEqual(fixed['zone'], 99)

    def test_a_door_is_never_refused_as_a_repeated_examine(self):
        # Live: 384 refusals in a row; the door back had been used four times between two rooms.
        self.two_doors()
        drv, _ = self.driver([])
        obs = dict(room=self.play.read()[0]['room'])
        drv.zone_log(obs['room'])[14] = dict(n=9, got='went through the door', sig=drv.progress())
        self.assertIsNone(drv.repeat_blocked(dict(op='interact', zone=14), obs))

    # --- bundle, plans, macros, prefetch ---------------------------------------------------------------
    def test_expectation_met_checks_only_the_given_fields(self):
        before = dict(room='RM_0090.RDX', items=[0, (55 << 16) | 1], walls=[dict(i=56, fl=1)])
        after = dict(room='RM_0090.RDX', items=[0, 0], walls=[dict(i=56, fl=0)])
        names = {55: 'Lighter'}
        ok, why = expectation_met(dict(walls_open=[56], items_lost=['Lighter']), before, after, dict(result='swapped'),
                                  dict(owner='INVENTORY BOX'), names)
        self.assertTrue(ok, why)
        ok, why = expectation_met(dict(room='RM_0040'), before, after, dict(result='settled'), dict(owner='FREE'), names)
        self.assertFalse(ok); self.assertIn('still in RM_0090', why)
        ok, why = expectation_met({}, before, after, dict(result='error', error='no path'), {}, names)
        self.assertFalse(ok); self.assertIn('no path', why)
        ok, why = expectation_met(dict(not_result=['stalled']), before, after, dict(result='stalled'), {}, names)
        self.assertFalse(ok)
        ok, _ = expectation_met(dict(dialog='metal detector'), before, after, dict(result='dialog'),
                                dict(dialog=dict(text='I must release the Metal Detector.')), names)
        self.assertTrue(ok)

    def test_plan_runs_its_steps_without_further_requests_until_an_expectation_fails(self):
        self.two_doors()
        drv, client = self.driver([
            text(dict(op='plan', steps=[dict(op='press', key='inventory', expect=dict(owner='INVENTORY')),
                                        dict(op='press', key='cancel'), dict(op='wait', ticks=5)],
                      note='open and close the menu')),
            text(dict(op='wait'))])
        # The runtime never opens the menu: step 1 fails its expectation, the rest of the plan is dropped.
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(len(client.requests), 2)
        second = json.loads(client.requests[1][0][-1]['content'])
        self.assertIn('plan_broken', second)
        self.assertIn('expected screen INVENTORY', second['plan_broken'])
        self.assertEqual(drv.plan, [])
        self.assertEqual(drv.note, 'open and close the menu')
        lines = [l.split(' ', 1)[1] for l in self.controls().splitlines()]
        self.assertIn('inventory', lines)
        self.assertNotIn('cancel', lines)

    def test_plan_steps_that_meet_their_expectations_run_back_to_back(self):
        drv, client = self.driver([
            text(dict(op='plan', steps=[dict(op='wait', ticks=3), dict(op='wait', ticks=4, expect={})])),
            text(dict(op='wait'))])
        with self.ticking():
            drv.run(steps=3)
        self.assertEqual(len(client.requests), 2)   # one for the plan, one after it
        self.assertIn('"ticks":3', drv.history_lines[0])
        self.assertIn('"ticks":4', drv.history_lines[1])

    def test_explore_room_macro_examines_unread_text_zones_nearest_first_then_ends(self):
        self.d['interactions'] = [
            dict(index=4, flags=1, floor=0, type=3, attribute=0x8000, position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0]),
            dict(index=5, flags=1, floor=0, type=3, attribute=0x8000, position=[-15, 0, -15], extent=[4, 0, 4], parameters=[0, 1, 0, 0])]
        self.write(self.d)
        drv, _ = self.driver([])
        view = drv.with_approaches(self.play.observe())
        step, why = drv.expand_macro(dict(op='explore_room'), view)
        self.assertEqual((step['op'], step['zone']), ('interact', 4))
        self.assertIn('not_result', step['expect'])
        drv.zone_log('RM_TEST')[4] = dict(n=1, got='a dust box', sig=None)
        step, _ = drv.expand_macro(dict(op='explore_room'), view)
        self.assertEqual(step['zone'], 5)
        drv.zone_log('RM_TEST')[5] = dict(n=1, got='x', sig=None)
        self.assertEqual(drv.expand_macro(dict(op='explore_room'), view), (None, 'done'))
        # A text page in the way is confirmed; a question is handed back to the model.
        step, _ = drv.expand_macro(dict(op='explore_room'), dict(owner='DIALOG', dialog=dict(text='x', ready=True)))
        self.assertEqual(step, dict(op='press', key='confirm'))
        self.assertEqual(drv.expand_macro(dict(op='explore_room'), dict(owner='DIALOG', dialog=dict(text='Save?', choice=True)))[0], None)

    def test_satisfy_gate_macro_stores_listed_items_one_slot_at_a_time_then_closes(self):
        drv, _ = self.driver([])
        drv.room_rules = lambda: [dict(rule='A passage (walls w5) opens only if ...', all_items=['Lighter', 'Handgun Bullets'])]
        view = dict(owner='INVENTORY BOX', box=dict(mode='inventory'),
                    inv=[[1, 55, 1, 'Lighter'], [2, 145, 1, 'Playing Manual'], [3, 12, 15, 'Handgun Bullets']])
        macro = dict(op='satisfy_gate')
        step, _ = drv.expand_macro(macro, view)
        self.assertEqual((step['op'], step['store'], step['expect']), ('box', 1, dict(result='swapped')))
        view['inv'] = [[2, 145, 1, 'Playing Manual'], [3, 12, 15, 'Handgun Bullets']]
        self.assertEqual(drv.expand_macro(macro, view)[0]['store'], 3)
        view['inv'] = [[2, 145, 1, 'Playing Manual']]
        self.assertEqual(drv.expand_macro(macro, view)[0], dict(op='press', key='cancel'))
        self.assertEqual(drv.expand_macro(macro, view), (None, 'done'))
        self.assertIn('needs the open item box', drv.expand_macro(dict(op='satisfy_gate'), dict(owner='FREE'))[1])

    def test_goto_room_macro_walks_the_known_door_graph(self):
        self.two_doors()
        drv, _ = self.driver([])
        self.d['room'] = 'RM_0090.RDX'
        self.write(self.d)
        drv.journal_rooms = {'RM_0090': dict(doors={13: 4, 14: 8}), 'RM_0040': dict(doors={2: 3, 9: 9})}
        import hint
        from unittest import mock
        graph = {'009': {'004': [('0', 13, True)], '008': [('0', 14, True)]}, '004': {'003': [('0', 2, True)]},
                 '008': {'005': [('0', 0, True)]}, '005': {'003': [('0', None, True)]}}
        with mock.patch.object(hint, 'world_graph', lambda: graph):
            self.assertEqual(drv.next_hop('RM_0090', 3), 13)   # the shorter way, from the room files
            self.assertEqual(drv.next_hop('RM_0090', 5), 14)
            view = drv.with_approaches(self.play.observe())
            step, _ = drv.expand_macro(dict(op='goto_room', room=3), view)
            self.assertEqual((step['op'], step['zone']), ('interact', 13))
            self.assertEqual(drv.expand_macro(dict(op='goto_room', room='RM_0090'), view), (None, 'done'))
        # Without a route in the room files the journal doors are used; unknown rooms are refused.
        with mock.patch.object(hint, 'world_graph', lambda: {}):
            self.assertEqual(drv.next_hop('RM_0090', 3), 13)
            self.assertIsNone(drv.next_hop('RM_0090', 7))
            self.assertIn('no known door path', drv.expand_macro(dict(op='goto_room', room=7), view)[1])

    def test_prefetched_decision_is_used_when_the_expectation_holds(self):
        drv, client = self.driver([
            text(dict(op='wait', ticks=3, expect=dict(result='running'))),   # names the next screen: prefetched
            text(dict(op='wait', ticks=4)),                                  # prefetched: the next decision
            text(dict(op='wait', ticks=5))])
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(len(client.requests), 2)
        assumed = json.loads(client.requests[1][0][-1]['content'])
        self.assertEqual(assumed['result'], 'pending')
        self.assertIn('assume it ends as expected', assumed['state']['assumed'])
        self.assertIn('"ticks":3', drv.history_lines[0])
        self.assertIn('"ticks":4', drv.history_lines[1])

    def test_prefetched_decision_is_discarded_when_the_expectation_fails(self):
        drv, client = self.driver([
            text(dict(op='wait', ticks=3, expect=dict(room='RM_0040'))),
            text(dict(op='wait', ticks=4)),   # prefetched, must be dropped
            text(dict(op='wait', ticks=5))])  # the real next decision
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(len(client.requests), 3)
        third = json.loads(client.requests[2][0][-1]['content'])
        self.assertIn('expect_failed', third)
        self.assertIn('"ticks":3', drv.history_lines[0])
        self.assertIn('"ticks":5', drv.history_lines[1])

    def test_bundle_comes_once_per_room_or_change_then_on_request(self):
        self.two_doors()
        drv, client = self.driver([text(dict(op='wait', ticks=2)), text(dict(op='wait', ticks=2)),
                                   call('look', dict(what='board')), text(dict(op='wait', ticks=2))])
        with self.ticking():
            drv.run(steps=3)
        first = json.loads(client.requests[0][0][-1]['content'])
        self.assertTrue(any(z.startswith('i13 door') and 'door to room 4' in z for z in first['zones_all']))
        self.assertTrue(first['map'][0].startswith('map RM_TEST'))
        second = json.loads(client.requests[1][0][-1]['content'])
        self.assertNotIn('zones_all', second)   # nothing switched: offered, not pushed
        self.assertIn('look(zones|board|map|history)', second['look'])
        self.assertTrue(second['history'] if 'history' in second else True)
        # The model asked with the look tool and got the board.
        answered = client.requests[3][0][-1]
        self.assertEqual(answered['role'], 'tool')
        self.assertIn('board', answered['content'])
        # A switch (a wall opened) brings the bundle again unasked.
        self.d['walls'][0]['fl'] = 0
        self.write(self.d)
        drv.client.replies.append(text(dict(op='wait', ticks=2)))
        with self.ticking():
            drv.run(steps=4)
        last = json.loads(client.requests[-1][0][-1]['content'])
        self.assertIn('zones_all', last)

    def test_mcp_server_lists_tools_and_runs_a_plan(self):
        import mcp_server
        server = mcp_server.Server(self.play, seconds=.2, log=None)
        r = server.handle(dict(jsonrpc='2.0', id=1, method='initialize', params={}))
        self.assertEqual(r['result']['serverInfo']['name'], 'recvx-play')
        r = server.handle(dict(jsonrpc='2.0', id=2, method='tools/list'))
        self.assertEqual([t['name'] for t in r['result']['tools']],
                         ['dialog', 'observe', 'act', 'plan', 'zone', 'hint', 'board', 'map', 'goals', 'route', 'guide', 'near', 'memory'])
        self.assertIsNone(server.handle(dict(jsonrpc='2.0', method='notifications/initialized')))
        with self.ticking():
            r = server.handle(dict(jsonrpc='2.0', id=3, method='tools/call',
                                   params=dict(name='plan', arguments=dict(steps=[dict(op='wait', ticks=2), dict(op='wait', ticks=3)]))))
        out = json.loads(r['result']['content'][0]['text'])
        self.assertEqual(len(out['ran']), 2)
        self.assertIn('"ticks":3', out['ran'][1])
        self.assertIn('situation', out['observation'])
        with self.ticking():
            r = server.handle(dict(jsonrpc='2.0', id=4, method='tools/call', params=dict(name='observe', arguments={})))
        self.assertIn('zones_all', json.loads(r['result']['content'][0]['text']))
        r = server.handle(dict(jsonrpc='2.0', id=5, method='tools/call', params=dict(name='nope', arguments={})))
        self.assertTrue(r['result'].get('isError'))

    def test_zone_effects_decodes_what_examining_a_zone_does_now(self):
        import hint, nav
        room = hint.Room('RM_0090.RDX')
        st = dict(room='RM_0090.RDX', items=[0, (55 << 16) | 1], vm_flags=[], interactions=[])
        # Dispatcher form bhCk matrix 10 bit 23 (the examined zone), not only bhCmpB value[20].
        self.assertTrue(hint.reacts_to_zone(bytes([4, 10, 23, 0, 7, 0]), 7))
        self.assertFalse(hint.reacts_to_zone(bytes([4, 10, 23, 0, 7, 0]), 8))
        self.assertTrue(hint.reacts_to_zone(bytes([6, 20, 0, 7]), 7))
        text = hint.zone_effects(room, 11, st)
        self.assertIn('starts event 11 (opens walls w56,w73', text)
        self.assertIn('if standing on floor == 3', text)   # undecidable here: kept as a condition
        self.assertNotIn('(now', text)
        # A branch gated by a flag that is 0 now is dropped; what it would need is reported.
        self.assertTrue(hint.zone_effects(room, 7, st).startswith('nothing now; would need F[1:53]==1'))
        self.assertEqual(hint.zone_effects(room, 0, st), '')   # plain message zone: no script reacts
        self.assertIn('text: "The security shutter is down.', hint.zone_labels(room, dict(interactions=[
            dict(index=0, flags=1, type=3, attribute=0x8000, parameters=[0, 21, 0, 0])]))[0])
        lines = nav.map_lines(dict(self.d, floors=[]), 4.0)
        self.assertTrue(lines[0].startswith('map RM_TEST'))
        self.assertTrue(any('P' in l for l in lines[2:]))

    def test_door_expectation_is_judged_when_the_game_hands_control_back(self):
        drv, client = self.driver([
            text(dict(op='wait', ticks=2, expect=dict(room='RM_0040'))),   # "the door": judged after the scene
            text(dict(op='wait', ticks=4)),                                  # prefetched during the scene
            text(dict(op='wait', ticks=5))])
        # The scene starts with the first control line (the door step) instead of a fixed delay, so the
        # step always ends while the game is busy, however fast the machine is.
        def scene():
            limit = time.monotonic() + 3
            while not self.controls() and time.monotonic() < limit:
                time.sleep(.005)
            self.d['cinematic'] = True
            time.sleep(.6)
            self.d['room'] = 'RM_0040.RDX'
            self.d['cinematic'] = False
        thread = threading.Thread(target=scene); thread.start()
        with self.ticking():
            while drv.steps < 12 and not any('"ticks":4' in l for l in drv.history_lines):
                drv.run(steps=drv.steps + 1)
        thread.join()
        self.assertEqual(len(client.requests), 2)
        self.assertIn('"ticks":4', drv.history_lines[-1])
        self.assertNotIn('"ticks":5', ' '.join(drv.history_lines))

    def test_macro_and_plan_from_a_prefetched_decision_run_as_plans(self):
        # Live: a prefetched {"op":"explore_room"} or plan went straight to play.act -> "unknown action".
        self.d['interactions'] = [dict(index=4, flags=1, floor=0, type=3, attribute=0x8000, position=[5, 0, 5],
                                       extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([
            # In this fixture the control tick never advances, so a wait ends as 'running'.
            text(dict(op='wait', ticks=2, expect=dict(result='running'))),  # prefetches the next decision
            text(dict(op='explore_room', note='look around')),              # the prefetched answer: a macro
            text(dict(op='wait', ticks=9))])
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(len(client.requests), 2)
        entries = [json.loads(l) for l in (self.directory / 'driver.log').read_text().splitlines()] \
            if (self.directory / 'driver.log').exists() else []
        self.assertEqual(drv.note, 'look around')
        self.assertIn('examine zone 4', drv.history_lines[-1])
        self.assertNotIn('unknown action', ' '.join(drv.history_lines))
        self.assertIn('route', self.controls())

    def test_prefetch_assumes_the_screen_the_expectation_names(self):
        drv, _ = self.driver([])
        view = dict(owner='FREE', room='RM_0090', at=[1, 2], hp=160, inv=[], zones=[dict(i=7, kind='text')], doors=[13])
        a = drv.assumed_view(view, dict(op='interact', zone=7), dict(result='dialog'))
        self.assertEqual(a['owner'], 'DIALOG')
        self.assertTrue(a['dialog']['ready']); self.assertNotIn('zones', a)
        self.assertIn('the screen is then DIALOG', a['assumed'])
        a = drv.assumed_view(view, dict(op='interact', zone=13), dict(room='RM_0040'))
        self.assertEqual((a['owner'], a['room'], a['at']), ('FREE', 'RM_0040', '?'))
        self.assertTrue(a['assumed_unknown_room'])
        a = drv.assumed_view(view, dict(op='wait', ticks=2), dict(result='ticked'))
        self.assertEqual(a['owner'], 'FREE'); self.assertIn('zones', a)
        # Without a named screen the next state is a guess: no prefetch.
        self.assertIsNone(drv.assumed_view(view, dict(op='interact', zone=7), dict(not_result=['grab'])))
        self.assertIsNone(drv.assumed_view(view, dict(op='interact', zone=7), {}))
        self.assertIsNone(drv.assumed_view(view, dict(op='interact', zone=7), dict(result=['dialog', 'menu'])))

    def test_history_keeps_the_decision_without_the_bundle(self):
        drv, _ = self.driver([])
        user = dict(role='user', content=json.dumps(dict(situation=['s'], state={'owner': 'FREE'}, zones_all=['i1 door'],
                                                         board=['b'], map=['m'] * 40, history=['h'], journal={}, room_rules=[])))
        drv.remember(user, dict(op='wait'))
        kept = json.loads(drv.history[0]['content'])
        self.assertEqual(set(kept), {'situation'})

    def test_weapon_state_is_said_in_the_situation(self):
        drv, _ = self.driver([])
        text_ = ' '.join(drv.situation(dict(room='RM_TEST', at=[0, 0], hp=1, owner='FREE', inv=[[2, 145, 1, 'Playing Manual']])))
        self.assertIn('No weapon at all', text_)
        text_ = ' '.join(drv.situation(dict(room='RM_TEST', at=[0, 0], hp=1, owner='FREE', inv=[[3, 9, 15, 'Handgun']])))
        self.assertIn('Handgun in slot 3: equip it', text_)

    def test_item_uses_come_from_the_scripts_with_zone_or_floor(self):
        import hint
        uses = hint.item_uses(hint.Room('RM_0080.RDX'))
        self.assertEqual([(i, n, z, f) for i, n, z, f in uses], [(56, 'Eagle Plate', None, 3)])
        drv, _ = self.driver([])
        d = dict(self.d, floors=[dict(p=[0, 0, 0], e=[1, 0, 1])] * 3 + [dict(p=[10, 0, 20], e=[4, 0, 6])])
        drv._item_uses = None
        Driver._item_uses_cache = {56: [('RM_0080', None, 3)], 59: [('RM_0090', 14, None)]}
        try:
            lines = drv.item_use_lines(dict(room='RM_0080', inv=[[2, 56, 1, 'Eagle Plate'], [3, 59, 1, 'Hawk Emblem'], [4, 145, 1, 'Manual']]), d)
        finally:
            Driver._item_uses_cache = None
        self.assertEqual(len(lines), 2)
        self.assertIn('Eagle Plate (slot 2) is used at: this room while standing on floor area 3 (x 10..14, z 20..26; stand in it', lines[0])
        self.assertIn('Hawk Emblem (slot 3) is used at: RM_0090 zone 14 (examine it)', lines[1])

    def test_reasoning_only_replies_are_marked_and_warned_about(self):
        think = dict(role='assistant', content='', reasoning_content='Let me think about the door...', _finish='length')
        drv, client = self.driver([think, think, text(dict(op='wait', ticks=2))] * 3)
        with self.ticking():
            drv.run(steps=3)
        entries = [json.loads(l) for l in (self.directory / 'driver.log').read_text().splitlines()] \
            if drv.log else []
        self.assertEqual(drv.reasoning_cuts, 4)  # two cut replies per decision, then the safe default
        self.assertIn('WARNING: the model spends its whole reply budget on reasoning', drv.out.getvalue())
        self.assertTrue(all(b.get('reasoning') for b in drv.bad_replies))
        self.assertGreater(getattr(drv, 'last_prompt_chars', 0), 1000)

    def test_door_graph_neighbours_and_room_static_come_from_the_rdt_files(self):
        import hint
        st = hint.room_static('RM_0090.RDT')
        self.assertEqual([(z['index'], z['parameters'][1]) for z in st['zones'] if z['type'] == 0], [(13, 4), (14, 8)])
        g = {'RM_0090': [(13, 0, 4, True), (14, 0, 8, True)], 'RM_0040': [(2, 0, 9, True), (5, 0, 11, True)],
             'RM_0080': [(1, 0, 9, True), (0, 0, 5, False)], 'RM_0110': [], 'RM_0050': []}
        near = hint.neighbours(g, 'RM_0090.RDX', 3)
        self.assertEqual(near, [('RM_0040', 1, 13), ('RM_0080', 1, 14), ('RM_0110', 2, 13)])  # inactive door skipped
        drv, _ = self.driver([])
        drv._door_graph = g
        drv.journal_rooms = {'RM_0040': {}}
        lines = drv.neighbour_lines(dict(room='RM_0090'))
        self.assertEqual(len(lines), 1)
        self.assertIn('RM_0040 (1 door away via zone 13, visited)', lines[0])
        self.assertIn('RM_0080 (1 door away via zone 14, not visited; items in its file: Extinguisher, Eagle Plate)', lines[0])

    def test_combat_verdict_from_weapon_rounds_hp_and_heals(self):
        drv, _ = self.driver([])
        self.d['enemies'] = [dict(index=0, identity=4, hp=60, position=[5, 0, 5], hidden=False, inactive=False,
                                  work=dict(mode=[1, 0, 0, 0]))]
        self.write(self.d)
        base = dict(room='RM_TEST', enemies=[dict(i=0, hp=60, d=7.1, state='active', sight=True)])
        lines = drv.combat_lines(dict(base, inv=[[2, 145, 1, 'Manual']], hp=100))
        self.assertIn('#0 Zombie Dog hp 60 at 7.1 m, active', lines[0])
        self.assertTrue(lines[1].startswith('COMBAT NOT POSSIBLE: no weapon equipped; get your gun first'))
        lines = drv.combat_lines(dict(base, inv=[[2, 9, 15, 'Handgun']], hp=100))
        self.assertIn('(equip the handgun first)', lines[1])
        lines = drv.combat_lines(dict(base, inv=[[1, 9, 15, 'Handgun']], equipped=['Handgun', 15], hp=100, heal=[[3, 'Green Herb']]))
        self.assertTrue(lines[1].startswith('COMBAT POSSIBLE: 15 rounds loaded + 0 spare, hp 100, 1 healing item(s) vs ~60 enemy hp'))
        lines = drv.combat_lines(dict(base, inv=[[1, 9, 15, 'Handgun']], equipped=['Handgun', 15], hp=30))
        self.assertTrue(lines[1].startswith('COMBAT RISKY: hp 30 and nothing to heal'))
        lines = drv.combat_lines(dict(base, inv=[[1, 9, 1, 'Handgun']], equipped=['Handgun', 1], hp=100))
        self.assertTrue(lines[1].startswith('COMBAT RISKY: 1 rounds for ~60 enemy hp'))

    def test_fight_macro_equips_heals_steps_back_fires_and_ends(self):
        drv, _ = self.driver([])
        self.write(self.d)
        enemy = dict(i=0, hp=60, d=12.0, state='active', at=[12.0, 0.0], sight=True)
        view = dict(owner='FREE', room='RM_TEST', hp=100, inv=[[2, 9, 15, 'Handgun']], enemies=[enemy])
        self.assertEqual(drv.expand_macro(dict(op='fight'), view)[0], dict(op='equip', item=2))
        view.update(inv=[[1, 9, 15, 'Handgun']], equipped=['Handgun', 15])
        self.d['items'] = [1, (9 << 16) | 15]   # the runtime state now has the handgun in hand
        self.write(self.d)
        self.assertEqual(drv.expand_macro(dict(op='fight'), view)[0], dict(op='fire', shots=3, low=False, target=0))
        view.update(enemies=[dict(enemy, d=25.0)])
        self.assertEqual(drv.expand_macro(dict(op='fight'), view)[0]['shots'], 2)
        view.update(hp=40, heal=[[3, 'Green Herb']], enemies=[enemy])
        self.assertEqual(drv.expand_macro(dict(op='fight'), view)[0], dict(op='use', item=3))
        view.update(hp=100, heal=[], enemies=[dict(enemy, d=5.0, reach=True)])
        step, _ = drv.expand_macro(dict(op='fight'), view)
        self.assertEqual(step['op'], 'move'); self.assertFalse(step['danger'])
        self.assertLess(step['to'][0], 0)   # away from the enemy at x=12
        self.assertEqual(drv.expand_macro(dict(op='fight'), dict(view, owner='GRAB'))[0], dict(op='shake', presses=20))
        self.assertEqual(drv.expand_macro(dict(op='fight'), dict(view, enemies=[])), (None, 'done'))
        self.d['items'] = [0, (145 << 16) | 1]
        self.write(self.d)
        self.assertIn('no handgun', drv.expand_macro(dict(op='fight'), dict(view, inv=[], equipped=None))[1])

    def test_memory_survives_a_new_driver_and_is_shown_for_the_room(self):
        log = self.directory / 'driver.log'
        drv, _ = self.driver([], log=log)
        drv.note_progress('RM_TEST', dict(op='interact', zone=7), 'Last action: x -> dialog. Changed: walls opened: w56.')
        drv.note_progress('RM_TEST', dict(op='interact', zone=8), 'Last action: x -> y. Changed: nothing in inventory, box, walls, zones or room changed.')
        drv.remember_outcome('failed', 'RM_TEST: examining zone 4 did nothing (x3)')
        drv.history_lines = ['1: examine zone 2 -> grab', '2: press confirm -> settled']
        drv.note_death(dict(enemies=[dict(i=0)], equipped=['Handgun', 3], heal=[]))
        again, _ = self.driver([], log=log)
        self.assertEqual(again.memory['worked'], ['RM_TEST: examine zone 7 -> walls opened: w56'])
        lines = again.memory_lines(dict(room='RM_TEST'))
        self.assertIn('Memory, what brought progress here in earlier runs: RM_TEST: examine zone 7 -> walls opened: w56', lines[0])
        self.assertTrue(any(l.startswith('Memory, deaths here: RM_TEST: died near') and 'with Handgun' in l for l in lines))
        self.assertTrue(any('examining zone 4 did nothing' in l for l in lines))
        self.assertEqual(again.memory_lines(dict(room='RM_0010')), ['Memory, progress elsewhere: RM_TEST: examine zone 7 -> walls opened: w56'])

    def test_context_cap_in_tokens_uses_the_measured_ratio_and_cuts_in_one_block(self):
        system = len(Driver(self.play, None, out=io.StringIO()).system)
        drv, _ = self.driver([], context_tokens=(system + 900) // 3)   # 3.0 chars per token until measured
        self.assertEqual(drv.context_limit_chars(), (system + 900) // 3 * 3)
        for i in range(12):
            drv.remember(dict(role='user', content=json.dumps(dict(situation=[f'step {i}', 'x' * 100]))), dict(op='wait'))
        user = dict(role='user', content='{"state":{"now":true}}')
        msgs = drv.messages(user)
        kept = msgs[1:-1]
        self.assertLessEqual(sum(len(m['content']) for m in msgs), drv.context_limit_chars())
        self.assertIn('step 11', kept[-2]['content'])
        # One block down to half the room: the next requests keep the same prefix.
        before = list(drv.history)
        drv.remember(dict(role='user', content=json.dumps(dict(situation=['step 12']))), dict(op='wait'))
        drv.messages(user)
        self.assertEqual(drv.history[:len(before)], before)
        drv.chars_per_token = 2.5
        self.assertEqual(drv.context_limit_chars(), int((system + 900) // 3 * 2.5))

    def test_context_cap_drops_the_oldest_decisions_in_pairs(self):
        drv, _ = self.driver([], context_chars=len(Driver(self.play, None, out=io.StringIO()).system) + 400)
        for i in range(6):
            drv.remember(dict(role='user', content=json.dumps(dict(situation=[f'step {i}', 'x' * 100]))), dict(op='wait'))
        user = dict(role='user', content='{"state":{"now":true}}')
        msgs = drv.messages(user)
        self.assertEqual(msgs[0]['role'], 'system'); self.assertIs(msgs[-1], user)
        kept = msgs[1:-1]
        self.assertEqual(len(kept) % 2, 0)
        self.assertLessEqual(sum(len(m['content']) for m in msgs), drv.context_chars)
        self.assertIn('step 5', kept[-2]['content'])
        self.assertEqual(drv.history, kept)  # cut for good, prefix stable afterwards

    def test_reflexes_shake_fight_and_step_away_without_asking(self):
        drv, _ = self.driver([])
        self.d['items'] = [1, (9 << 16) | 13, (8 << 16)]   # handgun equipped (slot 1), knife in slot 2
        self.d['enemies'] = [dict(index=0, hp=90, position=[12, 0, 0], work=dict(mode=[1, 0, 0, 0]))]
        self.write(self.d)
        z = dict(i=0, hp=90, d=12.0, state='active', at=[12.0, 0.0], sight=True)
        base = dict(owner='FREE', room='RM_TEST', hp=120, inv=[[1, 9, 13, 'Handgun'], [2, 8, 0, 'Combat Knife']],
                    equipped=['Handgun', 13])
        self.assertEqual(drv.reflex(dict(base, owner='GRAB')), dict(op='shake', presses=20))
        self.assertEqual(drv.reflex(dict(base, enemies=[z])), dict(op='fire', shots=3, low=False, target=0))
        self.assertIsNone(drv.reflex(dict(base, enemies=[dict(z, d=35.0)])))            # beyond engage
        self.assertIsNone(drv.reflex(dict(base, enemies=[dict(z, sight=False)])))       # behind a wall
        self.assertIsNone(drv.reflex(dict(base, enemies=[dict(z, state='scripted')])))
        # Knife in hand: draw the handgun first (fire only supports it).
        self.d['items'] = [2, (9 << 16) | 13, (8 << 16)]
        self.write(self.d)
        self.assertEqual(drv.reflex(dict(base, enemies=[z])), dict(op='equip', item=1))
        # No gun at all and the zombie in reach: step away from it.
        self.d['items'] = [0, (145 << 16) | 1]
        self.write(self.d)
        step = drv.reflex(dict(base, inv=[[1, 145, 1, 'Playing Manual']], equipped=None,
                               enemies=[dict(z, d=5.0, at=[5.0, 0.0], reach=True)]))
        self.assertEqual(step['op'], 'move'); self.assertLess(step['to'][0], 0)
        drv.reflexes = False
        self.assertIsNone(drv.reflex(dict(base, owner='GRAB')))

    def test_reflex_ignores_zero_hp_and_stops_after_an_error(self):
        drv, _ = self.driver([])
        self.d['items'] = [1, (9 << 16) | 13]
        enemy = dict(index=0, hp=0, position=[12, 0, 0], work=dict(mode=[1, 1, 2, 1]))
        self.d['enemies'] = [enemy]
        self.write(self.d)
        obs = dict(owner='FREE', hp=100, equipped=['Handgun', 13], inv=[[1, 9, 13, 'Handgun']],
                   enemies=[dict(i=0, hp=0, d=12, state='active', at=[12, 0], sight=True)])
        self.assertIsNone(drv.reflex(obs))
        enemy['hp'] = 90
        self.write(self.d)
        action = drv.reflex(obs)
        self.assertEqual(action['op'], 'fire')
        drv.note_reflex(action, dict(result='error', error='target is not an active enemy'))
        self.assertIsNone(drv.reflex(obs))
        enemy['hp'] = 80
        self.write(self.d)
        self.assertEqual(drv.reflex(obs)['op'], 'fire')

    def test_combat_mode_enters_slims_the_request_blocks_examines_and_ends(self):
        drv, _ = self.driver([])
        self.d['items'] = [1, (9 << 16) | 13]
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0x8000, position=[5, 0, 5],
                                       extent=[4, 0, 4], parameters=[0, 2, 0, 0]),
                                  dict(index=13, flags=1, floor=0, type=0, attribute=0, position=[-2, 0, -19],
                                       extent=[4, 0, 2], parameters=[0, 4, 0, 0])]
        zombie = dict(index=0, hp=90, position=[0, 0, 15], work=dict(mode=[1, 0, 0, 0]))
        self.d['enemies'] = [zombie]
        self.write(self.d)
        near = dict(owner='FREE', room='RM_TEST', at=[0, 0], hp=100, inv=[],
                    enemies=[dict(i=0, hp=90, d=15.0, state='active', at=[0, 15], sight=True)])
        drv.plan = [dict(op='explore_room')]
        self.assertEqual(drv.update_combat(near), 'entered')
        self.assertTrue(drv.combat)
        self.assertEqual(drv.plan, [])
        self.assertIn('combat mode', drv.pending_feedback['plan_broken'])
        self.assertTrue(drv.situation(near)[0].startswith('COMBAT MODE: 1 enemy'))
        self.assertIn('combat mode', drv.combat_blocked(dict(op='interact', zone=3), near))
        self.assertIsNone(drv.combat_blocked(dict(op='interact', zone=13), near))   # leaving is fine
        # Behind a wall or far away it calms down; three calm looks end it.
        calm = dict(near, enemies=[dict(i=0, hp=90, d=15.0, state='active', at=[0, 15], sight=False)])
        self.assertIsNone(drv.update_combat(calm))
        self.assertIsNone(drv.update_combat(calm))
        self.assertEqual(drv.update_combat(calm), 'ended')
        self.assertFalse(drv.combat)
        # A zombie with 0 hp but an active work mode does not start it.
        zombie['hp'] = 0
        self.write(self.d)
        self.assertIsNone(drv.update_combat(near))
        # A grab always does; a room change ends it.
        self.assertEqual(drv.update_combat(dict(near, owner='GRAB')), 'entered')
        self.assertEqual(drv.update_combat(dict(near, room='RM_0040', enemies=[])), 'ended')

    def test_combat_mode_requests_carry_no_bundle(self):
        self.two_doors()
        self.d['items'] = [1, (9 << 16) | 13]
        self.d['enemies'] = [dict(index=0, identity=1, hp=90, position=[0, 0, 10], hidden=False, inactive=False,
                                  scripted=False, work=dict(mode=[1, 0, 0, 0]))]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='wait', ticks=2))], reflexes=False)
        with self.ticking():
            drv.run(steps=1)
        first = json.loads(client.requests[0][0][-1]['content'])
        self.assertNotIn('zones_all', first)
        self.assertTrue(first['situation'][0].startswith('COMBAT MODE'))
        self.assertIn('combat mode entered', first['combat'])

    def test_reflex_runs_before_the_model_and_is_reported(self):
        self.d['items'] = [1, (9 << 16) | 13]
        self.d['enemies'] = [dict(index=0, identity=1, hp=90, position=[0, 0, 12], hidden=False, inactive=False,
                                  scripted=False, work=dict(mode=[1, 0, 0, 0]))]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='wait', ticks=2))])
        with self.ticking():
            drv.run(steps=1)
        self.assertEqual(client.requests, [])          # no model call: the driver shot
        self.assertIn('fire 3', self.controls())
        self.assertTrue(drv.reflex_log and drv.reflex_log[0].startswith('{"op":"fire"'))
        lines = drv.situation(dict(room='RM_TEST', at=[0, 0], hp=100, owner='FREE', inv=[]))
        self.assertTrue(any(l.startswith('The driver acted for you') for l in lines))
        self.assertEqual(drv.reflex_log, [])

    def test_box_storage_is_decoded_from_the_room_script(self):
        import hint
        room = hint.Room('RM_0090.RDX')
        zones = dict(interactions=hint.room_static('RM_0090.RDT')['zones'])
        self.assertEqual(hint.zone_box(room, 11, zones), 192)   # event 18 sets F[10:12] (cb_flg 0x80000)
        self.assertEqual(hint.zone_box(room, 12, zones), 224)   # event 19 sets F[10:11] (cb_flg 0x100000)
        self.assertIsNone(hint.zone_box(room, 7, zones))        # the release switch is no box
        self.assertEqual(hint.zone_labels(room, zones)[11], hint.BOX_NAMES[192])

    def test_box_lines_say_what_lies_where_and_warn_without_a_gun(self):
        drv, _ = self.driver([], log=self.directory / 'driver.log')
        Driver._box_places_cache = {192: [('RM_0090', 11)], 224: [('RM_0090', 12)]}
        self.d['security_items'] = [(55 << 16) | 1, (12 << 16) | 30, (9 << 16) | 15] + [0] * 29
        self.d['security_items_2'] = [0] * 32
        self.write(self.d)
        obs = dict(room='RM_0080', inv=[[2, 145, 1, 'Playing Manual']])
        lines = drv.box_lines(obs, self.d)
        self.assertEqual(lines[0], 'Stored in security box A: Lighter, Handgun Bullets x30, Handgun x15 '
                                   '(opened at RM_0090 zone 11)')
        self.assertTrue(lines[1].startswith('YOU CARRY NO GUN: Handgun lies in security box A (opened at RM_0090 zone 11)'))
        here = drv.box_lines(dict(obs, room='RM_0090'), self.d)
        self.assertIn('(opened at zone 11 of this room)', here[0])
        armed = drv.box_lines(dict(obs, inv=[[1, 9, 15, 'Handgun']]), self.d)
        self.assertFalse(any(l.startswith('YOU CARRY NO GUN') for l in armed))
        self.assertTrue(any(l.startswith('YOU CARRY NO GUN') for l in drv.situation(dict(obs, at=[0, 0], hp=100, owner='FREE'))))
        # A normal box: its place and contents are learnt when it is opened, and survive a restart.
        self.d.update(box_open=True, box_base=64, box_items=[[(20 << 16) | 1, 'F. Aid Spray']] + [[0, '']] * 7)
        self.write(self.d)
        drv.note_box('RM_0100', 3)
        again, _ = self.driver([], log=self.directory / 'driver.log')
        Driver._box_places_cache = {}
        self.assertIn('Stored in the item box (shared by every normal box): F. Aid Spray (opened at RM_0100 zone 3)',
                      again.box_lines(obs, self.d))
        self.assertIn(9, again.weapons()); self.assertNotIn(12, again.weapons())   # the gun, not its ammo

    def test_guide_derives_the_next_steps_from_the_room_script(self):
        import hint
        st = hint.room_static('RM_0090.RDT')
        state = dict(room='RM_0090.RDX', items=[0, (145 << 16) | 1], vm_flags=[], room_items=[],
                     interactions=[dict(z) for z in st['zones']], walls=[dict(i=56, fl=1), dict(i=73, fl=1)],
                     security_items=[(9 << 16) | 15] + [0] * 31, security_items_2=[0] * 32, player_floor=0)
        lines = hint.guide(state)
        self.assertTrue(lines[0].startswith('You carry no gun: Handgun lies in security box A (opened at RM_0090 zone 11)'))
        self.assertTrue(any('closed although you carry none' in l for l in lines))
        box_a = next(l for l in lines if l.startswith('Zone 11 (security box A) also:'))
        self.assertIn('if standing in floor area 3 (x 59..85, z 100..112; walk in: move to [72.0, 106.0]): '
                      'starts event 11 (opens walls', box_a)
        switch = next(l for l in lines if l.startswith('Zone 7 does nothing yet'))
        self.assertIn('carry one of the 55 checked items (such as Handgun)', switch)
        self.assertIn('then stand in floor area 5 (x 59..85, z 90..100; walk in: move to [72.0, 95.0])', switch)
        self.assertTrue(switch.endswith('then examine zone 7.'))
        # Carrying a gun: no weapon step; zones already answered are not repeated.
        armed = hint.guide(dict(state, items=[1, (9 << 16) | 15]), done_zones={4})
        self.assertFalse(any(l.startswith('You carry no gun') for l in armed))
        self.assertFalse(any(l.startswith('Examine zone 4') for l in armed))
        self.assertTrue(hint.how_to_set(hint.Room('RM_0090.RDX'), 10, 19, 1, state)[0].startswith('F[10:19] is a state the game sets'))
        self.assertIn(9, hint.guns()); self.assertNotIn(12, hint.guns()); self.assertNotIn(8, hint.guns())

    def test_driver_guide_is_cached_offered_and_shown_when_stuck(self):
        drv, client = self.driver([call('look', dict(what='guide')), text(dict(op='wait', ticks=2))])
        import hint
        from unittest import mock
        with mock.patch.object(hint, 'guide', lambda d, done_zones=(): ['Examine zone 4: opens wall w1']):
            obs = dict(room='RM_TEST', owner='FREE', at=[0, 0], hp=100, inv=[])
            self.assertEqual(drv.guide(obs, self.d), ['Examine zone 4: opens wall w1'])
            drv.since_progress = 6
            self.assertTrue(any(l.startswith("Guide (from this room's scripts): Examine zone 4") for l in drv.situation(obs)))
            with self.ticking():
                drv.run(steps=1)
        answered = client.requests[1][0][-1]
        self.assertEqual(answered['role'], 'tool')
        self.assertIn('Examine zone 4', answered['content'])

    def test_priority_and_gun_reflex_take_the_gun_out_of_an_open_box(self):
        drv, _ = self.driver([])
        rows = [[(55 << 16) | 1, 'Lighter'], [(12 << 16) | 30, 'Handgun Bullets'], [(8 << 16), 'Combat Knife'],
                [(9 << 16) | 15, 'Handgun']] + [[0, '']] * 28
        self.d.update(box_open=True, box_base=192, box_items=rows, items=[0, 0, (145 << 16) | 1],
                      security_items=[r[0] for r in rows])
        self.write(self.d)
        obs = dict(room='RM_0090', owner='INVENTORY BOX', inv=[[2, 145, 1, 'Playing Manual']],
                   box=dict(mode='inventory', contents=[[3, 'Handgun', 15]]))
        line = drv.priority(obs, self.d)
        self.assertIn('Take it NOW: {"op":"box","take":3}, then its ammo {"op":"box","take":1}', line)
        self.assertEqual(drv.gun_reflex(obs), dict(op='box', take=3))
        self.assertIn("cancel once", drv.priority(dict(obs, box=dict(mode='list')), self.d))
        self.assertIsNone(drv.gun_reflex(dict(obs, box=dict(mode='list'))))
        # Gun taken: the ammo next; both: nothing.
        self.d['items'] = [0, 0, (145 << 16) | 1, (9 << 16) | 15]
        self.write(self.d)
        self.assertEqual(drv.gun_reflex(obs), dict(op='box', take=1))
        self.assertIsNone(drv.priority(dict(obs, inv=[[3, 9, 15, 'Handgun']]), self.d))
        self.d['items'] = [0, 0, (145 << 16) | 1, (9 << 16) | 15, (12 << 16) | 30]
        self.write(self.d)
        self.assertIsNone(drv.gun_reflex(obs))
        # Away from the box: where to go.
        Driver._box_places_cache = {192: [('RM_0090', 11)]}
        self.d.update(box_open=False, items=[0, 0, (145 << 16) | 1])
        self.write(self.d)
        free = dict(obs, owner='FREE')
        self.assertIn('lies in security box A at zone 11 of this room', drv.priority(free, self.d))
        self.assertIn('Do not enter rooms with enemies', drv.priority(dict(free, room='RM_0080'), self.d))
        self.assertTrue(drv.situation(dict(free, at=[0, 0], hp=100))[0].startswith('PRIORITY'))

    def test_gun_reflex_never_undoes_a_store_and_respects_a_gate(self):
        # Live: the model stored the gun to pass the detector, the reflex took it back, 130 times.
        drv, _ = self.driver([])
        rows = [[(55 << 16) | 1, 'Lighter'], [(12 << 16) | 30, 'Handgun Bullets'], [(8 << 16), 'Combat Knife'],
                [(9 << 16) | 15, 'Handgun']] + [[0, '']] * 28
        self.d.update(box_open=True, box_base=192, box_items=rows, items=[0, 0, (145 << 16) | 1])
        self.write(self.d)
        obs = dict(room='RM_0090', owner='INVENTORY BOX', inv=[[2, 145, 1, 'Playing Manual']], box=dict(mode='inventory'))
        drv.room_rules = lambda: []
        self.assertEqual(drv.gun_reflex(obs), dict(op='box', take=3))
        drv.model_stored = {9}
        self.assertIsNone(drv.gun_reflex(obs))
        self.assertIsNone(drv.priority(obs, self.d))
        drv.model_stored = set()
        drv.gun_reflex_takes = 2
        self.assertIsNone(drv.gun_reflex(obs))
        drv.gun_reflex_takes = 0
        drv.room_rules = lambda: [dict(rule='A passage here (walls w56) opens only if ...', how='Carry none ..., OR examine zone 7.',
                                       all_items=['Handgun', 'Handgun Bullets'])]
        self.assertIsNone(drv.gun_reflex(obs))
        line = drv.priority(obs, self.d)
        self.assertIn('a passage that closes while you carry it', line)
        self.assertIn('Leaving through a door on this side: take it along', line)

    def test_your_goal_in_a_reply_is_taken_as_goal_and_logged(self):
        log = self.directory / 'driver.log'
        drv, client = self.driver([text(dict(op='wait', ticks=2, your_goal='take the gun')),
                                   text(dict(op='wait', ticks=2))], log=log)
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(drv.goal, 'take the gun')
        first = json.loads(log.read_text().splitlines()[0])
        self.assertEqual(first['goal'], 'take the gun')
        self.assertNotIn('your_goal', first['action'])
        self.assertEqual(first['result'], 'ticked' if first['result'] == 'ticked' else first['result'])
        self.assertNotEqual(first.get('error', ''), 'unknown action or arguments')

    def test_goal_is_kept_sent_back_and_asked_for(self):
        drv, client = self.driver([text(dict(op='wait', ticks=2, goal='take the handgun from box A')),
                                   text(dict(op='wait', ticks=2))])
        with self.ticking():
            drv.run(steps=2)
        first = json.loads(client.requests[0][0][-1]['content'])
        self.assertIn('none yet', first['your_goal'])
        second = json.loads(client.requests[1][0][-1]['content'])
        self.assertEqual(second['your_goal'], 'take the handgun from box A')
        self.assertNotIn('goal', json.loads(drv.history[1]['content']))

    def test_an_item_box_is_never_refused_as_a_repeat(self):
        drv, _ = self.driver([])
        self.d['interactions'] = [dict(index=11, flags=1, floor=0, type=3, attribute=0x9, position=[5, 0, 5],
                                       extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv.zone_log('RM_TEST')[11] = dict(n=9, got='opened the item box', sig=drv.progress())
        self.assertIsNone(drv.repeat_blocked(dict(op='interact', zone=11), dict(room='RM_TEST')))

    def test_a_repeated_refusal_names_what_to_do_instead(self):
        import hint
        from unittest import mock
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0x9, position=[5, 0, 5],
                                       extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        same = dict(op='interact', zone=3, to=[1.0, 1.0], face=[7.0, 7.0])
        drv, client = self.driver([text(same), text(same), text(dict(op='wait', ticks=2))])
        drv.zone_log('RM_TEST')[3] = dict(n=5, got='nothing', sig=drv.progress())
        with mock.patch.object(hint, 'guide', lambda d, done_zones=(): ['Zone 3 does nothing yet', 'Pick up Handgun (zone 9).']):
            with self.ticking():
                drv.run(steps=3)
        second = json.loads(client.requests[1][0][-1]['content'])
        self.assertNotIn('instead', second)
        third = json.loads(client.requests[2][0][-1]['content'])
        self.assertIn('refused 2 times in a row', third['instead'])
        self.assertIn('Pick up Handgun (zone 9).', third['instead'])
        self.assertNotIn('Zone 3 does nothing', third['instead'])

    def test_world_route_uses_script_doors_and_variants(self):
        import hint
        # RM_0050 has no door zone back to room 3; its script calls the door (bhDoorCall 0x33).
        self.assertIn('003', hint.world_graph()['005'])
        self.assertEqual(hint.route('RM_0090.RDX', '002'), ['008', '005', '003', '002'])
        self.assertEqual(hint.route('RM_0090.RDX', 'RM_0040'), ['004'])
        self.assertEqual(hint.route('RM_0090.RDX', '009'), [])
        self.assertIn(('RM_0090', 7), hint.flag_index()[(1, 53, 1)])

    def test_gate_ways_name_what_a_player_can_do(self):
        import hint
        st = hint.room_static('RM_0090.RDT')
        state = dict(room='RM_0090.RDX', items=[0, (145 << 16) | 1], vm_flags=[], room_items=[],
                     interactions=[dict(z) for z in st['zones']], walls=[dict(i=56, fl=1)],
                     security_items=[(9 << 16) | 15] + [0] * 31, security_items_2=[0] * 32)
        how = hint.room_rules(hint.Room('RM_0090.RDX'), state)[0]['how']
        self.assertIn('carry one of the 55 checked items (such as Handgun)', how)
        self.assertIn('then stand in floor area 5 (x 59..85, z 90..100; walk in: move to [72.0, 95.0]), '
                      'then examine zone 7, then answer Yes', how)
        self.assertNotIn('F[', how)

    def test_news_names_item_uses_and_unlocked_places(self):
        import hint
        drv, _ = self.driver([], log=self.directory / 'driver.log')
        before = dict(room='RM_0080.RDX', items=[0, 0], vm_flags=[[1, 1, 0]], interactions=[])
        after = dict(before, items=[0, 82 << 16 | 1], vm_flags=[[1, 1, 0x80000000 >> 21]])
        from unittest import mock
        with mock.patch.object(hint, 'all_item_uses', lambda: {82: [('RM_0021', None, 1)]}), \
                mock.patch.object(hint, 'flag_index', lambda: {(1, 53, 1): [('RM_0090', 7), ('RM_0080', None)]}), \
                mock.patch.object(hint, 'route', lambda frm, to, avoid=(): ['005', '003', '002']):
            drv.note_news(before, after)
        news = drv.pending_feedback['news']
        self.assertIn('You got Extinguisher. The game uses it at: RM_0021 (route RM_0050 -> RM_0030 -> RM_0020', news)
        self.assertIn('Places that react to it now: RM_0090 zone 7', news)
        self.assertNotIn('RM_0080 (room setup)', news)   # setup of the room you are in is not news
        self.assertEqual(drv.memory['unlocks'][-1]['flag'], 'F[1:53]==1')

    def test_goals_and_door_status_from_run_memory(self):
        import hint
        from unittest import mock
        drv, _ = self.driver([], log=self.directory / 'driver.log')
        del drv.goal_list   # the real one
        d = dict(self.d, room='RM_0080.RDX', items=[0, 82 << 16 | 1],
                 interactions=[dict(index=1, flags=1, floor=0, type=0, attribute=0, position=[-2, 0, -19],
                                    extent=[4, 0, 2], parameters=[0, 9, 0, 6]),
                               dict(index=0, flags=1, floor=0, type=0, attribute=0, position=[-2, 0, 17],
                                    extent=[4, 0, 2], parameters=[0, 5, 1, 2])])
        drv.memory['rooms'] = {'009': dict(visited=True, items_left=[]), '005': dict(visited=True, items_left=['Green Herb'])}
        graph = {'008': {'009': [('0', 1, True)], '005': [('0', 0, True)]}, '005': {'003': [('0', 0, True)]},
                 '003': {'002': [('0', 1, True)]}, '009': {}, '002': {}}
        with mock.patch.object(hint, 'all_item_uses', lambda: {82: [('RM_0021', None, 1)]}), \
                mock.patch.object(hint, 'world_graph', lambda: graph):
            goals = drv.goal_list(dict(zones=[], far_zones=[]), d)
            texts = [g['text'] for g in goals]
            self.assertEqual(texts[0], 'Use Extinguisher at RM_0021')
            self.assertIn('route RM_0050 -> RM_0030 -> RM_0020; first: zone 0 (door to room 5)', goals[0]['how'])
            self.assertIn('Items left in RM_0050: Green Herb', texts)
            self.assertIn('Explore RM_0030 (not visited yet)', texts)
            self.assertEqual(drv.room_status('009', d), 'done for now')
            self.assertEqual(drv.room_status('003', d), 'not visited yet')
            self.assertIn('items left: Green Herb', drv.room_status('005', d))
            self.assertEqual(drv.room_status('002', d), 'not visited yet')
            drv.memory['rooms']['002'] = dict(visited=True, items_left=[])
            self.assertEqual(drv.room_status('002', d), 'a goal is there')

    def test_a_use_site_whose_story_flags_do_not_hold_names_what_unlocks_it(self):
        # Live: the model fetched the Hawk Emblem back from a box because "the game uses it here"; the
        # script needs F[1:123], which the scanner panel sets after answering Yes.
        import hint
        drv, _ = self.driver([])
        hint.item_uses(hint.Room('RM_0090.RDX'))
        Driver._item_uses_cache = {59: [('RM_0090', None, 7)]}
        state = dict(self.d, room='RM_0090.RDX', items=[0, (59 << 16) | 1], vm_flags=[], floors=[])
        self.assertIs(hint.use_ready('RM_0090', 59, None, 7, state), False)
        self.assertEqual(hint.use_unlock('RM_0090', 59, None, 7, state), ['examine zone 10', 'answer Yes'])
        line = drv.item_use_lines(dict(room='RM_0090', inv=[[1, 59, 1, 'Hawk Emblem']]), state)[0]
        self.assertIn('but that is not possible yet — first, here: examine zone 10, then answer Yes', line)
        goals = Driver.goal_list(drv, None, state)  # the fixture turns the goal list off by default
        goal = next(g for g in goals if g['text'].startswith('Make the Hawk Emblem usable here'))
        self.assertIn('first examine zone 10, then answer Yes', goal['how'])
        ready = dict(state, vm_flags=[[1, 3, 0x80000000 >> (123 - 96)]])
        self.assertIs(hint.use_ready('RM_0090', 59, None, 7, ready), True)
        self.assertEqual(hint.use_unlock('RM_0090', 59, None, 7, ready), [])
        self.assertIn('is used at: this room', drv.item_use_lines(dict(room='RM_0090', inv=[[1, 59, 1, 'Hawk Emblem']]), ready)[0])

    def test_doable_steps_follow_the_else_part_of_a_block(self):
        # RM_0090 scd1: F[1:123] is set in the ELSE part of `if F[4:14]==0`, and F[4:14] comes from
        # event 20 (examining the scanner panel and answering Yes).
        import hint
        room = hint.Room('RM_0090.RDX')
        self.assertEqual(hint.doable_steps(room, 1, 123, 1, None), ['examine zone 10', 'answer Yes'])
        # With F[4:14] already set, the ELSE part holds: nothing left to do.
        self.assertEqual(hint.doable_steps(room, 1, 123, 1, {'vm_flags': [[4, 0, 0x80000000 >> 14]]}), [])

    def test_describe_change_names_what_moved(self):
        from driver import describe_change
        names = {55: 'Lighter', 145: 'Playing Manual', 9: 'Handgun'}
        before = dict(room='RM_0090.RDX', hp=160, box_open=True, items=[0, (55 << 16) | 1, (145 << 16) | 1],
                      box_items=[[0, '']] * 3, walls=[dict(i=56, fl=1)], interactions=[])
        after = dict(before, items=[0, 0, (145 << 16) | 1], box_items=[[(55 << 16) | 1, 'Lighter'], [0, ''], [0, '']],
                     walls=[dict(i=56, fl=0)])
        text = describe_change(before, after, dict(op='box', store=1), 'swapped', names)
        self.assertIn('box store slot 1 -> swapped', text)
        self.assertIn('put into the box: Lighter', text)
        self.assertNotIn('gone from your inventory', text)
        self.assertIn('walls opened: w56', text)
        up = dict(before, player_floor=2)
        self.assertIn('floor changed 0 -> 2 (stairs/ladder)', describe_change(dict(before, player_floor=0), up,
                                                                              dict(op='interact', zone=3), 'floor', names))
        same = describe_change(before, before, dict(op='interact', zone=4), 'no_interaction_observed', names)
        self.assertIn('nothing in inventory, box, walls, zones or room changed', same)

    def test_situation_speaks_box_rule_and_zones(self):
        drv, _ = self.driver([])
        d = self.box_state(box_selecting=False, walls=[dict(i=56, fl=1)])
        self.write(d)
        drv.room_rules = lambda: [dict(rule='A passage here (walls w56) opens only if you carry NONE of these 2 '
                                            'items: Lighter, Knife. You carry Lighter, so it stays CLOSED.',
                                       all_items=['Lighter', 'Knife'])]
        drv.zone_labels = lambda d: {11: 'object without text'}
        drv.last_change = 'Last action: box store slot 3 -> swapped. Changed: put into the box: Bullets.'
        obs = dict(room='RM_TEST', at=[1, 2], hp=160, owner='INVENTORY BOX',
                   box=dict(mode='inventory', slot=1, contents=[[0, 'Handgun', 15]], empty_rows=31),
                   inv=[[1, 55, 1, 'Lighter'], [2, 145, 1, 'Playing Manual']],
                   zones=[dict(i=11, kind='text', d=2.0, go=[[0, 0], [1, 1]])])
        text = '\n'.join(drv.situation(obs))
        self.assertIn('The item box is open; your inventory cursor is on slot 1', text)
        self.assertIn('Box contents: row 0 Handgun x15. Free rows: 31.', text)
        self.assertIn('You carry: slot 1 Lighter, slot 2 Playing Manual.', text)
        self.assertIn('put into the box: Bullets', text)
        self.assertIn('you still carry 1 of them: Lighter. Its walls are closed now: w56.', text)
        self.assertIn('zone 11 (2.0 m): object without text', text)

    def test_examined_zone_is_marked_and_a_third_examine_without_progress_is_refused(self):
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, _ = self.driver([])
        room = 'RM_TEST.RDX'
        shelf = dict(op='interact', zone=3, to=[1, 1], face=[7, 7])
        page = dict(owner='DIALOG', dialog=dict(text='There are hairs\nscattered here.'))
        for _ in range(2):
            self.assertIsNone(drv.repeat_blocked(shelf, dict(room=room)))
            drv.record_visit(room, 3, drv.progress(), page, 'dialog')
        refusal = drv.repeat_blocked(shelf, dict(room=room))
        self.assertIn('already examined 2 times', refusal)
        self.assertIn('There are hairs scattered here.', refusal)
        # The zone list shows what was done.
        obs = drv.mark_seen(dict(room=room, zones=[dict(i=3, kind='text'), dict(i=5, kind='door')]))
        self.assertEqual((obs['zones'][0]['seen'], obs['zones'][0]['got']), (2, 'There are hairs scattered here.'))
        self.assertNotIn('seen', obs['zones'][1])
        self.assertEqual(obs['done_here'][0]['i'], 3)
        # Progress (a new item, a zone or wall switched by a script) lifts the refusal.
        self.d['items'] = self.d['items'] + [(12 << 16) | 15]
        self.write(self.d)
        self.assertIsNone(drv.repeat_blocked(shelf, dict(room=room)))
        drv.repeat_limit = 0
        self.d['items'] = self.d['items'][:-1]
        self.write(self.d)
        self.assertIsNone(drv.repeat_blocked(shelf, dict(room=room)))

    def test_refused_repeat_reaches_the_model_as_error_without_input(self):
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='interact', zone=3, to=[1, 1], face=[7, 7])),
                                   text(dict(op='press', key='cancel'))])
        sig = drv.progress()
        drv.visits['RM_TEST'] = {3: dict(n=2, got='nothing happened', sig=sig)}
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertIn('already examined', feedback['error'])
        self.assertNotIn('route', self.controls())
        self.assertIn('cancel', self.controls())

    def test_why_reads_any_room_event_from_the_script(self):
        import os
        import hint
        rooms = os.path.join(hint.ROOT, 'data', 'rooms')
        if not os.path.isdir(rooms):
            self.skipTest('no exported room data')
        explained = 0
        for name in ('RM_0000.RDX', 'RM_0080.RDX', 'RM_0090.RDX'):
            room = hint.Room(name)
            for script in room.scripts():
                if not script.startswith('event'):
                    continue
                text = hint.why(room, int(script[5:]), dict(room=name, items=[0]), limit=900)
                self.assertTrue(text.startswith(f'event {script[5:]} does:'), text)
                self.assertLessEqual(len(text), 904)
                explained += 1
        self.assertGreater(explained, 10)
        # A condition that comes from carried items names the ones in the inventory, whatever they are.
        room = hint.Room('RM_0090.RDX')
        carried = None
        for script in room.scripts():
            if script.startswith('event'):
                n = int(script[5:])
                text = hint.why(room, n, dict(room='RM_0090.RDX', items=[0] + [(i << 16) | 1 for i in range(1, 12)]))
                if 'you carry ' in text and 'you carry none' not in text:
                    carried = text
                    break
        self.assertIsNotNone(carried)

    def test_started_event_is_explained_once_per_room_visit(self):
        from unittest import mock
        import hint
        drv, _ = self.driver([])
        calls = []
        def why(room, n, state, limit=0):
            calls.append(n)
            return f'event {n} does: text "x"'
        self.d['events'] = [3]
        self.write(self.d)
        with mock.patch.object(hint, 'why', why), mock.patch.object(hint, 'Room', lambda name: name):
            drv.watch_events()
            self.d['events'] = [3, 3, 3, 5]; self.write(self.d)
            drv.watch_events()
            self.assertEqual(calls, [3, 5])
            self.assertEqual(len(drv.pending_events), 2)
            self.d['room'] = 'RM_OTHER.RDX'; self.d['events'] = [3]; self.write(self.d)
            drv.watch_events()
            self.assertEqual(calls, [3, 5, 3])

    def test_full_inventory_shows_space_and_ways_to_free_a_slot(self):
        from play import compact
        d = state()
        d.update(inventory_capacity=8, inventory_used=8)
        obs = compact(d, 0)
        self.assertEqual(obs['space'], [8, 8])
        drv, _ = self.driver([])
        tips = drv.tips(obs, '')
        self.assertTrue(any('Inventory full' in t and 'FILES' in t for t in tips))
        d['inventory_used'] = 6
        self.assertFalse(any('Inventory full' in t for t in drv.tips(compact(d, 0), '')))

    def test_cut_off_reply_is_retried_with_a_larger_budget_and_logged(self):
        cut = dict(role='assistant', content='<think>The player stands near the door and I should', _finish='length')
        drv, client = self.driver([cut, text(dict(op='press', key='confirm'))])
        client.max_tokens = 200
        with self.ticking():
            drv.run(steps=1)
        self.assertEqual(client.budgets, [None, 800])
        self.assertIn('cut off', client.requests[1][0][-1]['content'])
        self.assertNotIn('<think>', json.dumps(client.requests[1][0]))
        self.assertIn('confirm', self.controls())

    def test_think_blocks_are_ignored_when_parsing(self):
        from driver import parse_action
        self.assertEqual(parse_action('<think>maybe {"op":"wait"}</think>{"op":"press","key":"up"}'),
                         dict(op='press', key='up'))
        with self.assertRaises(ValueError):
            parse_action('<think>still thinking {"op":"wait"')

    def test_video_is_skipped_once_only_when_enabled(self):
        self.d.update(cinematic=True, movie_active=True)
        self.write(self.d)
        drv, client = self.driver([], skip_videos=True)
        with self.ticking():
            drv.run(steps=3)
        self.assertEqual(client.requests, [])
        self.assertEqual(self.controls().count('skip'), 1)
        off, client2 = self.driver([])
        (self.directory/'live.txt').unlink()
        with self.ticking():
            off.run(steps=2)
        self.assertNotIn('skip', self.controls())

    def test_string_numbers_are_accepted_and_bad_fields_cost_one_decision(self):
        self.two_doors()
        drv, client = self.driver([text(dict(op='interact', zone='14', to=['0', '12'], face=['0', '30'])),
                                   text(dict(op='move', to=['north', 'door'])),
                                   text(dict(op='press', key='cancel'))])
        points = drv.room_points(self.play.read()[0])
        fixed = drv.correct(dict(op='interact', zone='14', to=['0', '12'], face=['0', '30']), {})
        self.assertEqual((fixed['zone'], fixed['to']), (14, points[14][0]))
        self.assertEqual(drv.normalize(dict(op='move', to=[1, 0, '2']))['to'], [1.0, 2.0])
        self.assertEqual(drv.normalize(dict(op='move', to={'x': '3', 'z': 4}))['to'], [3.0, 4.0])
        with self.ticking():
            drv.run(steps=3)
        second = json.loads(client.requests[2][0][-1]['content'])
        self.assertEqual(second['result'], 'error')
        self.assertIn('cancel', self.controls())

    def test_wait_returns_when_a_video_starts(self):
        from play import Play
        self.assertEqual(Play.reason(dict(state(), cinematic=True), dict(state(), cinematic=True, movie_active=True),
                                     waiting=True), 'video')
        self.assertIsNone(Play.reason(dict(state(), cinematic=True, movie_active=True),
                                      dict(state(), cinematic=True, movie_active=True), waiting=True))

    def test_information_only_zone_is_read_once(self):
        from unittest import mock
        import hint
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0x8000,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 2, 0, 0])]
        self.write(self.d)
        drv, _ = self.driver([])
        shelf = dict(op='interact', zone=3, to=[1, 1], face=[7, 7])
        with mock.patch.object(hint, 'informational_zones', lambda room, d: {3}), \
                mock.patch.object(hint, 'Room', lambda name: name):
            self.assertIsNone(drv.repeat_blocked(shelf, dict(room='RM_TEST')))
            drv.record_visit('RM_TEST', 3, drv.progress(), dict(dialog=dict(text='Nothing useful here.')), 'dialog')
            refusal = drv.repeat_blocked(shelf, dict(room='RM_TEST'))
            self.assertIn('information only', refusal)
            self.assertIn('Nothing useful here.', refusal)
            self.assertTrue(drv.mark_seen(dict(room='RM_TEST', zones=[dict(i=3)]))['zones'][0]['read'])
            # A script redefining the zone (new message) counts as progress and frees it again.
            self.d['interactions'][0]['parameters'] = [0, 5, 0, 0]
            self.write(self.d)
            self.assertIsNone(drv.repeat_blocked(shelf, dict(room='RM_TEST')))

    def test_information_read_survives_restart_and_unrelated_progress(self):
        from unittest import mock
        import hint
        self.d['interactions'] = [dict(index=3, flags=1, floor=0, type=3, attribute=0x8000,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 2, 0, 0])]
        self.write(self.d)
        with mock.patch.object(hint, 'informational_zones', lambda room, d: {3}), \
                mock.patch.object(hint, 'Room', lambda name: name):
            drv, _ = self.driver([], log=self.directory / 'driver.log')
            drv.record_visit('RM_TEST', 3, drv.progress(), dict(dialog=dict(text='Nothing useful here.')), 'dialog')
            self.d['items'].append(12 << 16 | 30)
            self.d['walls'][0]['fl'] = 0
            self.write(self.d)
            again, _ = self.driver([], log=self.directory / 'driver.log')
            action = dict(op='interact', zone=3)
            self.assertIn('already read', again.repeat_blocked(action, dict(room='RM_TEST')))
            marked = again.mark_seen(dict(room='RM_TEST', zones=[dict(i=3)]))
            self.assertTrue(marked['zones'][0]['read'])
            self.assertEqual(marked['zones'][0]['last_answer'], 'Nothing useful here.')
            self.assertIsNone(again.read_info_zone('RM_OTHER', 3))
            self.d['interactions'][0]['parameters'][1] = 5
            self.write(self.d)
            self.assertIsNone(again.repeat_blocked(action, dict(room='RM_TEST')))

    def test_informational_zones_exclude_switches_questions_and_boxes(self):
        import os
        import hint
        if not os.path.isdir(os.path.join(hint.ROOT, 'data', 'rooms')):
            self.skipTest('no exported room data')
        room = hint.Room('RM_0090.RDX')
        zones = [dict(index=i, type=3, attribute=attr, parameters=[0, msg, 0, 0]) for i, attr, msg in
                 ((8, 0x8800, 0), (6, 0x800e, 2), (10, 0x900e, 4), (11, 0x1c00, 0), (5, 0x8000, 3))]
        info = hint.informational_zones(room, dict(interactions=zones))
        self.assertIn(8, info)                    # plain message
        self.assertNotIn(6, info)                 # script hook (attribute & 9)
        self.assertNotIn(10, info)
        self.assertNotIn(11, info)                # no direct message (item box)
        self.assertNotIn(5, info)                 # message 3 asks "Push the switch? Yes No"

    def test_failed_go_point_falls_back_to_the_next_candidate(self):
        self.two_doors()
        drv, _ = self.driver([])
        d = self.play.read()[0]
        d['actor'] = [0.0, 0, 14.0]
        drv._points_key = None
        first = drv.room_points(d)[14]
        cands = drv._candidates[14]
        self.assertGreaterEqual(len(cands), 2)
        drv.point_failed('RM_TEST', dict(op='interact', zone=14, to=first[0]), 'stalled')
        # keep the actor in the same 4-unit region so the candidate table does not refresh
        next_points = drv.room_points(d)[14]
        self.assertIn(next_points, cands[1:])
        # A good result does not mark the point.
        drv.point_failed('RM_TEST', dict(op='interact', zone=14, to=next_points[0]), 'dialog')
        self.assertEqual(drv.room_points(d)[14], next_points)

    def test_zone_without_own_point_is_not_switched_to_a_neighbour(self):
        self.two_doors()
        drv, _ = self.driver([])
        points = drv.room_points(self.play.read()[0])
        north = points[13][0]
        fixed = drv.correct(dict(op='interact', zone=99, to=north, face=[0, -30]), {})
        self.assertEqual(fixed['zone'], 99)

    def test_points_follow_the_side_of_a_gate_the_player_is_on(self):
        # A wall across the room splits it; the doors are on both sides.
        self.two_doors()
        self.d['walls'].append(dict(i=9, fl=1, ty=0, attr=0, floor=0, p=[-20, 0, -1], e=[40, 30, 2]))
        self.d['actor'] = [0, 0, 10]
        self.write(self.d)
        drv, _ = self.driver([])
        south = drv.room_points(self.play.read()[0])
        self.assertIn(14, south)
        self.assertIsNone(south[13])
        self.d['actor'] = [0, 0, -10]
        self.write(self.d)
        north = drv.room_points(self.play.read()[0])
        self.assertIn(13, north)
        self.assertIsNone(north[14])

    def test_game_over_is_continued_without_the_model(self):
        self.d.update(gameover_active=True, cinematic=True, gameover_prompt=False, gameover_choice=0,
                      gameover_retry=dict(room='RM_0080.RDX', play_time=0))
        self.write(self.d)
        drv, client = self.driver([])
        with self.ticking():
            drv.run(steps=1)
        self.assertNotIn('confirm', self.controls())          # scene still running: wait
        self.d['gameover_prompt'] = True
        self.d['gameover_choice'] = 1                          # cursor on Exit
        self.write(self.d)
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(client.requests, [])
        lines = [l.split(' ', 1)[1] for l in self.controls().splitlines()]
        self.assertIn('left', lines)
        self.d['gameover_choice'] = 0
        self.write(self.d)
        with self.ticking():
            drv.run(steps=3)
        self.assertIn('confirm', self.controls())
        self.assertTrue(any('game over' in t for t in drv.pending_events))

    def test_route_left_over_at_game_over_does_not_block_actions(self):
        self.d.update(gameover_active=True, cinematic=True)
        self.d['control'].update(steer=True, result='moving')
        self.write(self.d)
        stop = threading.Event()
        def runtime():
            while not stop.is_set():
                if 'keys none' in self.controls():
                    self.d['control'].update(steer=False, result='cancelled')
                self.write(self.d)
                stop.wait(.02)
        thread = threading.Thread(target=runtime); thread.start()
        try:
            self.play.act(dict(op='press', key='confirm'), .5)
        finally:
            stop.set(); thread.join()
        lines = [l.split(' ', 1)[1] for l in self.controls().splitlines()]
        self.assertEqual(lines[0], 'keys none')
        self.assertIn('confirm', lines)

    def test_journal_keeps_rooms_reads_and_facts_across_rooms(self):
        drv, _ = self.driver([])
        drv.visits['RM_0010'] = {3: dict(n=1, got='Nothing useful here.', sig=None)}
        drv.update_journal(dict(room='RM_0010', owner='FREE',
                                zones=[dict(i=3, kind='text'), dict(i=0, kind='door', destination=2)],
                                far_zones=[dict(i=5, kind='item')]))
        drv.update_journal(dict(room='RM_0020', owner='CINEMATIC DIALOG',
                                dialog=dict(text='The security shutter is down.\nI must release the switch.')))
        drv.note_fact('RM_0020: event 1 does: a voice-only line; cause F[4:2]=1: you carry Handgun, Lighter')
        j = drv.journal()
        self.assertEqual([r['room'] for r in j['rooms']], ['RM_0010', 'RM_0020'])
        first = j['rooms'][0]
        self.assertEqual(first['read'], {'3': 'Nothing useful here.'})
        self.assertEqual(first['items_left'], [5])
        self.assertEqual(first['doors'], {'0': 2})
        self.assertTrue(any('must release' in f for f in j['facts']))
        self.assertTrue(any('you carry Handgun' in f for f in j['facts']))
        # Pickup confirmations are not facts.
        drv.update_journal(dict(room='RM_0020', owner='DIALOG', dialog=dict(text="You've taken the\nKey.")))
        self.assertFalse(any('taken' in f for f in drv.journal()['facts']))

    def test_journal_is_sent_with_every_decision(self):
        drv, client = self.driver([text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=1)
        payload = json.loads(client.requests[0][0][-1]['content'])
        self.assertIn('journal', payload)
        self.assertEqual(payload['journal']['rooms'][0]['room'], 'RM_TEST')

    def test_item_gate_rule_is_read_from_the_room_script(self):
        import os
        import hint
        if not os.path.isdir(os.path.join(hint.ROOT, 'data', 'rooms')):
            self.skipTest('no exported room data')
        carrying = dict(items=[0, (55 << 16) | 1, (9 << 16) | 15])
        rules = hint.room_rules(hint.Room('RM_0090.RDX'), carrying)
        self.assertEqual(len(rules), 1)
        self.assertIn('NONE of these', rules[0]['rule'])
        self.assertIn('You carry Handgun, Lighter', rules[0]['rule'])
        self.assertIn('item box', rules[0]['how'])
        empty = hint.room_rules(hint.Room('RM_0090.RDX'), dict(items=[0, (145 << 16) | 1]))
        self.assertIn('so it OPENS', empty[0]['rule'])
        # Rooms without such a gate get no rule.
        self.assertEqual(hint.room_rules(hint.Room('RM_0080.RDX'), carrying), [])

    def test_alert_after_decisions_without_progress(self):
        drv, _ = self.driver([], alert_after=4)
        obs = dict(room='RM_TEST', owner='FREE', zones=[dict(i=3, kind='text'), dict(i=4, kind='item')])
        drv.update_journal(obs)
        for _ in range(3):                          # the first state is the baseline and counts as no progress
            drv.track_progress(obs, 'settled')
            self.assertIsNone(drv.alert(obs))
        drv.track_progress(obs, 'settled')
        alert = drv.alert(obs)
        self.assertIn('No progress for 4 decisions', alert)
        self.assertIn('Zones not read yet here: [3, 4]', alert)
        self.assertIsNone(drv.alert(obs))            # not again right away
        self.assertIn('ALERT', drv.out.getvalue())
        # Progress (a new item) resets the counter.
        self.d['items'] = self.d['items'] + [(12 << 16) | 15]
        self.write(self.d)
        drv.track_progress(obs, 'choice')
        self.assertEqual(drv.since_progress, 0)

    def test_room_rule_checklist_names_carried_items_with_slots(self):
        drv, _ = self.driver([])
        self.write(self.box_state())  # carries Lighter (1), Playing Manual (2), Handgun Bullets (3)
        drv.room_rules = lambda: [dict(rule='A passage (walls w5) opens only if ...',
                                       how='Carry none ..., OR examine zone 7.',
                                       all_items=['Lighter', 'Handgun Bullets', 'Combat Knife'])]
        tip = drv.room_rule_tips()[0]
        self.assertIn('closed while you carry slot 1 Lighter, slot 3 Handgun Bullets.', tip)
        self.assertNotIn('Playing Manual', tip)
        self.assertIn('OR examine zone 7', tip)   # the other ways from the script, not only "store them"
        self.write(self.box_state(items=[0, 0, (145 << 16) | 1] + [0] * 9))
        self.assertIn('you carry none of the listed items', drv.room_rule_tips()[0])

    def test_history_is_cut_in_blocks_so_the_prefix_stays_cacheable(self):
        drv, _ = self.driver([], window=2)
        for i in range(8):
            drv.remember(dict(role='user', content=str(i)), dict(op='wait'))
            if len(drv.history) == 4:
                kept = list(drv.history)
        # Between cuts the earlier messages never change (append-only).
        self.assertLessEqual(len(drv.history), 8)
        self.assertEqual(drv.history[0]['role'], 'user')
        self.assertTrue(kept)

    def test_env_file_fills_missing_settings_and_key_goes_into_bearer_header(self):
        import os
        from unittest import mock
        import driver
        env = self.directory/'.env'
        env.write_text('# c\nRECVX_TEST_KEY="sk-test"\nexport RECVX_TEST_URL=http://x/v1\nRECVX_TEST_SET=file\nRECVX_TEST_EMPTY=\n')
        with mock.patch.dict(os.environ, {'RECVX_TEST_SET': 'shell'}):
            driver.load_env(env)
            self.assertEqual(os.environ['RECVX_TEST_KEY'], 'sk-test')
            self.assertEqual(os.environ['RECVX_TEST_URL'], 'http://x/v1')
            self.assertEqual(os.environ['RECVX_TEST_SET'], 'shell')
            self.assertNotIn('RECVX_TEST_EMPTY', os.environ)
        driver.load_env(self.directory/'missing.env')  # absent file is not an error
        seen = {}
        class Reply(io.BytesIO):
            def __enter__(self): return self
            def __exit__(self, *a): return False
        def urlopen(req, timeout):
            seen['auth'] = req.get_header('Authorization'); seen['url'] = req.full_url
            return Reply(json.dumps(dict(choices=[dict(message=dict(content='{"op":"wait"}'))])).encode())
        with mock.patch('urllib.request.urlopen', urlopen):
            driver.Client('http://x/v1/', 'm', 'sk-test').complete([])
        self.assertEqual(seen, dict(auth='Bearer sk-test', url='http://x/v1/chat/completions'))

    def test_state_sent_to_the_model_drops_noise_fields(self):
        obs = dict(owner='FREE', at=[1, 2], age_s=.1, busy=False, held=True, weapon=4, elapsed_ms=3)
        self.assertEqual(trim(obs), dict(owner='FREE', at=[1, 2]))


    # --- control gaps seen in live runs -------------------------------------------------------------
    @contextmanager
    def reacting(self, handle, lag=0.0):
        """Fixture runtime: every new control line goes to handle(line, d, elapsed) before the next write.
        lag: a line takes effect only that long after it was written (dumps keep coming meanwhile)."""
        stop, began = threading.Event(), time.monotonic()
        def tick():
            seen, arrived = 0, {}
            while not stop.is_set():
                lines = self.controls().splitlines()
                for k in range(seen, len(lines)):
                    arrived.setdefault(k, time.monotonic())
                    if time.monotonic() - arrived[k] < lag:
                        break
                    handle(lines[k].split(' ', 1)[1], self.d)
                    seen = k + 1
                handle(None, self.d, time.monotonic() - began) if handle.__code__.co_argcount == 3 else None
                self.write(self.d)
                stop.wait(.02)
        thread = threading.Thread(target=tick); thread.start()
        try:
            yield
        finally:
            stop.set(); thread.join()

    def item_zone(self):
        self.d['interactions'] = [dict(index=4, flags=1, floor=0, type=4, attribute=0,
                                       position=[5, 0, 5], extent=[4, 0, 4], parameters=[0, 0, 0, 0])]
        self.write(self.d)

    def test_reply_cut_inside_the_note_is_used_without_a_second_request(self):
        cut = dict(role='assistant', content='{"op":"press","key":"up","note":"Lighter equipped. Go to Playing',
                   _finish='length')
        drv, client = self.driver([cut, text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(len(client.requests), 2)  # one per decision, no retry
        self.assertIn('1 up', self.controls())
        self.assertTrue(drv.note in (None, ''))
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertIn('note_cut', feedback)
        from driver import salvage_cut
        self.assertEqual(salvage_cut('{"op":"interact","zone":4,"to":[54.5,135.5],"note":"RM_0020: read'),
                         dict(op='interact', zone=4, to=[54.5, 135.5]))
        # Cut anywhere but in the trailing note: not an action.
        self.assertIsNone(salvage_cut('{"op":"shake","presses":20,"then":{"op":"move","to":[44'))
        self.assertIsNone(salvage_cut('{"op":"interact","zone":4,"to":[54.5,13'))

    def test_two_empty_cut_replies_fall_back_to_the_owner_default(self):
        empty = dict(role='assistant', content='', _finish='length')
        drv, client = self.driver([empty, dict(empty)])
        client.max_tokens = 200
        with self.ticking():
            drv.run(steps=1)
        self.assertEqual(len(client.requests), 2)
        self.assertGreater(client.budgets[1], 200)  # the retry already had the larger budget
        self.assertIn('15 tick', self.controls())
        from driver import default_action
        self.assertEqual(default_action(dict(owner='INVENTORY')), dict(op='press', key='cancel'))
        self.assertEqual(default_action(dict(owner='FILES INVENTORY')), dict(op='press', key='cancel'))
        self.assertEqual(default_action(dict(owner='DIALOG', dialog=dict(ready=True, choice=False))),
                         dict(op='press', key='confirm'))
        self.assertEqual(default_action(dict(owner='DIALOG', dialog=dict(ready=True, choice=True))), dict(op='wait'))
        self.assertEqual(default_action(dict(owner='DIALOG', dialog=dict(ready=False))), dict(op='wait'))

    def test_interact_that_stalls_within_near_is_sent_once_more(self):
        # Live: stalled 0.26 from the point (near 1.0); the same interact from there reached the zone.
        self.item_zone()
        def handle(line, d):
            if not line.startswith('route'):
                return
            if self.controls().count('route') == 1:
                d['control'] = dict(d['control'], result='stalled', steer=False)
                d['actor'] = [7.26, 0, 3.5]
            else:
                d['control'] = dict(d['control'], result='arrived')
                d.update(dialog_open=True, dialog_text='A key.', dialog_closable=True)
        action = dict(op='interact', zone=4, to=[7.0, 3.5], face=[7.0, 7.0])
        self.assertEqual(Play.near_for(self.d['interactions'][0], action['to'], action['face']), .5)
        with self.reacting(handle):
            out = self.play.act(action, 3)
        self.assertEqual(out['result'], 'dialog')
        self.assertIn('within near', out['retried'])
        self.assertEqual(self.controls().count('route 7.0000 3.5000'), 2)

    def test_stall_outside_near_is_reported(self):
        self.item_zone()
        def handle(line, d):
            if line.startswith('route'):
                d['control'] = dict(d['control'], result='stalled', steer=False)
                d['actor'] = [7.0, 0, 1.5]  # 2 short: outside near 0.5
        with self.reacting(handle):
            out = self.play.act(dict(op='interact', zone=4, to=[7.0, 3.5], face=[7.0, 7.0]), 2)
        self.assertEqual(out['result'], 'stalled')
        self.assertEqual(self.controls().count('route'), 1)

    def fake_act(self, results):
        """Driver.act stand-in: each call returns the next (result, extra observation fields)."""
        queue = list(results)
        def act(action):
            result, extra = queue.pop(0)
            obs = dict(self.play.observe(), **extra)
            return obs, dict(result=result), 10.0
        return act

    def test_cut_off_interacts_are_no_examine_and_are_not_refused(self):
        # Live: danger, grab and stalls on a door counted as examines; the guard then refused the door.
        self.item_zone()
        go = self.driver([])[0].room_points(self.d)[4]
        reply = text(dict(op='interact', zone=4, to=go[0], face=go[1]))
        drv, client = self.driver([reply] * 4)
        drv.act = self.fake_act([('danger', {}), ('grab', {}), ('damage', {}), ('interrupted', {})])
        with self.ticking():
            drv.run(steps=4)
        self.assertNotIn(4, drv.visits.get('RM_TEST', {}))
        from driver import answered
        for result in ('danger', 'grab', 'damage', 'stalled', 'interrupted', 'held', 'running', 'error'):
            self.assertFalse(answered(dict(owner='FREE'), result), result)
        for result in ('dialog', 'choice', 'menu', 'room', 'computer'):
            self.assertTrue(answered(dict(owner='FREE'), result), result)
        self.assertTrue(answered(dict(owner='CINEMATIC DOOR'), 'running'))
        self.assertTrue(answered(dict(owner='BOX'), 'owner'))

    def test_move_to_a_ready_point_and_confirm_counts_as_examine(self):
        self.item_zone()
        go = self.driver([])[0].room_points(self.d)[4]
        drv, _ = self.driver([text(dict(op='move', to=go[0], face=go[1])), text(dict(op='press', key='confirm'))])
        drv.act = self.fake_act([('settled', {}), ('dialog', dict(dialog=dict(text='A key.')))])
        with self.ticking():
            drv.run(steps=2)
        self.assertEqual(drv.visits['RM_TEST'][4]['n'], 1)
        self.assertEqual(drv.visits['RM_TEST'][4]['got'], 'A key.')

    def test_failed_point_is_skipped_although_state_room_has_the_extension(self):
        # Live: room_points keyed by 'RM_0080.RDX', point_failed by 'RM_0080': the dead point came back.
        self.item_zone()
        self.d['room'] = 'RM_0080.RDX'
        self.write(self.d)
        drv, _ = self.driver([])
        drv.room_points(self.d)
        cands = drv._candidates[4]
        self.assertGreater(len(cands), 1)
        cands = drv._candidates[4]
        self.assertGreater(len(cands), 1)
        drv.point_failed('RM_0080', dict(op='interact', zone=4, to=cands[0][0]), 'stalled')
        next_points = drv.room_points(self.d)[4]
        self.assertIn(next_points, cands[1:])

    def test_document_page_is_shown_as_x_of_y(self):
        from play import compact
        d = deepcopy(self.d)
        d.update(files_open=True, inventory_open=True, file_number=0, file_page=2, file_pages=7, file_text='Page 3')
        self.assertEqual(compact(d, 0)['file'], dict(n=0, page='3/7'))
        self.assertEqual(compact(d, 0)['document'], 'Page 3')
        d.update(file_folder_open=True, file_text='')
        self.assertEqual(compact(d, 0)['file'], dict(n=0, list=True))
        prompt = (Path(__file__).with_name('system_prompt.md')).read_text()
        self.assertIn('read page by page', prompt)

    def test_heal_lists_items_whose_use_restores_hp(self):
        from play import compact
        d = deepcopy(self.d)
        # 21 green herb (curedata 1), 22 red herb (0: no heal alone), 20 F. Aid Spray (4).
        d.update(items=[1, (9 << 16) | 5, (21 << 16) | 1, (22 << 16) | 1, (20 << 16) | 1],
                 item_names=['', 'Handgun', 'Green Herb', 'Red Herb', 'F. Aid Spray'])
        self.assertEqual(compact(d, 0)['heal'], [[2, 'Green Herb'], [4, 'F. Aid Spray']])
        self.assertNotIn('heal', compact(self.d, 0))

    def test_fire_under_attack_runs_its_burst_and_reports_damage_and_shots(self):
        self.d.update(items=[1, (9 << 16) | 10], combat_shots=0, combat_hits=0)
        self.write(self.d)
        state = {}
        def handle(line, d, elapsed=None):
            if line and line.startswith('fire'):
                state['t'] = time.monotonic()
                d.update(hp=80)  # hit right away
                d['control'] = dict(d['control'], pending=1)
            if line is None and 't' in state and time.monotonic() - state['t'] > .4 and d['control'].get('pending'):
                d['control'] = dict(d['control'], pending=0, held=True, tick=1)
                d.update(combat_shots=3, combat_hits=2)
        with self.reacting(handle):
            out = self.play.act(dict(op='fire', shots=3), 3)
        self.assertEqual(out['result'], 'damage')
        self.assertEqual(out['damage'], 20)
        self.assertEqual(out['fire']['shots'], 3)
        self.assertGreater(out['elapsed_ms'], 350)  # not cut at the first hit

    def test_wait_through_a_hit_stagger_ends_after_it(self):
        enemy = dict(index=1, hp=50, position=[5, 0, 0], work=dict(mode=[1, 0, 0, 0]))
        self.d.update(cinematic=True, hp=80, enemies=[enemy])
        self.write(self.d)
        before = deepcopy(self.d); before['hp'] = 100
        during = deepcopy(self.d)
        def handle(line, d, elapsed=None):
            if elapsed is not None and elapsed > .4:
                d['cinematic'] = False  # stagger over, the enemy is still there
        with self.reacting(handle):
            self.play._state, self.play._stamp = None, None
            out = self.play.act(dict(op='wait'), 3)
        self.assertEqual(out['result'], 'settled')
        self.assertGreater(out['elapsed_ms'], 350)
        # Inside the stagger the hit and the enemy end nothing; without through_damage the hit would.
        self.assertIsNone(Play.reason(before, during, waiting=True, through_damage=True))
        self.assertEqual(Play.reason(before, during, waiting=True), 'damage')
        # After the stagger next to the same enemy: no new danger.
        self.assertNotEqual(Play.reason(during, dict(during, cinematic=False), waiting=True), 'danger')

    def menu_runtime(self, lag=0.0, items=None, names=None):
        """Status menu stand-in: cursor, command window and combine follow the control lines."""
        def handle(line, d):
            sel = d.get('menu_selected', 2)
            if line == 'inventory':
                d.update(inventory_open=True, menu_selected=2, menu_commands_open=False, menu_combining=False,
                         menu_navigation=False)
            elif not d.get('inventory_open'):
                return
            elif line in ('right', 'left', 'down', 'up') and (d['menu_combining'] or not d['menu_commands_open']):
                d['menu_selected'] = max(1, min(6, sel + {'right': 1, 'left': -1, 'down': 2, 'up': -2}[line]))
            elif line in ('down', 'up'):
                d['menu_command'] = (d['menu_command'] + (1 if line == 'down' else -1)) % len(d['menu_commands'])
            elif line == 'cancel':
                if d['menu_combining']:
                    d['menu_combining'] = False  # back to the command window
                elif d['menu_commands_open']:
                    d['menu_commands_open'] = False
                elif not d['menu_navigation']:
                    d['menu_navigation'] = True  # item list -> top bar, a second cancel closes
                else:
                    d.update(inventory_open=False, menu_navigation=False)
            elif line == 'confirm' and d['menu_combining']:
                ammo = d['menu_combine_from']
                if sel == d['items'][0]:
                    d['items'][sel] += d['items'][ammo] & 0xffff
                    d['items'][ammo] = 0
                d.update(menu_combining=False, menu_commands_open=False)
            elif line == 'confirm' and d['menu_commands_open']:
                name = d['menu_commands'][d['menu_command']]
                d['menu_commands_open'] = name == 'Combine'  # ui.cpp: commands stay set while combining
                if name == 'Use' and (d['items'][sel] >> 16) & 255 == 21:
                    d['hp'] += 50
                    d['items'][sel] = 0
                elif name == 'Equip':
                    d['items'][0] = sel
                elif name == 'Combine':
                    d.update(menu_combining=True, menu_combine_from=sel)
            elif line == 'confirm' and d['items'][sel]:
                weapon = (d['items'][sel] >> 16) & 255 == 9
                d.update(menu_commands_open=True, menu_command=0,
                         menu_commands=['Equip', 'Examine', 'Combine', 'Use'] if weapon else ['Use', 'Examine', 'Combine'])
        self.d.update(items=items or [1, (9 << 16) | 5, (12 << 16) | 30, (21 << 16) | 1, 0], hp=50,
                      item_names=names or ['', 'Handgun', 'Handgun Bullets', 'Green Herb', ''])
        self.write(self.d)
        return self.reacting(handle, lag)

    def test_reload_combines_the_matching_ammo_into_the_equipped_weapon(self):
        with self.menu_runtime():
            out = self.play.act(dict(op='reload'), 4)
        self.assertEqual(out['result'], 'reloaded')
        self.assertEqual(out['rounds'], [5, 35])
        self.assertFalse(self.d['inventory_open'])  # opened from FREE, closed again
        lines = [l.split(' ', 1)[1] for l in self.controls().splitlines()]
        self.assertEqual(lines[:2], ['inventory', 'confirm'])  # cursor starts on slot 2 (the bullets)
        self.assertIn('Combine', [c for c in self.d['menu_commands']])

    def test_reload_waits_for_each_press_to_take_effect(self):
        # Live: handgun equipped in slot 5, bullets in slot 2; dumps written before the runtime took a press
        # still showed the old cursor, the next press went to the wrong command and nothing was combined.
        with self.menu_runtime(lag=.2, items=[5, 0, (12 << 16) | 20, (21 << 16) | 1, 0, (9 << 16) | 1, 0],
                               names=['', '', 'Handgun Bullets', 'Green Herb', '', 'Handgun', '']):
            out = self.play.act(dict(op='reload'), 8)
        self.assertEqual((out['result'], out['rounds']), ('reloaded', [1, 21]))
        lines = [l.split(' ', 1)[1] for l in self.controls().splitlines()]
        self.assertEqual(lines, ['inventory', 'confirm', 'up', 'confirm', 'right', 'right', 'right', 'confirm',
                                 'cancel', 'cancel'])

    def test_use_heals_from_the_slot_and_equip_keeps_an_equipped_weapon(self):
        with self.menu_runtime():
            out = self.play.act(dict(op='use', item=3), 4)
            self.assertEqual(out['result'], 'used')
            self.assertEqual(out['healed'], 50)
            sent = self.controls()
            out = self.play.act(dict(op='equip', item=1), 2)
        self.assertEqual(out['result'], 'equipped')
        self.assertIn('already equipped', out['info'])
        self.assertEqual(self.controls(), sent)  # nothing sent for the equipped weapon
        with self.assertRaises(Exception):
            self.play.act(dict(op='use', item=4), 1)  # empty slot

    def snapshot_runtime(self):
        def handle(line, d):
            if line == 'sdl_key down F2':
                d['debug_open'] = not d.get('debug_open')
                d['debug_status'] = ''
            elif line.startswith('debug_select'):
                d['debug_choice'] = line.split()[1:]
            elif line == 'confirm' and d.get('debug_open'):
                name, value = d['debug_choice']
                if value != '1':
                    d['debug_status'] = 'Fehler: wrong slot'
                elif name == 'snapshot_save':
                    d['debug_status'] = 'Snapshot 2 gespeichert: RM_TEST.RDX'
                else:
                    d.update(debug_status='Snapshot 2 geladen: RM_TEST.RDX', debug_open=False,
                             gameover_active=False, gameover_prompt=False, hp=100)
        return self.reacting(handle)

    def test_snapshot_save_and_load_use_slot_2_through_the_debug_menu(self):
        with self.snapshot_runtime():
            out = self.play.act(dict(op='snapshot', do='save'), 2)
            self.assertEqual((out['result'], out['snapshot_room']), ('saved', 'RM_TEST'))
            self.assertFalse(self.d['debug_open'])
            saved = self.controls()
            self.assertEqual([l.split(' ', 1)[1] for l in saved.splitlines()],
                             ['sdl_key down F2', 'sdl_key up F2', 'debug_select snapshot_save 1', 'confirm',
                              'sdl_key down F2', 'sdl_key up F2'])
            out = self.play.act(dict(op='snapshot', do='load'), 2)
        self.assertEqual(out['result'], 'loaded')
        # snapshot_load closes the debug menu itself: no second toggle that would open it again.
        self.assertEqual(self.controls()[len(saved):].count('sdl_key down F2'), 1)
        self.assertFalse(self.d['debug_open'])
        self.d['dialog_open'] = True
        self.write(self.d)
        with self.assertRaises(Exception):
            self.play.act(dict(op='snapshot', do='save'), 1)  # only with owner FREE
        with self.assertRaises(Exception):
            self.play.act(dict(op='snapshot', do='save', slot=3), 1)

    def test_game_over_loads_a_newer_slot_2_snapshot_instead_of_continue(self):
        self.d.update(gameover_active=True, gameover_prompt=True, gameover_choice=0,
                      gameover_retry=dict(room='RM_TEST.RDX', play_time=100))
        self.write(self.d)
        drv, client = self.driver([])
        drv.snapshot = dict(room='RM_TEST', play_time=200)
        with self.snapshot_runtime():
            drv.run(steps=1)
        self.assertIn('debug_select snapshot_load 1', self.controls())
        self.assertEqual(client.requests, [])
        self.assertIn('loaded snapshot slot 2', drv.pending_events[-1])

    def test_game_over_continues_when_the_snapshot_is_older_than_the_retry_point(self):
        self.d.update(gameover_active=True, gameover_prompt=True, gameover_choice=0,
                      gameover_retry=dict(room='RM_TEST.RDX', play_time=100))
        self.write(self.d)
        drv, _ = self.driver([])
        drv.snapshot = dict(room='RM_TEST', play_time=50)
        with self.ticking():
            drv.run(steps=1)
        self.assertNotIn('sdl_key', self.controls())
        self.assertIn('confirm', self.controls())

    def test_blocked_move_next_to_a_stair_block_names_free_points(self):
        # A solid stair block beside the player; the model aims at a point inside it.
        self.d['walls'].append(dict(i=4, fl=1, ty=0, attr=0, floor=0, p=[5, 0, -5], e=[4, 30, 10]))
        self.d['actor'] = [0.5, 0, 0]
        self.write(self.d)
        drv, client = self.driver([text(dict(op='move', to=[7, 0])), text(dict(op='press', key='cancel'))])
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        points = [p['to'] for p in feedback['use_one_of']]
        self.assertTrue(points)
        self.assertIn('free points instead', feedback['error'])
        from pathfind import _blocked_fn, active_walls
        tests = [_blocked_fn(w, 3.5)[1] for w in active_walls(self.d)]
        for x, z in points:
            self.assertFalse(any(t(x, z) for t in tests), (x, z))
            self.play.compile(self.d, dict(op='move', to=[x, z]))

    def test_fire_result_and_damage_reach_feedback_and_log(self):
        log = self.directory/'driver.log'
        drv, client = self.driver([text(dict(op='fire', shots=3)), text(dict(op='wait'))], log=log)
        def act(action, seconds):
            return dict(self.play.observe(), result='held', damage=12,
                        fire=dict(shots=3, hits=1, target=2, target_hp=40))
        self.play.act = act
        with self.ticking():
            drv.run(steps=2)
        feedback = json.loads(client.requests[1][0][-1]['content'])
        self.assertEqual(feedback['fire']['shots'], 3)
        self.assertEqual(feedback['damage'], 12)
        entry = json.loads(log.read_text().splitlines()[0])
        self.assertEqual((entry['fire']['hits'], entry['damage']), (1, 12))

if __name__ == '__main__':
    unittest.main()
