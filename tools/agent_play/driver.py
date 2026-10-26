#!/usr/bin/env python3
"""Host driver: an LLM behind an OpenAI-compatible endpoint plays through play.py, one action per decision.

  driver.py [--base-url URL] [--model NAME] [--dir build/play] [--steps N] [--window 8] [--no-tools]

The runtime must already run with --control <dir>/live.txt and `autodump 5 auto` (see PLAYBOOK).
The driver never launches or restarts the game, never takes screenshots, never touches the desktop.

Per decision: compact state (play.py) -> one chat completion -> one JSON action -> play.act.
Speed comes from three things:
  - system_prompt.md is a fixed prefix, so an endpoint with prefix caching prefills it once;
  - history is append-only and cut in halves (not slid one step at a time), so the cached prefix stays valid;
  - scenes, doors, videos, pending routes and locked menus are waited out without asking the model.
Endpoint, model and key come from RECVX_LLM_BASE_URL / RECVX_LLM_MODEL / RECVX_LLM_API_KEY (or OPENAI_API_KEY),
read from the environment or the untracked repo .env (--env FILE); the key is sent as Bearer and never logged.
`zone` and `hint` are function tools; with --no-tools the model may reply {"op":"zone"|"hint",...} instead.
Every step is logged to <dir>/driver.log (JSON lines: llm_ms, act_ms, tokens, action, result).
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from play import Play, PlayError  # noqa: E402

HERE = Path(__file__).resolve().parent
PROMPT = HERE / 'system_prompt.md'
HINT_CMDS = ('room', 'zone', 'blocked', 'event', 'msg')
# Owners in which the game plays by itself; asking the model there only costs time.
PASSIVE = {'CINEMATIC', 'DOOR', 'VIDEO'}
# Observation fields the model does not need for a decision.
DROP = ('age_s', 'busy', 'held', 'elapsed_ms', 'stale', 'weapon')

TOOLS = [
    {'type': 'function', 'function': {
        'name': 'zone',
        'description': 'Exact bounds of zone i and up to 3 ready approach points {to, face} for interact.',
        'parameters': {'type': 'object', 'properties': {'i': {'type': 'integer'}}, 'required': ['i']}}},
    {'type': 'function', 'function': {
        'name': 'hint',
        'description': 'Read-only room/script lookup. room: all zones with text/item/door; zone i: what it does '
                       'and which flags gate it; blocked x z: why no path; event n / msg n: script / message.',
        'parameters': {'type': 'object', 'properties': {
            'cmd': {'type': 'string', 'enum': list(HINT_CMDS)},
            'args': {'type': 'array', 'items': {'type': 'string'}}}, 'required': ['cmd']}}},
]


class Client:
    """Minimal OpenAI-compatible /chat/completions client (urllib, no dependencies)."""

    def __init__(self, base_url, model, api_key='', *, temperature=0.2, max_tokens=200, timeout=60):
        self.url = base_url.rstrip('/') + '/chat/completions'
        self.model, self.api_key = model, api_key
        self.temperature, self.max_tokens, self.timeout = temperature, max_tokens, timeout

    def complete(self, messages, tools=None):
        body = dict(model=self.model, messages=messages, temperature=self.temperature, max_tokens=self.max_tokens)
        if tools:
            body.update(tools=tools, tool_choice='auto')
        headers = {'Content-Type': 'application/json'}
        if self.api_key:
            headers['Authorization'] = 'Bearer ' + self.api_key
        req = urllib.request.Request(self.url, json.dumps(body).encode(), headers)
        with urllib.request.urlopen(req, timeout=self.timeout) as r:
            data = json.load(r)
        return data['choices'][0]['message'], data.get('usage') or {}


def load_env(path):
    """KEY=VALUE lines from an untracked .env; real environment variables win. Values are never printed."""
    try:
        lines = Path(path).read_text().splitlines()
    except OSError:
        return
    for line in lines:
        line = line.strip()
        if not line or line.startswith('#') or '=' not in line:
            continue
        key, value = line.split('=', 1)
        key, value = key.removeprefix('export ').strip(), value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in '"\'':
            value = value[1:-1]
        if key and value and key not in os.environ:
            os.environ[key] = value


def parse_action(text):
    """First balanced JSON object in the reply (tolerates code fences and stray prose)."""
    if not text:
        raise ValueError('empty reply')
    start = text.find('{')
    while start >= 0:
        depth, quoted, escape = 0, False, False
        for i in range(start, len(text)):
            c = text[i]
            if quoted:
                if escape:
                    escape = False
                elif c == '\\':
                    escape = True
                elif c == '"':
                    quoted = False
                continue
            if c == '"':
                quoted = True
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    try:
                        value = json.loads(text[start:i+1])
                    except json.JSONDecodeError:
                        break
                    if isinstance(value, dict) and isinstance(value.get('op'), str):
                        return value
                    break
        start = text.find('{', start + 1)
    raise ValueError('no JSON action with "op" in reply')


def passive(obs):
    """True when the game owns the input and no answer is possible or needed."""
    if obs.get('busy') or (obs.get('menu') or {}).get('locked'):
        return True
    if obs.get('dialog'):
        return False
    return bool(obs.get('owner')) and set(obs['owner'].split()) <= PASSIVE


def trim(obs):
    return {k: v for k, v in obs.items() if k not in DROP}


class Driver:
    def __init__(self, play, client, *, tools=True, window=8, seconds=6.0, max_tool_calls=3, log=None,
                 out=sys.stdout):
        self.play, self.client, self.use_tools = play, client, tools
        self.window, self.seconds, self.max_tool_calls = window, seconds, max_tool_calls
        self.system = PROMPT.read_text()
        self.history = []          # [user, assistant] pairs of earlier decisions, append-only
        self.note = ''
        self.log = log
        self.out = out
        self.steps = 0

    # --- tools -----------------------------------------------------------------------------------------
    def tool(self, name, args):
        try:
            if name == 'zone':
                return json.dumps(self.play.describe_zone(int(args['i'])), separators=(',', ':'))
            if name == 'hint':
                cmd = args.get('cmd')
                if cmd not in HINT_CMDS:
                    return 'error: cmd must be one of ' + ', '.join(HINT_CMDS)
                argv = [str(a) for a in args.get('args') or []]
                if cmd in ('room', 'zone', 'blocked'):
                    argv.append(str(self.play.auto))
                r = subprocess.run([sys.executable, str(HERE / 'hint.py'), cmd] + argv,
                                   capture_output=True, text=True, timeout=20)
                return (r.stdout or r.stderr).strip()[:1500] or '(no output)'
        except (PlayError, KeyError, ValueError, TypeError, subprocess.TimeoutExpired) as e:
            return 'error: ' + str(e)
        return 'error: unknown tool ' + str(name)

    # --- one model decision ----------------------------------------------------------------------------
    def messages(self, user):
        return [{'role': 'system', 'content': self.system}] + self.history + [user]

    def decide(self, obs, feedback):
        payload = dict(feedback, state=trim(obs))
        if self.note:
            payload['note'] = self.note
        user = {'role': 'user', 'content': json.dumps(payload, separators=(',', ':'), ensure_ascii=False)}
        turn, usage, llm_ms = [], {}, 0.0
        lookups = 0
        for attempt in range(self.max_tool_calls + 3):
            # Once the lookup budget is spent the model gets no tools and must answer with an action.
            final = lookups >= self.max_tool_calls
            if final and turn and turn[-1]['role'] != 'user':
                turn.append({'role': 'user', 'content': 'Lookup budget used up. Reply now with exactly one '
                                                        'JSON game action (press/interact/move/fire/shake/keys/run/wait).'})
            started = time.monotonic()
            message, u = self.client.complete(self.messages(user) + turn,
                                              TOOLS if self.use_tools and not final else None)
            llm_ms += (time.monotonic() - started) * 1000
            for k, v in u.items():
                if isinstance(v, (int, float)):
                    usage[k] = usage.get(k, 0) + v
            details = u.get('prompt_tokens_details') or {}
            if details.get('cached_tokens'):
                usage['cached_tokens'] = usage.get('cached_tokens', 0) + details['cached_tokens']
            calls = message.get('tool_calls') or []
            if calls and not final:
                lookups += 1
                turn.append({'role': 'assistant', 'content': message.get('content') or '', 'tool_calls': calls})
                for call in calls:
                    fn = call.get('function', {})
                    try:
                        args = json.loads(fn.get('arguments') or '{}')
                    except json.JSONDecodeError:
                        args = {}
                    turn.append({'role': 'tool', 'tool_call_id': call.get('id', ''), 'content': self.tool(fn.get('name'), args)})
                continue
            text = message.get('content') or ''
            try:
                action = parse_action(text)
            except ValueError as e:
                turn.append({'role': 'assistant', 'content': text})
                turn.append({'role': 'user', 'content': 'error: ' + str(e) + '. Reply with one JSON action.'})
                continue
            if action['op'] in ('zone', 'hint'):
                # A lookup written as an action (with or without function calling) is answered, not dispatched.
                args = {k: v for k, v in action.items() if k not in ('op', 'note')}
                if action['op'] == 'hint' and 'args' not in args:
                    args = dict(cmd=args.get('cmd'), args=[str(v) for k, v in args.items() if k != 'cmd'])
                turn.append({'role': 'assistant', 'content': json.dumps(action)})
                if final:
                    turn.append({'role': 'user', 'content': 'No more lookups. Reply with one JSON game action.'})
                else:
                    lookups += 1
                    turn.append({'role': 'user', 'content': 'tool ' + action['op'] + ': ' + self.tool(action['op'], args)})
                continue
            return user, action, usage, llm_ms
        # No usable action: let the game run briefly and report it back instead of stopping the run.
        return user, dict(op='wait', no_action=True), usage, llm_ms

    def remember(self, user, action):
        # Tool exchanges stay out of the history; only the decision and its state are kept.
        self.history += [user, {'role': 'assistant', 'content': json.dumps(action, separators=(',', ':'))}]
        if len(self.history) > 4 * self.window:
            # Cut in one block so the cached prefix (system + kept turns) survives the next steps.
            self.history = self.history[-2 * self.window:]

    # --- the loop --------------------------------------------------------------------------------------
    def act(self, action):
        started = time.monotonic()
        try:
            obs = self.play.act(action, self.seconds)
            feedback = dict(result=obs.pop('result', 'running'))
            if 'fire' in obs:
                feedback['fire'] = obs.pop('fire')
        except PlayError as e:
            obs = self.play.observe()
            feedback = dict(result='error', error=str(e))
        return obs, feedback, (time.monotonic() - started) * 1000

    def ensure_auto_hold(self):
        d, _, _ = self.play.read()
        if (d.get('control') or {}).get('run') != 'auto':
            self.act({'op': 'run', 'mode': 'auto', 'dist': 18})

    def record(self, entry):
        self.steps += 1
        line = (f"{self.steps:4d} {entry.get('llm_ms', 0):6.0f}ms llm {entry['act_ms']:6.0f}ms act "
                f"{entry['owner']:<18.18} {json.dumps(entry['action'], separators=(',', ':'))[:90]:<90} -> {entry['result']}")
        print(line, file=self.out, flush=True)
        if self.log:
            with open(self.log, 'a') as f:
                f.write(json.dumps(entry, separators=(',', ':')) + '\n')

    def run(self, steps=0):
        self.ensure_auto_hold()
        obs, feedback = self.play.observe(), dict(result='start')
        stale_since = None
        while not steps or self.steps < steps:
            if obs.get('stale'):
                stale_since = stale_since or time.monotonic()
                if time.monotonic() - stale_since > 10:
                    print('runtime metrics stale for 10 s: game not running; driver stops', file=self.out)
                    return 2
                time.sleep(.5)
                obs = self.play.observe()
                continue
            stale_since = None
            if passive(obs):
                action, entry = {'op': 'wait'}, dict(auto=True)
            else:
                user, action, usage, llm_ms = self.decide(obs, feedback)
                fallback = action.pop('no_action', False)
                note = action.pop('note', None)
                if isinstance(note, str) and note.strip():
                    self.note = note.strip()[:160]
                self.remember(user, action)
                entry = dict(llm_ms=round(llm_ms), usage=usage)
            owner = obs.get('owner', '')
            obs, feedback, act_ms = self.act(action)
            if not entry.get('auto') and fallback:
                feedback.update(result='error', error='no game action in your last reply; the driver waited instead')
                entry['fallback'] = True
            entry.update(owner=owner, action=action, act_ms=round(act_ms), result=feedback['result'])
            if 'error' in feedback:
                entry['error'] = feedback['error']
            self.record(entry)
        return 0


def main():
    # The .env must be loaded before argparse reads its environment defaults.
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument('--env', default=str(HERE.parent.parent / '.env'))
    env = pre.parse_known_args()[0].env
    load_env(env)
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--env', default=env, help='untracked KEY=VALUE file (default: repo .env)')
    ap.add_argument('--base-url', default=os.environ.get('RECVX_LLM_BASE_URL', 'http://localhost:8080/v1'))
    ap.add_argument('--model', default=os.environ.get('RECVX_LLM_MODEL', 'local'))
    ap.add_argument('--dir', default='build/play')
    ap.add_argument('--steps', type=int, default=0, help='0 = until stopped')
    ap.add_argument('--window', type=int, default=8, help='decisions kept in the prompt (cut in halves)')
    ap.add_argument('--temperature', type=float, default=0.2)
    ap.add_argument('--max-tokens', type=int, default=200)
    ap.add_argument('--seconds', type=float, default=6.0, help='play.py wait budget per action (<= 8)')
    ap.add_argument('--no-tools', action='store_true', help='endpoint without function calling')
    ap.add_argument('--dry-run', action='store_true', help='print the first request and exit')
    a = ap.parse_args()
    play = Play(a.dir)
    client = Client(a.base_url, a.model, os.environ.get('RECVX_LLM_API_KEY') or os.environ.get('OPENAI_API_KEY', ''),
                    temperature=a.temperature, max_tokens=a.max_tokens)
    driver = Driver(play, client, tools=not a.no_tools, window=a.window, seconds=a.seconds,
                    log=Path(a.dir) / 'driver.log')
    if a.dry_run:
        obs = play.observe()
        user = {'role': 'user', 'content': json.dumps(dict(result='start', state=trim(obs)), separators=(',', ':'))}
        msgs = driver.messages(user)
        print(json.dumps(msgs, indent=1, ensure_ascii=False))
        print(f'system {len(driver.system)} chars, state {len(user["content"])} chars', file=sys.stderr)
        return 0
    try:
        return driver.run(a.steps)
    except KeyboardInterrupt:
        return 130
    except (urllib.error.URLError, TimeoutError) as e:
        print('LLM endpoint error: ' + str(e), file=sys.stderr)
        return 3


if __name__ == '__main__':
    sys.exit(main())
