#!/usr/bin/env python3
"""
MoLab Web GUI v2.0 - Refactored Version
Clean, modular architecture with proper separation of concerns

This is the new entry point that replaces the monolithic molab_web_gui.py

Improvements:
- Modular architecture with separate modules for each concern
- Router pattern instead of manual if/elif cascading
- Real simulation progress tracking (no fake data)
- Proper error handling and logging
- Platform-independent plugin discovery
- Input validation and sanitization
- Separated frontend (HTML/CSS/JS) from backend (Python)
- Clean code following SOLID principles
"""
import argparse
import sys
from pathlib import Path

# Add web_server package to path
sys.path.insert(0, str(Path(__file__).parent))

from web_server import ServerConfig, MoLabWebServer
from web_server.logger import setup_logger


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='MoLab Web GUI - Aerospace Simulation Interface'
    )
    
    parser.add_argument(
        '--port',
        type=int,
        default=8082,
        help='Port to run the server on (default: 8082)'
    )
    
    parser.add_argument(
        '--no-browser',
        action='store_true',
        help='Do not open browser automatically'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Enable verbose logging'
    )
    
    return parser.parse_args()


def main():
    """Main entry point"""
    args = parse_arguments()
    
    # Setup logging
    import logging
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logger = setup_logger('molab_web_gui', log_level)
    
    logger.info("=" * 60)
    logger.info("MoLab Web GUI v2.0 - Refactored")
    logger.info("=" * 60)
    
    try:
        # Create configuration
        config = ServerConfig(
            port=args.port,
            auto_open_browser=not args.no_browser
        )
        
        # Validate configuration
        config.validate()
        
        # Create and start server
        server = MoLabWebServer(config)
        server.start()
        
    except ValueError as e:
        logger.error(f"Configuration error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        logger.info("\nShutting down gracefully...")
        sys.exit(0)
    except Exception as e:
        logger.exception("Fatal error")
        sys.exit(1)


if __name__ == "__main__":
    main()
