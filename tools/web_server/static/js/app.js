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
    environment: null,           // last GET /api/environment response
    pluginBusy: new Set(),       // plugin ids with a request in flight
    
    /**
     * Initialize the application
     */
    async init() {
        console.log('Initializing Apogeo Web GUI v2.0...');
        
        // Initialize sub-modules
        this.simulation = Simulation;
        this.results = Results;
        
        // Initialize UI
        UI.init();
        
        // Initialize templates
        await Templates.init();

        // Plugin list and gravity inputs use event delegation, bound once.
        this.bindPluginEvents();
        this.bindGravityEvents();
        
        // Load initial data
        await this.loadInitialData();
        
        // Setup auto-retry for status loading
        this.setupStatusRetry();
        
        console.log('Apogeo Web GUI initialized successfully');
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
     * Load available plugins (the backend registry is the source of truth)
     */
    async loadPlugins() {
        try {
            console.log('Loading plugins...');

            const plugins = await API.getPlugins();
            this.setPlugins(plugins);
            await this.refreshEnvironment();

            console.log(`Loaded ${plugins.length} plugins`);

        } catch (error) {
            console.error('Error loading plugins:', error);
            this.showPluginsError('Failed to load plugins: ' + error.message);
        }
    },

    /**
     * Replace the plugin list and re-render
     */
    setPlugins(plugins) {
        this.availablePlugins = plugins;

        // Make available globally for backward compatibility (templates read it)
        window.availablePlugins = plugins;

        this.updatePluginsList();
    },

    /**
     * Replace one plugin with the state returned by the backend
     */
    replacePlugin(plugin) {
        const index = this.availablePlugins.findIndex(p => p.id === plugin.id);
        if (index >= 0) {
            this.availablePlugins[index] = plugin;
        }
    },

    /**
     * Refresh environment info (which plugins apply gravity) and the gravity notice
     */
    async refreshEnvironment() {
        try {
            this.environment = await API.getEnvironment();
        } catch (error) {
            console.error('Error loading environment info:', error);
            this.environment = null;
        }

        UI.renderGravityNotice(this.environment);
    },

    /**
     * Bind plugin list events once (event delegation survives re-renders)
     */
    bindPluginEvents() {
        const list = document.getElementById('plugins-list');
        if (list) {
            list.addEventListener('change', (event) => {
                const target = event.target;
                const action = target.dataset ? target.dataset.action : null;

                if (action === 'toggle') {
                    this.togglePlugin(target.dataset.pluginId, target.checked);
                } else if (action === 'param') {
                    this.onParameterInput(target);
                }
            });
        }

        const reset = document.querySelector('[data-action="reset-plugins"]');
        if (reset) {
            reset.addEventListener('click', () => this.resetPlugins());
        }
    },

    /**
     * Re-evaluate the gravity notice when the gravity inputs change
     */
    bindGravityEvents() {
        ['enable-gravity', 'gravity-magnitude'].forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.addEventListener('change', () => UI.renderGravityNotice(this.environment));
            }
        });
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
        this.availablePlugins.forEach((plugin) => {
            const busy = this.pluginBusy.has(plugin.id);
            const unavailable = !plugin.library_found;
            const classes = [
                'plugin-card',
                plugin.enabled ? 'enabled' : '',
                unavailable ? 'unavailable' : '',
                busy ? 'busy' : ''
            ].filter(Boolean).join(' ');

            // A missing library can always be switched off, but never on.
            const switchDisabled = busy || (unavailable && !plugin.enabled);
            const switchTitle = unavailable && !plugin.enabled
                ? 'This plugin library is not built. Build the project to enable it.'
                : (plugin.enabled ? 'Disable plugin' : 'Enable plugin');

            const badge = unavailable
                ? '<span class="plugin-badge warn">Not built</span>'
                : '<span class="plugin-badge ok">Built</span>';

            html += `
                <div class="${classes}" data-plugin-id="${this.escapeAttr(plugin.id)}">
                    <div class="plugin-header">
                        <div>
                            <div class="plugin-title">
                                <h4>${this.escapeHtml(plugin.name)}</h4>
                                <label class="plugin-switch" title="${this.escapeAttr(switchTitle)}">
                                    <input type="checkbox" data-action="toggle"
                                           data-plugin-id="${this.escapeAttr(plugin.id)}"
                                           ${plugin.enabled ? 'checked' : ''}
                                           ${switchDisabled ? 'disabled' : ''}>
                                    <span>${plugin.enabled ? 'Enabled' : 'Disabled'}</span>
                                </label>
                                ${badge}
                            </div>
                            <p class="plugin-description">${this.escapeHtml(plugin.description)}</p>
                            ${plugin.notes ? `<p class="plugin-note">ℹ️ ${this.escapeHtml(plugin.notes)}</p>` : ''}
                            <span class="plugin-type">Type: ${plugin.type === 0 ? 'Sequential' : 'Parallel Physics'}</span>
                        </div>
                    </div>
                    ${plugin.enabled && plugin.parameters ? this.generateParametersForm(plugin) : ''}
                </div>
            `;
        });
        
        pluginsList.innerHTML = html;
    },
    
    /**
     * Generate parameters form for a plugin
     */
    generateParametersForm(plugin) {
        const entries = Object.entries(plugin.parameters || {});
        if (entries.length === 0) return '';

        const busy = this.pluginBusy.has(plugin.id) ? 'disabled' : '';
        let html = '<div class="plugin-parameters">';
        html += '<h5>⚙️ Parameters:</h5>';
        
        entries.forEach(([key, value]) => {
            const inputId = `plugin-${plugin.id}-${key}`;
            const label = key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
            const data = `data-action="param" data-plugin-id="${this.escapeAttr(plugin.id)}" data-key="${this.escapeAttr(key)}"`;
            
            if (typeof value === 'boolean') {
                html += `
                    <div class="param-group">
                        <label>
                            <input type="checkbox" id="${this.escapeAttr(inputId)}" ${data} data-kind="boolean"
                                   ${value ? 'checked' : ''} ${busy}>
                            ${this.escapeHtml(label)}
                        </label>
                    </div>
                `;
            } else if (typeof value === 'number') {
                html += `
                    <div class="param-group">
                        <label for="${this.escapeAttr(inputId)}">${this.escapeHtml(label)}</label>
                        <input type="number" id="${this.escapeAttr(inputId)}" ${data} data-kind="number"
                               value="${value}" step="any" ${busy}>
                    </div>
                `;
            } else {
                html += `
                    <div class="param-group">
                        <label for="${this.escapeAttr(inputId)}">${this.escapeHtml(label)}</label>
                        <input type="text" id="${this.escapeAttr(inputId)}" ${data} data-kind="string"
                               value="${this.escapeAttr(String(value))}" ${busy}>
                    </div>
                `;
            }
        });
        
        html += '</div>';
        return html;
    },
    
    /**
     * Enable/disable a plugin in the backend, then show what the backend reports.
     *
     * The UI never assumes the change worked: on any failure it re-reads the real
     * state, so the toggle can never show something the backend does not have.
     *
     * @param {string} pluginId
     * @param {boolean} enabled  Explicit target state (idempotent request)
     */
    async togglePlugin(pluginId, enabled) {
        if (this.pluginBusy.has(pluginId)) return;

        this.pluginBusy.add(pluginId);
        this.updatePluginsList();

        try {
            const response = await API.togglePlugin(pluginId, enabled);
            this.replacePlugin(response.plugin);
            UI.updateStatus(`${response.plugin.name} ${response.plugin.enabled ? 'enabled' : 'disabled'}`, 'success');
        } catch (error) {
            console.error('Error toggling plugin:', error);
            UI.updateStatus(`Could not ${enabled ? 'enable' : 'disable'} "${pluginId}": ${error.message}`, 'error');
            await this.reloadPluginsQuietly();
        } finally {
            this.pluginBusy.delete(pluginId);
            this.updatePluginsList();
            await this.refreshEnvironment();
        }
    },

    /**
     * Read a parameter control and send it to the backend
     */
    onParameterInput(input) {
        const kind = input.dataset.kind;
        let value;

        if (kind === 'boolean') {
            value = input.checked;
        } else if (kind === 'number') {
            value = parseFloat(input.value);
            if (!Number.isFinite(value)) {
                UI.updateStatus(`"${input.dataset.key}" needs a valid number`, 'error');
                this.updatePluginsList();
                return;
            }
        } else {
            value = input.value;
        }

        this.updatePluginParameter(input.dataset.pluginId, input.dataset.key, value);
    },

    /**
     * Update plugin parameter value in the backend
     */
    async updatePluginParameter(pluginId, paramKey, value) {
        if (this.pluginBusy.has(pluginId)) return;

        this.pluginBusy.add(pluginId);
        this.updatePluginsList();

        try {
            const response = await API.setPluginParameters(pluginId, { [paramKey]: value });
            this.replacePlugin(response.plugin);
        } catch (error) {
            console.error('Error updating plugin parameter:', error);
            UI.updateStatus(`Could not update "${paramKey}": ${error.message}`, 'error');
            await this.reloadPluginsQuietly();
        } finally {
            this.pluginBusy.delete(pluginId);
            this.updatePluginsList();
            await this.refreshEnvironment();
        }
    },

    /**
     * Drop every override and go back to the configuration defaults
     */
    async resetPlugins() {
        if (!UI.confirm('Reset all plugins and their parameters to the defaults?')) {
            return;
        }

        try {
            const response = await API.resetPlugins();
            this.setPlugins(response.plugins);
            UI.updateStatus('Plugins reset to defaults', 'success');
        } catch (error) {
            console.error('Error resetting plugins:', error);
            UI.updateStatus(`Could not reset plugins: ${error.message}`, 'error');
        }

        await this.refreshEnvironment();
    },

    /**
     * Re-sync the list with the backend after a failed request (keeps the error message)
     */
    async reloadPluginsQuietly() {
        try {
            this.setPlugins(await API.getPlugins());
        } catch (error) {
            console.error('Error reloading plugins:', error);
        }
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
        UI.updateStatus(' Refreshing...', 'info');
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
    },

    /**
     * Escape a value for use inside a double-quoted HTML attribute
     */
    escapeAttr(text) {
        return String(text)
            .replace(/&/g, '&amp;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
    }
};

let appStarted = false;

function startApogeoApp() {
    if (appStarted) {
        return;
    }

    appStarted = true;
    console.log('DOM loaded, starting application...');
    App.init();
}

// Expose manual bootstrap entry-point for layout loader.
window.startApogeoApp = startApogeoApp;

if (window.__APOGEO_LAYOUT_LOADER__) {
    if (window.__APOGEO_LAYOUT_READY__) {
        startApogeoApp();
    } else {
        document.addEventListener('apogeo:layout-ready', startApogeoApp, { once: true });
    }
} else {
    document.addEventListener('DOMContentLoaded', startApogeoApp);
}

// Export globally
window.app = App;
