"""Exercise the actual simulator executable and its stdio protocol on every OS."""

import argparse
import json
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = None


class SimulatorProcessTests(unittest.TestCase):
    def setUp(self):
        if BINARY is None:
            self.skipTest('Run through CTest or supply --binary to the script')

    def plugin_path(self, name):
        suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'
        filename = ('' if sys.platform == 'win32' else 'lib') + name + suffix
        for root in (BINARY.parent, BINARY.parent.parent):
            for candidate in (root / 'plugins' / filename, root / 'plugins' / BINARY.parent.name / filename):
                if candidate.is_file():
                    return str(candidate)
        self.fail(f'Missing built plugin: {name}')

    def run_simulator(self, arguments, commands=None, overrides=None, flight_script=None):
        with tempfile.TemporaryDirectory() as directory:
            for folder in ('mass', 'limits', 'aero', 'defaults'):
                shutil.copytree(ROOT / 'data' / folder, Path(directory) / 'data' / folder)
            config = {
                'simulation': {'duration': 0.05, 'max_iterations': 5},
                'logging': {'console_output': False, 'file_output': False},
                'plugins': [],
                'initial_state_file': str(ROOT / 'data/defaults/default_state.json'),
                'output_directory': str(Path(directory) / 'output'),
            }
            if overrides:
                config.update(overrides)
            if flight_script:
                script = Path(directory) / 'flight.lua'
                script.write_text(flight_script, encoding='utf-8')
                config['vehicle_models'] = {'flight_computer': {'script_path': str(script)}}
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

    def test_loaded_plugins_and_flight_script_run_in_both_scheduling_modes(self):
        script = '''
        assert(io == nil and os == nil and loadfile == nil and dofile == nil)
        function on_tick(dt)
            assert(dt > 0)
            local altitude = get_altitude()
            local speed = get_vertical_velocity()
            set_throttle(altitude > 0 and 0.75 or 0)
            print_log("flight control tick")
        end
        '''
        for plugin_type in (0, 1):
            plugins = [{'name': name, 'type': plugin_type, 'library_path': self.plugin_path(name),
                        'enabled': True, 'parameters': {'debug_output': True, 'use_host_logger': True}}
                       for name in ('structures', 'aerodynamics', 'propulsion')]
            plugins[2]['parameters']['engine_curves_path'] = str(ROOT / 'plugins/propulsion/data/engine_curves.csv')
            messages = '\n'.join(json.dumps({'command': name, 'request_id': name, 'payload': payload})
                                 for name, payload in [('initialize', {}), ('run_ticks', {'count': 3}),
                                                       ('get_status', {}), ('shutdown', {})]) + '\n'
            result = self.run_simulator(['--ipc', 'stdio'], messages, {'plugins': plugins}, script)
            self.assertEqual(result.returncode, 0, result.stderr)
            responses = [json.loads(line) for line in result.stdout.splitlines() if line.strip()]
            errors = [item for item in responses if item.get('type') == 'error']
            self.assertEqual(errors, [], result.stdout)
            replies = {item['request_id']: item for item in responses if item.get('type') == 'ack'}
            self.assertEqual(replies['get_status']['data']['tick'], 3)
            self.assertIn('flight control tick', result.stdout)

    def test_invalid_flight_script_is_reported_without_crashing(self):
        for script in ('this is not lua', 'function on_tick(dt) error("script failure") end',
                       'function unrelated() end'):
            result = self.run_simulator(['--ticks', '1'], flight_script=script)
            self.assertEqual(result.returncode, 0, result.stderr)

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
