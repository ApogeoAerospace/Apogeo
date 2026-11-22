"""Fail CI for medium or higher security findings or unclassified warnings/errors."""

import argparse
import json
import math
from pathlib import Path


def blocking_findings(report):
    if report.get('version') != '2.1.0' or not report.get('runs'):
        raise ValueError('Missing SARIF 2.1.0 analysis runs')
    blocked = []
    for run in report['runs']:
        for invocation in run.get('invocations', []):
            if invocation.get('executionSuccessful') is False:
                raise ValueError('Scanner reported an unsuccessful execution')
        components = [run['tool']['driver'], *run['tool'].get('extensions', [])]
        rules = {rule['id']: rule for component in components for rule in component.get('rules', [])}
        for result in run.get('results', []):
            rule_id = result.get('ruleId', result.get('rule', {}).get('id', 'unknown'))
            rule = rules.get(rule_id, {})
            severity = rule.get('properties', {}).get('security-severity')
            level = result.get('level', rule.get('defaultConfiguration', {}).get('level', 'warning'))
            if severity is not None:
                score = float(severity)
                if not math.isfinite(score) or not 0 <= score <= 10:
                    raise ValueError('Invalid security severity')
                blocking = score >= 4.0
            else:
                blocking = level in ('warning', 'error')
            if blocking:
                blocked.append(rule_id)
    return blocked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    reports = sorted(args.directory.rglob('*.sarif'))
    if not reports:
        raise SystemExit('ERROR: No SARIF reports produced')
    count = 0
    for path in reports:
        findings = blocking_findings(json.loads(path.read_text(encoding='utf-8')))
        count += len(findings)
        for rule in findings:
            print(f'Blocking finding: {path.name}: {rule}')
    print(f'Blocking security findings: {count}; allowed: 0')
    if count:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
