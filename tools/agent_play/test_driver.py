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
