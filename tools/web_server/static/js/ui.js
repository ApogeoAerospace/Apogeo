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
    },
    
    /**
     * Update status message
     */
    updateStatus(message, type = 'info') {
        const statusDiv = document.getElementById('status');
        statusDiv.className = `status ${type}`;
        
        const icon = {
            'success': '✅',
            'error': '❌',
            'info': '🔄',
            'warning': '⚠️'
        }[type] || 'ℹ️';
        
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
     * Get form configuration
     */
    getFormConfig() {
        const config = {
            simulation: {
                time_step: parseFloat(document.getElementById('time-step')?.value || 0.1),
                duration: parseFloat(document.getElementById('duration')?.value || 10),
                max_iterations: 1000,
                enable_logging: document.getElementById('enable-logging')?.checked ?? true,
                log_level: document.getElementById('log-level')?.value || 'INFO',
                log_file: 'logs/molab_web.log'
            },
            physics: {
                enable_gravity: document.getElementById('enable-gravity')?.checked ?? true,
                gravity_magnitude: parseFloat(document.getElementById('gravity-magnitude')?.value || 9.81),
                enable_atmospheric_drag: document.getElementById('enable-drag')?.checked ?? false,
                integration_tolerance: 1e-6,
                integrator_type: document.getElementById('integrator-type')?.value || 'runge_kutta_4'
            },
            plugins: window.availablePlugins?.filter(p => p.enabled) || [],
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
            this.setCheckboxValue('enable-gravity', phys.enable_gravity);
            this.setInputValue('gravity-magnitude', phys.gravity_magnitude);
            this.setCheckboxValue('enable-drag', phys.enable_atmospheric_drag);
            this.setInputValue('integrator-type', phys.integrator_type);
        }
        
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
