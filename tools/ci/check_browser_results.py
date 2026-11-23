"""Require a nonempty browser JUnit report with no skipped or failed cases."""

import sys
from pathlib import Path
import xml.etree.ElementTree as ET


def validate(path):
    cases = ET.parse(path).getroot().findall('.//testcase')
    browser = [case for case in cases if 'test_browser_smoke' in case.get('classname', '')]
    if not browser:
        raise ValueError('No browser tests executed')
    invalid = [case.get('name') for case in browser if any(
        case.find(tag) is not None for tag in ('skipped', 'failure', 'error'))]
    if invalid:
        raise ValueError('Browser tests failed or skipped: ' + ', '.join(invalid))
    return len(browser)


if __name__ == '__main__':
    try:
        print(f'Required browser tests passed: {validate(Path(sys.argv[1]))}; skipped: 0')
    except (OSError, ValueError, ET.ParseError) as error:
        raise SystemExit(str(error)) from error
