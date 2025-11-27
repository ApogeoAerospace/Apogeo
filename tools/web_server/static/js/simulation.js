/**
 * Simulation module
 * Handles simulation execution with REAL progress tracking
 * This replaces the fake progress system from the original code
 */

const Simulation = {
    currentJobId: null,
    progressInterval: null,
    
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
                UI.updateStatus('Simulation started', 'success');
                
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
        
        this.progressInterval = setInterval(async () => {
            try {
                const status = await API.getSimulationStatus(this.currentJobId);
                
                // Update progress with REAL data from server
                UI.updateProgress(status.progress, status.message);
                
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
                // Continue polling - don't stop on network errors
            }
        }, CONFIG.POLLING.SIMULATION_PROGRESS);
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
