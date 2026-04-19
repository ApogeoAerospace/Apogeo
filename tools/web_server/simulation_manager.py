"""
Simulation Manager - Handles simulation execution and real progress tracking
Solves the critical issue of fake progress data
"""
import subprocess
import threading
import time
import uuid
import json
from pathlib import Path
from typing import Optional, Dict, Any
from enum import Enum
from dataclasses import dataclass, field
from datetime import datetime

from .logger import setup_logger
from .utils import write_json_file, get_simulator_path, normalize_config_for_simulator


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
        temp_config = None
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
                job.config["initial_state_file"] = str(self.config.project_root / "data" / "default_state.json")
            
            # Normalize config for current simulator schema
            normalized_config = normalize_config_for_simulator(job.config)

            # Create temporary config file
            temp_config = self.config.config_dir / f"temp_web_{timestamp}.json"
            
            if not write_json_file(temp_config, normalized_config):
                raise Exception("Failed to write config file")
            
            # Get simulator path
            simulator_path = get_simulator_path(self.config.build_dir)
            
            if not simulator_path.exists():
                raise Exception(f"Simulator not found at: {simulator_path}")
            
            simulation_cfg = normalized_config.get('simulation', {})
            duration = float(simulation_cfg.get('duration', 0.0))
            total_ticks = int(simulation_cfg.get('max_iterations', 0))
            if total_ticks <= 0:
                total_ticks = 1
            
            # Start the simulation process (IPC mode)
            job.status = SimulationStatus.RUNNING
            job.message = f"Running simulation (max {total_ticks} ticks)..."
            
            self.logger.info(f"Starting simulation process for job {job.id}")
            
            # Run simulation in IPC stdio mode for structured progress/events
            process = subprocess.Popen(
                [str(simulator_path), "--config", str(temp_config), "--ipc", "stdio"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            
            job.process = process
            
            init_response = self._send_ipc_command(process, "initialize", {}, job, total_ticks, duration)
            if init_response.get('type') != 'ack':
                error_message = init_response.get('error', {}).get('message', 'Failed to initialize IPC session')
                raise Exception(error_message)

            ipc_cfg = normalized_config.get('ipc', {})
            run_payload = {
                "tick_event_interval": int(ipc_cfg.get('tick_event_interval', 50)),
                "telemetry_interval_ticks": int(ipc_cfg.get('telemetry_interval_ticks', 5))
            }

            run_response = self._send_ipc_command(process, "run_full", run_payload, job, total_ticks, duration)
            if run_response.get('type') != 'ack':
                error_message = run_response.get('error', {}).get('message', 'Simulation execution failed')
                raise Exception(error_message)

            self._send_ipc_command(process, "shutdown", {}, job, total_ticks, duration)

            if process.stdin:
                process.stdin.close()

            process.wait(timeout=self.config.max_simulation_timeout)
            
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
            
        except Exception as e:
            job.status = SimulationStatus.FAILED
            job.error = str(e)
            job.message = f"Error: {str(e)}"
            self.logger.exception(f"Exception in simulation job {job.id}")
        
        finally:
            if temp_config and temp_config.exists():
                temp_config.unlink()
            job.end_time = datetime.now()
    
    def _send_ipc_command(
        self,
        process: subprocess.Popen,
        command_name: str,
        payload: Dict[str, Any],
        job: SimulationJob,
        total_ticks: int,
        total_duration: float
    ) -> Dict[str, Any]:
        """Send an IPC command and wait for its ack/error response."""
        if not process.stdin or not process.stdout:
            raise Exception("IPC pipes are not available")

        request_id = str(uuid.uuid4())
        command = {
            "type": "command",
            "id": request_id,
            "name": command_name,
            "payload": payload or {}
        }

        process.stdin.write(json.dumps(command) + "\n")
        process.stdin.flush()

        while True:
            line = process.stdout.readline()
            if not line:
                stderr = process.stderr.read().strip() if process.stderr else ""
                raise Exception(stderr or f"IPC connection closed while waiting for '{command_name}' response")

            message = self._process_ipc_message(job, line, total_ticks, total_duration)
            if not message:
                continue

            if message.get('type') in {'ack', 'error'} and message.get('request_id') == request_id:
                return message

    def _process_ipc_message(self, job: SimulationJob, line: str, total_ticks: int, total_duration: float) -> Optional[Dict[str, Any]]:
        """
        Process a line of IPC output from the simulator to update job status/progress.
        
        Args:
            job: SimulationJob instance
            line: IPC line from simulator stdout
            total_ticks: Total number of ticks
            total_duration: Expected simulation duration
        """
        try:
            message = json.loads(line.strip())
        except json.JSONDecodeError:
            return None

        msg_type = message.get('type')
        if msg_type == 'event':
            event_name = message.get('event')
            payload = message.get('payload', {})

            if event_name == 'simulation_started':
                mode = payload.get('mode', 'run_full')
                job.message = f"Simulation started ({mode})"
            elif event_name == 'tick_completed':
                tick = int(payload.get('tick', 0))
                sim_time = float(payload.get('sim_time', 0.0))

                tick_progress = (tick / total_ticks) if total_ticks > 0 else 0.0
                time_progress = (sim_time / total_duration) if total_duration > 0 else 0.0
                progress_ratio = max(tick_progress, time_progress)

                job.progress = min(max(progress_ratio * 100.0, 0.0), 99.9)
                job.message = f"Processing tick {tick}/{total_ticks}"
            elif event_name == 'simulation_finished':
                if payload.get('success', True):
                    job.progress = max(job.progress, 99.9)
                    job.message = "Finalizing simulation output..."
            elif event_name == 'error':
                error_msg = payload.get('message')
                if error_msg:
                    job.error = str(error_msg)

        return message
    
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
