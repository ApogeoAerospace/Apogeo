"""
MoLab Web Server - Main server implementation
Clean, modular HTTP server using router pattern
"""
import http.server
import socketserver
from pathlib import Path

from .config import ServerConfig
from .logger import setup_logger
from .router import Router, parse_request_path
from .simulation_manager import SimulationManager
from .handlers import (
    StaticHandler,
    StatusHandler,
    PluginsHandler,
    SimulationHandler,
    ResultsHandler,
    ConfigHandler
)


logger = setup_logger(__name__)


class MoLabRequestHandler(http.server.SimpleHTTPRequestHandler):
    """
    HTTP Request Handler for MoLab Web Interface
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
            except Exception as e:
                logger.exception(f"Error handling GET {clean_path}")
                self.send_error(500, f"Internal server error: {str(e)}")
        else:
            # Try to serve as static file
            self.handlers['static'].handle_static_file(self, clean_path)
    
    def do_POST(self):
        """Handle POST requests"""
        clean_path, query_params = parse_request_path(self.path)
        
        # Try to find handler in router
        result = self.router.find_handler(clean_path, 'POST')
        
        if result:
            handler_func, params = result
            params.update({k: v[0] if len(v) == 1 else v for k, v in query_params.items()})
            
            try:
                handler_func(self, params)
            except Exception as e:
                logger.exception(f"Error handling POST {clean_path}")
                self.send_error(500, f"Internal server error: {str(e)}")
        else:
            self.send_error(404, "Endpoint not found")
    
    def end_headers(self):
        """Add security headers"""
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('X-XSS-Protection', '1; mode=block')
        super().end_headers()


class MoLabWebServer:
    """
    MoLab Web Server
    
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
        
        self.logger = setup_logger(f"{__name__}.MoLabWebServer")
        self.simulation_manager = SimulationManager(self.config)
        self.router = Router()
        self.handlers = {}
        
        self._setup_handlers()
        self._setup_routes()
        
        # Set class variables for request handler
        MoLabRequestHandler.router = self.router
        MoLabRequestHandler.config = self.config
        MoLabRequestHandler.simulation_manager = self.simulation_manager
        MoLabRequestHandler.handlers = self.handlers
    
    def _setup_handlers(self):
        """Initialize all handlers"""
        self.handlers['static'] = StaticHandler(self.config)
        self.handlers['status'] = StatusHandler(self.config)
        self.handlers['plugins'] = PluginsHandler(self.config)
        self.handlers['simulation'] = SimulationHandler(self.config, self.simulation_manager)
        self.handlers['results'] = ResultsHandler(self.config)
        self.handlers['config'] = ConfigHandler(self.config)
        
        self.logger.info("Handlers initialized")
    
    def _setup_routes(self):
        """Setup all routes"""
        # API routes
        self.router.add_route(r'/api/status', self.handlers['status'].handle, ['GET'])
        self.router.add_route(r'/api/plugins', self.handlers['plugins'].handle, ['GET'])
        
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
        self.logger.info(f"Starting MoLab Web Interface on port {self.config.port}")
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
            with socketserver.TCPServer(("", self.config.port), MoLabRequestHandler) as httpd:
                self.logger.info(f"✅ Server started on http://localhost:{self.config.port}")
                
                # Open browser if configured
                if self.config.auto_open_browser:
                    self._open_browser()
                
                self.logger.info("Press Ctrl+C to stop the server")
                
                # Start serving
                httpd.serve_forever()
        
        except KeyboardInterrupt:
            self.logger.info("Server stopped by user")
        except OSError as e:
            if e.errno == 48 or e.errno == 98:  # Address already in use
                self.logger.error(f"Port {self.config.port} is already in use")
                self.logger.error("Please stop the other server or use a different port")
            else:
                self.logger.exception("Error starting server")
        except Exception as e:
            self.logger.exception("Unexpected error starting server")
    
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
