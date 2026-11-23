import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/ci/check_quality.py'
SPEC = importlib.util.spec_from_file_location('quality', SCRIPT)
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def lcov(found=100, hit=80):
    return f'SF:src/example.cpp\nLF:{found}\nLH:{hit}\nend_of_record\n'


class QualityGateTests(unittest.TestCase):
    def test_coverage_boundary_and_multiple_sources(self):
        self.assertIn('80.00%', GATE.coverage_gate(lcov(), 80))
        report = lcov() + lcov().replace('example.cpp', 'other.cpp')
        self.assertIn('(160/200)', GATE.coverage_gate(report, 80))

    def test_low_empty_and_malformed_coverage_fail(self):
        for report in (lcov(hit=79), '', lcov(0, 0), lcov(10, 11), lcov(-1, -1),
                       lcov().replace('LH:80\n', ''), lcov().replace('end_of_record', ''),
                       lcov() + lcov(), lcov().replace('LF:100', 'LF:invalid')):
            with self.subTest(report=report), self.assertRaises(ValueError):
                GATE.coverage_gate(report, 80)

    def test_invalid_threshold_fails(self):
        for minimum in (float('nan'), float('inf'), -1, 101):
            with self.subTest(minimum=minimum), self.assertRaises(ValueError):
                GATE.coverage_gate(lcov(), minimum)

    def test_tidy_limit_and_clean_output(self):
        GATE.tidy_gate('', 0, 10)
        GATE.tidy_gate('src/a.cpp:1:2: warning: slow [performance-test]\n' * 10, 0, 10)

    def test_tidy_security_errors_and_excess_warnings_fail(self):
        for report, code in (
            ('', 1), ('src/a.cpp:1:2: warning: slow [performance-test]\n' * 11, 0),
            ('src/a.cpp:1:2: warning: unsafe [clang-analyzer-test]', 0),
            ('src/a.cpp:1:2: warning: unsafe [other,cert-test]', 0),
            ('C:\\src\\a.cpp:1:2: error: broken [test]', 0),
            ('src/a.cpp:1:2: fatal error: broken', 0), ('error: no compile commands', 0),
            ('\x1b[31msrc/a.cpp:1:2: warning: unsafe [cert-test]\x1b[0m', 0),
        ):
            with self.subTest(report=report, code=code), self.assertRaises(ValueError):
                GATE.tidy_gate(report, code, 10)

    def results(self):
        return {job: {'result': 'success'} for job in GATE.REQUIRED_JOBS}

    def test_all_success_and_expected_dependency_skip(self):
        GATE.summary_gate(self.results(), 'pull_request')
        for event in ('push', 'workflow_dispatch', 'merge_group'):
            results = self.results()
            results['dependency-review']['result'] = 'skipped'
            GATE.summary_gate(results, event)

    def test_every_required_job_failure_cancellation_and_skip_blocks(self):
        for job in GATE.REQUIRED_JOBS:
            for status in ('failure', 'cancelled', 'skipped', 'in_progress'):
                with self.subTest(job=job, status=status), self.assertRaises(ValueError):
                    results = self.results()
                    results[job]['result'] = status
                    GATE.summary_gate(results, 'pull_request')

    def test_missing_results_and_unexpected_failures_block(self):
        for job in GATE.REQUIRED_JOBS:
            with self.subTest(job=job), self.assertRaises(ValueError):
                results = self.results()
                del results[job]
                GATE.summary_gate(results, 'push')
        results = self.results()
        results['new-job'] = {'result': 'failure'}
        with self.assertRaises(ValueError):
            GATE.summary_gate(results, 'push')

    def test_cli_tidy_failure_propagates_into_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'tidy.txt'
            for contents, code, success in (
                (None, 0, False), ('', 1, False), ('', 0, True),
                ('src/a.cpp:1:2: warning: slow [performance-test]\n' * 11, 0, False),
                ('src/a.cpp:1:2: warning: unsafe [cert-test]', 0, False),
            ):
                with self.subTest(contents=contents, code=code):
                    if contents is not None:
                        report.write_text(contents, encoding='utf-8')
                    result = subprocess.run([sys.executable, str(SCRIPT), 'tidy', str(report),
                                             '--maximum', '10', '--exit-code', str(code)], capture_output=True)
                    self.assertEqual(result.returncode == 0, success)
                    jobs = self.results()
                    jobs['static-analysis']['result'] = 'failure' if result.returncode else 'success'
                    env = dict(os.environ, JOB_RESULTS=json.dumps(jobs), EVENT_NAME='pull_request')
                    env.pop('GITHUB_STEP_SUMMARY', None)
                    result = subprocess.run([sys.executable, str(SCRIPT), 'summary'], env=env,
                                            capture_output=True)
                    self.assertEqual(result.returncode == 0, success)

    def test_cli_failure_propagates_into_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'coverage.info'
            for contents in (None, lcov(hit=79), lcov()):
                with self.subTest(contents=contents):
                    if contents is not None:
                        report.write_text(contents, encoding='utf-8')
                    result = subprocess.run([sys.executable, str(SCRIPT), 'coverage', str(report),
                                             '--minimum', '80'], capture_output=True)
                    self.assertEqual(result.returncode == 0, contents == lcov())
                    jobs = self.results()
                    jobs['build-and-test']['result'] = 'failure' if result.returncode else 'success'
                    env = dict(os.environ, JOB_RESULTS=json.dumps(jobs), EVENT_NAME='pull_request')
                    env.pop('GITHUB_STEP_SUMMARY', None)
                    result = subprocess.run([sys.executable, str(SCRIPT), 'summary'], env=env,
                                            capture_output=True)
                    self.assertEqual(result.returncode == 0, contents == lcov())
