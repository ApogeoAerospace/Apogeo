/**
/**
 * Charts module
 * Handles all chart creation and visualization using Chart.js
 */

if (window.Chart) {
    Chart.defaults.parsing = false;
    Chart.defaults.normalized = true;
}

const Charts = {
    charts: {},

    getColumnIndex(headers, name, fallbackIndex = -1) {
        const idx = headers.indexOf(name);
        return idx >= 0 ? idx : fallbackIndex;
    },
    
    /**
     * Create all charts from data
     */
    createAllCharts(data) {
        this.createTrajectoryChart(data);
        this.createVelocityChart(data);
        this.createAltitudeChart(data);
        this.createSpeedChart(data);
        this.createAtmosphereChart(data);
        this.createGravityChart(data);
    },
    
    /**
     * Destroy a chart if it exists
     */
    destroyChart(chartId) {
        if (this.charts[chartId]) {
            this.charts[chartId].destroy();
            delete this.charts[chartId];
        }
    },

    resizeChart(chartId) {
        const chart = this.charts[chartId];
        if (!chart) return;
        chart.resize();
        chart.update('none');
    },

    renderChart(chartId, data) {
        if (!data) return;

        switch (chartId) {
            case 'trajectory':
                this.createTrajectoryChart(data);
                break;
            case 'velocity':
                this.createVelocityChart(data);
                break;
            case 'altitude':
                this.createAltitudeChart(data);
                break;
            case 'speed':
                this.createSpeedChart(data);
                break;
            case 'atmosphere':
                this.createAtmosphereChart(data);
                break;
            case 'gravity':
                this.createGravityChart(data);
                break;
            default:
                break;
        }
    },
    
    /**
     * Create trajectory chart (Position X, Y, Z vs Time)
     */
    createTrajectoryChart(data) {
        this.destroyChart('trajectory');
        
        const ctx = document.getElementById('trajectory-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        const posXIdx = this.getColumnIndex(data.headers, 'position_x', 3);
        const posYIdx = this.getColumnIndex(data.headers, 'position_y', 4);
        const posZIdx = this.getColumnIndex(data.headers, 'position_z', 5);
        
        // Position X
        if (posXIdx >= 0) {
            datasets.push({
                label: '📍 Position X (m)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[posXIdx]})),
                borderColor: 'rgba(255, 99, 132, 1)',
                backgroundColor: 'rgba(255, 99, 132, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Position Y
        if (posYIdx >= 0) {
            datasets.push({
                label: '📍 Position Y (m)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[posYIdx]})),
                borderColor: 'rgba(54, 162, 235, 1)',
                backgroundColor: 'rgba(54, 162, 235, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Position Z
        if (posZIdx >= 0) {
            datasets.push({
                label: '📍 Position Z (m)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[posZIdx]})),
                borderColor: 'rgba(75, 192, 192, 1)',
                backgroundColor: 'rgba(75, 192, 192, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        this.charts.trajectory = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '🛸 3D Trajectory - Position Components vs Time'
                    },
                    legend: {
                        display: true,
                        position: 'top'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: {
                            display: true,
                            text: '⏱️ Time (seconds)'
                        }
                    },
                    y: {
                        type: 'linear',
                        title: {
                            display: true,
                            text: '📍 Position (meters)'
                        }
                    }
                }
            }
        });
    },
    
    /**
     * Create velocity chart (Velocity X, Y, Z vs Time)
     */
    createVelocityChart(data) {
        this.destroyChart('velocity');
        
        const ctx = document.getElementById('velocity-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        const vxIdx = this.getColumnIndex(data.headers, 'velocity_x', 6);
        const vyIdx = this.getColumnIndex(data.headers, 'velocity_y', 7);
        const vzIdx = this.getColumnIndex(data.headers, 'velocity_z', 8);
        
        // Velocity X
        if (vxIdx >= 0) {
            datasets.push({
                label: '🚀 Velocity X (m/s)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[vxIdx]})),
                borderColor: 'rgba(255, 159, 64, 1)',
                backgroundColor: 'rgba(255, 159, 64, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Velocity Y
        if (vyIdx >= 0) {
            datasets.push({
                label: '🚀 Velocity Y (m/s)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[vyIdx]})),
                borderColor: 'rgba(153, 102, 255, 1)',
                backgroundColor: 'rgba(153, 102, 255, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Velocity Z
        if (vzIdx >= 0) {
            datasets.push({
                label: '🚀 Velocity Z (m/s)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[vzIdx]})),
                borderColor: 'rgba(255, 205, 86, 1)',
                backgroundColor: 'rgba(255, 205, 86, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Total velocity magnitude
        if (vxIdx >= 0 && vyIdx >= 0 && vzIdx >= 0) {
            const totalVelocity = data.rows.map(row => {
                const vx = row[vxIdx] || 0;
                const vy = row[vyIdx] || 0;
                const vz = row[vzIdx] || 0;
                return {x: row[timeIdx], y: Math.sqrt(vx*vx + vy*vy + vz*vz)};
            });
            
            datasets.push({
                label: '⚡ Total Speed (m/s)',
                data: totalVelocity,
                borderColor: 'rgba(220, 53, 69, 1)',
                backgroundColor: 'rgba(220, 53, 69, 0.1)',
                fill: false,
                tension: 0.4,
                borderWidth: 2,
                pointRadius: 1
            });
        }
        
        this.charts.velocity = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '🚀 Velocity Analysis - Components and Total Speed'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: { display: true, text: '⏱️ Time (seconds)' }
                    },
                    y: {
                        title: { display: true, text: '🚀 Velocity (m/s)' }
                    }
                }
            }
        });
    },
    
    /**
     * Create altitude chart (Altitude vs Time)
     */
    createAltitudeChart(data) {
        this.destroyChart('altitude');
        
        const ctx = document.getElementById('altitude-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        const altitudeIdx = this.getColumnIndex(data.headers, 'position_z', 5);
        const vzIdx = this.getColumnIndex(data.headers, 'velocity_z', 8);
        
        // Altitude (Position Z)
        if (altitudeIdx >= 0) {
            datasets.push({
                label: '🏔️ Altitude (m)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[altitudeIdx]})),
                borderColor: 'rgba(40, 167, 69, 1)',
                backgroundColor: 'rgba(40, 167, 69, 0.2)',
                fill: true,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Vertical velocity
        if (vzIdx >= 0) {
            datasets.push({
                label: '📈 Vertical Velocity (m/s)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[vzIdx]})),
                borderColor: 'rgba(23, 162, 184, 1)',
                backgroundColor: 'rgba(23, 162, 184, 0.1)',
                fill: false,
                tension: 0.4,
                yAxisID: 'y1',
                pointRadius: 1
            });
        }
        
        this.charts.altitude = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '🏔️ Altitude Profile and Vertical Velocity'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: { display: true, text: '⏱️ Time (seconds)' }
                    },
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: { display: true, text: '🏔️ Altitude (meters)' }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: { display: true, text: '📈 Vertical Velocity (m/s)' },
                        grid: { drawOnChartArea: false }
                    }
                }
            }
        });
    },
    
    /**
     * Create speed chart (Total, Horizontal, Vertical speed)
     */
    createSpeedChart(data) {
        this.destroyChart('speed');
        
        const ctx = document.getElementById('speed-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        const vxIdx = this.getColumnIndex(data.headers, 'velocity_x', 6);
        const vyIdx = this.getColumnIndex(data.headers, 'velocity_y', 7);
        const vzIdx = this.getColumnIndex(data.headers, 'velocity_z', 8);

        if (vxIdx >= 0 && vyIdx >= 0 && vzIdx >= 0) {
            // Calculate speed metrics
            const totalSpeed = data.rows.map(row => {
                const vx = row[vxIdx] || 0;
                const vy = row[vyIdx] || 0;
                const vz = row[vzIdx] || 0;
                return {x: row[timeIdx], y: Math.sqrt(vx*vx + vy*vy + vz*vz)};
            });
            
            const horizontalSpeed = data.rows.map(row => {
                const vx = row[vxIdx] || 0;
                const vy = row[vyIdx] || 0;
                return {x: row[timeIdx], y: Math.sqrt(vx*vx + vy*vy)};
            });
            
            const verticalSpeed = data.rows.map(row => {
                const vz = row[vzIdx] || 0;
                return {x: row[timeIdx], y: Math.abs(vz)};
            });
            
            datasets.push({
                label: '⚡ Total Speed (m/s)',
                data: totalSpeed,
                borderColor: 'rgba(220, 53, 69, 1)',
                backgroundColor: 'rgba(220, 53, 69, 0.1)',
                fill: false,
                tension: 0.4,
                borderWidth: 2,
                pointRadius: 1
            });
            
            datasets.push({
                label: '🌍 Horizontal Speed (m/s)',
                data: horizontalSpeed,
                borderColor: 'rgba(102, 126, 234, 1)',
                backgroundColor: 'rgba(102, 126, 234, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
            
            datasets.push({
                label: '📈 Vertical Speed (m/s)',
                data: verticalSpeed,
                borderColor: 'rgba(255, 193, 7, 1)',
                backgroundColor: 'rgba(255, 193, 7, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        this.charts.speed = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '⚡ Speed Analysis - Total, Horizontal, and Vertical'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: { display: true, text: '⏱️ Time (seconds)' }
                    },
                    y: {
                        title: { display: true, text: '⚡ Speed (m/s)' }
                    }
                }
            }
        });
    },
    
    /**
     * Create atmosphere chart (Density, Pressure, Temperature)
     */
    createAtmosphereChart(data) {
        this.destroyChart('atmosphere');
        
        const ctx = document.getElementById('atmosphere-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        
        // Find column indices for atmospheric data
        const densityIdx = data.headers.indexOf('atm_density');
        const pressureIdx = data.headers.indexOf('atm_pressure');
        const temperatureIdx = data.headers.indexOf('atm_temperature');
        
        if (densityIdx > 0) {
            datasets.push({
                label: '🌫️ Density (kg/m³)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[densityIdx] || 0})),
                borderColor: 'rgba(54, 162, 235, 1)',
                backgroundColor: 'rgba(54, 162, 235, 0.1)',
                fill: false,
                tension: 0.4,
                yAxisID: 'y',
                pointRadius: 1
            });
        }
        
        if (pressureIdx > 0) {
            datasets.push({
                label: '📊 Pressure (Pa/1000)',
                data: data.rows.map(row => ({x: row[timeIdx], y: (row[pressureIdx] || 0) / 1000})),
                borderColor: 'rgba(255, 99, 132, 1)',
                backgroundColor: 'rgba(255, 99, 132, 0.1)',
                fill: false,
                tension: 0.4,
                yAxisID: 'y1',
                pointRadius: 1
            });
        }
        
        if (temperatureIdx > 0) {
            datasets.push({
                label: '🌡️ Temperature (K)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[temperatureIdx] || 0})),
                borderColor: 'rgba(255, 193, 7, 1)',
                backgroundColor: 'rgba(255, 193, 7, 0.1)',
                fill: false,
                tension: 0.4,
                yAxisID: 'y2',
                pointRadius: 1
            });
        }
        
        this.charts.atmosphere = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '🌫️ Atmospheric Properties - ISA Model Implementation'
                    },
                    legend: {
                        display: true,
                        position: 'top'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: { display: true, text: '⏱️ Time (seconds)' }
                    },
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: { display: true, text: '🌫️ Density (kg/m³)' }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: { display: true, text: '📊 Pressure (kPa)' },
                        grid: { drawOnChartArea: false }
                    },
                    y2: {
                        type: 'linear',
                        display: false,
                        position: 'right',
                        title: { display: true, text: '🌡️ Temperature (K)' }
                    }
                }
            }
        });
    },
    
    /**
     * Create gravity chart (Gravity components and magnitude)
     */
    createGravityChart(data) {
        this.destroyChart('gravity');
        
        const ctx = document.getElementById('gravity-chart');
        if (!ctx) return;
        
        const datasets = [];
        const timeIdx = this.getColumnIndex(data.headers, 'simulation_time', 1);
        
        // Find column indices for gravity data
        const gravityXIdx = data.headers.indexOf('gravity_x');
        const gravityYIdx = data.headers.indexOf('gravity_y');
        const gravityZIdx = data.headers.indexOf('gravity_z');
        
        if (gravityXIdx > 0) {
            datasets.push({
                label: '⚖️ Gravity X (m/s²)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[gravityXIdx] || 0})),
                borderColor: 'rgba(255, 99, 132, 1)',
                backgroundColor: 'rgba(255, 99, 132, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        if (gravityYIdx > 0) {
            datasets.push({
                label: '⚖️ Gravity Y (m/s²)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[gravityYIdx] || 0})),
                borderColor: 'rgba(54, 162, 235, 1)',
                backgroundColor: 'rgba(54, 162, 235, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        if (gravityZIdx > 0) {
            datasets.push({
                label: '⚖️ Gravity Z (m/s²)',
                data: data.rows.map(row => ({x: row[timeIdx], y: row[gravityZIdx] || 0})),
                borderColor: 'rgba(75, 192, 192, 1)',
                backgroundColor: 'rgba(75, 192, 192, 0.1)',
                fill: false,
                tension: 0.4,
                pointRadius: 1
            });
        }
        
        // Calculate gravity magnitude
        if (gravityXIdx > 0 && gravityYIdx > 0 && gravityZIdx > 0) {
            datasets.push({
                label: '🌍 Gravity Magnitude (m/s²)',
                data: data.rows.map(row => {
                    const gx = row[gravityXIdx] || 0;
                    const gy = row[gravityYIdx] || 0;
                    const gz = row[gravityZIdx] || 0;
                    return {x: row[timeIdx], y: Math.sqrt(gx*gx + gy*gy + gz*gz)};
                }),
                borderColor: 'rgba(255, 193, 7, 1)',
                backgroundColor: 'rgba(255, 193, 7, 0.1)',
                fill: false,
                tension: 0.4,
                borderWidth: 3,
                pointRadius: 1
            });
        }
        
        this.charts.gravity = new Chart(ctx, {
            type: 'line',
            data: { datasets },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: '⚖️ Gravity Field - Varies with Altitude (g = GM/r²)'
                    },
                    legend: {
                        display: true,
                        position: 'top'
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: { display: true, text: '⏱️ Time (seconds)' }
                    },
                    y: {
                        title: { display: true, text: '⚖️ Gravity (m/s²)' },
                        suggestedMin: -12,
                        suggestedMax: 0
                    }
                }
            }
        });
    }
};

// Export
window.Charts = Charts;
