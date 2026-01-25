"""
MoLab Web Server Package
Modular web interface for aerospace simulations
"""
from .config import ServerConfig
from .server import MoLabWebServer
from .simulation_manager import SimulationManager, SimulationStatus

__version__ = "2.0.0"

__all__ = [
    'ServerConfig',
    'MoLabWebServer',
    'SimulationManager',
    'SimulationStatus'
]
