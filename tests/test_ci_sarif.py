"""Security gate regression cases. Run with unittest discovery."""

import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/ci/check_sarif.py'
SPEC = importlib.util.spec_from_file_location('check_sarif', SCRIPT)
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class SarifGateTests(unittest.TestCase):
    def report(self, severity='4.0', level='warning'):
        return {'version': '2.1.0', 'runs': [{
            'tool': {'driver': {'rules': [{'id': 'security/test', 'properties': {
                'security-severity': severity}}]}},
            'results': [{'ruleId': 'security/test', 'level': level}],
        }]}

    def test_medium_and_high_are_blocked_even_when_result_is_note(self):
        for severity in ('4.0', '7.0', '9.8'):
            self.assertEqual(GATE.blocking_findings(self.report(severity, 'note')), ['security/test'])

    def test_low_security_severity_passes(self):
        self.assertEqual(GATE.blocking_findings(self.report('3.9')), [])

    def test_unclassified_warning_is_blocked(self):
        report = self.report()
        report['runs'][0]['tool']['driver']['rules'] = []
        self.assertEqual(GATE.blocking_findings(report), ['security/test'])

    def test_extension_rules_are_resolved(self):
        report = self.report('3.9')
        report['runs'][0]['tool']['extensions'] = [copy.deepcopy(report['runs'][0]['tool']['driver'])]
        report['runs'][0]['tool']['driver']['rules'] = []
        self.assertEqual(GATE.blocking_findings(report), [])

    def test_clean_scan_passes(self):
        report = self.report()
        report['runs'][0]['results'] = []
        self.assertEqual(GATE.blocking_findings(report), [])

    def test_failed_or_invalid_scan_cannot_pass(self):
        for report in ({}, {'version': '2.1.0', 'runs': []}, self.report('NaN')):
            with self.assertRaises(ValueError):
                GATE.blocking_findings(report)
        report = self.report()
        report['runs'][0]['invocations'] = [{'executionSuccessful': False}]
        with self.assertRaises(ValueError):
            GATE.blocking_findings(report)

    def test_missing_reports_fail_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
