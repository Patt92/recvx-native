#!/usr/bin/env python3
"""MCP server (stdio, JSON-RPC 2.0, one JSON object per line) over the AI-play driver: an MCP client
such as Claude Code plays the running game with a handful of tool calls per room instead of shell
commands.

  python3 tools/agent_play/mcp_server.py [--dir build/play] [--seconds 6]

The runtime must run with --control <dir>/live.txt and `autodump 5 auto` (see the playbook). Tools:
observe, act, plan, zone, hint, board, map. No model is called here; the client is the player."""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from driver import Driver, HINT_CMDS, MACROS, trim  # noqa: E402
from play import Play, PlayError  # noqa: E402

PROTOCOL = '2025-06-18'

TOOLS = [
    {'name': 'observe', 'description': 'The moment: situation sentences, every zone of the floor with what '
             'examining it does, the obstacle board, the room map, your last actions and the compact state.',
     'inputSchema': {'type': 'object', 'properties': {}}},
    {'name': 'act', 'description': 'Run one game action (press, interact, move, fire, shake, keys, run, type, '
             'box, use, equip, reload, snapshot, wait) or one macro (' + ', '.join(MACROS) + '); optional '
             '`expect` is checked against the outcome. Returns result, what changed and the new observation.',
     'inputSchema': {'type': 'object', 'properties': {'action': {'type': 'object'}}, 'required': ['action']}},
    {'name': 'plan', 'description': 'Run several steps back to back; each may carry `expect`. Stops at the '
             'first step that ends differently and says why.',
     'inputSchema': {'type': 'object', 'properties': {'steps': {'type': 'array', 'items': {'type': 'object'}}},
                     'required': ['steps']}},
    {'name': 'zone', 'description': 'Bounds and ready standing points (to, face) of zone i.',
     'inputSchema': {'type': 'object', 'properties': {'i': {'type': 'integer'}}, 'required': ['i']}},
    {'name': 'hint', 'description': 'Read the room script: cmd one of ' + ', '.join(HINT_CMDS) + '; args as strings.',
     'inputSchema': {'type': 'object', 'properties': {'cmd': {'type': 'string'},
                                                      'args': {'type': 'array', 'items': {'type': 'string'}}},
                     'required': ['cmd']}},
    {'name': 'board', 'description': 'Open obstacles: item gates with wall state, facts about locks and needs, '
             'doors and items of every visited room.', 'inputSchema': {'type': 'object', 'properties': {}}},
    {'name': 'map', 'description': 'The current room as text lines.', 'inputSchema': {'type': 'object', 'properties': {}}},
    {'name': 'guide', 'description': 'The next steps of the current room, derived from its scripts: weapon left '
             'in a box, item gates, items to pick up, zones that act now, zones that need something first and '
             'how to get it, closed doors and what opens them, where carried items are used.',
     'inputSchema': {'type': 'object', 'properties': {}}},
    {'name': 'near', 'description': 'Rooms within three doors of the current room (door tables of the game '
             'data) with the items their files list, and whether they were visited in this run.',
     'inputSchema': {'type': 'object', 'properties': {}}},
    {'name': 'memory', 'description': 'What brought progress, what did nothing and how the player died in '
             'earlier runs (build/play/memory.json); optional note to add under worked|failed.',
     'inputSchema': {'type': 'object', 'properties': {'kind': {'type': 'string', 'enum': ['worked', 'failed']},
                                                      'note': {'type': 'string'}}}},
]


class Server:
    def __init__(self, play, seconds=6.0, log=None):
        self.driver = Driver(play, None, tools=False, seconds=seconds, log=log, out=sys.stderr)
        self.play = play

    # --- tools --------------------------------------------------------------------------------------------
    def view(self):
        obs = self.play.observe()
        view = self.driver.mark_seen(self.driver.with_approaches(self.driver.mark_way_back(obs)))
        self.driver.update_journal(view)
        return view

    def observe(self):
        view = self.view()
        out = dict(situation=self.driver.situation(view), state=trim(view))
        if view.get('owner') == 'FREE':
            d, _, _ = self.play.read()
            out.update(zones_all=self.driver.zones_all(view, d), board=self.driver.board(view, d),
                       map=self.driver.room_map(d))
        if self.driver.history_lines:
            out['history'] = self.driver.history_lines[-10:]
        if self.driver.pending_feedback:
            out.update(self.driver.pending_feedback)
            self.driver.pending_feedback = {}
        return out

    def act(self, action):
        return self.plan([action])

    def plan(self, steps):
        steps = [dict(s) for s in steps if isinstance(s, dict) and isinstance(s.get('op'), str)]
        if not steps:
            raise PlayError('plan needs steps with an "op"')
        before = self.driver.steps
        self.driver.plan, self.driver.plan_total, self.driver.plan_done = steps, len(steps), 0
        self.driver.run(steps=before + 40, until_plan_done=True)
        log = self.driver.history_lines[-(self.driver.steps - before):] if self.driver.steps > before else []
        out = dict(ran=log, change=self.driver.last_change)
        out.update(self.driver.pending_feedback)
        self.driver.pending_feedback = {}
        out['observation'] = self.observe()
        return out

    def call(self, name, args):
        if name == 'observe':
            return self.observe()
        if name == 'act':
            return self.act(args.get('action') or {})
        if name == 'plan':
            return self.plan(args.get('steps') or [])
        if name == 'zone':
            return self.driver.tool('zone', dict(i=int(args.get('i'))))
        if name == 'hint':
            return self.driver.tool('hint', dict(cmd=args.get('cmd'), args=args.get('args') or []))
        if name == 'board':
            view = self.view()
            return self.driver.board(view, self.play.read()[0])
        if name == 'map':
            return self.driver.room_map(self.play.read()[0])
        if name == 'guide':
            return self.driver.guide(self.view(), self.play.read()[0])
        if name == 'near':
            return self.driver.neighbour_lines(self.view())
        if name == 'memory':
            if args.get('note'):
                self.driver.remember_outcome(args.get('kind') or 'worked', args['note'])
            return self.driver.memory
        raise PlayError(f'unknown tool {name}')

    # --- JSON-RPC -----------------------------------------------------------------------------------------
    def handle(self, message):
        method, params, ident = message.get('method'), message.get('params') or {}, message.get('id')
        if method == 'initialize':
            result = dict(protocolVersion=PROTOCOL, capabilities={'tools': {}},
                          serverInfo={'name': 'recvx-play', 'version': '1'})
        elif method == 'tools/list':
            result = {'tools': TOOLS}
        elif method == 'tools/call':
            try:
                value = self.call(params.get('name'), params.get('arguments') or {})
                result = {'content': [{'type': 'text', 'text': json.dumps(value, ensure_ascii=False)}]}
            except (PlayError, FileNotFoundError, ValueError, KeyError, TypeError) as e:
                result = {'content': [{'type': 'text', 'text': f'error: {e}'}], 'isError': True}
        elif method == 'ping':
            result = {}
        elif method and method.startswith('notifications/'):
            return None
        else:
            return {'jsonrpc': '2.0', 'id': ident, 'error': {'code': -32601, 'message': f'unknown method {method}'}}
        if ident is None:
            return None
        return {'jsonrpc': '2.0', 'id': ident, 'result': result}

    def serve(self, inp=sys.stdin, out=sys.stdout):
        for line in inp:
            line = line.strip()
            if not line:
                continue
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                continue
            reply = self.handle(message)
            if reply is not None:
                out.write(json.dumps(reply, ensure_ascii=False) + '\n')
                out.flush()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--dir', default='build/play')
    ap.add_argument('--seconds', type=float, default=6.0)
    ap.add_argument('--log', default=None, help='JSON lines of every action (default <dir>/mcp.log)')
    a = ap.parse_args()
    play = Play(a.dir)
    Server(play, a.seconds, a.log or os.path.join(a.dir, 'mcp.log')).serve()


if __name__ == '__main__':
    main()
