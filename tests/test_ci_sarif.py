"""Security gate regression cases. Run with unittest discovery."""

import copy
import importlib.util
import json
import os
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
            'tool': {'driver': {'name': 'CodeQL', 'rules': [{'id': 'security/test', 'properties': {
                'security-severity': severity}}]}},
            'results': [{'ruleId': 'security/test', 'level': level}],
        }]}

    def test_medium_and_high_are_blocked_even_when_result_is_note(self):
        for severity in ('4.0', '7.0', '9.8'):
            self.assertEqual(GATE.blocking_findings(self.report(severity, 'note')), ['security/test'])

    def test_low_security_severity_passes(self):
        self.assertEqual(GATE.blocking_findings(self.report('3.9')), [])

    def test_error_level_blocks_even_with_low_security_severity(self):
        self.assertEqual(GATE.blocking_findings(self.report('3.9', 'error')), ['security/test'])

    def test_invalid_level_cannot_pass(self):
        with self.assertRaises(ValueError):
            GATE.blocking_findings(self.report('3.9', 'invalid'))

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

    def test_index_only_rule_reference(self):
        report = self.report('9.8')
        report['runs'][0]['results'] = [{'ruleIndex': 0, 'level': 'note'}]
        self.assertEqual(GATE.blocking_findings(report), ['security/test'])

    def test_extension_index_resolves_correct_rule(self):
        report = self.report('3.9')
        driver = report['runs'][0]['tool']['driver']
        extension = copy.deepcopy(driver)
        extension['rules'][0]['properties']['security-severity'] = '9.8'
        report['runs'][0]['tool']['extensions'] = [extension]
        report['runs'][0]['results'] = [{'rule': {'index': 0, 'toolComponent': {'index': 0}}, 'level': 'note'}]
        self.assertEqual(GATE.blocking_findings(report), ['security/test'])

    def test_invalid_rule_reference_cannot_pass(self):
        for result in ({'ruleIndex': -1}, {'ruleIndex': 99}, {'ruleId': 'other', 'ruleIndex': 0}, {}):
            report = self.report()
            report['runs'][0]['results'] = [result]
            with self.assertRaises(ValueError):
                GATE.blocking_findings(report)

    def test_tool_error_notification_blocks_even_clean_results(self):
        for field in ('toolExecutionNotifications', 'toolConfigurationNotifications'):
            report = self.report()
            report['runs'][0]['results'] = []
            report['runs'][0]['invocations'] = [{field: [{'level': 'error'}]}]
            with self.assertRaises(ValueError):
                GATE.blocking_findings(report)

    def test_suppressed_or_unchanged_security_findings_still_block(self):
        report = self.report()
        report['runs'][0]['results'][0].update({
            'baselineState': 'unchanged', 'suppressions': [{'status': 'accepted'}]})
        self.assertEqual(GATE.blocking_findings(report), ['security/test'])

    def test_cli_exit_status_matches_findings(self):
        environment = os.environ.copy()
        environment.pop('GITHUB_STEP_SUMMARY', None)
        for severity, expected in (('3.9', 0), ('4.0', 1), ('NaN', 1)):
            with tempfile.TemporaryDirectory() as directory:
                Path(directory, 'analysis.sarif').write_text(json.dumps(self.report(severity)), encoding='utf-8')
                result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True, env=environment)
            self.assertEqual(result.returncode, expected)

    def test_reviewed_exception_requires_exact_rule_path_and_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'main.cpp').write_text('open_selected_config();\nother_access();\n')
            report = self.report('9.8')
            result = report['runs'][0]['results'][0]
            result['locations'] = [{'physicalLocation': {
                'artifactLocation': {'uri': 'main.cpp'}, 'region': {'startLine': 1}}}]
            policy = [{'rule_id': 'security/test', 'path': 'main.cpp',
                       'statement': 'open_selected_config();', 'reason': 'Explicit local file selection'}]
            self.assertEqual(GATE.blocking_findings(report, policy, root), [])
            result['locations'][0]['physicalLocation']['region']['startLine'] = 2
            self.assertEqual(GATE.blocking_findings(report, policy, root), ['security/test'])
            result['locations'][0]['physicalLocation']['region']['startLine'] = 1
            policy[0]['rule_id'] = 'another-rule'
            self.assertEqual(GATE.blocking_findings(report, policy, root), ['security/test'])


if __name__ == '__main__':
    unittest.main()
