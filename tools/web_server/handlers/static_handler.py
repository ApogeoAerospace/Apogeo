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
        '.ico': 'image/x-icon',
        '.webp': 'image/webp',
        '.mp4': 'video/mp4',
        '.woff2': 'font/woff2',
        '.map': 'application/json'
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
        
        # Additional security: verify the resolved path is under static_dir.
        # relative_to avoids the prefix trap of a sibling like "static_evil/".
        try:
            full_path = full_path.resolve()
            full_path.relative_to(self.config.static_dir.resolve())
        except (ValueError, OSError):
            request_handler.send_error(403, "Forbidden")
            return

        if not full_path.exists():
            # Browsers always ask for /favicon.ico; answer quietly instead of a 404
            # that shows up as a console error.
            if file_path == 'favicon.ico':
                request_handler.send_response(204)
                request_handler.end_headers()
                return

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
