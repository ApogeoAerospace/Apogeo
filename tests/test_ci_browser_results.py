import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/ci/check_browser_results.py'
SPEC = importlib.util.spec_from_file_location('browser_results', SCRIPT)
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class BrowserResultsTests(unittest.TestCase):
    def check(self, cases):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'results.xml'
            report.write_text(f'<testsuites><testsuite>{cases}</testsuite></testsuites>', encoding='utf-8')
            return GATE.validate(report)

    def test_passing_browser_case(self):
        self.assertEqual(self.check('<testcase classname="tests.test_browser_smoke.BrowserSmokeTests" name="test_ui"/>'), 1)

    def test_empty_or_api_only_reports_fail(self):
        for cases in ('', '<testcase classname="test_api" name="test_request"/>'):
            with self.assertRaises(ValueError):
                self.check(cases)

    def test_skips_failures_and_errors_are_blocking(self):
        for tag in ('skipped', 'failure', 'error'):
            with self.assertRaises(ValueError):
                self.check(f'<testcase classname="test_browser_smoke" name="test_ui"><{tag}/></testcase>')

    def test_missing_report_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(OSError):
                GATE.validate(Path(directory) / 'missing.xml')
