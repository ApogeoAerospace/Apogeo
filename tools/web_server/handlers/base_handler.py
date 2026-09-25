"""
Base handler class with common functionality
"""
import json
from http.server import SimpleHTTPRequestHandler
from typing import Any, Dict, Optional
from pathlib import Path

from ..logger import setup_logger


class BaseHandler:
    """Base class for all handlers"""
    
    def __init__(self, config, simulation_manager=None):
        """
        Initialize handler
        
        Args:
            config: ServerConfig instance
            simulation_manager: SimulationManager instance (optional)
        """
        self.config = config
        self.simulation_manager = simulation_manager
        self.logger = setup_logger(self.__class__.__name__)
    
    def send_json_response(self, request_handler: SimpleHTTPRequestHandler, 
                          data: Any, status_code: int = 200):
        """
        Send JSON response
        
        Args:
            request_handler: HTTP request handler
            data: Data to send as JSON
            status_code: HTTP status code
        """
        request_handler.send_response(status_code)
        request_handler.send_header('Content-type', 'application/json')
        request_handler.send_header('Access-Control-Allow-Origin', '*')
        request_handler.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        request_handler.end_headers()
        
        json_data = json.dumps(data, indent=2)
        request_handler.wfile.write(json_data.encode('utf-8'))
    
    def send_error_response(self, request_handler: SimpleHTTPRequestHandler,
                           message: str, status_code: int = 400):
        """
        Send error response
        
        Args:
            request_handler: HTTP request handler
            message: Error message
            status_code: HTTP status code
        """
        self.send_json_response(request_handler, {
            'success': False,
            'error': message
        }, status_code)
    
    def send_success_response(self, request_handler: SimpleHTTPRequestHandler,
                             message: str, data: Optional[Dict] = None):
        """
        Send success response
        
        Args:
            request_handler: HTTP request handler
            message: Success message
            data: Additional data to include
        """
        response = {
            'success': True,
            'message': message
        }
        
        if data:
            response.update(data)
        
        self.send_json_response(request_handler, response)
    
    def read_json_body(self, request_handler: SimpleHTTPRequestHandler) -> Optional[Dict]:
        """
        Read and parse JSON request body
        
        Args:
            request_handler: HTTP request handler
            
        Returns:
            Parsed JSON data or None
        """
        try:
            content_length = int(request_handler.headers.get('Content-Length', 0))
            if content_length == 0:
                return None
            
            post_data = request_handler.rfile.read(content_length)
            return json.loads(post_data.decode('utf-8'))
        
        except (json.JSONDecodeError, ValueError) as e:
            self.logger.error(f"Failed to parse JSON body: {e}")
            return None
    
    def send_file_response(self, request_handler: SimpleHTTPRequestHandler,
                          file_path: Path, content_type: str = 'text/html'):
        """
        Send file response
        
        Args:
            request_handler: HTTP request handler
            file_path: Path to file
            content_type: MIME content type
        """
        try:
            with open(file_path, 'rb') as f:
                content = f.read()
            
            request_handler.send_response(200)
            request_handler.send_header('Content-type', content_type)
            request_handler.send_header('Content-Length', str(len(content)))
            request_handler.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
            request_handler.send_header('Pragma', 'no-cache')
            request_handler.send_header('Expires', '0')
            request_handler.end_headers()
            request_handler.wfile.write(content)
        
        except FileNotFoundError:
            request_handler.send_error(404, "File not found")
        except PermissionError:
            request_handler.send_error(403, "Permission denied")
