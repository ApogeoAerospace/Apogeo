"""Exercise the actual simulator executable and its stdio protocol on every OS."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = None


class SimulatorProcessTests(unittest.TestCase):
    def setUp(self):
        if BINARY is None:
            self.skipTest('Run through CTest or supply --binary to the script')

    def run_simulator(self, arguments, commands=None):
        with tempfile.TemporaryDirectory() as directory:
            config = {
                'simulation': {'duration': 0.05, 'max_iterations': 5},
                'logging': {'console_output': False, 'file_output': False},
                'plugins': [],
                'initial_state_file': str(ROOT / 'data/defaults/default_state.json'),
                'output_directory': str(Path(directory) / 'output'),
            }
            path = Path(directory) / 'config.json'
            path.write_text(json.dumps(config), encoding='utf-8')
            return subprocess.run(
                [str(BINARY), '--config', str(path), *arguments], cwd=directory,
                input=commands, capture_output=True, text=True, timeout=30,
            )

    def test_cli_help_version_and_invalid_arguments(self):
        for argument in ('--help', '--version'):
            result = self.run_simulator([argument])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('Apogeo', result.stdout)
        for arguments in (['--unknown'], ['--ticks', '0'], ['--ticks', 'bad'],
                          ['--config'], ['--state'], ['--ipc'], ['--ipc', 'unsupported']):
            result = self.run_simulator(arguments)
            self.assertNotEqual(result.returncode, 0, arguments)

    def test_tick_and_full_simulation_exit_successfully(self):
        for arguments in (['--ticks', '2'], []):
            result = self.run_simulator(arguments)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_ipc_lifecycle_and_error_recovery(self):
        commands = [
            ('run_ticks', {'count': 1}, False),
            ('get_status', {}, True),
            ('initialize', {}, True),
            ('run_ticks', {}, False),
            ('run_ticks', {'count': 0}, False),
            ('run_ticks', {'count': -1}, False),
            ('run_ticks', {'count': 2}, True),
            ('get_status', {}, True),
            ('unknown', {}, False),
            ('shutdown', {}, True),
            ('initialize', {}, True),
            ('run_full', {}, True),
            ('shutdown', {}, True),
        ]
        lines = ['not json', ''] + [json.dumps({
            'command': name, 'request_id': str(index), 'payload': payload,
        }) for index, (name, payload, _) in enumerate(commands)]
        result = self.run_simulator(['--ipc', 'stdio'], '\n'.join(lines) + '\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        responses = [json.loads(line) for line in result.stdout.splitlines() if line.strip()]
        replies = {item['request_id']: item for item in responses if item['type'] in ('ack', 'error')}
        self.assertTrue(any(item.get('error', {}).get('code') == 'parse_error' for item in responses))
        for index, (name, _, success) in enumerate(commands):
            self.assertEqual(replies[str(index)]['ok'], success, (name, replies[str(index)]))
        self.assertTrue(any(item.get('event') == 'simulation_finished' for item in responses))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    BINARY = args.binary.resolve()
    unittest.main(argv=[__file__, *remaining])
