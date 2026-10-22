"""
Apogeo Web Server - Main server implementation
Clean, modular HTTP server using router pattern
"""
import http.server
import socketserver
import sys
from pathlib import Path

from .config import ServerConfig
from .handlers.base_handler import MAX_BODY_BYTES, allowed_cors_origin
from .logger import setup_logger
from .plugin_registry import PluginRegistry
from .router import Router, parse_request_path
from .simulation_manager import SimulationManager
from .handlers import (
    StaticHandler,
    StatusHandler,
    PluginsHandler,
    EnvironmentHandler,
    SimulationHandler,
    ResultsHandler,
    ConfigHandler
)


logger = setup_logger(__name__)

# The browser closed the connection while we were answering (page reload, closed tab).
_CLIENT_DISCONNECTS = (BrokenPipeError, ConnectionResetError, ConnectionAbortedError)

# errno values for "address already in use": POSIX (98), macOS (48), Windows (10048).
_ADDRESS_IN_USE = {48, 98, 10048}

# Oversized request bodies are read and discarded up to this many bytes before the 413
# is sent, so the client receives the error instead of a connection reset mid-upload.
_MAX_DRAIN_BYTES = 4 * 1024 * 1024


class PortInUseError(RuntimeError):
    """Raised when the configured port is already taken."""


class _ThreadingServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    """
    Threaded TCP server.

    The panel polls job status every few hundred milliseconds while other requests
    (results, plugin toggles) are in flight; a single-threaded server would make
    them wait on each other.
    """
    allow_reuse_address = True
    daemon_threads = True

    def handle_error(self, request, client_address):
        """Do not print a traceback when the browser simply closes the connection.

        Reloading the page or closing the tab while a request is in flight makes the next
        write fail with BrokenPipeError / ConnectionResetError / ConnectionAbortedError
        (very common on Windows). It is not a server error, so it is only logged at debug level.
        """
        error = sys.exc_info()[1]
        if isinstance(error, _CLIENT_DISCONNECTS):
            logger.debug("Client %s closed the connection early: %s", client_address[0], error)
            return
        super().handle_error(request, client_address)


class ApogeoRequestHandler(http.server.SimpleHTTPRequestHandler):
    """
    HTTP Request Handler for Apogeo Web Interface
    Uses router pattern instead of manual if/elif cascading
    """
    
    # Class variables set by server initialization
    router: Router = None
    config: ServerConfig = None
    simulation_manager: SimulationManager = None
    handlers: dict = None
    
    def __init__(self, *args, **kwargs):
        """Initialize request handler"""
        # Suppress default logging
        super().__init__(*args, directory=str(self.config.static_dir), **kwargs)
    
    def log_message(self, format, *args):
        """Override to use our logger"""
        logger.info(f"{self.address_string()} - {format % args}")
    
    def do_GET(self):
        """Handle GET requests"""
        clean_path, query_params = parse_request_path(self.path)
        
        # Try to find handler in router
        result = self.router.find_handler(clean_path, 'GET')
        
        if result:
            handler_func, params = result
            params.update({k: v[0] if len(v) == 1 else v for k, v in query_params.items()})
            
            try:
                handler_func(self, params)
            except _CLIENT_DISCONNECTS as e:
                # Nobody is left to answer; sending a 500 would only raise again.
                logger.debug(f"Client closed the connection during GET {clean_path}: {e}")
            except Exception as e:
                logger.exception(f"Error handling GET {clean_path}")
                self.send_error(500, f"Internal server error: {str(e)}")
        else:
            # Try to serve as static file
            self.handlers['static'].handle_static_file(self, clean_path)
    
    def _reject_oversized_body(self, content_length: int):
        """Answer 413 for a body above MAX_BODY_BYTES, discarding the upload first."""
        remaining = min(content_length, _MAX_DRAIN_BYTES)
        while remaining > 0:
            chunk = self.rfile.read(min(65536, remaining))
            if not chunk:
                break
            remaining -= len(chunk)

        self.close_connection = True
        self.handlers['status'].send_json_response(self, {
            'success': False,
            'error': f'Request body too large (limit {MAX_BODY_BYTES // 1024} KB)',
            'code': 'payload_too_large'
        }, 413)

    def do_POST(self):
        """Handle POST requests"""
        try:
            content_length = int(self.headers.get('Content-Length', 0) or 0)
        except ValueError:
            content_length = 0

        if content_length > MAX_BODY_BYTES:
            self._reject_oversized_body(content_length)
            return

        clean_path, query_params = parse_request_path(self.path)
        
        # Try to find handler in router
        result = self.router.find_handler(clean_path, 'POST')
        
        if result:
            handler_func, params = result
            params.update({k: v[0] if len(v) == 1 else v for k, v in query_params.items()})
            
            try:
                handler_func(self, params)
            except _CLIENT_DISCONNECTS as e:
                # Nobody is left to answer; sending a 500 would only raise again.
                logger.debug(f"Client closed the connection during POST {clean_path}: {e}")
            except Exception as e:
                logger.exception(f"Error handling POST {clean_path}")
                self.send_error(500, f"Internal server error: {str(e)}")
        else:
            self.send_error(404, "Endpoint not found")
    
    def do_OPTIONS(self):
        """Answer CORS preflight requests for allow-listed origins only."""
        allowed = allowed_cors_origin(self.config, self.headers.get('Origin'))
        if not allowed:
            self.send_error(403, "Origin not allowed")
            return

        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', allowed)
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.send_header('Access-Control-Max-Age', '600')
        self.send_header('Vary', 'Origin')
        self.send_header('Content-Length', '0')
        self.end_headers()

    def end_headers(self):
        """Add security headers"""
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('X-XSS-Protection', '1; mode=block')
        super().end_headers()


class ApogeoWebServer:
    """
    Apogeo Web Server
    
    Main server class that coordinates all components
    """
    
    def __init__(self, config: ServerConfig = None):
        """
        Initialize web server
        
        Args:
            config: Server configuration
        """
        self.config = config or ServerConfig()
        self.config.validate()
        
        self.logger = setup_logger(f"{__name__}.ApogeoWebServer")
        self.simulation_manager = SimulationManager(self.config)
        self.plugin_registry = PluginRegistry(self.config)
        self.router = Router()
        self.handlers = {}
        
        self._setup_handlers()
        self._setup_routes()
        
        # Set class variables for request handler
        ApogeoRequestHandler.router = self.router
        ApogeoRequestHandler.config = self.config
        ApogeoRequestHandler.simulation_manager = self.simulation_manager
        ApogeoRequestHandler.handlers = self.handlers
    
    def _setup_handlers(self):
        """Initialize all handlers"""
        self.handlers['static'] = StaticHandler(self.config)
        self.handlers['status'] = StatusHandler(self.config)
        self.handlers['plugins'] = PluginsHandler(self.config, self.plugin_registry)
        self.handlers['environment'] = EnvironmentHandler(self.config, self.plugin_registry)
        self.handlers['simulation'] = SimulationHandler(
            self.config, self.simulation_manager, self.plugin_registry)
        self.handlers['results'] = ResultsHandler(self.config)
        self.handlers['config'] = ConfigHandler(self.config, self.plugin_registry)
        
        self.logger.info("Handlers initialized")
    
    def _setup_routes(self):
        """Setup all routes"""
        # API routes
        self.router.add_route(r'/api/status', self.handlers['status'].handle, ['GET'])
        self.router.add_route(r'/api/plugins', self.handlers['plugins'].handle, ['GET'])
        self.router.add_route(r'/api/plugins/preset', self.handlers['plugins'].handle_preset, ['POST'])
        self.router.add_route(r'/api/plugins/reset', self.handlers['plugins'].handle_reset, ['POST'])
        self.router.add_route(r'/api/plugins/(?P<plugin_id>[A-Za-z0-9_\-]+)/toggle',
                            self.handlers['plugins'].handle_toggle, ['POST'])
        self.router.add_route(r'/api/plugins/(?P<plugin_id>[A-Za-z0-9_\-]+)/parameters',
                            self.handlers['plugins'].handle_parameters, ['POST'])
        self.router.add_route(r'/api/environment', self.handlers['environment'].handle, ['GET'])
        
        # Simulation routes
        self.router.add_route(r'/api/run', self.handlers['simulation'].handle_run, ['POST'])
        self.router.add_route(r'/api/simulation/status/(?P<job_id>[a-f0-9\-]+)', 
                            self.handlers['simulation'].handle_status, ['GET'])
        self.router.add_route(r'/api/simulation/cancel/(?P<job_id>[a-f0-9\-]+)', 
                            self.handlers['simulation'].handle_cancel, ['POST'])
        
        # Results routes
        self.router.add_route(r'/api/results', self.handlers['results'].handle_list, ['GET'])
        self.router.add_route(r'/api/results/(?P<filename>[^/]+)', 
                            self.handlers['results'].handle_get, ['GET'])
        
        # Configuration routes
        self.router.add_route(r'/api/config/save', self.handlers['config'].handle_save, ['POST'])
        self.router.add_route(r'/api/config/load', self.handlers['config'].handle_load, ['GET'])
        self.router.add_route(r'/api/config/(?P<filename>[^/]+)', 
                            self.handlers['config'].handle_get, ['GET'])
        
        self.logger.info(f"Registered {len(self.router.routes)} routes")
    
    def start(self):
        """Start the web server"""
        self.logger.info(f"Starting Apogeo Web Interface on port {self.config.port}")
        self.logger.info(f"Project root: {self.config.project_root}")
        self.logger.info(f"Static files: {self.config.static_dir}")
        
        # Check if static directory exists
        if not self.config.static_dir.exists():
            self.logger.warning(f"Static directory does not exist: {self.config.static_dir}")
            self.logger.info("Creating static directory...")
            self.config.static_dir.mkdir(parents=True, exist_ok=True)
        
        # Print route information
        self.logger.info("Available routes:")
        for route_info in self.router.list_routes():
            self.logger.info(f"  {route_info['methods']} {route_info['pattern']} -> {route_info['handler']}")
        
        try:
            # Create server
            with _ThreadingServer(("", self.config.port), MoLabRequestHandler) as httpd:
                self.httpd = httpd
                self.logger.info(f"Server started on http://localhost:{self.config.port}")

                # Open browser if configured
                if self.config.auto_open_browser:
                    self._open_browser()

                self.logger.info("Press Ctrl+C to stop the server")

                # Start serving
                httpd.serve_forever()

        except KeyboardInterrupt:
            self.logger.info("Server stopped by user")
        except OSError as e:
            if e.errno in _ADDRESS_IN_USE:
                raise PortInUseError(
                    f"Port {self.config.port} is already in use. "
                    "Stop the other server or pass a different --port."
                ) from e
            raise

    def stop(self):
        """Stop a server started with start() from another thread (used by tests)."""
        httpd = getattr(self, 'httpd', None)
        if httpd is not None:
            httpd.shutdown()

    def _open_browser(self):
        """Open web browser"""
        import webbrowser
        import time
        import threading
        
        def open_with_delay():
            time.sleep(1)  # Wait for server to start
            try:
                webbrowser.open(f'http://localhost:{self.config.port}')
                self.logger.info("Opened browser")
            except Exception as e:
                self.logger.warning(f"Could not open browser automatically: {e}")
                self.logger.info(f"Please open manually: http://localhost:{self.config.port}")
        
        thread = threading.Thread(target=open_with_delay)
        thread.daemon = True
        thread.start()
