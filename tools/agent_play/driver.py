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
import math
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
HINT_CMDS = ('room', 'zone', 'blocked', 'event', 'msg', 'why', 'rules')
# Owners in which the game plays by itself; asking the model there only costs time.
PASSIVE = {'CINEMATIC', 'DOOR', 'VIDEO'}
# Longest note kept as the model's memory (the prompt asks for at most this many characters).
NOTE_MAX = 80
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
                       'and which flags gate it; blocked x z: why no path; why n: why event n runs (conditions, '
                       'items you carry that trigger it, its texts/voice); event n / msg n: script / message.',
        'parameters': {'type': 'object', 'properties': {
            'cmd': {'type': 'string', 'enum': list(HINT_CMDS)},
            'args': {'type': 'array', 'items': {'type': 'string'}}}, 'required': ['cmd']}}},
]


class Client:
    """Minimal OpenAI-compatible /chat/completions client (urllib, no dependencies)."""

    # Transient failures worth another try: timeouts, refused/reset connections, overload, server errors.
    RETRY_HTTP = {408, 409, 425, 429, 500, 502, 503, 504}

    def __init__(self, base_url, model, api_key='', *, temperature=0.2, max_tokens=200, timeout=60,
                 retries=0, retry_delay=3.0, out=sys.stderr, sleep=time.sleep, extra_body=None):
        self.url = base_url.rstrip('/') + '/chat/completions'
        self.model, self.api_key = model, api_key
        self.temperature, self.max_tokens, self.timeout = temperature, max_tokens, timeout
        self.retries, self.retry_delay, self.out, self.sleep = retries, retry_delay, out, sleep
        self.extra_body = dict(extra_body or {})

    def complete(self, messages, tools=None, max_tokens=None):
        """One request; transient errors are retried after a few seconds (retries 0 = until it works)."""
        attempt = 0
        while True:
            try:
                return self._complete(messages, tools, max_tokens)
            except urllib.error.HTTPError as e:
                if e.code not in self.RETRY_HTTP:
                    raise
                reason = f'HTTP {e.code}'
                e.close()
            except (urllib.error.URLError, TimeoutError, ConnectionError, OSError, json.JSONDecodeError) as e:
                reason = str(getattr(e, 'reason', e)) or type(e).__name__
            attempt += 1
            if self.retries and attempt > self.retries:
                raise TimeoutError(f'LLM endpoint failed {attempt} times, last: {reason}')
            delay = min(self.retry_delay * 2 ** min(attempt - 1, 3), 20.0)
            print(f'LLM endpoint: {reason}; retry {attempt} in {delay:.0f} s', file=self.out, flush=True)
            self.sleep(delay)

    def _complete(self, messages, tools=None, max_tokens=None):
        body = dict(self.extra_body, model=self.model, messages=messages, temperature=self.temperature,
                    max_tokens=max_tokens or self.max_tokens)
        if tools:
            body.update(tools=tools, tool_choice='auto')
        headers = {'Content-Type': 'application/json'}
        if self.api_key:
            headers['Authorization'] = 'Bearer ' + self.api_key
        req = urllib.request.Request(self.url, json.dumps(body).encode(), headers)
        with urllib.request.urlopen(req, timeout=self.timeout) as r:
            data = json.load(r)
        choice = data['choices'][0]
        message = dict(choice.get('message') or {})
        message['_finish'] = choice.get('finish_reason')
        return message, data.get('usage') or {}


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


def strip_thinking(text):
    """Remove <think>...</think> blocks; an unterminated block means the reply was cut while thinking."""
    import re
    text = re.sub(r'<think>.*?</think>', '', text or '', flags=re.S)
    return text.split('<think>')[0]


def parse_action(text):
    """First balanced JSON object in the reply (tolerates code fences, stray prose and think blocks)."""
    text = strip_thinking(text)
    if not text.strip():
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


def salvage_cut(text):
    """A reply cut at the token limit inside its trailing `note`: the action fields before it are complete.
    Returns the action without the note, or None when the cut is anywhere else."""
    text = strip_thinking(text)
    start = text.find('{')
    if start < 0:
        return None
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
        elif c in '{[':
            depth += 1
        elif c in '}]':
            depth -= 1
        elif c == ',' and depth == 1 and text[i + 1:].lstrip().startswith('"note"'):
            try:
                value = json.loads(text[start:i] + '}')
            except json.JSONDecodeError:
                return None
            return value if isinstance(value, dict) and isinstance(value.get('op'), str) else None
    return None


def default_action(obs):
    """What the driver does after two empty cut replies in a row, by owner (never a game decision)."""
    dialog = obs.get('dialog') or {}
    if dialog:
        return {'op': 'press', 'key': 'confirm'} if dialog.get('ready') and not dialog.get('choice') else {'op': 'wait'}
    ui = set((obs.get('owner') or '').split())
    if ui & {'INVENTORY', 'BOX', 'FILES'}:
        return {'op': 'press', 'key': 'cancel'}
    if obs.get('owner') == 'FREE':
        return {'op': 'wait', 'ticks': 15}
    return {'op': 'wait'}


def passive(obs):
    """True when the game owns the input and no answer is possible or needed."""
    if obs.get('busy') or (obs.get('menu') or {}).get('locked'):
        return True
    if obs.get('dialog'):
        return False
    if obs.get('computer'):
        return obs['computer'].get('waits') in ('printing', 'script')
    return bool(obs.get('owner')) and set(obs['owner'].split()) <= PASSIVE


# Results that show what a zone answers (text, choice, menu, terminal, another room, a new owner); a
# danger/grab/damage/stall/interruption/hold stopped the player before the zone could answer.
ANSWERS = ('dialog', 'choice', 'menu', 'room', 'computer', 'owner')


def answered(obs, result):
    ui = (obs.get('owner') or '').split()
    return result in ANSWERS or bool(obs.get('dialog')) or 'DOOR' in ui or 'BOX' in ui


def trim(obs):
    return {k: v for k, v in obs.items() if k not in DROP}


class Driver:
    def __init__(self, play, client, *, tools=True, window=8, seconds=6.0, max_tool_calls=3, log=None,
                 out=sys.stdout, save_rooms=3, repeat_limit=2, skip_videos=False, alert_after=20):
        self.play, self.client, self.use_tools = play, client, tools
        self.window, self.seconds, self.max_tool_calls = window, seconds, max_tool_calls
        self.system = PROMPT.read_text()
        self.history = []          # [user, assistant] pairs of earlier decisions, append-only
        self.note = ''
        self.log = log
        self.out = out
        self.steps = 0
        self.bad_replies, self.note_cut = [], False  # note_cut: last reply was cut inside its note
        self._points_key, self._points = None, {}  # zone -> [to, face] for the current room and floor
        self._candidates = {}       # zone -> up to three [to, face] candidates
        self._failed_points = set() # (room, zone, to) that stalled or triggered nothing
        self.room = self.previous_room = None
        self.idle_waits, self.idle_view = 0, None  # passive waits in a row without any visible change
        self.last_owner = ''
        # Run journal: what happened in this run, shown with every decision (no game knowledge, only
        # what the game itself showed): per room the zones read and their answers, open items/doors,
        # important texts (locks, needs, codes) and the causes of events that reacted to the player.
        self.journal_rooms = {}    # room -> dict(seen=[...], open_items=[...], doors={i: dest})
        self.journal_facts = []    # short lines, newest last, deduplicated
        self.journal_order = []    # rooms in visit order
        self._rules_key, self._rules = None, []
        self.alert_after = alert_after  # decisions without progress before an alert (0 = off)
        self.since_progress = 0
        self.progress_marks = set()
        self.alerted_at = 0
        self.tasks_seen = set()    # room events running at the last observation
        self.dispatched_seen, self.events_room = 0, None
        self.reported = set()      # events explained during this room visit (scripts restart some every pass)
        self.pending_events = []   # explanations to hand over with the next decision
        # Zone memory per room: what an examine gave and the progress state at that time, so the model
        # sees what it already did and a repeat without anything new is refused.
        self.visits = {}           # room -> zone -> dict(n, got, sig)
        self.repeat_limit = repeat_limit
        self._info_room, self._info = None, set()
        self.skip_videos = skip_videos
        self.video_skipped = False  # skip sent for the video that is playing now
        # Save policy: a save (or a load) counts as fresh until the player has entered `save_rooms` rooms
        # not seen since. 0 disables the policy.
        self.save_rooms = save_rooms
        self.rooms_since_save = set()  # the start of a run (new game or loaded game) counts as fresh
        self.save_count = None
        self.save_room_seen = None
        self.texts = []            # last game texts (dialog, menu, document, terminal) the model has seen
        self.move_zone = None      # (room, zone) whose ready point the last move ended on (move + confirm)
        self.snapshot = None       # slot-2 snapshot saved in this run: dict(room, play_time)
        self.snapshot_tried = None # retry play time of the game over a load was already tried for

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
                if cmd in ('room', 'zone', 'blocked', 'why', 'rules'):
                    argv.append(str(self.play.auto))
                elif cmd in ('event', 'msg') and len(argv) == 1:
                    # hint.py would read the room from build/play/auto.json; use this driver's state.
                    d, _, _ = self.play.read()
                    argv.append(d['room'])
                r = subprocess.run([sys.executable, str(HERE / 'hint.py'), cmd] + argv,
                                   capture_output=True, text=True, timeout=20)
                return (r.stdout or r.stderr).strip()[:1500] or '(no output)'
        except (PlayError, KeyError, ValueError, TypeError, subprocess.TimeoutExpired) as e:
            return 'error: ' + str(e)
        return 'error: unknown tool ' + str(name)

    # --- one model decision ----------------------------------------------------------------------------
    def messages(self, user):
        return [{'role': 'system', 'content': self.system}] + self.history + [user]

    def remember_texts(self, obs):
        """Keep the last few texts the game showed: a hint read on one page must survive the next screen."""
        found = [(obs.get('dialog') or {}).get('text', ''), obs.get('menu_text', ''), obs.get('document', ''),
                 '\n'.join((obs.get('computer') or {}).get('screen', []))]
        for t in found:
            t = ' '.join(t.split())
            if len(t) > 3 and (not self.texts or self.texts[-1] != t):
                self.texts = (self.texts + [t])[-6:]

    def track_saves(self, obs):
        """Count new rooms since the last save or load (typewriter save_count, load screen)."""
        d, _, _ = self.play.read()
        t = d.get('typewriter') or {}
        count = t.get('save_count')
        if count is not None and self.save_count is not None and count > self.save_count:
            self.rooms_since_save = set()
        if count is not None:
            self.save_count = count
        if t.get('active') and t.get('mode') == 'load':
            self.rooms_since_save = set()  # a loaded game was saved right where it starts
        room = obs.get('room')
        if room and room != self.save_room_seen:
            if self.save_room_seen is not None:
                self.rooms_since_save.add(room)
            self.save_room_seen = room

    def save_allowed(self):
        if not self.save_rooms:
            return True
        return len(self.rooms_since_save) >= self.save_rooms

    def save_reason(self):
        return (f'saved or loaded {len(self.rooms_since_save)} new room(s) ago; saves are useful after '
                f'{self.save_rooms} new rooms or before a boss')

    def tips(self, obs, previous_owner):
        """Facts the model tends to miss, derived from the state only (no room knowledge)."""
        out = []
        menu = obs.get('menu') or {}
        if obs.get('owner') == 'INVENTORY' and previous_owner and 'DIALOG' in previous_owner and not menu.get('commands'):
            out.append('The game opened the status menu itself after the text: it expects you to use, equip or '
                       'combine something now. Do not cancel before doing what the text asked.')
        space = obs.get('space')
        if space and space[0] >= space[1] and obs.get('owner') in ('FREE', 'DIALOG', 'CINEMATIC DIALOG'):
            out.append('Inventory full: before picking up more, free a slot: read documents you carry (Use; a read '
                       'manual moves to FILES), combine ammo into its weapon, combine or use herbs, or store items '
                       'you do not need now in an item box.')
        dialog = obs.get('dialog') or {}
        if dialog.get('choice') and any(w in dialog.get('text', '').lower() for w in ('save', 'ink ribbon')):
            out.append('Save not needed now: choose No (' + self.save_reason() + ').' if not self.save_allowed()
                       else 'A save is reasonable now (enough progress since the last one).')
        equipped = ((obs.get('equipped') or [''])[0] or '').lower()
        names = [row[3].lower() for row in obs.get('inv', []) if len(row) > 3 and row[3]]
        # Pickup confirmations name the item too; they are not a request to use it.
        asks = [t.lower() for t in self.texts[-2:] if not t.lower().startswith(("you've taken", 'take the'))]
        named = [n for n in names if n and n != equipped and any(n in t for t in asks)]
        if named:
            out.append('The last text names an item you carry: ' + ', '.join(sorted(set(named))) +
                       '. Equip or use it (inventory -> select it -> Equip/Use).')
        return out

    def decide(self, obs, feedback, previous_owner=''):
        self.remember_texts(obs)
        payload = dict(feedback, state=trim(obs))
        if self.texts:
            payload['recent_texts'] = self.texts[-4:]
        payload['journal'] = self.journal()
        rules = self.room_rules()
        if rules:
            payload['room_rules'] = rules
        alert = self.alert(obs)
        if alert:
            payload['alert'] = alert
        if self.pending_events:
            payload['events_explained'] = self.pending_events
            self.pending_events = []
        tips = self.tips(obs, previous_owner)
        if tips:
            payload['tips'] = tips
        if self.note:
            payload['note'] = self.note
        user = {'role': 'user', 'content': json.dumps(payload, separators=(',', ':'), ensure_ascii=False)}
        turn, usage, llm_ms = [], {}, 0.0
        lookups, budget, empty_cuts = 0, None, 0
        self.bad_replies, self.note_cut = [], False
        for attempt in range(self.max_tool_calls + 3):
            # Once the lookup budget is spent the model gets no tools and must answer with an action.
            final = lookups >= self.max_tool_calls
            if final and turn and turn[-1]['role'] != 'user':
                turn.append({'role': 'user', 'content': 'Lookup budget used up. Reply now with exactly one '
                                                        'JSON game action (press/interact/move/fire/shake/keys/run/wait/use/reload).'})
            started = time.monotonic()
            message, u = self.client.complete(self.messages(user) + turn,
                                              TOOLS if self.use_tools and not final else None, budget)
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
                error = str(e)
                finish = message.get('_finish')
                thought = message.get('reasoning_content') or ''
                self.bad_replies.append(dict(finish=finish, text=(text or thought)[:160]))
                action = salvage_cut(text) if finish == 'length' else None
                if action is not None:
                    # Cut inside the trailing note: the action is complete, only the note is lost.
                    self.bad_replies[-1]['salvaged'] = self.note_cut = True
            if action is None:
                empty_cuts = empty_cuts + 1 if finish == 'length' and not strip_thinking(text).strip() else 0
                if empty_cuts >= 2:
                    # Two empty cut replies in a row (reasoning or whitespace only): do not spend another
                    # long request; take the owner's safe default and tell the model.
                    return user, dict(default_action(obs), no_action=True), usage, llm_ms
                if finish == 'length':
                    # Cut off before the JSON (long reasoning or prose): retry with a larger budget and do not
                    # feed the cut text back, it only primes more of the same.
                    budget = max(4 * self.client.max_tokens, 600) if hasattr(self.client, 'max_tokens') else None
                    turn.append({'role': 'assistant', 'content': '(reply cut off at the token limit)'})
                    turn.append({'role': 'user', 'content': 'Your reply was cut off before the action. Reply with '
                                 'ONLY the JSON object, no explanation and no reasoning, e.g. {"op":"press","key":"confirm"}.'})
                else:
                    turn.append({'role': 'assistant', 'content': strip_thinking(text)[:400]})
                    turn.append({'role': 'user', 'content': 'error: ' + error + '. Reply with ONLY one JSON action object.'})
                continue
            if action['op'] in HINT_CMDS and action['op'] != 'zone':
                # {"op":"blocked","x":..} or {"op":"room"}: a hint command written as an action.
                rest = []
                for k, v in action.items():
                    if k not in ('op', 'note', 'cmd', 'args'):
                        rest += [str(x) for x in v] if isinstance(v, list) else [str(v)]
                action = dict(op='hint', cmd=action['op'], args=action.get('args') or rest)
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
        # No usable action: take the owner's safe default and report it back instead of stopping the run.
        return user, dict(default_action(obs), no_action=True), usage, llm_ms

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
            # Outcome details of the action (shots/hits, hp lost, a resent route, healed hp, rounds, snapshot).
            for key in ('fire', 'damage', 'retried', 'healed', 'rounds', 'snapshot_room', 'info'):
                if key in obs:
                    feedback[key] = obs.pop(key)
        except (TypeError, ValueError, KeyError, IndexError) as e:
            # A malformed model value (string coordinate, wrong nesting) must cost one decision, not the run.
            obs = self.play.observe()
            feedback = dict(result='error', error=f'invalid action fields ({type(e).__name__}: {e}); '
                                                  'send numbers, e.g. {"op":"interact","zone":4,"to":[1.5,2.0]}')
        except PlayError as e:
            obs = self.play.observe()
            feedback = dict(result='error', error=str(e))
            if action.get('op') == 'move' and ('blocked' in str(e) or 'no path' in str(e)):
                try:
                    d, _, _ = self.play.read()
                    points = self.play.free_near(d, action.get('to'))
                    feedback['use_one_of'] = [dict(to=p) for p in points]
                    if points:
                        # Small models skim fields; the free points also go into the error text itself.
                        feedback['error'] += f'. Move to one of these free points instead: {points}'
                except (PlayError, TypeError, ValueError):
                    pass
            if action.get('op') == 'interact' and isinstance(action.get('zone'), int):
                # A made-up standing point is the most common error: hand over the computed ones.
                try:
                    z = self.play.describe_zone(action['zone'])
                    feedback['use_one_of'] = [dict(to=a['to'], face=a['face']) for a in z.get('approach', [])]
                    if z.get('approach_note'):
                        feedback['approach_note'] = z['approach_note']
                except PlayError:
                    pass
        return obs, feedback, (time.monotonic() - started) * 1000

    def progress(self):
        """What can change by playing: inventory, pickup bits, zones and walls switched by scripts."""
        d, _, _ = self.play.read()
        return (tuple(d.get('items', [])), tuple(d.get('item_flags', [])),
                tuple((a.get('flags', 0) & 1, a.get('attribute'), tuple(a.get('parameters', [])))
                      for a in d.get('interactions', [])),
                tuple(w.get('fl', 0) & 1 for w in d.get('walls', [])))

    def zone_log(self, room):
        return self.visits.setdefault(room, {})

    def record_visit(self, room, zone, before, obs, result):
        """After an interact: keep what the zone answered (first text page, a menu, nothing)."""
        got = ' '.join(((obs.get('dialog') or {}).get('text', '')).split())
        if not got and 'BOX' in obs.get('owner', ''):
            got = 'opened the item box'
        if not got and 'DOOR' in obs.get('owner', '').split():
            got = 'went through the door'
        if not got:
            got = {'menu': 'opened a menu', 'room': 'went to another room', 'no_interaction_observed': 'nothing happened'}.get(result, result)
        v = self.zone_log(room).setdefault(zone, dict(n=0, got='', sig=None))
        v['n'] += 1
        v['got'] = got[:80]
        v['sig'] = before

    def zone_of_move(self, room, action, result):
        """(room, zone) when a finished move ended on a zone's ready point (a candidate of this room)."""
        to = action.get('to')
        if action.get('op') != 'move' or result not in ('settled', 'held', 'damage') or \
                not (isinstance(to, list) and len(to) == 2) or (self._points_key or [''])[0][:7] != room:
            return None
        for zone, cands in self._candidates.items():
            if any(math.hypot(c[0][0] - to[0], c[0][1] - to[1]) < .05 for c in cands):
                return room, zone
        return None

    def repeat_blocked(self, action, obs):
        """An interact on a zone already examined `repeat_limit` times with no progress since is refused."""
        if not self.repeat_limit or action.get('op') != 'interact':
            return None
        v = self.zone_log(obs.get('room')).get(action.get('zone'))
        limit = 1 if action.get('zone') in self.info_zones() else self.repeat_limit
        if not v or v['n'] < limit or v['sig'] != self.progress():
            return None
        if limit == 1:
            return (f"zone {action['zone']} is information only (no script reacts to it) and was read already: "
                    f"{v['got']!r}. Do something new: another zone, a door, or use what it told you.")
        return (f"zone {action['zone']} already examined {v['n']} times and nothing changed since "
                f"(inventory, items, zones, walls); last answer: {v['got']!r}. Do something new: another zone, "
                "a door, use an item there via inventory, or read hint zone.")

    def info_zones(self):
        """Text zones of this room that only show a message (cached per room)."""
        d, _, _ = self.play.read()
        room = d.get('room')
        if self._info_room != room:
            try:
                import hint
                self._info = hint.informational_zones(hint.Room(room), d)
            except Exception:
                self._info = set()
            self._info_room = room
        return self._info

    def mark_seen(self, obs):
        """Annotate zones with what examining them gave; list examined zones of this room as `done`."""
        log = self.zone_log(obs.get('room'))
        if not log:
            return obs
        info = self.info_zones()
        for z in obs.get('zones', []):
            v = log.get(z['i'])
            if v:
                z['seen'] = v['n']
                z['got'] = v['got']
                if z['i'] in info:
                    z['read'] = True  # information only, already read: no reason to examine it again
        obs['done_here'] = [dict(i=i, n=v['n'], got=v['got']) for i, v in sorted(log.items())]
        return obs

    FACT_WORDS = ('locked', 'need', 'must', 'cannot', "can't", 'password', 'code', 'release', 'require',
                  'only', 'missing', 'key', 'emblem', 'switch', 'down', 'blocked', 'metal')

    def note_fact(self, text):
        line = ' '.join(text.replace('|', ' ').split())[:140]
        if line and line not in self.journal_facts:
            self.journal_facts = (self.journal_facts + [line])[-12:]

    def update_journal(self, obs):
        """Keep the run journal current from the compact observation."""
        room = obs.get('room')
        if not room:
            return
        if room not in self.journal_rooms:
            self.journal_rooms[room] = dict(seen={}, open_items=[], doors={})
        if not self.journal_order or self.journal_order[-1] != room:
            self.journal_order = (self.journal_order + [room])[-10:]
        r = self.journal_rooms[room]
        for z in obs.get('zones', []) + obs.get('far_zones', []):
            if z.get('kind') == 'door' and z.get('destination') is not None:
                r['doors'][z['i']] = z['destination']
        items = [z['i'] for z in obs.get('zones', []) + obs.get('far_zones', [])
                 if z.get('kind') == 'item' and z.get('item') in (None, 'present')]
        if obs.get('owner') == 'FREE':
            r['open_items'] = sorted(set(items))
        for i, v in self.zone_log(room).items():
            r['seen'][i] = v['got'][:60]
        dialog = (obs.get('dialog') or {}).get('text', '')
        for t in (dialog, obs.get('menu_text', ''), obs.get('document', '')):
            low = (t or '').lower()
            if any(w in low for w in self.FACT_WORDS) and not low.startswith(("you've taken", 'take the')):
                self.note_fact(f'{room}: {t}')

    def room_rules(self):
        """Rules of the current room derived from its scripts (hint.room_rules), cached per room and
        carried items; printed to the console when they change."""
        d, _, _ = self.play.read()
        key = (d.get('room'), tuple(sorted({(v >> 16) & 255 for v in d.get('items', [])[1:] if v})))
        if key != self._rules_key:
            try:
                import hint
                rules = hint.room_rules(hint.Room(d['room']), d)
            except Exception:
                rules = []
            for r in rules:
                if r not in self._rules:
                    print(f"RULE {d.get('room', '')[:7]}: {r['rule']}", file=self.out, flush=True)
            self._rules_key, self._rules = key, [{k: v for k, v in r.items() if k != 'all_items'} for r in rules]
        return self._rules

    def track_progress(self, obs, result):
        """Progress = something new: a room, an item picked up, a zone answering for the first time, a
        wall or zone switched by a script. No progress for `alert_after` decisions raises an alert."""
        d, _, _ = self.play.read()
        marks = {('room', d.get('room'))}
        marks |= {('item', (v >> 16) & 255) for v in d.get('items', [])[1:] if v}
        marks |= {('flag', i, f) for i, f in enumerate(d.get('item_flags', [])) if f}
        marks |= {('wall', w['i'], w.get('fl', 0) & 1) for w in d.get('walls', [])}
        for room, zones in self.visits.items():
            marks |= {('read', room, z) for z in zones}
        first = not self.progress_marks
        new = marks - self.progress_marks
        self.progress_marks |= marks
        if new and not first:
            self.since_progress = 0
            self.alerted_at = 0
        else:
            self.since_progress += 1

    def alert(self, obs):
        """No progress for a while: say so, and what is known that could help (rules, facts, open work)."""
        if not self.alert_after or self.since_progress < self.alert_after or \
                (self.alerted_at and self.since_progress - self.alerted_at < max(1, self.alert_after // 2)):
            return None
        self.alerted_at = self.since_progress
        room = obs.get('room')
        r = self.journal_rooms.get(room, {})
        unread = [z['i'] for z in obs.get('zones', []) + obs.get('far_zones', [])
                  if z.get('kind') in ('text', 'item') and z['i'] not in r.get('seen', {})]
        lines = [f'No progress for {self.since_progress} decisions in {room}.']
        if self._rules:
            lines.append('Room rule: ' + self._rules[0]['rule'] + ' ' + self._rules[0].get('how', ''))
        facts = [f for f in self.journal_facts if f.startswith(room or '?')]
        if facts:
            lines.append('Facts here: ' + ' | '.join(facts[-3:]))
        if unread:
            lines.append(f'Zones not read yet here: {unread}.')
        if r.get('open_items'):
            lines.append(f"Items still lying here: {r['open_items']}.")
        lines.append('Stop repeating the same actions. Act on the rule/facts above, or try a zone or door you '
                     'have not used, or call hint zone/rules.')
        text = ' '.join(lines)
        print('ALERT ' + text, file=self.out, flush=True)
        return text

    def journal(self):
        """Compact journal for the model: rooms visited with what is done and open, then facts."""
        rooms = []
        for room in self.journal_order:
            r = self.journal_rooms.get(room, {})
            entry = dict(room=room)
            if r.get('seen'):
                entry['read'] = {str(i): got for i, got in sorted(r['seen'].items())}
            if r.get('open_items'):
                entry['items_left'] = r['open_items']
            if r.get('doors'):
                entry['doors'] = {str(i): d for i, d in sorted(r['doors'].items())}
            rooms.append(entry)
        out = dict(rooms=rooms)
        if self.journal_facts:
            out['facts'] = self.journal_facts
        return out

    def watch_events(self):
        """Explain room events that just started and have content (text, voice, items, doors)."""
        d, _, _ = self.play.read()
        room = d.get('room')
        now = {t.get('event') for t in d.get('event_tasks') or [] if isinstance(t, dict)}
        # `events` is the runtime's list of dispatched events; it grows, so short events are not missed.
        dispatched = d.get('events') or []
        if len(dispatched) < self.dispatched_seen or room != self.events_room:
            self.dispatched_seen = 0 if room != self.events_room else len(dispatched)
        started = set(dispatched[self.dispatched_seen:]) | (now - self.tasks_seen)
        if room != self.events_room:
            self.reported = set()
        self.dispatched_seen, self.events_room = len(dispatched), room
        for n in sorted(started - self.reported):
            self.reported.add(n)
            try:  # fresh each visit: the cause names the items carried now
                import hint
                text = hint.why(hint.Room(room), n, d, limit=900)
            except Exception as e:  # a script the decoder cannot read must not stop play
                text = f'event {n}: no explanation ({type(e).__name__})'
            if 'does: nothing visible' not in text and 'no explanation' not in text and text not in self.pending_events:
                self.pending_events = (self.pending_events + [text])[-2:]
                cause = next((l for l in text.splitlines() if l.startswith('cause ')), '')
                if cause:
                    # Keep what matters to the player: what the event did and which carried items trigger it.
                    does = text.splitlines()[0].split('does: ', 1)[-1]
                    carried = cause.split('; ')[-1]
                    self.note_fact(f"{(room or '')[:7]}: event {n} ({does[:50]}) runs because {carried}")
        self.tasks_seen = now

    @staticmethod
    def number(v):
        """A model value as a float: numbers and numeric strings ("3.5"); anything else None."""
        if isinstance(v, bool):
            return None
        if isinstance(v, (int, float)):
            return float(v) if math.isfinite(v) else None
        if isinstance(v, str):
            try:
                f = float(v.strip())
            except ValueError:
                return None
            return f if math.isfinite(f) else None
        return None

    def normalize(self, action):
        """Turn numeric strings in zone/to/face/shots/ticks/presses/target into numbers; keep the rest."""
        out = dict(action)
        for key in ('zone', 'shots', 'ticks', 'presses', 'target', 'store', 'take'):
            if key in out and not isinstance(out[key], int):
                f = self.number(out[key])
                if f is not None and f == int(f):
                    out[key] = int(f)
        for key in ('to', 'face'):
            v = out.get(key)
            if isinstance(v, dict) and {'x', 'z'} <= set(v):
                v = [v['x'], v['z']]
            if isinstance(v, (list, tuple)) and len(v) == 3:
                v = [v[0], v[2]]   # [x, y, z] from the 3D position fields
            if isinstance(v, (list, tuple)) and len(v) == 2:
                pair = [self.number(x) for x in v]
                if None not in pair:
                    out[key] = pair
        then = out.get('then')
        if isinstance(then, dict):
            out['then'] = self.normalize(then)
        return out

    def correct(self, action, obs):
        """interact: the geometry decides where to stand, and the coordinates decide which zone was meant.

        Invented standing points were the most common rejected action, and the model mixes up zone
        numbers (it sent the coordinates of one door with the number of the other). If the given point
        is the ready point of another zone, or much closer to another zone's point, that zone is used.
        Never raises: anything unexpected leaves the action as the model sent it (play.py validates)."""
        try:
            return self._correct(self.normalize(action))
        except Exception:
            return action

    def _correct(self, action):
        if action.get('op') != 'interact':
            return action
        try:
            d, _, _ = self.play.read()
            points = self.room_points(d)
        except (PlayError, OSError, ValueError):
            return action
        zone, to = action.get('zone'), action.get('to')
        if isinstance(to, list) and len(to) == 2 and all(isinstance(v, float) for v in to) and points:
            dist = {i: math.hypot(p[0][0] - to[0], p[0][1] - to[1]) for i, p in points.items()}
            best = min(dist, key=dist.get)
            own = dist.get(zone)
            # Another zone's ready point within 3 units, while the named zone's point is 3x as far: switch.
            # A named zone without its own point (unreachable, other floor) is never switched away from.
            if best != zone and own is not None and dist[best] < 3 and own > 3 * max(dist[best], 1):
                zone = best
        if zone not in points:
            return action if zone == action.get('zone') else dict(action, zone=zone)
        go = points[zone]
        if zone == action.get('zone') and isinstance(to, list) and len(to) == 2 and \
                all(isinstance(v, float) for v in to) and abs(to[0] - go[0][0]) < .01 and abs(to[1] - go[0][1]) < .01:
            return action
        return dict(action, zone=zone, to=go[0], face=go[1])

    def mark_way_back(self, obs):
        """Name the door that leads to the room just left, so 'nearest door' is not mistaken for 'onward'."""
        room = obs.get('room')
        if room and room != self.room:
            self.previous_room, self.room = self.room, room
        if not self.previous_room:
            return obs
        try:
            back = int(self.previous_room[4:6])  # RM_SRR0: room number RR within the stage
        except ValueError:
            return obs
        for z in obs.get('zones', []):
            if z.get('kind') == 'door' and z.get('destination') == back:
                z['back'] = True
        obs['came_from'] = self.previous_room
        return obs

    def room_points(self, d):
        """Ready standing points for every active zone on this floor, once per room (geometry is fixed;
        script-switched zones and walls change the key, so the table is rebuilt when they do).

        Up to three candidates per zone; a candidate that did not work (route stalled, nothing
        happened) is skipped from then on in this room, so the next one becomes `go`."""
        # Points depend on what is reachable from where the player stands: the reachable area (its
        # first free cell) is part of the key, so crossing a gate or a detector recomputes them.
        g = self.play.grids.get(d, radius=3.5)
        reach = g.distances(d['actor'][0], d['actor'][2])
        area = min(reach) if reach else None
        key = (d.get('room'), d.get('player_floor', 0), area, len(reach),
               tuple(a.get('flags', 0) & 1 for a in d.get('interactions', [])),
               tuple(w.get('fl', 0) & 1 for w in d.get('walls', [])))
        if self._points_key != key:
            candidates = {}
            for a in d.get('interactions', []):
                if not a.get('flags', 0) & 1 or a.get('floor', 0) != d.get('player_floor', 0):
                    continue
                if a.get('type') not in (0, 1, 2, 3, 4):
                    continue
                try:
                    found = self.play.approaches(d, a, limit=3)
                except PlayError:
                    found = []
                if found:
                    candidates[a['index']] = [[f['to'], f['face']] for f in found]
            self._points_key, self._candidates = key, candidates
        room = (d.get('room') or '')[:7]  # same key as the compact state ('RM_0080'), used by point_failed
        points = {}
        for zone, cands in self._candidates.items():
            usable = [c for c in cands if (room, zone, tuple(c[0])) not in self._failed_points]
            if usable:
                points[zone] = usable[0]
        self._points = points
        return points

    def point_failed(self, room, action, result):
        """Remember a go point that did not trigger its zone; the next candidate is used next time."""
        if action.get('op') != 'interact' or result not in ('stalled', 'no_interaction_observed'):
            return
        to = action.get('to')
        if isinstance(to, list) and len(to) == 2:
            self._failed_points.add(((room or '')[:7], action.get('zone'), tuple(to)))

    def with_approaches(self, obs):
        """Give zones a ready `go` point (to, face); zones beyond the nearest five are listed in far_zones."""
        if obs.get('owner') != 'FREE':
            return obs
        d, _, _ = self.play.read()
        points = self.room_points(d)
        shown = set()
        for z in obs.get('zones', []):
            if z['i'] in points and z.get('item') != 'taken':
                z['go'] = points[z['i']]
            shown.add(z['i'])
        kinds = {0: 'door', 1: 'stairs', 2: 'stairs', 3: 'text', 4: 'item'}
        far = []
        for a in d.get('interactions', []):
            i = a['index']
            if i in shown or i not in points:
                continue
            far.append(dict(i=i, kind=kinds.get(a['type'], '?'), go=points[i],
                            **({'destination': a['parameters'][1]} if a['type'] == 0 else {})))
        if far:
            obs['far_zones'] = far
        return obs

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
            view = json.dumps(trim(obs), sort_keys=True)
            self.idle_waits = self.idle_waits + 1 if view == self.idle_view else 0
            self.idle_view = view
            self.track_saves(obs)
            self.watch_events()
            tw = obs.get('typewriter') or {}
            gameover = obs.get('gameover') or {}
            if gameover:
                # Game over: the game plays its scene, then asks Continue/Exit. The driver continues by
                # itself (the retry point restores the room); the model is told afterwards.
                retry = gameover.get('retry_play_time')
                newer = self.snapshot and self.snapshot.get('play_time') is not None and \
                    (retry is None or retry < 0 or self.snapshot['play_time'] > retry)
                if not gameover.get('prompt'):
                    action, entry = {'op': 'wait'}, dict(auto=True)
                elif newer and self.snapshot_tried != retry:
                    # A slot-2 snapshot saved in this run is newer than the retry point: load it instead.
                    self.snapshot_tried = retry
                    action, entry = {'op': 'snapshot', 'do': 'load'}, dict(auto=True, snapshot_loaded=self.snapshot)
                elif gameover.get('highlighted') != 'Continue':
                    action, entry = {'op': 'press', 'key': 'left'}, dict(auto=True)
                else:
                    action, entry = {'op': 'press', 'key': 'confirm'}, dict(auto=True, continued=gameover)
                    self.pending_events = (self.pending_events + [
                        'You died (game over) and continued from the retry point '
                        f"({gameover.get('retry_room') or 'last save'}). Items and progress since then are lost. "
                        'Avoid what killed you: heal earlier, shoot from range, do not walk into enemies.'])[-2:]
                owner = obs.get('owner', '')
                self.last_owner = owner
                obs, feedback, act_ms = self.act(action)
                if feedback['result'] == 'loaded':
                    self.pending_events = (self.pending_events + [
                        f"You died (game over). The driver loaded snapshot slot 2 (saved in {self.snapshot.get('room')}), "
                        'which is newer than the retry point. Avoid what killed you: heal earlier, shoot from range.'])[-2:]
                elif 'error' in feedback:
                    entry['error'] = feedback['error']  # a failed load falls back to Continue next time
                entry.update(owner=owner, action=action, act_ms=round(act_ms), result=feedback['result'])
                self.record(entry)
                continue
            playing = 'VIDEO' in obs.get('owner', '').split()
            if not playing:
                self.video_skipped = False
            if self.skip_videos and playing and not self.video_skipped:
                # User option: skip FMV videos (the game's own video skip); scenes are not touched.
                action, entry = {'op': 'press', 'key': 'skip'}, dict(auto=True, video_skipped=True)
                self.video_skipped = True
            elif passive(obs) and self.idle_waits < 4:
                action, entry = {'op': 'wait'}, dict(auto=True)
            elif 'TYPEWRITER' in obs.get('owner', '') and tw.get('mode') == 'save' and not self.save_allowed():
                # Leave the save screen without a model call; the ribbon and the time stay unspent.
                action, entry = {'op': 'press', 'key': 'cancel'}, dict(auto=True, save_skipped=self.save_reason())
            else:
                if self.idle_waits >= 4:
                    # Nothing changed over several waits: the game may wait for a button after all.
                    feedback = dict(feedback, unchanged_for=self.idle_waits)
                view = self.mark_seen(self.with_approaches(self.mark_way_back(obs)))
                self.update_journal(view)
                user, action, usage, llm_ms = self.decide(view, feedback,
                                                          self.last_owner)
                fallback = action.pop('no_action', False)
                note = action.pop('note', None)
                if isinstance(note, str) and note.strip():
                    self.note = note.strip()[:NOTE_MAX]
                self.remember(user, action)
                entry = dict(llm_ms=round(llm_ms), usage=usage)
                if self.bad_replies:
                    entry['bad_replies'] = self.bad_replies
            owner = obs.get('owner', '')
            self.last_owner = owner
            self.remember_texts(obs)
            room_before, progress_before = obs.get('room'), self.progress()
            fixed = self.correct(action, obs)
            blocked = self.repeat_blocked(fixed, obs)
            if blocked:
                obs, feedback, act_ms = self.play.observe(), dict(result='error', error=blocked), 0.0
            else:
                if fixed.get('zone') != action.get('zone'):
                    entry['zone_corrected'] = [action.get('zone'), fixed.get('zone')]
                obs, feedback, act_ms = self.act(fixed)
                if 'zone_corrected' in entry:
                    feedback['zone_corrected'] = (f"you named zone {action.get('zone')} but stood for zone "
                                                  f"{fixed.get('zone')}; used zone {fixed.get('zone')}")
                action = fixed
                if action.get('op') == 'interact' and feedback['result'] in ('stalled', 'no_interaction_observed'):
                    # The point did not reach the zone: not an examine; try the next candidate next time.
                    self.point_failed(room_before, action, feedback['result'])
                elif action.get('op') == 'interact' and answered(obs, feedback['result']):
                    self.record_visit(room_before, action.get('zone'), progress_before, obs, feedback['result'])
                elif action == {'op': 'press', 'key': 'confirm'} and owner == 'FREE' and self.move_zone and \
                        self.move_zone[0] == room_before and answered(obs, feedback['result']):
                    # move to a zone's ready point + confirm is the same examine as interact.
                    self.record_visit(room_before, self.move_zone[1], progress_before, obs, feedback['result'])
                if feedback['result'] == 'saved':
                    self.snapshot = dict(room=feedback.get('snapshot_room'), play_time=self.play.read()[0].get('play_time'))
            self.move_zone = self.zone_of_move(room_before, action, feedback['result'])
            if not entry.get('auto') and fallback:
                feedback.update(result='error', error='no game action in your last reply; the driver sent '
                                f"{json.dumps(action, separators=(',', ':'))} instead. Reply with only the JSON action.")
                entry['fallback'] = True
            if not entry.get('auto') and self.note_cut:
                feedback['note_cut'] = f'your reply was cut inside note; keep note under {NOTE_MAX} characters'
            if 'fire' in feedback:
                entry['fire'] = feedback['fire']
            if feedback.get('damage'):
                entry['damage'] = feedback['damage']
            if entry.get('save_skipped'):
                feedback['save_skipped'] = entry['save_skipped']
            if not entry.get('auto'):
                self.track_progress(obs, feedback['result'])
            entry.update(owner=owner, action=action, act_ms=round(act_ms), result=feedback['result'])
            if 'error' in feedback:
                entry['error'] = feedback['error']
            if 'use_one_of' in feedback:
                entry['use_one_of'] = feedback['use_one_of']
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
    ap.add_argument('--extra-body', default=os.environ.get('RECVX_LLM_EXTRA_BODY', ''),
                    help='JSON merged into every request, e.g. \'{"chat_template_kwargs":{"enable_thinking":false}}\'')
    ap.add_argument('--timeout', type=float, default=60.0, help='seconds per model request')
    ap.add_argument('--retries', type=int, default=0, help='retries on timeout/connection errors (0 = until it works)')
    ap.add_argument('--retry-delay', type=float, default=3.0, help='first retry pause, doubled up to 20 s')
    ap.add_argument('--seconds', type=float, default=6.0, help='play.py wait budget per action (<= 8)')
    ap.add_argument('--repeat-limit', type=int, default=2,
                    help='refuse examining the same zone more than N times without progress (0 = off)')
    ap.add_argument('--alert-after', type=int, default=20,
                    help='alert the model after N decisions without progress (0 = off)')
    ap.add_argument('--skip-videos', action='store_true',
                    default=os.environ.get('RECVX_SKIP_VIDEOS', '').strip().lower() in ('1', 'true', 'yes', 'on'),
                    help='skip FMV videos automatically (also RECVX_SKIP_VIDEOS=1 in .env)')
    ap.add_argument('--save-rooms', type=int, default=3,
                    help='allow saving only after N new rooms since the last save/load (0 = always allow)')
    ap.add_argument('--no-tools', action='store_true', help='endpoint without function calling')
    ap.add_argument('--dry-run', action='store_true', help='print the first request and exit')
    a = ap.parse_args()
    play = Play(a.dir)
    client = Client(a.base_url, a.model, os.environ.get('RECVX_LLM_API_KEY') or os.environ.get('OPENAI_API_KEY', ''),
                    temperature=a.temperature, max_tokens=a.max_tokens, timeout=a.timeout,
                    retries=a.retries, retry_delay=a.retry_delay,
                    extra_body=json.loads(a.extra_body) if a.extra_body.strip() else None)
    driver = Driver(play, client, tools=not a.no_tools, window=a.window, seconds=a.seconds,
                    log=Path(a.dir) / 'driver.log', save_rooms=a.save_rooms, skip_videos=a.skip_videos,
                    alert_after=a.alert_after,
                    repeat_limit=a.repeat_limit)
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
