/**
 * Main application module
 * Coordinates all other modules and handles initialization
 */

const App = {
    // Sub-modules
    simulation: null,
    results: null,
    plugins: null,
    
    // State
    availablePlugins: [],
    
    /**
     * Initialize the application
     */
    async init() {
        console.log('Initializing MoLab Web GUI v2.0...');
        
        // Initialize sub-modules
        this.simulation = Simulation;
        this.results = Results;
        
        // Initialize UI
        UI.init();
        
        // Initialize templates
        await Templates.init();
        
        // Load initial data
        await this.loadInitialData();
        
        // Setup auto-retry for status loading
        this.setupStatusRetry();
        
        console.log('MoLab Web GUI initialized successfully');
    },
    
    /**
     * Load initial data
     */
    async loadInitialData() {
        // Load system status
        await this.loadStatus();
        
        // Load plugins
        await this.loadPlugins();
    },
    
    /**
     * Load system status
     */
    async loadStatus() {
        try {
            UI.updateStatus('Loading system status...', 'info');
            
            const status = await API.getStatus();
            
            if (status.simulator_built && status.simulator_executable) {
                UI.updateStatus('System ready - Simulator built and available', 'success');
            } else if (status.simulator_exists && !status.simulator_executable) {
                UI.updateStatus('Simulator exists but is not executable', 'warning');
            } else {
                UI.updateStatus('Simulator not built - Please build the project first', 'error');
            }
            
            // Log detailed status for debugging
            console.log('System Status:', status);
            
        } catch (error) {
            console.error('Error loading status:', error);
            UI.updateStatus(`❌ Error loading status: ${error.message}`, 'error');
        }
    },
    
    /**
     * Load available plugins
     */
    async loadPlugins() {
        try {
            console.log('Loading plugins...');
            
            const plugins = await API.getPlugins();
            this.availablePlugins = plugins;
            
            // Make available globally for backward compatibility
            window.availablePlugins = plugins;
            
            this.updatePluginsList();
            
            console.log(`Loaded ${plugins.length} plugins`);
            
        } catch (error) {
            console.error('Error loading plugins:', error);
            this.showPluginsError('Failed to load plugins: ' + error.message);
        }
    },
    
    /**
     * Update plugins list UI
     */
    updatePluginsList() {
        const pluginsList = document.getElementById('plugins-list');
        
        if (this.availablePlugins.length === 0) {
            pluginsList.innerHTML = '<p class="text-center" style="color: #6c757d;">❌ No plugins found. Build the project first.</p>';
            return;
        }
        
        let html = '';
        this.availablePlugins.forEach((plugin, index) => {
            const enabledClass = plugin.enabled ? 'enabled' : '';
            
            html += `
                <div class="plugin-card ${enabledClass}" onclick="app.togglePlugin(${index})">
                    <div class="plugin-header">
                        <div>
                            <h4>${plugin.enabled ? '✅' : '❌'} ${this.escapeHtml(plugin.name)}</h4>
                            <p class="plugin-description">${this.escapeHtml(plugin.description)}</p>
                            <span class="plugin-type">Type: ${plugin.type === 0 ? 'Sequential' : 'Parallel Physics'}</span>
                        </div>
                    </div>
                    ${plugin.enabled && plugin.parameters ? this.generateParametersForm(plugin, index) : ''}
                </div>
            `;
        });
        
        pluginsList.innerHTML = html;
    },
    
    /**
     * Generate parameters form for a plugin
     */
    generateParametersForm(plugin, index) {
        if (!plugin.parameters) return '';
        
        let html = '<div class="plugin-parameters" onclick="event.stopPropagation();">';
        html += '<h5>⚙️ Parameters:</h5>';
        
        Object.entries(plugin.parameters).forEach(([key, value]) => {
            const inputId = `plugin-${index}-${key}`;
            const label = key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
            
            if (typeof value === 'boolean') {
                html += `
                    <div class="param-group">
                        <label>
                            <input type="checkbox" id="${inputId}" ${value ? 'checked' : ''} 
                                   onchange="app.updatePluginParameter(${index}, '${key}', this.checked)">
                            ${label}
                        </label>
                    </div>
                `;
            } else if (typeof value === 'number') {
                html += `
                    <div class="param-group">
                        <label for="${inputId}">${label}</label>
                        <input type="number" id="${inputId}" value="${value}" step="any"
                               onchange="app.updatePluginParameter(${index}, '${key}', parseFloat(this.value))">
                    </div>
                `;
            } else {
                html += `
                    <div class="param-group">
                        <label for="${inputId}">${label}</label>
                        <input type="text" id="${inputId}" value="${this.escapeHtml(String(value))}"
                               onchange="app.updatePluginParameter(${index}, '${key}', this.value)">
                    </div>
                `;
            }
        });
        
        html += '</div>';
        return html;
    },
    
    /**
     * Toggle plugin enabled state
     */
    togglePlugin(index) {
        if (index < 0 || index >= this.availablePlugins.length) return;
        
        this.availablePlugins[index].enabled = !this.availablePlugins[index].enabled;
        this.updatePluginsList();
    },
    
    /**
     * Update plugin parameter value
     */
    updatePluginParameter(pluginIndex, paramKey, value) {
        if (pluginIndex < 0 || pluginIndex >= this.availablePlugins.length) return;
        
        if (!this.availablePlugins[pluginIndex].parameters) {
            this.availablePlugins[pluginIndex].parameters = {};
        }
        
        this.availablePlugins[pluginIndex].parameters[paramKey] = value;
        console.log(`Updated plugin ${pluginIndex} parameter ${paramKey} = ${value}`);
    },
    
    /**
     * Show plugins error
     */
    showPluginsError(message) {
        const pluginsList = document.getElementById('plugins-list');
        pluginsList.innerHTML = `<p class="text-center" style="color: #dc3545;">❌ ${this.escapeHtml(message)}</p>`;
    },
    
    /**
     * Save current configuration
     */
    async saveConfig() {
        const filename = UI.prompt('Configuration name:', 'web_config');
        if (!filename) return;
        
        try {
            const config = UI.getFormConfig();
            await API.saveConfig(filename, config);
            UI.showSuccess('Configuration saved successfully!');
        } catch (error) {
            console.error('Error saving config:', error);
            UI.showError('Failed to save configuration: ' + error.message);
        }
    },
    
    /**
     * Force refresh of status and plugins
     */
    async forceRefresh() {
        UI.updateStatus('🔄 Refreshing...', 'info');
        await this.loadInitialData();
    },
    
    /**
     * Setup auto-retry for status loading
     */
    setupStatusRetry() {
        setTimeout(() => {
            const statusDiv = document.getElementById('status');
            if (statusDiv && statusDiv.innerHTML.includes('Loading system status')) {
                console.log('Status still loading after 10s, retrying...');
                this.loadStatus();
            }
        }, CONFIG.POLLING.RETRY_DELAY);
    },
    
    /**
     * Escape HTML to prevent XSS
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
};

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('DOM loaded, starting application...');
    App.init();
});

// Export globally
window.app = App;
