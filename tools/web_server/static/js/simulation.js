/**
 * Simulation module
 * Handles simulation execution with REAL progress tracking
 * This replaces the fake progress system from the original code
 */

const Simulation = {
    currentJobId: null,
    progressInterval: null,
    pollInFlight: false,
    consecutivePollFailures: 0,
    lastRenderedLogCount: -1,

    // Give up after this many unreachable polls in a row (~5 s at the 250 ms cadence).
    MAX_POLL_FAILURES: 20,
    
    /**
     * Run a new simulation
     */
    async runSimulation() {
        try {
            // Get configuration from form
            const config = UI.getFormConfig();
            
            // Validate configuration
            if (!this.validateConfig(config)) {
                return;
            }
            
            // Update UI
            UI.updateStatus('Starting simulation...', 'info');
            UI.toggleProgressSection(true);
            UI.toggleRunButtons(true);
            UI.updateProgress(0, 'Initializing...');
            
            // Start simulation
            const response = await API.runSimulation(config);
            
            if (response.success) {
                this.currentJobId = response.job_id;
                this.lastRenderedLogCount = -1;
                this.renderWarnings(response.warnings);
                UI.updateStatus(
                    response.warnings && response.warnings.length ? response.warnings[0] : 'Simulation started',
                    response.warnings && response.warnings.length ? 'warning' : 'success'
                );
                
                // Start monitoring progress (REAL progress, not fake!)
                this.startProgressMonitoring();
            } else {
                throw new Error(response.error || 'Failed to start simulation');
            }
            
        } catch (error) {
            console.error('Error running simulation:', error);
            UI.showError(error.message);
            this.resetUI();
        }
    },
    
    /**
     * Start monitoring simulation progress
     * THIS IS REAL PROGRESS TRACKING - not fake data!
     */
    startProgressMonitoring() {
        if (this.progressInterval) {
            clearInterval(this.progressInterval);
        }

        const jobId = this.currentJobId;
        this.pollInFlight = false;
        this.consecutivePollFailures = 0;

        this.progressInterval = setInterval(async () => {
            // Skip a tick while the previous request is still pending: overlapping
            // responses could otherwise trigger the completion handler twice.
            if (this.pollInFlight) {
                return;
            }
            this.pollInFlight = true;

            try {
                const status = await API.getSimulationStatus(jobId);

                // The user may have cancelled/finished and started another job meanwhile.
                if (jobId !== this.currentJobId) {
                    return;
                }

                this.consecutivePollFailures = 0;

                // Update progress with REAL data from server
                UI.updateProgress(status.progress, status.message);
                this.renderJobLog(status);

                // Check if simulation is complete
                if (status.status === 'completed') {
                    this.onSimulationComplete(status);
                } else if (status.status === 'failed') {
                    this.onSimulationFailed(status);
                } else if (status.status === 'cancelled') {
                    this.onSimulationCancelled(status);
                }

            } catch (error) {
                console.error('Error checking simulation status:', error);
                this.consecutivePollFailures += 1;

                if (this.consecutivePollFailures >= this.MAX_POLL_FAILURES) {
                    this.onSimulationFailed({
                        error: 'Lost connection with the web server while the simulation was running.'
                    });
                }
            } finally {
                this.pollInFlight = false;
            }
        }, CONFIG.POLLING.SIMULATION_PROGRESS);
    },

    /**
     * Show non-blocking warnings returned when the job was created
     */
    renderWarnings(warnings) {
        const box = document.getElementById('progress-warnings');
        if (box) {
            box.textContent = (warnings || []).join(' ');
        }
    },

    /**
     * Render the simulator's IPC "log" events (plugin and core messages)
     */
    renderJobLog(status) {
        const details = document.getElementById('job-log-details');
        const output = document.getElementById('job-log');
        const counter = document.getElementById('job-log-count');
        if (!details || !output || !Array.isArray(status.logs)) {
            return;
        }

        if (status.log_count === this.lastRenderedLogCount) {
            return;
        }
        this.lastRenderedLogCount = status.log_count;

        details.style.display = status.logs.length ? 'block' : 'none';
        if (counter) {
            counter.textContent = `(${status.log_count})`;
        }

        // textContent only: log text comes from plugins and must never be parsed as HTML.
        output.textContent = '';
        status.logs.forEach(entry => {
            const line = document.createElement('div');
            line.className = `log-${entry.level}`;
            line.textContent = `[${entry.level}] ${entry.component ? entry.component + ': ' : ''}${entry.message}`;
            output.appendChild(line);
        });
        output.scrollTop = output.scrollHeight;
    },
    
    /**
     * Stop progress monitoring
     */
    stopProgressMonitoring() {
        if (this.progressInterval) {
            clearInterval(this.progressInterval);
            this.progressInterval = null;
        }
    },
    
    /**
     * Handle simulation completion
     */
    async onSimulationComplete(status) {
        this.stopProgressMonitoring();
        
        UI.updateProgress(100, 'Simulation completed successfully!');
        UI.updateStatus('Simulation completed successfully!', 'success');
        
        // Auto-load results after a short delay
        setTimeout(async () => {
            await window.app.results.loadResults();
            UI.showTab('results');
            this.resetUI();
        }, 2000);
    },
    
    /**
     * Handle simulation failure
     */
    onSimulationFailed(status) {
        this.stopProgressMonitoring();
        
        const errorMsg = status.error || 'Simulation failed';
        UI.updateStatus(`Simulation failed: ${errorMsg}`, 'error');
        UI.showError(errorMsg);
        
        this.resetUI();
    },
    
    /**
     * Handle simulation cancellation
     */
    onSimulationCancelled(status) {
        this.stopProgressMonitoring();
        
        UI.updateStatus('Simulation cancelled by user', 'warning');
        this.resetUI();
    },
    
    /**
     * Cancel running simulation
     */
    async cancelSimulation() {
        if (!this.currentJobId) {
            return;
        }
        
        if (!UI.confirm('Are you sure you want to cancel this simulation?')) {
            return;
        }
        
        try {
            await API.cancelSimulation(this.currentJobId);
            UI.updateStatus('Cancelling simulation...', 'warning');
        } catch (error) {
            console.error('Error cancelling simulation:', error);
            UI.showError('Failed to cancel simulation');
        }
    },
    
    /**
     * Reset UI to initial state
     */
    resetUI() {
        UI.toggleProgressSection(false);
        UI.toggleRunButtons(false);
        UI.updateProgress(0, '');
        this.currentJobId = null;
        this.pollInFlight = false;
    },
    
    /**
     * Validate simulation configuration
     */
    validateConfig(config) {
        if (!config.simulation) {
            UI.showError('Missing simulation configuration');
            return false;
        }
        
        if (config.simulation.time_step <= 0) {
            UI.showError('Time step must be positive');
            return false;
        }
        
        if (config.simulation.duration <= 0) {
            UI.showError('Duration must be positive');
            return false;
        }
        
        if (config.simulation.time_step > config.simulation.duration) {
            UI.showError('Time step cannot be larger than duration');
            return false;
        }
        
        return true;
    }
};

// Export
window.Simulation = Simulation;
