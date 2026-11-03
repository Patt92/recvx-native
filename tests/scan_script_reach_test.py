#!/usr/bin/env python3
"""Static script reach scan (tools/scan_script_reach.py): every room script decodes with the native lengths,
every command, controller sub-command and operand domain is known to the native EventVm/world binding.
Counter-check: a copied room whose scd0 is overwritten with bhCk on matrix 73 (outside flag() 1..16, which the
native VM rejects and thereby stalls the task) must be reported, and a dropped WHILE body offset must not be
decoded as the original's (event.c:9132 bhWhile: body at p + p[1], not p + 2 + p[1])."""
import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('scan_script_reach', ROOT / 'tools/scan_script_reach.py')
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)
ROOM = ROOT / 'data/rooms/RM_0050.RDT'


@unittest.skipUnless(ROOM.is_file(), 'needs extracted room data')
class ScanScriptReachTest(unittest.TestCase):
    def test_all_rooms_known_to_the_native_port(self):
        rooms, found, unknown = scan.scan(ROOT / 'data')
        self.assertGreaterEqual(rooms, 205)
        self.assertEqual(dict(unknown), {}, 'script commands or operands the native port does not know')
        # Reached controller sub-commands are exactly the ones the port binds (events.cpp/world.cpp).
        self.assertTrue({k[1] for k in found if k[0] == 'pl'} <= scan.NATIVE_PL)
        self.assertTrue({k[1] for k in found if k[0] == 'sub'} <= scan.NATIVE_SUB)

    def test_counter_check_reports_a_flag_matrix_outside_the_original(self):
        with tempfile.TemporaryDirectory() as tmp:
            rooms = Path(tmp) / 'rooms'; rooms.mkdir()
            b = bytearray(ROOM.read_bytes())
            base, scd0, _, _ = scan.room_tables(bytes(b))
            b[scd0:scd0 + 7] = bytes([0x04, 73, 0, 0, 0, 0, 0x00])  # bhCk matrix 73 ... bhEnd
            (rooms / 'RM_0050.RDT').write_bytes(b)
            count, _, unknown = scan.scan(Path(tmp))
            self.assertEqual(count, 1)
            self.assertTrue(any('bhCk matrix' in str(k[1]) for k in unknown), dict(unknown))
            self.assertIn(f'RM_0050@{scd0 - base:04x}', sum(unknown.values(), []))

    def test_while_body_offset_follows_bhwhile(self):
        # 0xfc rel: body at p + rel. With p + 2 + rel the walker would land inside the condition's operands.
        b = bytes([0xfc, 0x08, 0x04, 1, 0, 0, 0, 0, 0x00, 0x00])
        found, unknown = scan.defaultdict(list), scan.defaultdict(list)
        scan.TABLES = {}
        scan.walk(b, 0, 0, set(), found, 'X', unknown, None, {})
        self.assertEqual(dict(unknown), {})
        self.assertEqual(found[('op', 0x04)], ['X@0002'])


if __name__ == '__main__':
    unittest.main()
