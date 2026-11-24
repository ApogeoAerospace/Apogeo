"""
Simulation Manager - Handles simulation execution and real progress tracking
Solves the critical issue of fake progress data
"""
import queue
import subprocess
import threading
import time
import uuid
import json
from collections import deque
from pathlib import Path
from typing import Optional, Dict, Any, List
from enum import Enum
from dataclasses import dataclass, field
from datetime import datetime

from .logger import setup_logger
from .utils import (
    write_json_file,
    read_json_file,
    get_simulator_path,
    normalize_config_for_simulator,
    resolve_gravity,
)

# How many IPC ``log`` events are kept per job, and how many are sent to the UI.
MAX_JOB_LOG_ENTRIES = 200
STATUS_LOG_TAIL = 50


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
    cancel_requested: bool = False
    warnings: List[str] = field(default_factory=list)
    plugins: List[str] = field(default_factory=list)
    gravity: Dict[str, Any] = field(default_factory=dict)
    logs: deque = field(default_factory=lambda: deque(maxlen=MAX_JOB_LOG_ENTRIES))
    log_count: int = 0
    stderr_tail: deque = field(default_factory=lambda: deque(maxlen=200))

    def add_log(self, level: str, component: str, message: str) -> None:
        """Append an IPC log event (bounded)."""
        self.log_count += 1
        self.logs.append({
            'seq': self.log_count,
            'level': level,
            'component': component,
            'message': message,
        })

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
            'output_files': self.output_files,
            'warnings': list(self.warnings),
            'plugins': list(self.plugins),
            'gravity': dict(self.gravity),
            'log_count': self.log_count,
            'logs': list(self.logs)[-STATUS_LOG_TAIL:],
        }


class _JobCancelled(Exception):
    """Internal control-flow signal: the user cancelled the job."""


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
        self._lock = threading.RLock()
        self.logger = setup_logger(f"{__name__}.SimulationManager")
        
    def create_job(self, config: Dict[str, Any], warnings: Optional[List[str]] = None,
                   gravity: Optional[Dict[str, Any]] = None) -> str:
        """
        Create a new simulation job
        
        Args:
            config: Simulation configuration
            warnings: Non-blocking warnings to surface with the job status
            gravity: Gravity resolved from the *incoming* payload. Pass it when the
                config was already normalized, because normalization rewrites every
                payload into the canonical ``environment.gravity`` block and the
                original ``source`` (legacy vs. current contract) would be lost.
            
        Returns:
            Job ID
        """
        job_id = str(uuid.uuid4())
        job = SimulationJob(id=job_id, config=config)
        job.gravity = gravity or resolve_gravity(config)
        job.warnings = list(warnings or [])
        job.plugins = [
            str(p.get('name')) for p in config.get('plugins', [])
            if isinstance(p, dict) and p.get('enabled', True) and p.get('name')
        ] if isinstance(config.get('plugins'), list) else []
        with self._lock:
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
        with self._lock:
            job = self.jobs.get(job_id)
            if job is None:
                self.logger.error(f"Job not found: {job_id}")
                return False

            if job.status != SimulationStatus.PENDING:
                self.logger.warning(f"Job {job_id} is not in PENDING state")
                return False

            # Mark as initializing right away so a second start cannot race.
            job.status = SimulationStatus.INITIALIZING
            job.message = "Initializing simulation..."

        # Daemon thread: a running simulation must not keep the server process alive
        # after Ctrl+C. The child simulator is terminated by the thread's cleanup.
        thread = threading.Thread(target=self._run_simulation, args=(job,), daemon=True)
        thread.start()

        return True
    
    def _run_simulation(self, job: SimulationJob):
        """
        Run the simulation (executed in a separate thread)

        Args:
            job: SimulationJob instance
        """
        temp_config: Optional[Path] = None
        temp_state_file: Optional[Path] = None
        stderr_lines: deque = job.stderr_tail
        process: Optional[subprocess.Popen] = None
        try:
            job.status = SimulationStatus.INITIALIZING
            job.start_time = datetime.now()
            job.message = "Initializing simulation..."

            # Unique, collision-free names (two jobs may start within one second).
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            tag = f"{timestamp}_{job.id[:8]}"

            # Handle initial state (always materialize a temp state so dt follows UI)
            temp_state_file = self.config.initial_state_dir / f"temp_web_state_{tag}.json"
            simulation_cfg = job.config.get('simulation', {})
            time_step = float(simulation_cfg.get('time_step', 0.01))

            # Gravity is owned by the environment domain. The resolved vector is
            # seeded into GeneralState.gravity, which environment-aware plugins read.
            gravity = job.gravity or resolve_gravity(job.config)
            job.gravity = gravity

            if 'initial_state' in job.config and job.config['initial_state']:
                initial_state_data = self._create_initial_state_data(
                    job.config['initial_state'], time_step, gravity['vector'])
            else:
                default_state_file = self.config.project_root / "data" / "defaults" / "default_state.json"
                initial_state_data = read_json_file(default_state_file)
                if not initial_state_data:
                    raise Exception(f"Failed to read default state file: {default_state_file}")

                initial_state_data['dt'] = time_step
                initial_state_data['gravity'] = self._gravity_to_state(gravity['vector'])

            if not write_json_file(temp_state_file, initial_state_data):
                raise Exception("Failed to write initial state file")

            job.config["initial_state_file"] = temp_state_file.as_posix()

            # Normalize config for current simulator schema
            normalized_config = normalize_config_for_simulator(job.config)

            # Create temporary config file
            temp_config = self.config.temp_dir / f"temp_web_{tag}.json"

            if not write_json_file(temp_config, normalized_config):
                raise Exception("Failed to write config file")

            # Get simulator path
            simulator_path = get_simulator_path(self.config.build_dir)
            override = self.config.simulator_command

            if not override and not simulator_path.exists():
                raise Exception(f"Simulator not found at: {simulator_path}")

            command = (list(override) if override else [str(simulator_path)]) + [
                "--config", str(temp_config), "--ipc", "stdio"
            ]

            simulation_cfg = normalized_config.get('simulation', {})
            duration = float(simulation_cfg.get('duration', 0.0))
            total_ticks = int(simulation_cfg.get('max_iterations', 0))
            if total_ticks <= 0:
                total_ticks = 1

            if job.cancel_requested:
                raise _JobCancelled()

            # Start the simulation process (IPC mode)
            job.status = SimulationStatus.RUNNING
            job.message = f"Running simulation (max {total_ticks} ticks)..."

            self.logger.info(f"Starting simulation process for job {job.id}")

            # The project root is the working directory: relative paths used by the
            # simulator and its plugins (data/, scripts/, output/) resolve there no
            # matter where the web server was launched from.
            process = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                cwd=str(self.config.project_root)
            )

            job.process = process
            if job.cancel_requested:
                # Cancel arrived between the check above and process creation.
                process.terminate()

            # Drain stderr continuously; an unread pipe can fill up and block the child.
            threading.Thread(
                target=self._drain_stream, args=(process.stderr, stderr_lines), daemon=True
            ).start()

            stdout_lines: "queue.Queue" = queue.Queue()
            threading.Thread(
                target=self._pump_stdout, args=(process.stdout, stdout_lines), daemon=True
            ).start()

            init_response = self._send_ipc_command(
                process, "initialize", {}, job, total_ticks, duration, stdout_lines)
            if init_response.get('type') != 'ack':
                error_message = init_response.get('error', {}).get('message', 'Failed to initialize IPC session')
                raise Exception(error_message)

            ipc_cfg = normalized_config.get('ipc', {})
            run_payload = {
                "tick_event_interval": int(ipc_cfg.get('tick_event_interval', 50)),
                "telemetry_interval_ticks": int(ipc_cfg.get('telemetry_interval_ticks', 5))
            }

            run_response = self._send_ipc_command(
                process, "run_full", run_payload, job, total_ticks, duration, stdout_lines)
            if run_response.get('type') != 'ack':
                error_message = run_response.get('error', {}).get('message', 'Simulation execution failed')
                raise Exception(error_message)

            # Best effort: the run already finished; a slow shutdown must not fail it.
            try:
                self._send_ipc_command(process, "shutdown", {}, job, total_ticks, duration, stdout_lines)
            except _JobCancelled:
                raise
            except Exception as shutdown_error:
                self.logger.warning(f"Shutdown command not confirmed for job {job.id}: {shutdown_error}")

            if process.stdin:
                process.stdin.close()

            process.wait(timeout=self.config.max_simulation_timeout)

            # Process finished
            returncode = process.returncode

            if job.cancel_requested:
                raise _JobCancelled()

            if returncode == 0:
                job.status = SimulationStatus.COMPLETED
                job.progress = 100.0
                job.message = "Simulation completed successfully"
                job.output_files = self._find_output_files(timestamp)
                self.logger.info(f"Simulation job {job.id} completed successfully")
            else:
                job.status = SimulationStatus.FAILED
                job.error = "\n".join(stderr_lines) or "Unknown error"
                job.message = "Simulation failed"
                self.logger.error(f"Simulation job {job.id} failed: {job.error}")

        except _JobCancelled:
            job.status = SimulationStatus.CANCELLED
            job.message = "Cancelled by user"

        except subprocess.TimeoutExpired:
            if process is not None:
                process.kill()
            job.status = SimulationStatus.FAILED
            job.error = f"Simulation exceeded the {self.config.max_simulation_timeout}s timeout"
            job.message = f"Error: {job.error}"
            self.logger.error(f"Simulation job {job.id} timed out")

        except Exception as e:
            if job.cancel_requested:
                # The process was terminated on purpose; do not report it as a failure.
                job.status = SimulationStatus.CANCELLED
                job.message = "Cancelled by user"
            else:
                job.status = SimulationStatus.FAILED
                job.error = str(e)
                job.message = f"Error: {str(e)}"
                self.logger.exception(f"Exception in simulation job {job.id}")

        finally:
            if process is not None and process.poll() is None:
                try:
                    process.terminate()
                    process.wait(timeout=5)
                except Exception:
                    process.kill()
            for temp_file in (temp_config, temp_state_file):
                try:
                    if temp_file and temp_file.exists():
                        temp_file.unlink()
                except OSError:
                    self.logger.warning(f"Could not remove temporary file: {temp_file}")
            job.end_time = datetime.now()

    @staticmethod
    def _drain_stream(stream, sink: deque) -> None:
        """Read a text stream to EOF, keeping the latest lines."""
        try:
            for line in stream:
                line = line.rstrip()
                if line:
                    sink.append(line)
        except (ValueError, OSError):
            pass

    # Seconds allowed for commands that should answer quickly; ``run_full`` may take as
    # long as ``max_simulation_timeout``.
    _COMMAND_TIMEOUTS = {"initialize": 60.0, "shutdown": 15.0}

    def _command_timeout(self, command_name: str) -> float:
        limit = float(self.config.max_simulation_timeout)
        return min(self._COMMAND_TIMEOUTS.get(command_name, limit), limit)

    @staticmethod
    def _pump_stdout(stream, lines: "queue.Queue") -> None:
        """Move simulator stdout lines into a queue so waiting on them can time out.

        A plain ``readline()`` cannot be interrupted (and ``select`` does not work on
        Windows pipes). ``None`` is queued at EOF.
        """
        try:
            for line in stream:
                lines.put(line)
        except (ValueError, OSError):
            pass
        finally:
            lines.put(None)

    def _send_ipc_command(
        self,
        process: subprocess.Popen,
        command_name: str,
        payload: Dict[str, Any],
        job: SimulationJob,
        total_ticks: int,
        total_duration: float,
        lines: "queue.Queue"
    ) -> Dict[str, Any]:
        """
        Send an IPC command and wait for its ack/error response.

        The wait is bounded and cancellable. ``run_full`` is special: on a failure while
        running, the protocol emits ``simulation_finished`` with ``success: false`` and
        NO ack (docs/IPC_PROTOCOL.md), so that event is turned into an error response.
        """
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

        timeout = self._command_timeout(command_name)
        deadline = time.monotonic() + timeout

        while True:
            if job.cancel_requested:
                raise _JobCancelled()

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise Exception(
                    f"Simulator did not answer '{command_name}' within {timeout:g}s "
                    "(the run was stopped)"
                )

            try:
                line = lines.get(timeout=min(0.25, remaining))
            except queue.Empty:
                continue

            if line is None:
                # EOF: the process is gone. Let the stderr drain thread read the last lines.
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    pass
                time.sleep(0.05)
                stderr = "\n".join(job.stderr_tail).strip()
                raise Exception(stderr or f"IPC connection closed while waiting for '{command_name}' response")

            message = self._process_ipc_message(job, line, total_ticks, total_duration)
            if not message:
                continue

            if message.get('type') in {'ack', 'error'} and message.get('request_id') == request_id:
                return message

            if (command_name == 'run_full' and message.get('type') == 'event'
                    and message.get('event') == 'simulation_finished'
                    and message.get('payload', {}).get('success') is False):
                return {
                    'type': 'error',
                    'ok': False,
                    'request_id': request_id,
                    'error': {
                        'code': 'engine_error',
                        'message': job.error or 'Simulation failed while running',
                    },
                }

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
            elif event_name == 'log':
                job.add_log(
                    str(payload.get('level', 'INFO')),
                    str(payload.get('component', '')),
                    str(payload.get('message', '')),
                )
            elif event_name == 'error':
                error_msg = payload.get('message')
                if error_msg:
                    job.error = str(error_msg)

        return message
    
    @staticmethod
    def _gravity_to_state(vector) -> Dict[str, float]:
        """Convert a gravity vector [x, y, z] to the state-file representation."""
        return {"x": float(vector[0]), "y": float(vector[1]), "z": float(vector[2])}

    def _create_initial_state_data(self, initial_state: Dict[str, Any], time_step: float,
                                   gravity_vector) -> Dict[str, Any]:
        """
        Create initial state data structure

        Args:
            initial_state: Initial state from config
            time_step: Integrator time step (seconds)
            gravity_vector: Effective gravity [x, y, z] in m/s^2 (zeros when disabled),
                resolved from the environment domain by ``resolve_gravity``

        Returns:
            Complete initial state data
        """
        return {
            "sim_time": 0.0,
            "dt": time_step,
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
            "angular_velocity": {"x": 0.0, "y": 0.0, "z": 0.0},
            "total_mass": initial_state.get('mass', 1000.0),
            "cg_location": {"x": 0.0, "y": 0.0, "z": 0.0},
            "inertia_tensor": {"ixx": 10.0, "iyy": 12.0, "izz": 8.0, "ixy": 0.0, "ixz": 0.0, "iyz": 0.0},
            "propellant_masses": [200.0, 150.0],
            "mach_number": 0.0,
            "dynamic_pressure": 0.0,
            "angle_of_attack": 0.0,
            "sideslip_angle": 0.0,
            "atm_density": 1.225,
            "atm_pressure": 101325.0,
            "atm_temperature": 288.15,
            "gravity": self._gravity_to_state(gravity_vector),
            "wind_velocity": {"x": 5.0, "y": 0.0, "z": 0.0},
            "engines": [],
            "surface_deflections": []
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
        with self._lock:
            job = self.jobs.get(job_id)
        return job.to_dict() if job else None

    def cancel_job(self, job_id: str) -> bool:
        """
        Cancel a pending, initializing or running job

        The run thread observes ``cancel_requested`` so that terminating the child
        process is reported as CANCELLED instead of FAILED.

        Args:
            job_id: Job ID to cancel

        Returns:
            True if the cancellation was requested
        """
        with self._lock:
            job = self.jobs.get(job_id)
            if job is None:
                return False

            if job.status in (SimulationStatus.COMPLETED, SimulationStatus.FAILED,
                              SimulationStatus.CANCELLED):
                return False

            job.cancel_requested = True
            job.message = "Cancelling..."
            process = job.process

        if process is not None and process.poll() is None:
            process.terminate()

        if job.status == SimulationStatus.PENDING:
            job.status = SimulationStatus.CANCELLED
            job.message = "Cancelled by user"

        self.logger.info(f"Cancellation requested for simulation job {job_id}")
        return True

    def cleanup_old_jobs(self, max_age_hours: int = 24):
        """
        Clean up old completed jobs
        
        Args:
            max_age_hours: Maximum age in hours to keep jobs
        """
        now = datetime.now()
        with self._lock:
            jobs_to_remove = [
                job_id for job_id, job in self.jobs.items()
                if job.end_time and (now - job.end_time).total_seconds() / 3600 > max_age_hours
            ]

            for job_id in jobs_to_remove:
                del self.jobs[job_id]
                self.logger.info(f"Cleaned up old job: {job_id}")
