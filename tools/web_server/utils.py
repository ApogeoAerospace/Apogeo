"""
Utility functions for MoLab Web Server
"""
import platform
import json
from pathlib import Path
from typing import Optional, Dict, Any, List
import csv


def get_simulator_path(build_dir: Path) -> Path:
    """
    Get the correct simulator binary path for the current platform
    
    Args:
        build_dir: Path to build directory
        
    Returns:
        Path to simulator executable
    """
    bin_dir = build_dir / "bin"
    
    if platform.system() == "Windows":
        return bin_dir / "simulator.exe"
    else:
        return bin_dir / "simulator"


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
    required_sections = ['simulation', 'physics', 'output']
    
    for section in required_sections:
        if section not in config:
            return False, f"Missing required section: {section}"
    
    # Validate simulation section
    sim = config['simulation']
    if 'time_step' not in sim or not isinstance(sim['time_step'], (int, float)):
        return False, "Invalid or missing simulation.time_step"
    
    if sim['time_step'] <= 0:
        return False, "simulation.time_step must be positive"
    
    if 'duration' not in sim or not isinstance(sim['duration'], (int, float)):
        return False, "Invalid or missing simulation.duration"
    
    if sim['duration'] <= 0:
        return False, "simulation.duration must be positive"
    
    # Validate physics section
    physics = config['physics']
    if 'integrator_type' not in physics:
        return False, "Missing physics.integrator_type"
    
    valid_integrators = ['euler', 'runge_kutta_4', 'verlet']
    if physics['integrator_type'] not in valid_integrators:
        return False, f"Invalid integrator_type. Must be one of: {valid_integrators}"
    
    return True, None
