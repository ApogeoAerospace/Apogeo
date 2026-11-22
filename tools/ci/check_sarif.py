"""Fail CI for medium or higher security findings or unclassified warnings/errors."""

import argparse
import json
import math
import os
from pathlib import Path


def resolve_rule(result, tool):
    """Resolve SARIF's ID, index, and extension-component rule references."""
    reference = result.get('rule', {})
    rule_id = result.get('ruleId', reference.get('id'))
    component = reference.get('toolComponent')
    components = [tool['driver'], *tool.get('extensions', [])]
    if component is not None:
        extensions = tool.get('extensions', [])
        if 'index' in component:
            index = component['index']
            if type(index) is not int or not 0 <= index < len(extensions):
                raise ValueError('Invalid SARIF tool component index')
            components = [extensions[index]]
        else:
            components = [item for item in components if
                          ('name' in component and item.get('name') == component['name']) or
                          ('guid' in component and item.get('guid') == component['guid'])]
            if len(components) != 1:
                raise ValueError('Unresolved SARIF tool component')
    index = result.get('ruleIndex', reference.get('index'))
    if index is not None:
        rules = (components[0] if component is not None else tool['driver']).get('rules', [])
        if type(index) is not int or not 0 <= index < len(rules):
            raise ValueError('Invalid SARIF rule index')
        rule = rules[index]
        if rule_id is not None and rule.get('id') != rule_id:
            raise ValueError('SARIF rule ID and index disagree')
        return rule.get('id', 'unknown'), rule
    if not isinstance(rule_id, str) or not rule_id:
        raise ValueError('SARIF result has no rule reference')
    matches = [rule for item in components for rule in item.get('rules', []) if rule.get('id') == rule_id]
    if len(matches) > 1 and any(rule != matches[0] for rule in matches[1:]):
        raise ValueError('Ambiguous SARIF rule ID')
    return rule_id, matches[0] if matches else {}


def blocking_findings(report):
    if not isinstance(report, dict) or report.get('version') != '2.1.0' or not report.get('runs'):
        raise ValueError('Missing SARIF 2.1.0 analysis runs')
    blocked = []
    for run in report['runs']:
        for invocation in run.get('invocations', []):
            if invocation.get('executionSuccessful') is False:
                raise ValueError('Scanner reported an unsuccessful execution')
            for field in ('toolExecutionNotifications', 'toolConfigurationNotifications'):
                if any(item.get('level') == 'error' for item in invocation.get(field, [])):
                    raise ValueError('Scanner reported an error notification')
        if not run.get('tool', {}).get('driver', {}).get('name'):
            raise ValueError('Missing SARIF scanner identity')
        for result in run.get('results', []):
            rule_id, rule = resolve_rule(result, run['tool'])
            severity = rule.get('properties', {}).get('security-severity')
            level = result.get('level', rule.get('defaultConfiguration', {}).get('level', 'warning'))
            if level not in ('none', 'note', 'warning', 'error'):
                raise ValueError('Invalid SARIF result level')
            if severity is not None:
                if isinstance(severity, bool):
                    raise ValueError('Invalid security severity')
                score = float(severity)
                if not math.isfinite(score) or not 0 <= score <= 10:
                    raise ValueError('Invalid security severity')
                blocking = score >= 4.0 or level == 'error'
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
    try:
        for path in reports:
            findings = blocking_findings(json.loads(path.read_text(encoding='utf-8')))
            count += len(findings)
            for rule in findings:
                print(f'Blocking finding: {path.name}: {rule}')
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        raise SystemExit(f'ERROR: Invalid or incomplete analysis: {error}') from error
    print(f'Blocking security findings: {count}; allowed: 0')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
            summary.write(f'Blocking CodeQL security/correctness findings: **{count}**; allowed: **0**.\n')
    if count:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
