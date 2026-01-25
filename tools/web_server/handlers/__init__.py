"""
Request handlers organized by domain
"""
from .static_handler import StaticHandler
from .api_handlers import (
    StatusHandler,
    PluginsHandler,
    SimulationHandler,
    ResultsHandler,
    ConfigHandler
)

__all__ = [
    'StaticHandler',
    'StatusHandler',
    'PluginsHandler',
    'SimulationHandler',
    'ResultsHandler',
    'ConfigHandler'
]
