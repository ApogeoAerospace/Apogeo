/**
 * API module
 * Encapsulates all HTTP requests to the backend
 */

const API = {
    /**
     * Generic fetch wrapper with error handling
     */
    async fetch(url, options = {}) {
        let response;
        try {
            response = await fetch(url, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            });
        } catch (networkError) {
            // fetch only rejects when the server is unreachable (down, blocked, CORS).
            console.error(`API unreachable (${url}):`, networkError);
            const error = new Error('Cannot reach the web server. Is it still running?');
            error.network = true;
            throw error;
        }

        // The backend answers errors with JSON ({success:false, error, code}); surface
        // that message instead of a bare "HTTP 409: Conflict".
        let body = null;
        try {
            body = await response.json();
        } catch (parseError) {
            body = null;
        }

        if (!response.ok) {
            const message = (body && body.error) || `HTTP ${response.status}: ${response.statusText}`;
            const error = new Error(message);
            error.status = response.status;
            error.code = body && body.code;
            console.error(`API Error (${url}):`, message);
            throw error;
        }

        return body;
    },

    /**
     * POST helper with a JSON body
     */
    async post(url, payload) {
        return await this.fetch(url, {
            method: 'POST',
            body: payload === undefined ? undefined : JSON.stringify(payload)
        });
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
     * Enable/disable a plugin in the backend registry.
     * `enabled` should be explicit so repeated requests are idempotent.
     */
    async togglePlugin(pluginId, enabled) {
        return await this.post(
            `${CONFIG.API.PLUGINS}/${encodeURIComponent(pluginId)}/toggle`,
            { enabled }
        );
    },

    /**
     * Update plugin parameters in the backend registry
     */
    async setPluginParameters(pluginId, parameters) {
        return await this.post(
            `${CONFIG.API.PLUGINS}/${encodeURIComponent(pluginId)}/parameters`,
            { parameters }
        );
    },

    /**
     * Apply a plugin set (mission templates) in one atomic request
     */
    async applyPluginPreset(enabled, overrides = {}) {
        return await this.post(CONFIG.API.PLUGINS_PRESET, { enabled, overrides });
    },

    /**
     * Drop every plugin override and return to the configuration defaults
     */
    async resetPlugins() {
        return await this.post(CONFIG.API.PLUGINS_RESET);
    },

    /**
     * Get environment info: default gravity and which plugins apply it
     */
    async getEnvironment() {
        return await this.fetch(CONFIG.API.ENVIRONMENT);
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
