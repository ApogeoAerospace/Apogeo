/**
 * API module
 * Encapsulates all HTTP requests to the backend
 */

const API = {
    /**
     * Generic fetch wrapper with error handling
     */
    async fetch(url, options = {}) {
        try {
            const response = await fetch(url, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error(`API Error (${url}):`, error);
            throw error;
        }
    },
    
    /**
     * Get system status
     */
    async getStatus() {
        return await this.fetch(CONFIG.API.STATUS);
    },
    
    /**
     * Get available plugins
     */
    async getPlugins() {
        return await this.fetch(CONFIG.API.PLUGINS);
    },
    
    /**
     * Start a simulation
     */
    async runSimulation(config) {
        return await this.fetch(CONFIG.API.RUN, {
            method: 'POST',
            body: JSON.stringify(config)
        });
    },
    
    /**
     * Get simulation status
     */
    async getSimulationStatus(jobId) {
        return await this.fetch(`${CONFIG.API.SIMULATION_STATUS}/${jobId}`);
    },
    
    /**
     * Cancel a simulation
     */
    async cancelSimulation(jobId) {
        return await this.fetch(`${CONFIG.API.SIMULATION_CANCEL}/${jobId}`, {
            method: 'POST'
        });
    },
    
    /**
     * Get list of results
     */
    async getResults() {
        return await this.fetch(CONFIG.API.RESULTS_LIST);
    },
    
    /**
     * Get specific result data
     */
    async getResultData(filename) {
        return await this.fetch(`${CONFIG.API.RESULTS_GET}/${filename}`);
    },
    
    /**
     * Save configuration
     */
    async saveConfig(filename, config) {
        return await this.fetch(CONFIG.API.CONFIG_SAVE, {
            method: 'POST',
            body: JSON.stringify({ filename, config })
        });
    },
    
    /**
     * Load available configurations
     */
    async loadConfigs() {
        return await this.fetch(CONFIG.API.CONFIG_LOAD);
    },
    
    /**
     * Get specific configuration
     */
    async getConfig(filename) {
        return await this.fetch(`${CONFIG.API.CONFIG_GET}/${filename}`);
    }
};

// Export
window.API = API;
