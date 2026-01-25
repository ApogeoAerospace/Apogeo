/**
 * Configuration module
 * Centralizes configuration values and constants
 */

const CONFIG = {
    // API endpoints
    API: {
        STATUS: '/api/status',
        PLUGINS: '/api/plugins',
        RUN: '/api/run',
        SIMULATION_STATUS: '/api/simulation/status',
        SIMULATION_CANCEL: '/api/simulation/cancel',
        RESULTS_LIST: '/api/results',
        RESULTS_GET: '/api/results',
        CONFIG_SAVE: '/api/config/save',
        CONFIG_LOAD: '/api/config/load',
        CONFIG_GET: '/api/config'
    },
    
    // Polling intervals
    POLLING: {
        STATUS_CHECK: 5000,        // 5 seconds
        SIMULATION_PROGRESS: 250,  // 250ms - smooth real-time updates
        RETRY_DELAY: 10000         // 10 seconds
    },
    
    // Chart configuration
    CHARTS: {
        DEFAULT_HEIGHT: 400,
        COLORS: {
            PRIMARY: 'rgba(102, 126, 234, 1)',
            SECONDARY: 'rgba(118, 75, 162, 1)',
            SUCCESS: 'rgba(40, 167, 69, 1)',
            DANGER: 'rgba(220, 53, 69, 1)',
            WARNING: 'rgba(255, 193, 7, 1)',
            INFO: 'rgba(23, 162, 184, 1)'
        }
    },
    
    // Default simulation configuration
    DEFAULT_CONFIG: {
        simulation: {
            time_step: 0.1,
            duration: 10.0,
            max_iterations: 1000,
            enable_logging: true,
            log_level: "INFO",
            log_file: "logs/molab_web.log"
        },
        physics: {
            enable_gravity: true,
            gravity_magnitude: 9.81,
            enable_atmospheric_drag: false,
            integration_tolerance: 1e-6,
            integrator_type: "runge_kutta_4"
        },
        plugins: [],
        output: {
            enable_csv: true,
            enable_json: false,
            enable_binary: false,
            output_interval: 1,
            output_directory: "output"
        }
    }
};

// Export for use in other modules
window.CONFIG = CONFIG;
