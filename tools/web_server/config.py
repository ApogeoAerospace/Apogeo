"""
Configuration module for MoLab Web Server
Centralizes all configuration values
"""
from pathlib import Path
from typing import Dict, Any
import logging


class ServerConfig:
    """Server configuration with validation"""
    
    def __init__(self, port: int = 8082, auto_open_browser: bool = True):
        self.port = port
        self.auto_open_browser = auto_open_browser
        self.project_root = Path(__file__).parent.parent.parent
        self.build_dir = self.project_root / "out" / "build"
        self.config_dir = self.project_root / "data" / "config"
        self.output_dir = self.build_dir / "output"
        self.initial_state_dir = self.project_root / "data" / "initial_state"
        self.static_dir = Path(__file__).parent / "static"
        
        # Simulation settings
        self.max_simulation_timeout = 600  # 10 minutes
        self.simulation_check_interval = 0.5  # seconds
        
        # Output directories to search (in priority order)
        self.output_search_dirs = [
            self.output_dir,
            self.project_root / "output",
            self.project_root / "tools" / "output",
            self.project_root / "results"
        ]
        
        # Logging
        self.log_level = logging.INFO
        self.log_format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        
    def validate(self) -> bool:
        """Validate configuration"""
        if not 1024 <= self.port <= 65535:
            raise ValueError(f"Invalid port: {self.port}. Must be between 1024 and 65535")
        
        if not self.project_root.exists():
            raise ValueError(f"Project root does not exist: {self.project_root}")
        
        return True
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            'port': self.port,
            'project_root': str(self.project_root),
            'build_dir': str(self.build_dir),
            'config_dir': str(self.config_dir),
            'output_dir': str(self.output_dir)
        }
