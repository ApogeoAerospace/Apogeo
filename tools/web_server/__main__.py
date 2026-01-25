"""
Allow running web_server as a module:
python -m web_server
"""
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from web_server import ServerConfig, MoLabWebServer
from web_server.logger import setup_logger


def main():
    """Run server as module"""
    logger = setup_logger('web_server.__main__')
    
    try:
        config = ServerConfig()
        config.validate()
        
        server = MoLabWebServer(config)
        server.start()
        
    except Exception as e:
        logger.exception("Fatal error")
        sys.exit(1)


if __name__ == "__main__":
    main()
