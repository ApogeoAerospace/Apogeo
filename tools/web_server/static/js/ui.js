/**
 * UI module
 * Handles all UI interactions and updates
 */

const UI = {
    /**
     * Initialize UI event listeners
     */
    init() {
        this.setupTabs();
        this.setupChartTabs();
    },
    
    /**
     * Setup main navigation tabs
     */
    setupTabs() {
        const tabs = document.querySelectorAll('.tab[data-tab]');
        
        tabs.forEach(tab => {
            tab.addEventListener('click', () => {
                const tabName = tab.dataset.tab;
                this.showTab(tabName);
            });
        });
    },
    
    /**
     * Setup chart navigation tabs
     */
    setupChartTabs() {
        const chartTabs = document.querySelectorAll('.chart-tab[data-chart]');
        
        chartTabs.forEach(tab => {
            tab.addEventListener('click', () => {
                const chartName = tab.dataset.chart;
                this.showChart(chartName);
            });
        });
    },
    
    /**
     * Show a specific tab
     */
    showTab(tabName) {
        // Hide all tab contents
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });
        
        // Deactivate all tabs
        document.querySelectorAll('.tab').forEach(tab => {
            tab.classList.remove('active');
        });
        
        // Show selected tab content
        const tabContent = document.getElementById(`${tabName}-tab`);
        if (tabContent) {
            tabContent.classList.add('active');
        }
        
        // Activate selected tab
        const activeTab = document.querySelector(`.tab[data-tab="${tabName}"]`);
        if (activeTab) {
            activeTab.classList.add('active');
        }
    },
    
    /**
     * Show a specific chart
     */
    showChart(chartName) {
        // Hide all chart panels
        document.querySelectorAll('.chart-panel').forEach(panel => {
            panel.classList.remove('active');
        });
        
        // Deactivate all chart tabs
        document.querySelectorAll('.chart-tab').forEach(tab => {
            tab.classList.remove('active');
        });
        
        // Show selected chart panel
        const chartPanel = document.getElementById(`${chartName}-chart-panel`);
        if (chartPanel) {
            chartPanel.classList.add('active');
        }
        
        // Activate selected chart tab
        const activeTab = document.querySelector(`.chart-tab[data-chart="${chartName}"]`);
        if (activeTab) {
            activeTab.classList.add('active');
        }

        // Re-render with current dataset when switching tabs to avoid
        // stale hidden-layout chart sizing/parsing artifacts.
        const currentData = window.app?.results?.currentResultData;
        if (currentData && window.Charts && typeof window.Charts.renderChart === 'function') {
            window.Charts.renderChart(chartName, currentData);
        }

        // Charts rendered while hidden need an explicit resize once visible.
        if (window.Charts && typeof window.Charts.resizeChart === 'function') {
            requestAnimationFrame(() => {
                setTimeout(() => window.Charts.resizeChart(chartName), 50);
                setTimeout(() => window.Charts.resizeChart(chartName), 200);
            });
        }
    },
    
    /**
     * Update status message
     */
    updateStatus(message, type = 'info') {
        const statusDiv = document.getElementById('status');
        statusDiv.className = `status ${type}`;
        
        const icon = {
            'success': '',
            'error': '',
            'info': '',
            'warning': ''
        }[type] || ' ';
        
        statusDiv.innerHTML = `${icon} ${message}`;
    },
    
    /**
     * Update progress bar with smooth animation
     */
    updateProgress(percentage, message = '') {
        const progressFill = document.getElementById('progress-fill');
        const progressText = document.getElementById('progress-text');
        const progressMessage = document.getElementById('progress-message');
        
        if (progressFill) {
            // Smooth transition animation
            progressFill.style.transition = 'width 0.3s ease-out, background-color 0.3s ease';
            progressFill.style.width = `${percentage}%`;
            
            // Dynamic color based on progress
            if (percentage < 25) {
                progressFill.style.backgroundColor = '#667eea'; // Blue - starting
            } else if (percentage < 50) {
                progressFill.style.backgroundColor = '#17a2b8'; // Cyan - warming up
            } else if (percentage < 75) {
                progressFill.style.backgroundColor = '#28a745'; // Green - good progress
            } else if (percentage < 95) {
                progressFill.style.backgroundColor = '#ffc107'; // Yellow - almost done
            } else {
                progressFill.style.backgroundColor = '#28a745'; // Green - complete
            }
            
            // Add pulsing animation when active
            if (percentage > 0 && percentage < 100) {
                progressFill.style.animation = 'progress-pulse 1.5s ease-in-out infinite';
            } else {
                progressFill.style.animation = 'none';
            }
        }
        
        if (progressText) {
            progressText.textContent = `${percentage.toFixed(1)}%`;
            // Add subtle scale animation on update
            progressText.style.animation = 'text-update 0.3s ease';
        }
        
        if (progressMessage && message) {
            progressMessage.textContent = message;
            progressMessage.style.animation = 'fade-in 0.3s ease';
        }
    },
    
    /**
     * Show/hide progress section
     */
    toggleProgressSection(show) {
        const progressSection = document.getElementById('progress-section');
        if (progressSection) {
            progressSection.style.display = show ? 'block' : 'none';
        }
    },
    
    /**
     * Toggle run/cancel buttons
     */
    toggleRunButtons(isRunning) {
        const runBtn = document.getElementById('run-btn');
        const cancelBtn = document.getElementById('cancel-btn');
        
        if (runBtn) {
            runBtn.style.display = isRunning ? 'none' : 'inline-block';
        }
        
        if (cancelBtn) {
            cancelBtn.style.display = isRunning ? 'inline-block' : 'none';
        }
    },
    
    /**
     * Read the gravity direction inputs as [x, y, z] (defaults to straight down).
     */
    getGravityDirection() {
        const read = (id, fallback) => {
            const value = parseFloat(document.getElementById(id)?.value);
            return Number.isFinite(value) ? value : fallback;
        };
        return [read('gravity-dir-x', 0), read('gravity-dir-y', 0), read('gravity-dir-z', -1)];
    },

    /**
     * Get form configuration
     *
     * Plugins are intentionally NOT part of this payload: their enabled state and
     * parameters live in the backend registry (toggled with /api/plugins/*), which is
     * the single source of truth the simulator is started from.
     */
    getFormConfig() {
        const timeStep = parseFloat(document.getElementById('time-step')?.value || 0.1);
        const duration = parseFloat(document.getElementById('duration')?.value || 10);
        const maxIterations = (timeStep > 0 && duration > 0)
            ? Math.max(1, Math.ceil(duration / timeStep))
            : 1000;
        const gravityMagnitude = parseFloat(document.getElementById('gravity-magnitude')?.value);

        const config = {
            simulation: {
                time_step: timeStep,
                duration: duration,
                max_iterations: maxIterations,
                enable_logging: document.getElementById('enable-logging')?.checked ?? true,
                log_level: document.getElementById('log-level')?.value || 'INFO',
                log_file: 'logs/apogeo_web.log'
            },
            physics: {
                enable_atmospheric_drag: document.getElementById('enable-drag')?.checked ?? false,
                integration_tolerance: 1e-6,
                integrator_type: document.getElementById('integrator-type')?.value || 'runge_kutta_4'
            },
            environment: {
                gravity: {
                    enabled: document.getElementById('enable-gravity')?.checked ?? true,
                    magnitude: Number.isFinite(gravityMagnitude) ? gravityMagnitude : 9.81,
                    direction: this.getGravityDirection()
                }
            },
            output: {
                enable_csv: document.getElementById('enable-csv')?.checked ?? true,
                enable_json: document.getElementById('enable-json')?.checked ?? false,
                enable_binary: false,
                output_interval: parseInt(document.getElementById('output-interval')?.value || 1),
                output_directory: document.getElementById('output-directory')?.value || 'output'
            }
        };
        
        // Include initial state from template if available
        if (window.templateInitialState) {
            config.initial_state = window.templateInitialState;
        }
        
        return config;
    },
    
    /**
     * Set form configuration
     */
    setFormConfig(config) {
        if (config.simulation) {
            const sim = config.simulation;
            this.setInputValue('time-step', sim.time_step);
            this.setInputValue('duration', sim.duration);
            this.setInputValue('log-level', sim.log_level);
            this.setCheckboxValue('enable-logging', sim.enable_logging);
        }
        
        if (config.physics) {
            const phys = config.physics;
            this.setCheckboxValue('enable-drag', phys.enable_atmospheric_drag);
            this.setInputValue('integrator-type', phys.integrator_type);
        }

        this.setGravityFormValues(config);
        
        if (config.output) {
            const out = config.output;
            this.setCheckboxValue('enable-csv', out.enable_csv);
            this.setCheckboxValue('enable-json', out.enable_json);
            this.setInputValue('output-interval', out.output_interval);
            this.setInputValue('output-directory', out.output_directory);
        }
    },
    
    /**
     * Set input value safely
     */
    setInputValue(id, value) {
        const element = document.getElementById(id);
        if (element && value !== undefined) {
            element.value = value;
        }
    },
    
    /**
     * Set checkbox value safely
     */
    setCheckboxValue(id, value) {
        const element = document.getElementById(id);
        if (element && value !== undefined) {
            element.checked = value;
        }
    },
    
    /**
     * Fill the gravity inputs from a config. `environment.gravity` is the current
     * contract; `physics.enable_gravity` / `physics.gravity_magnitude` are the
     * deprecated fields still found in older saved configs and templates.
     */
    setGravityFormValues(config) {
        const legacy = config.physics || {};
        const gravity = (config.environment && config.environment.gravity) || {};

        const enabled = gravity.enabled !== undefined ? gravity.enabled : legacy.enable_gravity;
        const magnitude = gravity.magnitude !== undefined ? gravity.magnitude : legacy.gravity_magnitude;

        this.setCheckboxValue('enable-gravity', enabled);
        this.setInputValue('gravity-magnitude', magnitude);

        if (Array.isArray(gravity.direction) && gravity.direction.length === 3) {
            this.setInputValue('gravity-dir-x', gravity.direction[0]);
            this.setInputValue('gravity-dir-y', gravity.direction[1]);
            this.setInputValue('gravity-dir-z', gravity.direction[2]);
        }
    },

    /**
     * Render the gravity notice under the gravity inputs.
     *
     * Gravity only affects a trajectory when an enabled plugin applies it, either from the
     * vector in the state ("state_vector", e.g. the Example Plugin) or with its own model
     * ("own_model", the Environment module). The notice explains which case applies, and
     * warns when gravity would be missing or applied twice.
     *
     * @param {Object|null} env  Response of GET /api/environment (null = unknown)
     */
    renderGravityNotice(env) {
        const notice = document.getElementById('gravity-notice');
        if (!notice) {
            return;
        }

        notice.className = 'gravity-notice';
        notice.textContent = '';

        if (!env) {
            return;
        }

        const gravity = this.getFormConfig().environment.gravity;
        const wantsGravity = gravity.enabled && gravity.magnitude > 0;
        const providers = env.gravity_providers || [];
        const ownModel = providers.filter(p => p.mode === 'own_model').map(p => p.id);
        const fromState = providers.filter(p => p.mode === 'state_vector').map(p => p.id);

        const show = (kind, text) => {
            notice.classList.add(kind);
            notice.textContent = text;
        };

        if (providers.length > 1) {
            show('warning',
                `Gravity is applied by more than one plugin (${providers.map(p => p.id).join(', ')}): ` +
                'their forces add up and the vehicle falls too fast. Keep only one.');
        } else if (ownModel.length && !wantsGravity) {
            show('warning',
                `Gravity is off here, but ${ownModel.join(', ')} applies its own gravity model and cannot be ` +
                'switched off from the panel. Disable that plugin to remove gravity.');
        } else if (ownModel.length) {
            show('info',
                `${ownModel.join(', ')} applies its own gravity model (about 9.8 m/s² toward the ground). ` +
                'The magnitude and direction above are only written to the simulation state and the CSV.');
        } else if (fromState.length && wantsGravity) {
            show('info', `Gravity is applied by ${fromState.join(', ')} using the vector above.`);
        } else if (!wantsGravity) {
            show('info', 'Gravity is off: nothing will accelerate the vehicle downwards.');
        } else {
            show('warning',
                env.environment_plugin && !env.environment_plugin.available
                    ? 'Gravity is enabled, but nothing applies it: the Environment module is not built in this checkout. '
                      + 'Enable the Example Plugin and turn on "Apply Gravity" to see it in action.'
                    : 'Gravity is enabled, but no enabled plugin applies it. Enable the Environment module '
                      + 'or the Example Plugin with "Apply Gravity".');
        }
    },

    /**
     * Show error modal
     */
    showError(message) {
        alert(`Error: ${message}`);
        this.updateStatus(message, 'error');
    },
    
    /**
     * Show success message
     */
    showSuccess(message) {
        this.updateStatus(message, 'success');
    },
    
    /**
     * Confirm action
     */
    confirm(message) {
        return window.confirm(message);
    },
    
    /**
     * Prompt for input
     */
    prompt(message, defaultValue = '') {
        return window.prompt(message, defaultValue);
    }
};

// Export
window.UI = UI;
