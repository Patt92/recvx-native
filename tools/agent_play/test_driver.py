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

from driver import Driver, parse_action, passive, trim
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
        return Driver(self.play, client, seconds=.2, out=io.StringIO(), **kw), client

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
        self.assertEqual({t['function']['name'] for t in tools}, {'zone', 'hint'})
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
        with self.assertRaisesRegex(Exception, 'cancel back first'):
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
        first = drv.room_points(d)[14]
        cands = drv._candidates[14]
        self.assertGreaterEqual(len(cands), 2)
        drv.point_failed('RM_TEST.RDX', dict(op='interact', zone=14, to=first[0]), 'stalled')
        self.assertEqual(drv.room_points(d)[14], cands[1])
        # A good result does not mark the point.
        drv.point_failed('RM_TEST.RDX', dict(op='interact', zone=14, to=cands[1][0]), 'dialog')
        self.assertEqual(drv.room_points(d)[14], cands[1])

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
        self.assertNotIn(13, south)
        self.d['actor'] = [0, 0, -10]
        self.write(self.d)
        north = drv.room_points(self.play.read()[0])
        self.assertIn(13, north)
        self.assertNotIn(14, north)

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
        drv.point_failed('RM_0080', dict(op='interact', zone=4, to=cands[0][0]), 'stalled')
        self.assertEqual(drv.room_points(self.d)[4], cands[1])

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
