"""API Handlers - Business logic for API endpoints"""
from http.server import SimpleHTTPRequestHandler
from datetime import datetime
import os
import platform
from typing import Any, Dict, List

from .base_handler import BaseHandler
from ..plugin_registry import PluginRegistry, PluginRegistryError
from ..utils import (
    get_simulator_path,
    find_file_in_directories,
    read_csv_file,
    write_json_file,
    read_json_file,
    sanitize_filename,
    validate_config,
    normalize_config_for_simulator,
    resolve_gravity,
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
        
        # os.access works on every platform; the POSIX mode bits do not exist on Windows.
        is_executable = False
        if simulator_exists:
            try:
                is_executable = simulator_path.is_file() and os.access(simulator_path, os.X_OK)
            except OSError:
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
    """Handler for the plugin registry (list, toggle, parameters, presets)."""

    def __init__(self, config, registry: PluginRegistry):
        super().__init__(config)
        self.registry = registry

    def _fail(self, request_handler: SimpleHTTPRequestHandler, error: PluginRegistryError):
        self.send_json_response(request_handler, {
            'success': False,
            'error': error.message,
            'code': error.code
        }, error.status_code)

    def handle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/plugins

        Returns the effective plugin list: configuration defaults merged with the
        state toggled from the panel, plus whether each library is built.
        """
        self.send_json_response(request_handler, self.registry.list_plugins())

    def handle_toggle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/plugins/{plugin_id}/toggle

        Body (optional): ``{"enabled": true|false}``. Without a body the current
        state is flipped. Enabling a plugin whose library is not built returns 409.
        """
        plugin_id = params.get('plugin_id', '')
        body = self.read_json_body(request_handler) or {}
        enabled = body.get('enabled') if isinstance(body, dict) else None

        if enabled is not None and not isinstance(enabled, bool):
            self.send_error_response(request_handler, "'enabled' must be a boolean")
            return

        try:
            plugin = self.registry.set_enabled(plugin_id, enabled)
        except PluginRegistryError as error:
            self._fail(request_handler, error)
            return

        self.send_success_response(request_handler, "Plugin state updated", {'plugin': plugin})

    def handle_parameters(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/plugins/{plugin_id}/parameters

        Body: ``{"parameters": {"key": value}}``. Only keys declared in the plugin's
        default configuration are accepted, with the same JSON type.
        """
        plugin_id = params.get('plugin_id', '')
        body = self.read_json_body(request_handler)
        if not isinstance(body, dict) or 'parameters' not in body:
            self.send_error_response(request_handler, "Body must be {\"parameters\": {...}}")
            return

        try:
            plugin = self.registry.set_parameters(plugin_id, body['parameters'])
        except PluginRegistryError as error:
            self._fail(request_handler, error)
            return

        self.send_success_response(request_handler, "Plugin parameters updated", {'plugin': plugin})

    def handle_preset(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/plugins/preset

        Body: ``{"enabled": ["id", ...], "overrides": {"id": {...}}}``. Used by mission
        templates to apply a plugin set atomically. Plugins that are not built are
        skipped and flagged with ``skipped: true`` rather than failing the preset.
        """
        body = self.read_json_body(request_handler)
        if not isinstance(body, dict) or not isinstance(body.get('enabled'), list):
            self.send_error_response(request_handler, "Body must contain an 'enabled' list")
            return

        overrides = body.get('overrides')
        if overrides is not None and not isinstance(overrides, dict):
            self.send_error_response(request_handler, "'overrides' must be an object")
            return

        try:
            plugins = self.registry.apply_preset(body['enabled'], overrides)
        except PluginRegistryError as error:
            self._fail(request_handler, error)
            return

        self.send_success_response(request_handler, "Plugin preset applied", {'plugins': plugins})

    def handle_reset(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """Handle POST /api/plugins/reset - drop every override."""
        try:
            plugins = self.registry.reset()
        except PluginRegistryError as error:
            self._fail(request_handler, error)
            return

        self.send_success_response(request_handler, "Plugin state reset", {'plugins': plugins})


class EnvironmentHandler(BaseHandler):
    """Handler for the environment domain (gravity ownership and defaults)."""

    def __init__(self, config, registry: PluginRegistry):
        super().__init__(config)
        self.registry = registry

    def handle(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle GET /api/environment

        Gravity is applied by the Environment module (or any plugin that consumes
        ``GeneralState.gravity``), not by the core. This endpoint reports the default
        gravity settings and which enabled plugins will actually apply it, so the UI
        can warn when gravity is requested but nothing applies it.
        """
        plugins = self.registry.list_plugins()
        providers = self.registry.gravity_providers(plugins)
        environment_plugin = next((p for p in plugins if p['id'] == 'environment'), None)

        web_defaults = read_json_file(self.config.config_dir / "web_config.json") or {}
        gravity = resolve_gravity(web_defaults)

        self.send_json_response(request_handler, {
            'gravity': {
                'enabled': gravity['enabled'],
                'magnitude': gravity['magnitude'],
                'direction': gravity['direction'],
            },
            'gravity_providers': providers,
            'gravity_consumers': [p['id'] for p in providers],
            'applies_gravity': bool(providers),
            'gravity_conflict': len(providers) > 1,
            'environment_plugin': {
                'available': bool(environment_plugin and environment_plugin['library_found']),
                'enabled': bool(environment_plugin and environment_plugin['enabled']),
            },
        })


class SimulationHandler(BaseHandler):
    """Handler for simulation execution"""

    def __init__(self, config, simulation_manager, registry: PluginRegistry):
        super().__init__(config, simulation_manager)
        self.registry = registry
    
    def handle_run(self, request_handler: SimpleHTTPRequestHandler, params: dict):
        """
        Handle POST /api/run

        Start a new simulation job. The plugin set comes from the registry (the state
        toggled in the panel). A legacy client may still send ``plugins`` explicitly;
        that list overrides the registry for this run only and is validated first.
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

        try:
            config['plugins'] = self.registry.resolve_run_plugins(config.get('plugins'))
        except PluginRegistryError as error:
            self.send_json_response(request_handler, {
                'success': False,
                'error': error.message,
                'code': error.code
            }, error.status_code)
            return

        # Resolve gravity before normalizing: normalization rewrites legacy fields
        # into environment.gravity, which would hide where the values came from.
        gravity = resolve_gravity(config)

        # Normalize payload to current simulator schema
        config = normalize_config_for_simulator(config)
        warnings = self._collect_warnings(config)

        # Create and start simulation job
        try:
            job_id = self.simulation_manager.create_job(config, warnings, gravity)
            success = self.simulation_manager.start_job(job_id)

            if success:
                self.send_success_response(request_handler, "Simulation started", {
                    'job_id': job_id,
                    'warnings': warnings
                })
            else:
                self.send_error_response(request_handler, "Failed to start simulation")

        except Exception as e:
            self.logger.exception("Error starting simulation")
            self.send_error_response(request_handler, str(e), 500)

    def _collect_warnings(self, config: Dict[str, Any]) -> List[str]:
        """Non-blocking problems the user should know about before results arrive."""
        warnings: List[str] = []
        gravity = resolve_gravity(config)
        wants_gravity = gravity['enabled'] and gravity['magnitude'] > 0
        plugin_view = [
            {
                'id': p.get('name'),
                'enabled': p.get('enabled', True),
                'parameters': p.get('parameters', {}),
            }
            for p in config.get('plugins', []) if isinstance(p, dict)
        ]
        providers = self.registry.gravity_providers(plugin_view)
        own_model = [p['id'] for p in providers if p['mode'] == 'own_model']

        if len(providers) > 1:
            warnings.append(
                "Gravity is applied by more than one plugin (" +
                ", ".join(p['id'] for p in providers) +
                "): their forces add up and the vehicle will fall too fast. Keep only one."
            )
        elif wants_gravity and not providers:
            warnings.append(
                "Gravity is enabled but no enabled plugin applies it. The core only carries "
                "the gravity vector; enable the Environment module (or the Example Plugin "
                "with apply_gravity) for it to affect the trajectory."
            )

        if own_model and not wants_gravity:
            warnings.append(
                f"Gravity is off in the simulation state, but {', '.join(own_model)} applies its "
                "own gravity model and cannot be switched off from here. Disable the module to remove gravity."
            )
        elif own_model and wants_gravity and len(providers) == 1 and (
                abs(gravity['magnitude'] - 9.81) > 1e-9 or
                [round(c, 9) for c in gravity['direction']] != [0.0, 0.0, -1.0]):
            warnings.append(
                f"The gravity magnitude/direction you set are written to the state, but {', '.join(own_model)} "
                "uses its own gravity model and ignores them."
            )
        return warnings

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

    def __init__(self, config, registry: PluginRegistry):
        super().__init__(config)
        self.registry = registry
    
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
        
        # The panel no longer sends plugins (the registry owns them); store the effective
        # set so the saved file is a complete, runnable configuration.
        if 'plugins' not in config:
            config['plugins'] = self.registry.simulator_plugin_configs()

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
