"""
HTTP Router - Pattern-based routing system
Replaces manual if/elif cascading with a clean router pattern
"""
from typing import Callable, Dict, Optional, Tuple
import re
from urllib.parse import urlparse, parse_qs

from .logger import setup_logger


logger = setup_logger(__name__)


class Route:
    """Represents a single route"""
    
    def __init__(self, pattern: str, handler: Callable, methods: list = None):
        """
        Initialize route
        
        Args:
            pattern: URL pattern (can include regex)
            handler: Handler function
            methods: List of allowed HTTP methods
        """
        self.pattern = pattern
        self.handler = handler
        self.methods = methods or ['GET']
        self.regex = re.compile(f"^{pattern}$")
    
    def match(self, path: str, method: str) -> Optional[Dict]:
        """
        Check if route matches the path and method
        
        Args:
            path: Request path
            method: HTTP method
            
        Returns:
            Dictionary with match info or None
        """
        if method not in self.methods:
            return None
        
        match = self.regex.match(path)
        if match:
            return {'params': match.groupdict()}
        
        return None


class Router:
    """HTTP Router with pattern matching"""
    
    def __init__(self):
        """Initialize router"""
        self.routes: list[Route] = []
    
    def add_route(self, pattern: str, handler: Callable, methods: list = None):
        """
        Add a route
        
        Args:
            pattern: URL pattern
            handler: Handler function
            methods: List of allowed HTTP methods
        """
        route = Route(pattern, handler, methods)
        self.routes.append(route)
        logger.debug(f"Added route: {pattern} [{', '.join(route.methods)}]")
    
    def get(self, pattern: str):
        """Decorator for GET routes"""
        def decorator(handler: Callable):
            self.add_route(pattern, handler, ['GET'])
            return handler
        return decorator
    
    def post(self, pattern: str):
        """Decorator for POST routes"""
        def decorator(handler: Callable):
            self.add_route(pattern, handler, ['POST'])
            return handler
        return decorator
    
    def route(self, pattern: str, methods: list):
        """Decorator for routes with custom methods"""
        def decorator(handler: Callable):
            self.add_route(pattern, handler, methods)
            return handler
        return decorator
    
    def find_handler(self, path: str, method: str) -> Optional[Tuple[Callable, Dict]]:
        """
        Find handler for given path and method
        
        Args:
            path: Request path
            method: HTTP method
            
        Returns:
            Tuple of (handler, params) or None
        """
        for route in self.routes:
            match_info = route.match(path, method)
            if match_info:
                return route.handler, match_info['params']
        
        return None
    
    def list_routes(self) -> list:
        """
        List all registered routes
        
        Returns:
            List of route information
        """
        return [
            {
                'pattern': route.pattern,
                'methods': route.methods,
                'handler': route.handler.__name__
            }
            for route in self.routes
        ]


def parse_request_path(path: str) -> Tuple[str, Dict[str, list]]:
    """
    Parse request path and extract query parameters
    
    Args:
        path: Full request path with query string
        
    Returns:
        Tuple of (clean_path, query_params)
    """
    parsed = urlparse(path)
    clean_path = parsed.path
    query_params = parse_qs(parsed.query)
    
    return clean_path, query_params
