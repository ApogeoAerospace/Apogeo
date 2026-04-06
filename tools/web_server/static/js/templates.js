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
                name: "Launch - Suborbital Flight",
                description: "Rocket launch with ascent and descent (parabolic trajectory)",
                config: {
                    simulation: {
                        time_step: 0.05,
                        duration: 300.0,
                        max_iterations: 6000
                    },
                    physics: {
                        enable_gravity: true,
                        gravity_magnitude: 9.81,
                        enable_atmospheric_drag: true,
                        integrator_type: "runge_kutta_4"
                    },
                    initial_state: {
                        position: [0.0, 0.0, 0.0],
                        velocity: [0.0, 0.0, 150.0],
                        mass: 50000.0
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
                    physics: {
                        enable_gravity: true,
                        gravity_magnitude: 9.81,
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
                    physics: {
                        enable_gravity: true,
                        gravity_magnitude: 9.81,
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
                    physics: {
                        enable_gravity: true,
                        gravity_magnitude: 9.81,
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
                const templateName = button.dataset.template;
                this.loadTemplate(templateName);
            });
        });
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
            UI.setCheckboxValue('enable-gravity', config.physics.enable_gravity);
            UI.setInputValue('gravity-magnitude', config.physics.gravity_magnitude);
            UI.setCheckboxValue('enable-drag', config.physics.enable_atmospheric_drag);
            UI.setInputValue('integrator-type', config.physics.integrator_type);
        }
        
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
        
        // Note: Plugin configuration would be applied here if plugins are loaded
        // This is handled separately in the plugins module
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
    }
};

// Export
window.Templates = Templates;
