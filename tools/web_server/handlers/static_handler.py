"""
Static file handler
"""
from pathlib import Path
from http.server import SimpleHTTPRequestHandler

from .base_handler import BaseHandler


class StaticHandler(BaseHandler):
    """Handler for static files (HTML, CSS, JS)"""
    
    # MIME types for static files
    MIME_TYPES = {
        '.html': 'text/html',
        '.css': 'text/css',
        '.js': 'application/javascript',
        '.json': 'application/json',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg',
        '.gif': 'image/gif',
        '.svg': 'image/svg+xml',
        '.ico': 'image/x-icon'
    }
    
    def handle_static_file(self, request_handler: SimpleHTTPRequestHandler, 
                          file_path: str):
        """
        Serve a static file
        
        Args:
            request_handler: HTTP request handler
            file_path: Requested file path (relative to static dir)
        """
        # Map root to index.html first
        if file_path == '/' or file_path == '':
            file_path = 'index.html'
        
        # Remove leading slash if present
        if file_path.startswith('/'):
            file_path = file_path[1:]
        
        # Security: prevent path traversal AFTER processing
        if '..' in file_path:
            request_handler.send_error(403, "Forbidden")
            return
        
        # Construct full file path
        full_path = self.config.static_dir / file_path
        
        # Additional security: verify the resolved path is under static_dir
        try:
            full_path = full_path.resolve()
            if not str(full_path).startswith(str(self.config.static_dir.resolve())):
                request_handler.send_error(403, "Forbidden")
                return
        except Exception:
            request_handler.send_error(403, "Forbidden")
            return
        
        if not full_path.exists():
            # Provide helpful message for missing files
            if file_path == 'index.html':
                error_msg = (
                    "index.html not found. "
                    "Please ensure the static files are in place at: "
                    f"{self.config.static_dir}"
                )
            else:
                error_msg = f"File not found: {file_path}"
            request_handler.send_error(404, error_msg)
            return
        
        if not full_path.is_file():
            request_handler.send_error(403, "Not a file")
            return
        
        # Determine content type
        suffix = full_path.suffix.lower()
        content_type = self.MIME_TYPES.get(suffix, 'application/octet-stream')
        
        # Send file
        self.send_file_response(request_handler, full_path, content_type)
