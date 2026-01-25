"""
Example tests for MoLab Web Server
Demonstrates how to test the modular architecture

Run with: pytest tools/web_server/tests/
"""
import pytest
from pathlib import Path
import sys

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from web_server.config import ServerConfig
from web_server.router import Router
from web_server.utils import validate_config, sanitize_filename


class TestServerConfig:
    """Test configuration management"""
    
    def test_default_config(self):
        """Test default configuration values"""
        config = ServerConfig()
        assert config.port == 8082
        assert config.auto_open_browser == True
        assert config.max_simulation_timeout == 600
    
    def test_custom_port(self):
        """Test custom port configuration"""
        config = ServerConfig(port=9000)
        assert config.port == 9000
    
    def test_port_validation(self):
        """Test port validation"""
        with pytest.raises(ValueError):
            config = ServerConfig(port=100)  # Invalid port
            config.validate()
    
    def test_config_to_dict(self):
        """Test configuration serialization"""
        config = ServerConfig()
        config_dict = config.to_dict()
        assert 'port' in config_dict
        assert 'project_root' in config_dict


class TestRouter:
    """Test HTTP router"""
    
    def test_route_registration(self):
        """Test adding routes"""
        router = Router()
        
        def handler(req, params):
            return "OK"
        
        router.add_route(r'/api/test', handler, ['GET'])
        
        routes = router.list_routes()
        assert len(routes) == 1
        assert routes[0]['pattern'] == r'/api/test'
    
    def test_route_matching(self):
        """Test route pattern matching"""
        router = Router()
        
        def handler(req, params):
            return "OK"
        
        router.add_route(r'/api/test', handler, ['GET'])
        
        result = router.find_handler('/api/test', 'GET')
        assert result is not None
        
        result = router.find_handler('/api/test', 'POST')
        assert result is None
    
    def test_route_with_parameters(self):
        """Test route with regex parameters"""
        router = Router()
        
        def handler(req, params):
            return params['id']
        
        router.add_route(r'/api/item/(?P<id>\d+)', handler, ['GET'])
        
        result = router.find_handler('/api/item/123', 'GET')
        assert result is not None
        handler_func, params = result
        assert params['id'] == '123'


class TestUtilities:
    """Test utility functions"""
    
    def test_sanitize_filename(self):
        """Test filename sanitization"""
        # Normal filename
        assert sanitize_filename('test.csv') == 'test.csv'
        
        # Path traversal attempt
        assert sanitize_filename('../../../etc/passwd') == 'passwd'
        assert sanitize_filename('..\\..\\windows\\system32') == 'system32'
        
        # Special characters
        result = sanitize_filename('test@#$%.csv')
        assert '@' not in result
        assert '#' not in result
    
    def test_validate_config_valid(self):
        """Test configuration validation with valid config"""
        config = {
            'simulation': {
                'time_step': 0.1,
                'duration': 10.0
            },
            'physics': {
                'integrator_type': 'runge_kutta_4'
            },
            'output': {}
        }
        
        is_valid, error = validate_config(config)
        assert is_valid == True
        assert error is None
    
    def test_validate_config_invalid(self):
        """Test configuration validation with invalid config"""
        # Missing simulation section
        config = {
            'physics': {},
            'output': {}
        }
        
        is_valid, error = validate_config(config)
        assert is_valid == False
        assert 'simulation' in error.lower()
    
    def test_validate_config_negative_timestep(self):
        """Test validation rejects negative timestep"""
        config = {
            'simulation': {
                'time_step': -0.1,  # Invalid!
                'duration': 10.0
            },
            'physics': {
                'integrator_type': 'runge_kutta_4'
            },
            'output': {}
        }
        
        is_valid, error = validate_config(config)
        assert is_valid == False
        assert 'positive' in error.lower()


class TestSimulationManager:
    """Test simulation management"""
    
    def test_job_creation(self):
        """Test creating a simulation job"""
        # This would require mocking - example only
        pass
    
    def test_job_status(self):
        """Test getting job status"""
        # This would require mocking - example only
        pass


# Example integration test
class TestIntegration:
    """Integration tests"""
    
    def test_server_initialization(self):
        """Test server can be initialized"""
        config = ServerConfig(port=8083, auto_open_browser=False)
        config.validate()
        
        # This creates the server but doesn't start it
        from web_server.server import MoLabWebServer
        server = MoLabWebServer(config)
        
        assert server.config.port == 8083
        assert len(server.router.routes) > 0


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
