"""Enforce coverage, clang-tidy, and aggregate CI gates without silent skips."""

import argparse
import json
import math
import os
from pathlib import Path
import re


REQUIRED_JOBS = {
    'build-and-test', 'api-tests', 'browser-tests', 'static-analysis',
    'code-quality', 'sanitizer-tests', 'security-checks', 'dependency-review', 'codeql',
}


def coverage_gate(report, minimum):
    if not math.isfinite(minimum) or not 0 <= minimum <= 100:
        raise ValueError('Invalid coverage threshold')
    records = {}
    source = None
    counters = {}
    for line in report.splitlines():
        if line.startswith('SF:'):
            if source is not None or not line[3:] or line[3:] in records:
                raise ValueError('Invalid or duplicate LCOV source record')
            source, counters = line[3:], {}
        elif line.startswith(('LF:', 'LH:')):
            key = line[:2]
            if source is None or key in counters:
                raise ValueError('Invalid LCOV counter record')
            counters[key] = int(line[3:])
        elif line == 'end_of_record':
            if source is None or set(counters) != {'LF', 'LH'}:
                raise ValueError('Incomplete LCOV record')
            if not 0 <= counters['LH'] <= counters['LF']:
                raise ValueError('Invalid LCOV line counts')
            records[source] = counters
            source = None
    if source is not None or not records:
        raise ValueError('Missing or incomplete LCOV source records')
    found = sum(record['LF'] for record in records.values())
    hit = sum(record['LH'] for record in records.values())
    if found <= 0:
        raise ValueError('Coverage report contains no executable lines')
    coverage = 100 * hit / found
    if coverage < minimum:
        raise ValueError(f'Insufficient line coverage: {coverage:.2f}%; required: {minimum}%')
    return f'Line coverage: {coverage:.2f}% ({hit}/{found}); required: {minimum}%'


def tidy_gate(report, exit_code, maximum):
    if exit_code < 0 or maximum < 0:
        raise ValueError('Invalid clang-tidy gate inputs')
    report = re.sub(r'\x1b\[[0-9;]*m', '', report)
    warnings = errors = security = 0
    for line in report.splitlines():
        diagnostic = re.match(r'^.+?:\d+:\d+: (warning|error|fatal error): (.*)$', line)
        if diagnostic:
            level, message = diagnostic.groups()
            warnings += level == 'warning'
            errors += level != 'warning'
            security += bool(re.search(r'\[[^\]]*(?:clang-analyzer-|cert-)', message))
        elif re.match(r'^(?:clang-tidy: )?(?:fatal )?error:', line):
            errors += 1
    detail = (f'Warnings: {warnings}; maximum: {maximum}; errors: {errors}; '
              f'analyzer/CERT diagnostics: {security}; tool exit: {exit_code}')
    if exit_code or errors or security or warnings > maximum:
        raise ValueError(detail)
    return detail


def summary_gate(results, event):
    if not isinstance(results, dict) or REQUIRED_JOBS.difference(results):
        raise ValueError('Missing required job results')
    failed = []
    for job, value in results.items():
        status = value['result']
        expected_skip = job == 'dependency-review' and event in ('push', 'workflow_dispatch', 'merge_group')
        if status != 'success' and not (expected_skip and status == 'skipped'):
            failed.append(job)
    if failed:
        raise ValueError('Failed or incomplete gates: ' + ', '.join(sorted(failed)))
    return 'All required quality and security gates passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='gate', required=True)
    coverage = commands.add_parser('coverage')
    coverage.add_argument('report', type=Path)
    coverage.add_argument('--minimum', type=float, required=True)
    tidy = commands.add_parser('tidy')
    tidy.add_argument('report', type=Path)
    tidy.add_argument('--exit-code', type=int, required=True)
    tidy.add_argument('--maximum', type=int, required=True)
    commands.add_parser('summary')
    args = parser.parse_args()
    try:
        if args.gate == 'coverage':
            message = coverage_gate(args.report.read_text(encoding='utf-8'), args.minimum)
        elif args.gate == 'tidy':
            message = tidy_gate(args.report.read_text(encoding='utf-8'), args.exit_code, args.maximum)
        else:
            results = json.loads(os.environ['JOB_RESULTS'])
            event = os.environ['EVENT_NAME']
            if os.environ.get('GITHUB_STEP_SUMMARY'):
                with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
                    summary.write('# CI Quality and Security\n\n| Job | Result |\n|---|---|\n')
                    for job, value in sorted(results.items()):
                        summary.write(f'| {job} | {value["result"]} |\n')
            message = summary_gate(results, event)
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(f'ERROR: {error}') from error
    print(message)


if __name__ == '__main__':
    main()
