"""
Utility functions for Apogeo Web Server
"""
import platform
import json
import copy
import math
from pathlib import Path
from typing import Optional, Dict, Any, List
import csv

# Gravity defaults shared by the validator, the normalizer and the state seeding.
DEFAULT_GRAVITY_MAGNITUDE = 9.81
DEFAULT_GRAVITY_DIRECTION = (0.0, 0.0, -1.0)  # +Z is altitude in the web templates
MAX_GRAVITY_MAGNITUDE = 50.0


def get_simulator_path(build_dir: Path) -> Path:
    """
    Get the correct simulator binary path for the current platform
    
    Args:
        build_dir: Path to build directory
        
    Returns:
        Path to simulator executable
    """
    is_windows = platform.system() == "Windows"
    exe_name = "simulator.exe" if is_windows else "simulator"

    # Common layouts seen in this repository and CMake multi-config builds.
    candidates = [
        build_dir / "bin" / exe_name,
        build_dir / exe_name,
        build_dir / "Debug" / exe_name,
        build_dir / "Release" / exe_name,
        build_dir / "x64-Debug" / "bin" / exe_name,
        build_dir / "x64-Debug" / exe_name,
        build_dir / "x64-Debug" / "Debug" / exe_name,
        build_dir / "x64-Release" / "bin" / exe_name,
        build_dir / "x64-Release" / exe_name,
        build_dir / "x64-Release" / "Release" / exe_name,
    ]

    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate

    # Fallback: search below build dir for an executable named simulator.
    try:
        for candidate in build_dir.rglob(exe_name):
            if candidate.is_file():
                return candidate
    except OSError:
        pass

    # Keep previous behavior as deterministic fallback when not found.
    return build_dir / "bin" / exe_name


def is_port_available(port: int) -> bool:
    """
    Check if a port is available
    
    Args:
        port: Port number to check
        
    Returns:
        True if port is available
    """
    import socket
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('', port))
            return True
        except OSError:
            return False


def find_file_in_directories(filename: str, directories: List[Path]) -> Optional[Path]:
    """
    Search for a file in multiple directories
    
    Args:
        filename: Name of file to find
        directories: List of directories to search
        
    Returns:
        Path to file if found, None otherwise
    """
    for directory in directories:
        if not directory.exists():
            continue
            
        file_path = directory / filename
        if file_path.exists() and file_path.is_file():
            return file_path
    
    return None


def read_json_file(file_path: Path) -> Optional[Dict[str, Any]]:
    """
    Safely read a JSON file
    
    Args:
        file_path: Path to JSON file
        
    Returns:
        Parsed JSON data or None if error
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (json.JSONDecodeError, FileNotFoundError, PermissionError) as e:
        return None


def write_json_file(file_path: Path, data: Dict[str, Any], indent: int = 2) -> bool:
    """
    Safely write a JSON file
    
    Args:
        file_path: Path to JSON file
        data: Data to write
        indent: JSON indentation
        
    Returns:
        True if successful
    """
    try:
        file_path.parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=indent)
        return True
    except (PermissionError, OSError) as e:
        return False


def read_csv_file(file_path: Path) -> Optional[Dict[str, Any]]:
    """
    Read and parse a CSV file with simulation results
    
    Args:
        file_path: Path to CSV file
        
    Returns:
        Dictionary with headers, rows, and summary statistics
    """
    try:
        data = {
            "headers": [],
            "rows": [],
            "summary": {}
        }
        
        with open(file_path, 'r', encoding='utf-8') as f:
            reader = csv.reader(f)
            headers = next(reader)
            data["headers"] = headers
            
            rows = []
            for row in reader:
                # Convert numeric values
                numeric_row = []
                for value in row:
                    try:
                        numeric_row.append(float(value))
                    except ValueError:
                        numeric_row.append(value)
                rows.append(numeric_row)
            
            data["rows"] = rows
        
        # Generate summary statistics
        if rows:
            data["summary"] = _calculate_summary_statistics(rows)
        
        return data
        
    except (FileNotFoundError, PermissionError, csv.Error) as e:
        return None


def _calculate_summary_statistics(rows: List[List[Any]]) -> Dict[str, Any]:
    """
    Calculate summary statistics from CSV rows
    
    Args:
        rows: List of data rows
        
    Returns:
        Dictionary with summary statistics
    """
    # Calculate distance traveled
    total_distance = 0.0
    if len(rows) > 1:
        for i in range(1, len(rows)):
            if len(rows[i]) >= 6 and len(rows[i-1]) >= 6:
                try:
                    dx = rows[i][3] - rows[i-1][3]  # position_x
                    dy = rows[i][4] - rows[i-1][4]  # position_y
                    dz = rows[i][5] - rows[i-1][5]  # position_z
                    total_distance += (dx**2 + dy**2 + dz**2)**0.5
                except (IndexError, TypeError):
                    pass
    
    # Build summary
    summary = {
        "total_points": len(rows),
        "distance_traveled": total_distance
    }
    
    # Add time information
    if len(rows) > 1 and len(rows[0]) > 1 and len(rows[-1]) > 1:
        try:
            summary["duration"] = rows[-1][1] - rows[0][1]
        except (IndexError, TypeError):
            summary["duration"] = 0
    
    # Add initial position
    if len(rows) > 0 and len(rows[0]) >= 6:
        try:
            summary["initial_position"] = {
                "x": rows[0][3],
                "y": rows[0][4],
                "z": rows[0][5]
            }
        except (IndexError, TypeError):
            summary["initial_position"] = {"x": 0, "y": 0, "z": 0}
    
    # Add final position
    if len(rows) > 0 and len(rows[-1]) >= 6:
        try:
            summary["final_position"] = {
                "x": rows[-1][3],
                "y": rows[-1][4],
                "z": rows[-1][5]
            }
        except (IndexError, TypeError):
            summary["final_position"] = {"x": 0, "y": 0, "z": 0}
    
    # Add initial velocity
    if len(rows) > 0 and len(rows[0]) >= 9:
        try:
            summary["initial_velocity"] = {
                "x": rows[0][6],
                "y": rows[0][7],
                "z": rows[0][8]
            }
        except (IndexError, TypeError):
            summary["initial_velocity"] = {"x": 0, "y": 0, "z": 0}
    
    # Add final velocity
    if len(rows) > 0 and len(rows[-1]) >= 9:
        try:
            summary["final_velocity"] = {
                "x": rows[-1][6],
                "y": rows[-1][7],
                "z": rows[-1][8]
            }
        except (IndexError, TypeError):
            summary["final_velocity"] = {"x": 0, "y": 0, "z": 0}
    
    return summary


def sanitize_filename(filename: str) -> str:
    """
    Sanitize a filename to prevent path traversal
    
    Args:
        filename: Filename to sanitize
        
    Returns:
        Sanitized filename
    """
    # Remove path separators and special characters
    import re
    filename = Path(filename).name  # Get just the filename
    filename = re.sub(r'[^\w\s\-\.]', '', filename)
    return filename


def validate_config(config: Dict[str, Any]) -> tuple[bool, Optional[str]]:
    """
    Validate a simulation configuration
    
    Args:
        config: Configuration dictionary
        
    Returns:
        Tuple of (is_valid, error_message)
    """
    required_sections = ['simulation']
    
    for section in required_sections:
        if section not in config:
            return False, f"Missing required section: {section}"
    
    # Validate simulation section (supports both legacy and current schema)
    sim = config['simulation']
    
    if 'duration' not in sim or not isinstance(sim['duration'], (int, float)):
        return False, "Invalid or missing simulation.duration"
    
    if sim['duration'] <= 0:
        return False, "simulation.duration must be positive"

    # Legacy support: if time_step is provided, validate it and derive max_iterations compatibility.
    if 'time_step' in sim:
        if not isinstance(sim['time_step'], (int, float)):
            return False, "simulation.time_step must be numeric"
        if sim['time_step'] <= 0:
            return False, "simulation.time_step must be positive"

    if 'max_iterations' in sim:
        if not isinstance(sim['max_iterations'], int):
            return False, "simulation.max_iterations must be an integer"
        if sim['max_iterations'] <= 0:
            return False, "simulation.max_iterations must be positive"
    
    # Optional physics section validation (legacy web payload)
    physics = config.get('physics')
    if physics is not None:
        if not isinstance(physics, dict):
            return False, "physics section must be an object"

        integrator_type = physics.get('integrator_type')
        if integrator_type is not None:
            valid_integrators = ['euler', 'runge_kutta_4', 'verlet']
            if integrator_type not in valid_integrators:
                return False, f"Invalid integrator_type. Must be one of: {valid_integrators}"

        # Deprecated gravity fields (now owned by the environment domain).
        if 'enable_gravity' in physics and not isinstance(physics['enable_gravity'], bool):
            return False, "physics.enable_gravity must be a boolean"
        if 'gravity_magnitude' in physics:
            ok, error = _validate_gravity_magnitude(physics['gravity_magnitude'], 'physics.gravity_magnitude')
            if not ok:
                return False, error

    # Gravity lives in the environment domain.
    environment = config.get('environment')
    if environment is not None:
        if not isinstance(environment, dict):
            return False, "environment section must be an object"
        gravity = environment.get('gravity')
        if gravity is not None:
            ok, error = _validate_gravity_block(gravity)
            if not ok:
                return False, error

    plugins = config.get('plugins')
    if plugins is not None and not isinstance(plugins, list):
        return False, "plugins section must be a list"

    return True, None


def _validate_gravity_magnitude(value: Any, field: str) -> tuple[bool, Optional[str]]:
    """Gravity magnitude must be a finite number in [0, MAX_GRAVITY_MAGNITUDE]."""
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        return False, f"{field} must be a finite number"
    if value < 0 or value > MAX_GRAVITY_MAGNITUDE:
        return False, f"{field} must be between 0 and {MAX_GRAVITY_MAGNITUDE:g} m/s^2"
    return True, None


def _validate_gravity_block(gravity: Any) -> tuple[bool, Optional[str]]:
    if not isinstance(gravity, dict):
        return False, "environment.gravity must be an object"

    if 'enabled' in gravity and not isinstance(gravity['enabled'], bool):
        return False, "environment.gravity.enabled must be a boolean"

    if 'magnitude' in gravity:
        ok, error = _validate_gravity_magnitude(gravity['magnitude'], 'environment.gravity.magnitude')
        if not ok:
            return False, error

    if 'direction' in gravity:
        direction = gravity['direction']
        valid_vector = (
            isinstance(direction, (list, tuple)) and len(direction) == 3 and
            all(isinstance(c, (int, float)) and not isinstance(c, bool) and math.isfinite(c) for c in direction)
        )
        if not valid_vector:
            return False, "environment.gravity.direction must be [x, y, z] with finite numbers"
        if math.sqrt(sum(float(c) ** 2 for c in direction)) == 0.0:
            return False, "environment.gravity.direction must not be the zero vector"

    return True, None


def resolve_gravity(config: Dict[str, Any]) -> Dict[str, Any]:
    """
    Resolve the effective gravity settings of a web payload.

    Gravity is owned by the environment domain: ``environment.gravity`` is the
    current contract. The legacy ``physics.enable_gravity`` and
    ``physics.gravity_magnitude`` fields are still accepted so older clients and
    saved configs keep working; ``environment.gravity`` wins when both exist.

    Returns:
        Dict with ``enabled``, ``magnitude``, ``direction`` (unit vector),
        ``vector`` (m/s^2, zero when disabled) and ``source``
        (``environment`` or ``legacy_physics`` or ``default``).
    """
    enabled = True
    magnitude = DEFAULT_GRAVITY_MAGNITUDE
    direction = list(DEFAULT_GRAVITY_DIRECTION)
    source = 'default'

    physics = config.get('physics') if isinstance(config, dict) else None
    if isinstance(physics, dict):
        if 'enable_gravity' in physics:
            enabled = bool(physics['enable_gravity'])
            source = 'legacy_physics'
        if 'gravity_magnitude' in physics:
            magnitude = float(physics['gravity_magnitude'])
            source = 'legacy_physics'

    environment = config.get('environment') if isinstance(config, dict) else None
    gravity = environment.get('gravity') if isinstance(environment, dict) else None
    if isinstance(gravity, dict):
        if 'enabled' in gravity:
            enabled = bool(gravity['enabled'])
        if 'magnitude' in gravity:
            magnitude = float(gravity['magnitude'])
        if 'direction' in gravity:
            direction = [float(c) for c in gravity['direction']]
        source = 'environment'

    norm = math.sqrt(sum(c * c for c in direction)) or 1.0
    direction = [c / norm for c in direction]
    vector = [c * magnitude for c in direction] if enabled else [0.0, 0.0, 0.0]

    return {
        'enabled': enabled,
        'magnitude': magnitude,
        'direction': direction,
        'vector': vector,
        'source': source,
    }


def normalize_config_for_simulator(config: Dict[str, Any]) -> Dict[str, Any]:
    """
    Normalize web UI config to the current simulator config schema.

    This keeps backward compatibility with legacy web payload fields while
    producing a config accepted by the current C++ codebase.

    Args:
        config: Incoming web configuration

    Returns:
        Normalized configuration dictionary
    """
    normalized = copy.deepcopy(config)

    simulation = normalized.setdefault('simulation', {})
    duration = float(simulation.get('duration', 100.0))

    # Current schema uses max_iterations. If legacy time_step is available,
    # always derive max_iterations from duration/time_step to keep duration
    # and end-conditions consistent with UI changes.
    time_step = simulation.get('time_step')
    if isinstance(time_step, (int, float)) and time_step > 0:
        simulation['max_iterations'] = max(1, int(duration / float(time_step)))
    elif 'max_iterations' not in simulation:
        simulation['max_iterations'] = 10000

    # Keep duration/logging fields expected by current ConfigManager.
    simulation.setdefault('enable_logging', True)
    simulation.setdefault('log_file', 'logs/apogeo_web.log')
    simulation.setdefault('log_level', 'INFO')

    # Legacy output block -> current root output_directory.
    output = normalized.get('output')
    if isinstance(output, dict) and output.get('output_directory'):
        normalized['output_directory'] = output['output_directory']
    normalized.setdefault('output_directory', 'output/')

    # Legacy physics.integrator_type -> current mission_environment.integrator_config.method.
    physics = normalized.get('physics')
    if isinstance(physics, dict) and physics.get('integrator_type'):
        mission_environment = normalized.setdefault('mission_environment', {})
        integrator_config = mission_environment.setdefault('integrator_config', {})
        integrator_config['method'] = physics['integrator_type']

    # Gravity belongs to the environment domain. Keep one canonical block; the web
    # server seeds the effective vector into GeneralState.gravity. Plugin parameters
    # are plugin-owned (docs/CONFIG_REFERENCE.md), so nothing is injected into them.
    gravity = resolve_gravity(normalized)
    environment = normalized.get('environment')
    if not isinstance(environment, dict):
        environment = normalized['environment'] = {}
    environment['gravity'] = {
        'enabled': gravity['enabled'],
        'magnitude': gravity['magnitude'],
        'direction': gravity['direction'],
    }

    # Ensure ipc block exists for IPC mode throttling defaults.
    ipc = normalized.setdefault('ipc', {})
    ipc.setdefault('tick_event_interval', 50)
    ipc.setdefault('telemetry_interval_ticks', 5)

    return normalized
