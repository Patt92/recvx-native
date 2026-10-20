#!/usr/bin/env python3
"""Contracts for the native-port status report; never touches quarantine."""
import importlib.util
import sys
import tempfile
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

    def test_landing_page_keeps_progress_in_external_report(self):
        status = (ROOT / 'docs/PORT_STATUS.md').read_text()
        readme = (ROOT / 'README.md').read_text()
        self.assertIn(port_check.README_EXTERNAL, readme)
        self.assertIn('](docs/PORT_STATUS.md)', readme)
        self.assertNotIn(port_check.README_BEGIN, readme)
        self.assertNotIn(port_check.README_END, readme)
        self.assertNotIn('<!-- native-progress:begin -->', readme)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'README.md'
            path.write_text(readme)
            port_check.update_readme(path, status)
            self.assertEqual(path.read_text(), readme)

    def test_inline_progress_still_comes_from_report(self):
        status = (ROOT / 'docs/PORT_STATUS.md').read_text()
        block = port_check.readme_block_from_status(status)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'README.md'
            path.write_text(f'# Inline report\n{port_check.README_BEGIN}\n{port_check.README_END}\n')
            port_check.update_readme(path, status)
            self.assertEqual(path.read_text(), f'# Inline report\n{block}\n')

    def test_external_progress_requires_link_and_rejects_inline_markers(self):
        status = (ROOT / 'docs/PORT_STATUS.md').read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'README.md'
            for text in (port_check.README_EXTERNAL,
                         port_check.README_EXTERNAL + '\n[Report](docs/PORT_STATUS.md)\n' + port_check.README_BEGIN):
                path.write_text(text)
                with self.assertRaises(SystemExit):
                    port_check.update_readme(path, status)
                self.assertEqual(path.read_text(), text)


if __name__ == '__main__':
    unittest.main()
