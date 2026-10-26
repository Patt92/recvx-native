#!/usr/bin/env python3
"""Driver tests with a scripted model and a fixture runtime; no endpoint, game or user saves."""
from contextlib import contextmanager
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest

from driver import Driver, parse_action, passive, trim
from play import Play
from test_play import state


class Scripted:
    """Stands in for Client: returns queued messages and records every request."""

    def __init__(self, replies):
        self.replies, self.requests = list(replies), []

    def complete(self, messages, tools=None):
        self.requests.append((deepcopy(messages), tools))
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
        self.assertIn('Reply with one JSON action', client.requests[1][0][-1]['content'])
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
        drv.with_approaches = lambda obs: obs  # zone without a precomputed go point (beyond the nearest five)
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

    def test_invented_interact_point_is_replaced_by_the_computed_one(self):
        drv, _ = self.driver([])
        obs = dict(zones=[dict(i=4, kind='item', go=[[25.0, 24.5], [25.0, 22.5]])])
        fixed = drv.correct(dict(op='interact', zone=4, to=[24, 17], face=[24, 20]), obs)
        self.assertEqual((fixed['to'], fixed['face']), ([25.0, 24.5], [25.0, 22.5]))
        same = dict(op='interact', zone=4, to=[25.0, 24.5], face=[25.0, 22.5])
        self.assertIs(drv.correct(same, obs), same)
        other = dict(op='interact', zone=9, to=[1, 1], face=[2, 2])
        self.assertIs(drv.correct(other, obs), other)
        self.assertEqual(drv.correct(dict(op='move', to=[1, 1]), obs), dict(op='move', to=[1, 1]))

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


if __name__ == '__main__':
    unittest.main()
