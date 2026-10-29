/**
 * Templates module
 * Manages mission templates
 * Templates are loaded from a separate JSON file instead of being hardcoded
 */

const Templates = {
    templates: null,
    
    /**
     * Initialize templates
     */
    async init() {
        // For now, define templates inline
        // In a production system, these would be loaded from the server
        this.templates = {
            suborbital: {
                name: "Parabolic Trajectory",
                description: "Pure ballistic arc under gravity (no propulsion/aero plugins)",
                config: {
                    simulation: {
                        time_step: 0.01,
                        duration: 30.0,
                        max_iterations: 3000
                    },
                    environment: {
                        gravity: { enabled: true, magnitude: 9.81, direction: [0, 0, -1] }
                    },
                    physics: {
                        enable_atmospheric_drag: false,
                        integrator_type: "runge_kutta_4"
                    },
                    initial_state: {
                        position: [0.0, 0.0, 0.0],
                        velocity: [120.0, 0.0, 120.0],
                        mass: 1000.0
                    },
                    plugins: {
                        enabled: ["environment"],
                        overrides: {
                            environment: {
                                enable_atmospheric_model: false,
                                enable_wind_effects: false,
                                enable_gravity_variation: true
                            }
                        }
                    }
                }
            },
            atmospheric: {
                name: "Atmospheric Test",
                description: "Low altitude flight with atmospheric effects",
                config: {
                    simulation: {
                        time_step: 0.1,
                        duration: 60.0,
                        max_iterations: 600
                    },
                    environment: {
                        gravity: { enabled: true, magnitude: 9.81, direction: [0, 0, -1] }
                    },
                    physics: {
                        enable_atmospheric_drag: true,
                        integrator_type: "runge_kutta_4"
                    },
                    initial_state: {
                        position: [0.0, 0.0, 5000.0],
                        velocity: [100.0, 0.0, 50.0],
                        mass: 5000.0
                    }
                }
            },
            landing: {
                name: "Re-entry & Landing",
                description: "Final descent from altitude (landing phase only)",
                config: {
                    simulation: {
                        time_step: 0.02,
                        duration: 120.0,
                        max_iterations: 6000
                    },
                    environment: {
                        gravity: { enabled: true, magnitude: 9.81, direction: [0, 0, -1] }
                    },
                    physics: {
                        enable_atmospheric_drag: true,
                        integrator_type: "runge_kutta_4"
                    },
                    initial_state: {
                        position: [0.0, 0.0, 6381000.0],
                        velocity: [50.0, 0.0, -100.0],
                        mass: 10000.0
                    }
                }
            },
            orbital: {
                name: "Orbital Insertion",
                description: "Multi-stage ascent to orbital velocity",
                config: {
                    simulation: {
                        time_step: 0.1,
                        duration: 600.0,
                        max_iterations: 6000
                    },
                    environment: {
                        gravity: { enabled: true, magnitude: 9.81, direction: [0, 0, -1] }
                    },
                    physics: {
                        enable_atmospheric_drag: true,
                        integrator_type: "runge_kutta_4"
                    },
                    initial_state: {
                        position: [0.0, 0.0, 0.0],
                        velocity: [0.0, 0.0, 0.0],
                        mass: 100000.0
                    }
                }
            }
        };
        
        this.setupTemplateButtons();
    },
    
    /**
     * Setup template button event listeners
     */
    setupTemplateButtons() {
        const templateButtons = document.querySelectorAll('.template-card[data-template]');
        
        templateButtons.forEach(button => {
            button.addEventListener('click', () => {
                this.setSelectedTemplate(button);
                const templateName = button.dataset.template;
                this.loadTemplate(templateName);
            });
        });
    },

    /**
     * Mark the selected template button with a persistent visual state
     */
    setSelectedTemplate(selectedButton) {
        const templateButtons = document.querySelectorAll('.template-card[data-template]');
        templateButtons.forEach(button => button.classList.remove('selected'));
        selectedButton.classList.add('selected');
    },
    
    /**
     * Load a template
     */
    loadTemplate(templateName) {
        const template = this.templates[templateName];
        
        if (!template) {
            UI.showError(`Template not found: ${templateName}`);
            return;
        }
        
        // Apply template configuration
        this.applyTemplate(template);
        
        // Show confirmation
        UI.showSuccess(`Loaded template: ${template.name}`);
    },
    
    /**
     * Apply template configuration to form
     */
    applyTemplate(template) {
        const config = template.config;
        
        // Apply simulation parameters
        if (config.simulation) {
            UI.setInputValue('time-step', config.simulation.time_step);
            UI.setInputValue('duration', config.simulation.duration);
        }
        
        // Apply physics parameters
        if (config.physics) {
            UI.setCheckboxValue('enable-drag', config.physics.enable_atmospheric_drag);
            UI.setInputValue('integrator-type', config.physics.integrator_type);
        }

        // Gravity belongs to the environment domain (legacy physics.* still understood)
        UI.setGravityFormValues(config);
        
        // Store initial state for simulation
        if (config.initial_state) {
            window.templateInitialState = {
                position: config.initial_state.position,
                velocity: config.initial_state.velocity || [0, 0, 0],
                mass: config.initial_state.mass || 1000.0
            };
            
            // Calculate and log altitude for user info
            const pos = config.initial_state.position;
            
            // FIX: Detect coordinate system before calculating altitude
            const EARTH_RADIUS = 6378137.0; // meters (WGS84)
            const GEOCENTRIC_THRESHOLD = 1000000; // 1M meters
            const magnitude = Math.sqrt(pos[0]**2 + pos[1]**2 + pos[2]**2);
            
            let altitude;
            let coordSystem;
            if (magnitude > GEOCENTRIC_THRESHOLD) {
                // Geocentric coordinates (ECEF)
                altitude = magnitude - EARTH_RADIUS;
                coordSystem = 'GEOCENTRIC';
            } else {
                // Local coordinates (topocentric)
                altitude = pos[2]; // Z is directly the altitude
                coordSystem = 'LOCAL';
            }
            
            console.log('Template Initial State Applied:');
            console.log(`  Position: [${pos}]`);
            console.log(`  Coordinate System: ${coordSystem}`);
            console.log(`  Altitude: ${altitude.toFixed(0)} m (${(altitude/1000).toFixed(1)} km)`);
            console.log(`  Velocity: [${config.initial_state.velocity}]`);
            console.log(`  Mass: ${config.initial_state.mass} kg`);
        }

        if (config.plugins) {
            // Fire-and-forget on purpose: it reports its own result in the status bar.
            this.applyPluginPreset(config.plugins);
        } else if (window.app) {
            window.app.refreshEnvironment();
        }
    },
    
    /**
     * Get template by name
     */
    getTemplate(name) {
        return this.templates[name] || null;
    },
    
    /**
     * List all available templates
     */
    listTemplates() {
        return Object.keys(this.templates).map(key => ({
            id: key,
            name: this.templates[key].name,
            description: this.templates[key].description
        }));
    },

    /**
     * Apply a template's plugin set through the backend registry.
     *
     * The backend applies the whole set atomically and skips plugins that are not
     * built, so a template that asks for the Environment module still loads on a
     * checkout where that module does not exist yet; the user is told what was skipped.
     */
    async applyPluginPreset(pluginConfig) {
        const enabled = (pluginConfig.enabled || []).map(p => String(p).toLowerCase());
        const overrides = pluginConfig.overrides || {};

        try {
            const response = await API.applyPluginPreset(enabled, overrides);
            if (window.app) {
                window.app.setPlugins(response.plugins);
            }

            const skipped = response.plugins.filter(p => p.skipped).map(p => p.name);
            if (skipped.length > 0) {
                UI.updateStatus(
                    `Template loaded, but these plugins are not built and were skipped: ${skipped.join(', ')}`,
                    'warning'
                );
            }
        } catch (error) {
            console.error('Error applying plugin preset:', error);
            UI.updateStatus(`Could not apply the template's plugins: ${error.message}`, 'error');
        }

        if (window.app) {
            await window.app.refreshEnvironment();
        }
    }
};

// Export
window.Templates = Templates;
