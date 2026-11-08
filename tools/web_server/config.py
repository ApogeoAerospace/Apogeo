"""
Configuration module for Apogeo Web Server
Centralizes all configuration values
"""
import logging
import os
from pathlib import Path
from typing import Any, Dict, List, Optional

from .utils import get_simulator_path


def discover_build_dir(project_root: Path) -> Path:
    """
    Locate the CMake build directory that holds the simulator binary.

    Lookup order:
      1. ``MOLAB_BUILD_DIR`` environment variable
      2. ``<root>/build``      (documented flow: ``cmake -S . -B build``)
      3. ``<root>/out/build``  (Visual Studio / CMake presets)

    The first candidate that contains a simulator wins. When nothing has been
    built yet, the first existing candidate (or ``<root>/build``) is returned so
    the status endpoint can report a meaningful path.
    """
    candidates: List[Path] = []
    env_dir = os.environ.get("MOLAB_BUILD_DIR")
    if env_dir:
        candidates.append(Path(env_dir).expanduser())
    candidates.extend([project_root / "build", project_root / "out" / "build"])

    for candidate in candidates:
        if candidate.is_dir() and get_simulator_path(candidate).is_file():
            return candidate

    for candidate in candidates:
        if candidate.is_dir():
            return candidate

    return project_root / "build"


class ServerConfig:
    """Server configuration with validation"""

    def __init__(self, port: int = 8082, auto_open_browser: bool = True,
                 build_dir: Optional[Path] = None,
                 allowed_origins: Optional[List[str]] = None,
                 project_root: Optional[Path] = None,
                 simulator_command: Optional[List[str]] = None):
        self.port = port
        self.auto_open_browser = auto_open_browser
        # ``project_root`` is overridable so tests can run against a scratch tree.
        self.project_root = Path(project_root) if project_root else Path(__file__).parent.parent.parent
        self.build_dir = Path(build_dir) if build_dir else discover_build_dir(self.project_root)
        # Optional command used instead of the discovered simulator binary, e.g.
        # ["python", "fake_simulator.py"] in tests or ["gdb", "--args", "simulator"] to debug.
        # The server appends "--config <file> --ipc stdio".
        self.simulator_command = list(simulator_command) if simulator_command else None
        self.config_dir = self.project_root / "data" / "defaults"
        # Runtime scratch space (plugin overlay, temp configs); git-ignored.
        self.runtime_dir = self.project_root / "data" / "config"
        self.temp_dir = self.runtime_dir / "tmp"
        # The simulator runs with the project root as working directory, so its
        # relative "output/" directory resolves here.
        self.output_dir = self.project_root / "output"
        self.initial_state_dir = self.temp_dir
        self.static_dir = Path(__file__).parent / "static"

        # Simulation settings
        self.max_simulation_timeout = 600  # 10 minutes
        self.simulation_check_interval = 0.5  # seconds

        # Output directories to search (in priority order)
        self.output_search_dirs = [
            self.output_dir,
            self.build_dir / "output",
            self.project_root / "tools" / "output",
            self.project_root / "results",
        ]

        # Browsers allowed to call the API from another origin (CORS). Same-origin
        # requests never need this; it only helps front-end development servers.
        self.allowed_origins = list(allowed_origins) if allowed_origins is not None else [
            f"http://localhost:{port}",
            f"http://127.0.0.1:{port}",
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
