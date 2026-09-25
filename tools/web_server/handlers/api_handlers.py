"""API Handlers - Business logic for API endpoints"""
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from datetime import datetime
import platform
from typing import Optional, List

from .base_handler import BaseHandler
from ..utils import (
    get_simulator_path,
    find_file_in_directories,
    read_csv_file,
    write_json_file,
    read_json_file,
    sanitize_filename,
    validate_config,
    normalize_config_for_simulator
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
    
    # Plugin metadata (UI-oriented; runtime config comes from JSON files)
    PLUGIN_DEFINITIONS = {
        "example_plugin": {
            "display_name": "Example Plugin",
            "description": "Example plugin for testing and demonstration"
        },
        "aerodynamics": {
            "display_name": "Aerodynamics",
            "description": "Real aerodynamic forces: drag, lift, compressibility effects"
        },
        "propulsion": {
            "display_name": "Propulsion",
            "description": "Rocket/jet propulsion with fuel consumption and altitude compensation"
        },
        "structures": {
            "display_name": "Structures",
            "description": "Mass tracking, center of gravity, and structural analysis"
        },
        "environment": {
            "display_name": "Environment",
            "description": "Atmospheric model, wind effects, and gravity variation"
        },
        "programming": {
            "display_name": "Programming",
            "description": "Programmable logic and control sequences"
        }
    }

    def _resolve_library_path(self, library_path: str, extensions: List[str]) -> Optional[Path]:
        """Resolve plugin library path against project/build layouts."""
        if not library_path:
            return None

        candidate = Path(library_path)
        if not candidate.is_absolute():
            candidate = self.config.project_root / candidate

        if candidate.exists():
            return candidate

        bases = [
            self.config.project_root,
            self.config.build_dir,
            self.config.build_dir / "bin",
            self.config.build_dir / "lib",
            self.config.build_dir / "plugins"
        ]

        raw_name = Path(library_path).name
        name_variants = {raw_name}
        if raw_name.startswith("lib"):
            name_variants.add(raw_name[3:])
        else:
            name_variants.add(f"lib{raw_name}")

        for base in bases:
            for name_variant in name_variants:
                for ext in ["", *extensions]:
                    p = base / f"{name_variant}{ext}"
                    if p.exists():
                        return p

        return None
    
    def handle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/plugins
        
        Returns list of available plugins
        """
        plugins = []

        # Get platform-specific extensions
        system = platform.system()
        extensions = self.LIBRARY_EXTENSIONS.get(system, ['.so'])

        config_candidates = [
            self.config.project_root / "data" / "config" / "web_last_config.json",
            self.config.project_root / "data" / "defaults" / "default_config.json"
        ]

        config_data = None
        for config_file in config_candidates:
            config_data = read_json_file(config_file)
            if config_data:
                break

        plugin_configs = config_data.get("plugins", []) if isinstance(config_data, dict) else []

        for plugin_cfg in plugin_configs:
            if not isinstance(plugin_cfg, dict):
                continue

            name = plugin_cfg.get("name", "")
            if not name:
                continue

            metadata = self.PLUGIN_DEFINITIONS.get(name, {})
            resolved_path = self._resolve_library_path(str(plugin_cfg.get("library_path", "")), extensions)

            plugins.append({
                "name": metadata.get("display_name", name),
                "id": name,
                "type": int(plugin_cfg.get("type", 0)),
                "description": metadata.get("description", f"{name} plugin"),
                "enabled": bool(plugin_cfg.get("enabled", False)),
                "parameters": plugin_cfg.get("parameters", {}),
                "library_path": str(resolved_path) if resolved_path else str(plugin_cfg.get("library_path", "")),
                "library_found": bool(resolved_path)
            })

        # Fallback for environments where no config was available.
        if not plugins:
            for plugin_name, metadata in self.PLUGIN_DEFINITIONS.items():
                plugins.append({
                    "name": metadata.get("display_name", plugin_name),
                    "id": plugin_name,
                    "type": 0,
                    "description": metadata.get("description", f"{plugin_name} plugin"),
                    "enabled": False,
                    "parameters": {},
                    "library_path": "",
                    "library_found": False
                })
        
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

        # Normalize payload to current simulator schema
        config = normalize_config_for_simulator(config)
        
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
