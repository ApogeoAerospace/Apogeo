"""Regression cases for findings from the CI security scan."""

from pathlib import Path
from types import SimpleNamespace
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from web_server.handlers.base_handler import allowed_cors_origin
from web_server.server import ApogeoRequestHandler


class SecurityRegressionTests(unittest.TestCase):
    def test_cors_rejects_header_splitting_even_if_configured(self):
        origin = 'http://localhost:8080\r\nX-Injected: yes'
        config = SimpleNamespace(allowed_origins=[origin])
        self.assertIsNone(allowed_cors_origin(config, origin))

    def test_request_log_cannot_inject_new_lines(self):
        handler = object.__new__(ApogeoRequestHandler)
        handler.client_address = ('127.0.0.1', 12345)
        with patch('web_server.server.logger') as logger:
            handler.log_message('GET %s', '/attack\r\nFORGED ENTRY')
        args = logger.info.call_args.args
        rendered = args[0] % args[1:]
        self.assertNotIn('\r', rendered)
        self.assertNotIn('\n', rendered)
        self.assertIn('\\r\\nFORGED ENTRY', rendered)


if __name__ == '__main__':
    unittest.main()
