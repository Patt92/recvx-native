#!/usr/bin/env python3
"""Contracts for the native-port status report; never touches quarantine."""
import importlib.util
import sys
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('port_check', ROOT / 'tools/port_check.py')
port_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(port_check)


class PortCheckTest(unittest.TestCase):
    def test_retired_body_claim_is_not_native_evidence(self):
        self.assertFalse(port_check.ledger_evidence_valid(
            'Original function extracted verbatim and generated for the host'))

    def test_semantics_reference_is_still_allowed(self):
        self.assertTrue(port_check.ledger_evidence_valid(
            'Native state machine follows the original operand order and has a runtime test'))

    def test_report_has_no_original_corpus_denominator_or_name_match_evidence(self):
        records, _, _ = port_check.classify()
        self.assertTrue(records)
        self.assertFalse(any(record['source'] == 'verweis' for record in records))
        self.assertFalse(any(record['source'] == 'forbidden' for record in records))
        self.assertTrue(any(record['status'] == 'unbestaetigt' for record in records))


if __name__ == '__main__':
    unittest.main()
