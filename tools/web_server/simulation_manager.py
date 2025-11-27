"""
Simulation Manager - Handles simulation execution and real progress tracking
Solves the critical issue of fake progress data
"""
import subprocess
import threading
import time
import uuid
from pathlib import Path
from typing import Optional, Dict, Any, Callable
from enum import Enum
from dataclasses import dataclass, field
from datetime import datetime

from .logger import setup_logger
from .utils import write_json_file, get_simulator_path


logger = setup_logger(__name__)


class SimulationStatus(Enum):
    """Simulation status enum"""
    PENDING = "pending"
    INITIALIZING = "initializing"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


@dataclass
class SimulationJob:
    """Represents a simulation job"""
    id: str
    config: Dict[str, Any]
    status: SimulationStatus = SimulationStatus.PENDING
    progress: float = 0.0
    message: str = "Waiting to start"
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    process: Optional[subprocess.Popen] = None
    error: Optional[str] = None
    output_files: list = field(default_factory=list)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            'id': self.id,
            'status': self.status.value,
            'progress': self.progress,
            'message': self.message,
            'start_time': self.start_time.isoformat() if self.start_time else None,
            'end_time': self.end_time.isoformat() if self.end_time else None,
            'error': self.error,
            'output_files': self.output_files
        }


class SimulationManager:
    """
    Manages simulation execution with real progress tracking
    
    This replaces the fake progress monitoring with actual process tracking
    """
    
    def __init__(self, config):
        """
        Initialize simulation manager
        
        Args:
            config: ServerConfig instance
        """
        self.config = config
        self.jobs: Dict[str, SimulationJob] = {}
        self.logger = setup_logger(f"{__name__}.SimulationManager")
        
    def create_job(self, config: Dict[str, Any]) -> str:
        """
        Create a new simulation job
        
        Args:
            config: Simulation configuration
            
        Returns:
            Job ID
        """
        job_id = str(uuid.uuid4())
        job = SimulationJob(id=job_id, config=config)
        self.jobs[job_id] = job
        
        self.logger.info(f"Created simulation job: {job_id}")
        return job_id
    
    def start_job(self, job_id: str) -> bool:
        """
        Start a simulation job
        
        Args:
            job_id: Job ID to start
            
        Returns:
            True if started successfully
        """
        if job_id not in self.jobs:
            self.logger.error(f"Job not found: {job_id}")
            return False
        
        job = self.jobs[job_id]
        
        if job.status != SimulationStatus.PENDING:
            self.logger.warning(f"Job {job_id} is not in PENDING state")
            return False
        
        # Start simulation in a thread
        thread = threading.Thread(target=self._run_simulation, args=(job,))
        thread.daemon = False  # Not daemon - we want to track it properly
        thread.start()
        
        return True
    
    def _run_simulation(self, job: SimulationJob):
        """
        Run the simulation (executed in a separate thread)
        
        Args:
            job: SimulationJob instance
        """
        try:
            job.status = SimulationStatus.INITIALIZING
            job.start_time = datetime.now()
            job.message = "Initializing simulation..."
            
            # Prepare configuration files
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            
            # Handle initial state
            if 'initial_state' in job.config and job.config['initial_state']:
                temp_state_file = self.config.initial_state_dir / f"temp_web_state_{timestamp}.json"
                
                initial_state_data = self._create_initial_state_data(job.config['initial_state'])
                
                if not write_json_file(temp_state_file, initial_state_data):
                    raise Exception("Failed to write initial state file")
                
                job.config["initial_state_file"] = str(temp_state_file)
            else:
                # Use default state file
                job.config["initial_state_file"] = str(self.config.project_root / "data" / "initial_state.json")
            
            # Create temporary config file
            temp_config = self.config.config_dir / f"temp_web_{timestamp}.json"
            
            if not write_json_file(temp_config, job.config):
                raise Exception("Failed to write config file")
            
            # Get simulator path
            simulator_path = get_simulator_path(self.config.build_dir)
            
            if not simulator_path.exists():
                raise Exception(f"Simulator not found at: {simulator_path}")
            
            # Calculate ticks
            ticks = int(job.config['simulation']['duration'] / job.config['simulation']['time_step'])
            
            # Start the simulation process
            job.status = SimulationStatus.RUNNING
            job.message = f"Running simulation ({ticks} ticks)..."
            
            self.logger.info(f"Starting simulation process for job {job.id}")
            
            # Run simulation with real-time output capture
            process = subprocess.Popen(
                [str(simulator_path), "--config", str(temp_config), "--ticks", str(ticks)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            
            job.process = process
            
            # Monitor the process (OPTIMIZED: non-blocking read)
            import select
            import sys
            
            while process.poll() is None:
                # Non-blocking read with minimal latency
                if sys.platform == 'win32':
                    # Windows: use readline but don't sleep as much
                    line = process.stdout.readline()
                    if line:
                        self._process_output_line(job, line, ticks)
                    # Reduced sleep for better responsiveness
                    time.sleep(0.01)  # 10ms instead of 100ms
                else:
                    # Unix: use select for true non-blocking
                    ready = select.select([process.stdout], [], [], 0.01)
                    if ready[0]:
                        line = process.stdout.readline()
                        if line:
                            self._process_output_line(job, line, ticks)
                    else:
                        time.sleep(0.01)
            
            # Process finished
            returncode = process.returncode
            
            if returncode == 0:
                job.status = SimulationStatus.COMPLETED
                job.progress = 100.0
                job.message = "Simulation completed successfully"
                job.output_files = self._find_output_files(timestamp)
                self.logger.info(f"Simulation job {job.id} completed successfully")
            else:
                # Read error output
                stderr = process.stderr.read()
                job.status = SimulationStatus.FAILED
                job.error = stderr or "Unknown error"
                job.message = "Simulation failed"
                self.logger.error(f"Simulation job {job.id} failed: {job.error}")
            
            # Cleanup temporary config file
            if temp_config.exists():
                temp_config.unlink()
            
        except Exception as e:
            job.status = SimulationStatus.FAILED
            job.error = str(e)
            job.message = f"Error: {str(e)}"
            self.logger.exception(f"Exception in simulation job {job.id}")
        
        finally:
            job.end_time = datetime.now()
    
    def _process_output_line(self, job: SimulationJob, line: str, total_ticks: int):
        """
        Process a line of output from the simulator to update progress
        
        Args:
            job: SimulationJob instance
            line: Output line from simulator
            total_ticks: Total number of ticks
        """
        # Look for progress indicators in output
        # This assumes the simulator outputs something like "Tick: 100/1000"
        # Adjust the parsing based on actual simulator output format
        
        if "Tick:" in line or "tick" in line.lower():
            try:
                # Try to extract tick number
                parts = line.split()
                for i, part in enumerate(parts):
                    if 'tick' in part.lower() and i + 1 < len(parts):
                        tick_str = parts[i + 1].strip(':,')
                        if '/' in tick_str:
                            current_tick = int(tick_str.split('/')[0])
                        else:
                            current_tick = int(tick_str)
                        
                        job.progress = min((current_tick / total_ticks) * 100, 99.9)
                        job.message = f"Processing tick {current_tick}/{total_ticks}"
                        break
            except (ValueError, IndexError):
                pass
    
    def _create_initial_state_data(self, initial_state: Dict[str, Any]) -> Dict[str, Any]:
        """
        Create initial state data structure
        
        Args:
            initial_state: Initial state from config
            
        Returns:
            Complete initial state data
        """
        return {
            "_comment": "Temporary state from web GUI template",
            "position": {
                "x": initial_state['position'][0],
                "y": initial_state['position'][1],
                "z": initial_state['position'][2]
            },
            "velocity": {
                "x": initial_state['velocity'][0],
                "y": initial_state['velocity'][1],
                "z": initial_state['velocity'][2]
            },
            "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
            "mass": initial_state.get('mass', 1000.0),
            "atm_density": 1.225,
            "atm_pressure": 101325.0,
            "atm_temperature": 288.15,
            "gravity": {"x": 0.0, "y": 0.0, "z": -9.81},
            "UTC": 0,
            "Time": 0.0,
            "wind_speed": {"x": 5.0, "y": 0.0, "z": 0.0}
        }
    
    def _find_output_files(self, timestamp: str) -> list:
        """
        Find output files generated by the simulation
        
        Args:
            timestamp: Timestamp used in file naming
            
        Returns:
            List of output file paths
        """
        output_files = []
        
        for output_dir in self.config.output_search_dirs:
            if output_dir.exists():
                # Look for files matching the timestamp
                for file_path in output_dir.glob(f"*{timestamp}*"):
                    output_files.append(str(file_path))
                
                # Also check for recent files
                for file_path in output_dir.glob("*.csv"):
                    if (time.time() - file_path.stat().st_mtime) < 60:  # Files modified in last 60 seconds
                        output_files.append(str(file_path))
        
        return list(set(output_files))  # Remove duplicates
    
    def get_job_status(self, job_id: str) -> Optional[Dict[str, Any]]:
        """
        Get status of a job
        
        Args:
            job_id: Job ID
            
        Returns:
            Job status dictionary or None
        """
        if job_id not in self.jobs:
            return None
        
        return self.jobs[job_id].to_dict()
    
    def cancel_job(self, job_id: str) -> bool:
        """
        Cancel a running job
        
        Args:
            job_id: Job ID to cancel
            
        Returns:
            True if cancelled successfully
        """
        if job_id not in self.jobs:
            return False
        
        job = self.jobs[job_id]
        
        if job.status == SimulationStatus.RUNNING and job.process:
            job.process.terminate()
            job.status = SimulationStatus.CANCELLED
            job.message = "Cancelled by user"
            self.logger.info(f"Cancelled simulation job {job_id}")
            return True
        
        return False
    
    def cleanup_old_jobs(self, max_age_hours: int = 24):
        """
        Clean up old completed jobs
        
        Args:
            max_age_hours: Maximum age in hours to keep jobs
        """
        now = datetime.now()
        jobs_to_remove = []
        
        for job_id, job in self.jobs.items():
            if job.end_time:
                age_hours = (now - job.end_time).total_seconds() / 3600
                if age_hours > max_age_hours:
                    jobs_to_remove.append(job_id)
        
        for job_id in jobs_to_remove:
            del self.jobs[job_id]
            self.logger.info(f"Cleaned up old job: {job_id}")
