"""
API Handlers - Business logic for API endpoints
"""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from datetime import datetime
import platform

from .base_handler import BaseHandler
from ..utils import (
    get_simulator_path,
    find_file_in_directories,
    read_csv_file,
    write_json_file,
    read_json_file,
    sanitize_filename,
    validate_config
)


class StatusHandler(BaseHandler):
    """Handler for system status endpoint"""
    
    def handle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/status
        
        Returns system status including simulator availability
        """
        simulator_path = get_simulator_path(self.config.build_dir)
        simulator_exists = simulator_path.exists()
        
        # Additional checks
        is_executable = False
        if simulator_exists:
            try:
                is_executable = simulator_path.stat().st_mode & 0o111 != 0
            except:
                pass
        
        status = {
            "simulator_built": simulator_exists and is_executable,
            "simulator_path": str(simulator_path),
            "simulator_exists": simulator_exists,
            "simulator_executable": is_executable,
            "project_root": str(self.config.project_root),
            "build_dir": str(self.config.build_dir),
            "platform": platform.system(),
            "timestamp": datetime.now().isoformat()
        }
        
        self.send_json_response(request_handler, status)


class PluginsHandler(BaseHandler):
    """Handler for plugins listing endpoint"""
    
    # Platform-specific library extensions
    LIBRARY_EXTENSIONS = {
        'Windows': ['.dll', '.pyd'],
        'Darwin': ['.dylib', '.so'],
        'Linux': ['.so']
    }
    
    # Plugin definitions
    PLUGIN_DEFINITIONS = {
        "example_plugin": {
            "name": "Example Plugin",
            "type": 1,
            "description": "Example plugin for testing and demonstration",
            "enabled": False
        },
        "aerodynamics": {
            "name": "Aerodynamics",
            "type": 1,
            "description": "Real aerodynamic forces: drag, lift, compressibility effects",
            "enabled": False,
            "parameters": {
                "reference_area": 0.785,
                "drag_coefficient": 0.3,
                "enable_drag": True,
                "enable_altitude_effects": True
            }
        },
        "propulsion": {
            "name": "Propulsion",
            "type": 1,
            "description": "Rocket/jet propulsion with fuel consumption and altitude compensation",
            "enabled": False,
            "parameters": {
                "sea_level_thrust": 50000.0,
                "specific_impulse_sl": 250.0,
                "engine_on": True,
                "throttle_setting": 0.8
            }
        },
        "structures": {
            "name": "Structures",
            "type": 0,
            "description": "Mass tracking, center of gravity, and structural analysis",
            "enabled": False,
            "parameters": {
                "enable_mass_tracking": True,
                "enable_inertia_calculation": True,
                "enable_structural_analysis": True
            }
        },
        "environment": {
            "name": "Environment",
            "type": 1,
            "description": "Atmospheric model, wind effects, and gravity variation",
            "enabled": False,
            "parameters": {
                "enable_atmospheric_model": True,
                "enable_wind_effects": True,
                "enable_gravity_variation": True
            }
        },
        "programming": {
            "name": "Programming",
            "type": 0,
            "description": "Programmable logic and control sequences",
            "enabled": False
        }
    }
    
    def handle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/plugins
        
        Returns list of available plugins
        """
        plugins = []
        
        # Get platform-specific extensions
        system = platform.system()
        extensions = self.LIBRARY_EXTENSIONS.get(system, ['.so'])
        
        # Search directories
        plugins_dirs = [
            self.config.build_dir / "plugins",
            self.config.build_dir / "lib",
            self.config.build_dir / "bin"
        ]
        
        # Find plugin libraries
        found_plugins = set()
        
        for plugins_dir in plugins_dirs:
            if not plugins_dir.exists():
                continue
            
            for ext in extensions:
                for plugin_file in plugins_dir.glob(f"*{ext}"):
                    # Extract plugin name from filename
                    plugin_name = plugin_file.stem
                    
                    # Remove common prefixes
                    for prefix in ['lib', 'plugin_', 'molab_']:
                        if plugin_name.startswith(prefix):
                            plugin_name = plugin_name[len(prefix):]
                    
                    if plugin_name in self.PLUGIN_DEFINITIONS:
                        if plugin_name not in found_plugins:
                            plugin_def = self.PLUGIN_DEFINITIONS[plugin_name].copy()
                            plugin_def["library_path"] = str(plugin_file.absolute())
                            plugins.append(plugin_def)
                            found_plugins.add(plugin_name)
        
        self.send_json_response(request_handler, plugins)


class SimulationHandler(BaseHandler):
    """Handler for simulation execution"""
    
    def handle_run(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/run
        
        Start a new simulation job
        """
        # Read configuration from request body
        config = self.read_json_body(request_handler)
        
        if not config:
            self.send_error_response(request_handler, "Invalid or missing configuration")
            return
        
        # Validate configuration
        is_valid, error_message = validate_config(config)
        if not is_valid:
            self.send_error_response(request_handler, f"Configuration validation failed: {error_message}")
            return
        
        # Create and start simulation job
        try:
            job_id = self.simulation_manager.create_job(config)
            success = self.simulation_manager.start_job(job_id)
            
            if success:
                self.send_success_response(request_handler, "Simulation started", {
                    'job_id': job_id
                })
            else:
                self.send_error_response(request_handler, "Failed to start simulation")
        
        except Exception as e:
            self.logger.exception("Error starting simulation")
            self.send_error_response(request_handler, str(e), 500)
    
    def handle_status(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/simulation/status/{job_id}
        
        Get status of a simulation job
        """
        job_id = params.get('job_id')
        
        if not job_id:
            self.send_error_response(request_handler, "Missing job_id parameter")
            return
        
        status = self.simulation_manager.get_job_status(job_id)
        
        if status:
            self.send_json_response(request_handler, status)
        else:
            self.send_error_response(request_handler, "Job not found", 404)
    
    def handle_cancel(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/simulation/cancel/{job_id}
        
        Cancel a running simulation
        """
        job_id = params.get('job_id')
        
        if not job_id:
            self.send_error_response(request_handler, "Missing job_id parameter")
            return
        
        success = self.simulation_manager.cancel_job(job_id)
        
        if success:
            self.send_success_response(request_handler, "Simulation cancelled")
        else:
            self.send_error_response(request_handler, "Failed to cancel simulation or job not found")


class ResultsHandler(BaseHandler):
    """Handler for simulation results"""
    
    def handle_list(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/results
        
        List available simulation results
        """
        results = []
        
        # Search in multiple output directories
        for output_dir in self.config.output_search_dirs:
            if not output_dir.exists():
                continue
            
            for result_file in output_dir.glob("*.csv"):
                results.append({
                    "filename": result_file.name,
                    "path": str(result_file),
                    "size": result_file.stat().st_size,
                    "modified": result_file.stat().st_mtime
                })
        
        # Sort by modification time (newest first)
        results.sort(key=lambda x: x['modified'], reverse=True)
        
        self.send_json_response(request_handler, results)
    
    def handle_get(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/results/{filename}
        
        Get data from a specific result file
        """
        filename = params.get('filename')
        
        if not filename:
            self.send_error_response(request_handler, "Missing filename parameter")
            return
        
        # Sanitize filename to prevent path traversal
        filename = sanitize_filename(filename)
        
        # Find file in search directories
        file_path = find_file_in_directories(filename, self.config.output_search_dirs)
        
        if not file_path:
            self.send_error_response(request_handler, "File not found", 404)
            return
        
        # Read and parse CSV file
        data = read_csv_file(file_path)
        
        if data:
            self.send_json_response(request_handler, data)
        else:
            self.send_error_response(request_handler, "Failed to read result file", 500)


class ConfigHandler(BaseHandler):
    """Handler for configuration management"""
    
    def handle_save(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/config/save
        
        Save a configuration file
        """
        data = self.read_json_body(request_handler)
        
        if not data:
            self.send_error_response(request_handler, "Invalid request body")
            return
        
        filename = data.get('filename', 'web_config')
        config = data.get('config', {})
        
        if not config:
            self.send_error_response(request_handler, "Missing configuration data")
            return
        
        # Validate configuration
        is_valid, error_message = validate_config(config)
        if not is_valid:
            self.send_error_response(request_handler, f"Invalid configuration: {error_message}")
            return
        
        # Sanitize filename
        filename = sanitize_filename(filename)
        if not filename.endswith('.json'):
            filename += '.json'
        
        # Save configuration
        config_file = self.config.config_dir / filename
        
        if write_json_file(config_file, config):
            self.send_success_response(request_handler, f"Configuration saved as {filename}")
        else:
            self.send_error_response(request_handler, "Failed to save configuration", 500)
    
    def handle_load(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/config/load
        
        Load available configurations
        """
        configs = []
        
        if not self.config.config_dir.exists():
            self.send_json_response(request_handler, configs)
            return
        
        for config_file in self.config.config_dir.glob("*.json"):
            config_data = read_json_file(config_file)
            
            if config_data:
                configs.append({
                    "filename": config_file.stem,
                    "config": config_data
                })
        
        self.send_json_response(request_handler, configs)
    
    def handle_get(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/config/{filename}
        
        Get a specific configuration
        """
        filename = params.get('filename')
        
        if not filename:
            self.send_error_response(request_handler, "Missing filename parameter")
            return
        
        # Sanitize filename
        filename = sanitize_filename(filename)
        if not filename.endswith('.json'):
            filename += '.json'
        
        config_file = self.config.config_dir / filename
        
        if not config_file.exists():
            self.send_error_response(request_handler, "Configuration not found", 404)
            return
        
        config_data = read_json_file(config_file)
        
        if config_data:
            self.send_json_response(request_handler, config_data)
        else:
            self.send_error_response(request_handler, "Failed to read configuration", 500)
