/**
 * Results module
 * Handles loading and displaying simulation results with advanced aerospace metrics
 * VERSION: 2.0 - GEOCENTRIC COORDINATES FIX (2025-11-26 00:40)
 */

const Results = {
    version: '2.0-GEOCENTRIC-FIX',
    currentResultData: null,
    
    /**
     * Load available results
     */
    async loadResults() {
        try {
            UI.updateStatus('Loading results...', 'info');
            
            const results = await API.getResults();
            
            if (results.length === 0) {
                this.showNoResults();
                return;
            }
            
            this.displayResultsList(results);
            
            // Auto-load the most recent result
            if (results.length > 0) {
                await this.loadResult(results[0].filename);
            }
            
            UI.updateStatus('Results loaded', 'success');
            
        } catch (error) {
            console.error('Error loading results:', error);
            UI.showError('Failed to load results: ' + error.message);
        }
    },
    
    /**
     * Display "no results" message
     */
    showNoResults() {
        const resultsList = document.getElementById('results-list');
        resultsList.innerHTML = `
            <p class="text-center" style="padding: 40px; color: #6c757d;">
                ❌ No simulation results found.<br>
                Run a simulation first to see results here.
            </p>
        `;
    },
    
    /**
     * Display results list using HTML template
     */
    displayResultsList(results) {
        const resultsList = document.getElementById('results-list');
        const template = document.getElementById('result-item-template');
        
        // Clear existing results
        resultsList.innerHTML = '';
        
        results.forEach(result => {
            const date = new Date(result.modified * 1000).toLocaleString();
            const size = (result.size / 1024).toFixed(1);
            
            // Clone template
            const clone = template.content.cloneNode(true);
            
            // Populate data
            clone.querySelector('.result-filename').textContent = `📊 ${result.filename}`;
            clone.querySelector('.result-date').textContent = date;
            clone.querySelector('.result-size').textContent = size;
            
            // Set up button click handler
            const btn = clone.querySelector('.result-visualize-btn');
            btn.addEventListener('click', () => this.loadResult(result.filename));
            
            resultsList.appendChild(clone);
        });
    },
    
    /**
     * Load a specific result
     */
    async loadResult(filename) {
        try {
            UI.updateStatus('Loading result data...', 'info');
            
            const data = await API.getResultData(filename);
            
            if (data.error) {
                throw new Error(data.error);
            }
            
            this.currentResultData = data;
            
            // Show chart container
            document.getElementById('chart-container').style.display = 'block';
            
            // Update summary statistics with FULL aerospace analysis
            this.updateSummaryStats(data);
            
            // Create charts
            window.Charts.createAllCharts(data);
            
            UI.updateStatus(`Loaded: ${filename}`, 'success');
            
        } catch (error) {
            console.error('Error loading result:', error);
            UI.showError('Failed to load result: ' + error.message);
        }
    },
    
    /**
     * Update summary statistics using data attributes (NO HTML generation!)
     */
    updateSummaryStats(data) {
        if (!data.summary || !data.rows || data.rows.length === 0) {
            return;
        }
        
        const summary = data.summary;
        const duration = summary.duration || 0;
        const totalPoints = summary.total_points || 0;
        const distance = summary.distance_traveled || 0;
        
        const initialPos = summary.initial_position || {x: 0, y: 0, z: 0};
        const finalPos = summary.final_position || {x: 0, y: 0, z: 0};
        const initialVel = summary.initial_velocity || {x: 0, y: 0, z: 0};
        const finalVel = summary.final_velocity || {x: 0, y: 0, z: 0};
        
        // Calculate all metrics
        const metrics = this.calculateMetrics(data, summary, initialPos, finalPos, initialVel, finalVel);
        
        // DEBUG: Mostrar informacion de conversion detallada
        console.log('=== ALTITUDE CONVERSION DEBUG ===');
        console.log('Initial Position:', initialPos);
        console.log('Final Position:', finalPos);
        console.log('Initial Pos Magnitude:', Math.sqrt(initialPos.x**2 + initialPos.y**2 + initialPos.z**2));
        console.log('Final Pos Magnitude:', Math.sqrt(finalPos.x**2 + finalPos.y**2 + finalPos.z**2));
        console.log('Is Geocentric:', metrics.isGeocentric);
        console.log('Initial Alt (converted):', metrics.initialAlt, 'm');
        console.log('Final Alt (converted):', metrics.finalAlt, 'm');
        console.log('Altitude Change:', metrics.altitudeChange, 'm');
        console.log('Duration:', duration, 's');
        console.log('Ascent Rate:', metrics.ascentRate, 'm/s');
        console.log('================================');
        
        // Update all values using data attributes
        this.setStat('duration', duration.toFixed(2));
        this.setStat('totalPoints', totalPoints);
        this.setStat('distance', distance.toFixed(1));
        this.setStat('straightDistance', metrics.straightLineDistance.toFixed(1));
        
        this.setStat('altitudeChange', (metrics.altitudeChange >= 0 ? '+' : '') + metrics.altitudeChange.toFixed(1));
        this.setStatClass('altitudeChange', metrics.altitudeChange >= 0 ? 'positive' : 'negative');
        
        this.setStat('horizontalDistance', metrics.horizontalDistance.toFixed(1));
        this.setStat('maxAltitude', metrics.maxAltitude.toFixed(1));
        this.setStat('flightEfficiency', metrics.flightEfficiency.toFixed(1));
        this.setStatClass('flightEfficiency', 
            metrics.flightEfficiency >= 80 ? 'positive' : 
            metrics.flightEfficiency >= 60 ? 'neutral' : 'negative');
        
        this.setStat('initialSpeed', metrics.initialSpeed.toFixed(1));
        this.setStat('finalSpeed', metrics.finalSpeed.toFixed(1));
        this.setStat('speedChange', (metrics.speedChange >= 0 ? '+' : '') + metrics.speedChange.toFixed(1));
        this.setStatClass('speedChange', metrics.speedChange >= 0 ? 'positive' : 'negative');
        
        this.setStat('avgAcceleration', metrics.avgAcceleration.toFixed(2));
        this.setStatClass('avgAcceleration', Math.abs(metrics.avgAcceleration) > 9.81 ? 'highlight' : '');
        
        this.setStat('avgSpeed', metrics.avgSpeed.toFixed(1));
        this.setStat('ascentRate', (metrics.ascentRate >= 0 ? '+' : '') + metrics.ascentRate.toFixed(1));
        this.setStatClass('ascentRate', metrics.ascentRate >= 0 ? 'positive' : 'negative');
        
        this.setStat('energyChange', (metrics.energyChange >= 0 ? '+' : '') + metrics.energyChange.toFixed(1));
        this.setStatClass('energyChange', metrics.energyChange >= 0 ? 'positive' : 'negative');
        
        this.setStat('hvRatio', metrics.verticalVelocity > 0 ? (metrics.horizontalVelocity/metrics.verticalVelocity).toFixed(2) : 'N/A');
        
        this.setStat('finalVelX', `X: ${finalVel.x.toFixed(1)} m/s`);
        this.setStat('finalVelY', `Y: ${finalVel.y.toFixed(1)} m/s`);
        this.setStat('finalVelHTotal', `Total: ${metrics.horizontalVelocity.toFixed(1)} m/s`);
        this.setStat('finalVelZ', `Z: ${finalVel.z.toFixed(1)} m/s`);
        this.setStat('finalVelZAbs', `|Z|: ${metrics.verticalVelocity.toFixed(1)} m/s`);
        this.setStat('finalVelDirection', finalVel.z >= 0 ? '⬆️ Ascending' : '⬇️ Descending');
        

        this.setStat('initialPosX', `X: ${initialPos.x.toFixed(1)}m`);
        this.setStat('initialPosY', `Y: ${initialPos.y.toFixed(1)}m`);
        this.setStat('initialPosZ', `Altitude: ${metrics.initialAlt.toFixed(1)}m`);
        this.setStat('finalPosX', `X: ${finalPos.x.toFixed(1)}m`);
        this.setStat('finalPosY', `Y: ${finalPos.y.toFixed(1)}m`);
        this.setStat('finalPosZ', `Altitude: ${metrics.finalAlt.toFixed(1)}m`);
        
        this.setStat('maxAltitudeKm', (metrics.maxAltitude/1000).toFixed(1));
        this.setStat('altitudeText', metrics.maxAltitude > 100000 ? 'Space boundary!' : 'Atmospheric');
        this.setStatClass('maxAltitudeKm', metrics.maxAltitude > 100000 ? 'highlight' : '');
        
        this.setStat('maxMach', metrics.maxMach.toFixed(2));
        this.setStat('machText', metrics.maxMach > 1 ? 'Supersonic!' : 'Subsonic');
        this.setStatClass('maxMach', metrics.maxMach > 1 ? 'highlight' : '');
        
        this.setStat('maxGForce', metrics.maxGForce.toFixed(1));
        this.setStat('gForceText', metrics.maxGForce > 6 ? 'Extreme!' : metrics.maxGForce > 3 ? 'High stress' : 'Normal');
        this.setStatClass('maxGForce', metrics.maxGForce > 3 ? 'warning' : '');
        
        this.setStat('orbitalVelocity', metrics.orbitalVelocityPercent.toFixed(1));
        this.setStat('orbitalText', metrics.orbitalVelocityPercent > 90 ? 'Orbital!' : 'Sub-orbital');
        this.setStatClass('orbitalVelocity', metrics.orbitalVelocityPercent > 80 ? 'positive' : '');
        
        // Use real altitude, not position_z
        const flightType = this.getFlightType(metrics.finalAlt, finalVel);
        const flightPhase = this.getFlightPhase(metrics.finalAlt);
        const missionSuccess = this.isMissionSuccessful(metrics.finalAlt, finalVel);
        const realismScore = this.calculateRealismScore(data, metrics.isGeocentric);
        
        this.setStat('flightType', flightType);
        this.setStat('flightPhase', flightPhase);
        this.setStat('missionSuccess', missionSuccess ? 'SUCCESS' : 'PARTIAL');
        this.setStat('successIcon', missionSuccess ? '✅' : '⚠️');
        this.setStatClass('missionSuccess', missionSuccess ? 'positive' : 'negative');
        
        this.setStat('realismScore', realismScore.toFixed(0));
        this.setStat('realismText', realismScore > 80 ? 'Highly realistic' : realismScore > 60 ? 'Moderate' : 'Check physics');
        this.setStatClass('realismScore', realismScore > 80 ? 'positive' : realismScore > 60 ? 'warning' : 'negative');
        
        // Update performance alerts
        this.updatePerformanceAlerts({
            flightEfficiency: metrics.flightEfficiency,
            energyChange: metrics.energyChange,
            avgAcceleration: metrics.avgAcceleration,
            ascentRate: metrics.ascentRate,
            finalSpeed: metrics.finalSpeed,
            altitudeChange: metrics.altitudeChange,
            duration
        });
    },
    
    /**
     * Detect if coordinates are geocentric (ECEF) or local
     * FIXED: Use proper threshold and prefer INITIAL position for detection
     */
    isGeocentricCoordinates(initialPos, finalPos) {
        const EARTH_RADIUS = 6378137.0; // meters (WGS84)
        // FIX: Threshold based on Earth radius
        // In geocentric coordinates, magnitude is ~6.4M meters
        // In local coordinates, magnitude is typically < 500,000 meters
        const GEOCENTRIC_THRESHOLD = 1000000; // 1M meters - conservative threshold
        
        // Validate that required properties are available
        if (!initialPos || !finalPos) return false;
        
        // PRIORITY: Detect based on INITIAL position
        // (more reliable than mixing initial + final)
        const initialMag = Math.sqrt((initialPos.x||0)**2 + (initialPos.y||0)**2 + (initialPos.z||0)**2);
        
        // Rule 1: If initial magnitude > 1M meters, likely geocentric
        if (initialMag > GEOCENTRIC_THRESHOLD) {
            console.log('[GEOCENTRIC DETECTED] initial magnitude =', initialMag.toFixed(1), 'meters (threshold:', GEOCENTRIC_THRESHOLD.toFixed(1), ')');
            return true;
        }
        
        // Rule 2: If near Earth radius (±500km), definitely geocentric
        const distanceFromEarthSurface = Math.abs(initialMag - EARTH_RADIUS);
        if (distanceFromEarthSurface < 500000 && initialMag > 5500000) {
            // Between 5.9M - 6.9M meters => geocentric
            console.log('[GEOCENTRIC DETECTED] near Earth surface, initial mag =', initialMag.toFixed(1), 'meters, distance from surface =', distanceFromEarthSurface.toFixed(1), 'm');
            return true;
        }
        
        // Rule 3: Additional validation with final position (only if initial is ambiguous)
        const finalMag = Math.sqrt((finalPos.x||0)**2 + (finalPos.y||0)**2 + (finalPos.z||0)**2);
        if (initialMag < 100000 && finalMag > GEOCENTRIC_THRESHOLD) {
            // Rare case: initial seems local but final seems geocentric
            // This indicates data issues - use direct Z as fallback
            console.warn('[MIXED COORDINATES WARNING] initial mag =', initialMag.toFixed(1), ', final mag =', finalMag.toFixed(1));
            console.warn('[ASSUMING LOCAL] Will use Z coordinate directly');
            return false; // Force local interpretation to avoid errors
        }
        
        console.log('[LOCAL COORDINATES DETECTED] initial magnitude =', initialMag.toFixed(1), 'meters');
        return false;
    },
    
    /**
     * Calculate all metrics from data
     */
    calculateMetrics(data, summary, initialPos, finalPos, initialVel, finalVel) {
        const duration = summary.duration || 0;
        const distance = summary.distance_traveled || 0;
        
        // FIX: Detect whether we are in geocentric coordinates
        const EARTH_RADIUS = 6378137.0; // meters (WGS84)
        const isGeocentric = this.isGeocentricCoordinates(initialPos, finalPos);
        
        // Convert positions to real altitudes
        let initialAlt, finalAlt;
        if (isGeocentric) {
            // Geocentric coordinates: compute distance to center and subtract radius
            const initialR = Math.sqrt(initialPos.x**2 + initialPos.y**2 + initialPos.z**2);
            const finalR = Math.sqrt(finalPos.x**2 + finalPos.y**2 + finalPos.z**2);
            initialAlt = initialR - EARTH_RADIUS;
            finalAlt = finalR - EARTH_RADIUS;
        } else {
                // Local coordinates: Z is directly the altitude
            initialAlt = initialPos.z;
            finalAlt = finalPos.z;
        }
        
        const initialSpeed = Math.sqrt(initialVel.x**2 + initialVel.y**2 + initialVel.z**2);
        const finalSpeed = Math.sqrt(finalVel.x**2 + finalVel.y**2 + finalVel.z**2);
        const speedChange = finalSpeed - initialSpeed;
        const altitudeChange = finalAlt - initialAlt;
        const horizontalDistance = Math.sqrt((finalPos.x - initialPos.x)**2 + (finalPos.y - initialPos.y)**2);
        
        const avgAcceleration = duration > 0 ? speedChange / duration : 0;
        
        // Calculate maximum altitude correcting for coordinate system
        const maxAltitude = Math.max(...data.rows.map(row => {
            const posX = row[3] || 0;
            const posY = row[4] || 0;
            const posZ = row[5] || 0;
            
            if (isGeocentric) {
                const r = Math.sqrt(posX**2 + posY**2 + posZ**2);
                return r - EARTH_RADIUS;
            } else {
                // Local coordinates: Z is directly altitude
                return posZ;
            }
        }));
        
        // VALIDATION: If maxAltitude is absurdly negative, it likely
        // coordinate detection failed. Use direct Z values.
        const maxAltitudeValidated = (maxAltitude < -1000000) 
            ? Math.max(...data.rows.map(row => row[5] || 0))  // Fallback: use direct Z
            : maxAltitude;
        
        const avgSpeed = distance > 0 && duration > 0 ? distance / duration : 0;
        
        const initialKineticEnergy = 0.5 * initialSpeed**2;
        const finalKineticEnergy = 0.5 * finalSpeed**2;
        const energyChange = finalKineticEnergy - initialKineticEnergy;
        
        const straightLineDistance = Math.sqrt(
            (finalPos.x - initialPos.x)**2 + 
            (finalPos.y - initialPos.y)**2 + 
            (finalPos.z - initialPos.z)**2
        );
        const flightEfficiency = straightLineDistance > 0 ? (straightLineDistance / distance * 100) : 0;
        
        const ascentRate = duration > 0 ? altitudeChange / duration : 0;
        const horizontalVelocity = Math.sqrt(finalVel.x**2 + finalVel.y**2);
        const verticalVelocity = Math.abs(finalVel.z);
        
        const maxMach = Math.max(...data.rows.map(row => {
            const vx = row[6] || 0;
            const vy = row[7] || 0;
            const vz = row[8] || 0;
            const speed = Math.sqrt(vx**2 + vy**2 + vz**2);
            return speed / 343.0;
        }));
        
        // Calculate G-force by deriving acceleration from velocity changes
        let maxGForce = 0;
        if (data.rows.length > 1) {
            for (let i = 1; i < data.rows.length; i++) {
                const vx1 = data.rows[i-1][6] || 0;
                const vy1 = data.rows[i-1][7] || 0;
                const vz1 = data.rows[i-1][8] || 0;
                const vx2 = data.rows[i][6] || 0;
                const vy2 = data.rows[i][7] || 0;
                const vz2 = data.rows[i][8] || 0;
                const t1 = data.rows[i-1][1] || 0;
                const t2 = data.rows[i][1] || 0;
                const dt = t2 - t1;
                
                if (dt > 0) {
                    const ax = (vx2 - vx1) / dt;
                    const ay = (vy2 - vy1) / dt;
                    const az = (vz2 - vz1) / dt;
                    const accelMag = Math.sqrt(ax**2 + ay**2 + az**2);
                    const gForce = accelMag / 9.81;
                    if (gForce > maxGForce) maxGForce = gForce;
                }
            }
        }
        
        const orbitalVelocityPercent = (finalSpeed / 7840) * 100;
        
        return {
            initialSpeed, finalSpeed, speedChange, altitudeChange, horizontalDistance,
            avgAcceleration, maxAltitude: maxAltitudeValidated, avgSpeed, energyChange, straightLineDistance,
            flightEfficiency, ascentRate, horizontalVelocity, verticalVelocity,
            maxMach, maxGForce, orbitalVelocityPercent,
            initialAlt, finalAlt, isGeocentric  // Include real altitudes
        };
    },
    
    /**
     * Set a stat value using data-stat attribute
     */
    setStat(name, value) {
        const element = document.querySelector(`[data-stat="${name}"]`);
        if (element) {
            element.textContent = value;
        }
    },
    
    /**
     * Add/remove CSS class from stat element
     */
    setStatClass(name, className) {
        const element = document.querySelector(`[data-stat="${name}"]`);
        if (element) {
            element.className = element.className.replace(/positive|negative|warning|neutral|highlight/g, '').trim();
            if (className) {
                element.classList.add('value', className);
            }
        }
    },
    
    /**
     * Update performance alerts using template
     */
    updatePerformanceAlerts(metrics) {
        const alertsContainer = document.getElementById('performance-alerts');
        const template = document.getElementById('performance-alert-template');
        const alerts = this.analyzePerformance(metrics);
        
        alertsContainer.innerHTML = '';
        
        alerts.forEach(alert => {
            const clone = template.content.cloneNode(true);
            clone.querySelector('.performance-alert').classList.add(alert.type);
            clone.querySelector('.alert-title').textContent = alert.title;
            clone.querySelector('.alert-message').textContent = alert.message;
            alertsContainer.appendChild(clone);
        });
    },
    
    /**
     * Analyze performance and generate alerts (returns array of objects, no HTML!)
     */
    analyzePerformance(metrics) {
        const alerts = [];
        
        if (metrics.flightEfficiency < 50) {
            alerts.push({
                type: 'error',
                title: '⚠️ Low Flight Efficiency',
                message: `Flight efficiency is ${metrics.flightEfficiency.toFixed(1)}%. Consider optimizing trajectory.`
            });
        } else if (metrics.flightEfficiency > 90) {
            alerts.push({
                type: 'success',
                title: '✅ Excellent Flight Efficiency',
                message: `Outstanding efficiency of ${metrics.flightEfficiency.toFixed(1)}%.`
            });
        }
        
        if (metrics.energyChange > 1000) {
            alerts.push({
                type: 'warning',
                title: '🔥 High Energy Gain',
                message: `Energy increase of ${metrics.energyChange.toFixed(1)} J/kg. Check propulsion.`
            });
        } else if (metrics.energyChange < -1000) {
            alerts.push({
                type: 'warning',
                title: '🛑 High Energy Loss',
                message: `Energy loss of ${Math.abs(metrics.energyChange).toFixed(1)} J/kg. Check drag.`
            });
        }
        
        if (Math.abs(metrics.avgAcceleration) > 20) {
            alerts.push({
                type: 'error',
                title: '🚨 Extreme Acceleration',
                message: `Acceleration of ${metrics.avgAcceleration.toFixed(2)} m/s² exceeds safe limits.`
            });
        }
        
        if (metrics.ascentRate > 50) {
            alerts.push({
                type: 'warning',
                title: '📈 Rapid Ascent',
                message: `High ascent rate of ${metrics.ascentRate.toFixed(1)} m/s. Monitor structural loads.`
            });
        } else if (metrics.ascentRate < -50) {
            alerts.push({
                type: 'warning',
                title: '📉 Rapid Descent',
                message: `High descent rate of ${Math.abs(metrics.ascentRate).toFixed(1)} m/s. Check landing systems.`
            });
        }
        
        if (metrics.finalSpeed > 200) {
            alerts.push({
                type: 'warning',
                title: '🚀 High Terminal Speed',
                message: `Final speed of ${metrics.finalSpeed.toFixed(1)} m/s is very high.`
            });
        }
        
        if (metrics.duration && metrics.duration < 1) {
            alerts.push({
                type: 'warning',
                title: '⏱️ Short Mission Duration',
                message: `Mission duration of ${metrics.duration.toFixed(2)}s is very short.`
            });
        }
        
        // If no issues, add success alert
        if (alerts.length === 0) {
            alerts.push({
                type: 'success',
                title: '✅ All Systems Nominal',
                message: 'Mission parameters within acceptable ranges. No issues detected.'
            });
        }
        
        return alerts;
    },
    
    /**
     * Determine flight type
     */
    getFlightType(altitude, finalVel) {
        const speed = Math.sqrt(finalVel.x**2 + finalVel.y**2 + finalVel.z**2);
        
        if (altitude > 100000 && speed > 7000) return 'Orbital 🛰️';
        if (altitude > 100000) return 'Suborbital 🌌';
        if (finalVel.z > 0) return 'Ascending 🚀';
        if (finalVel.z < -50) return 'Landing 🛬';
        return 'Atmospheric 🌍';
    },
    
    /**
     * Determine flight phase
     */
    getFlightPhase(altitude) {
        if (altitude > 400000) return 'Deep Space 🌌';
        if (altitude > 100000) return 'Space ✨';
        if (altitude > 50000) return 'Upper Atmosphere ☁️';
        if (altitude > 10000) return 'Stratosphere 🌤️';
        return 'Ground Level 🌍';
    },
    
    /**
     * Check mission success
     * FIXED: Better criteria for different mission types
     */
    isMissionSuccessful(altitude, finalVel) {
        const speed = Math.sqrt(finalVel.x**2 + finalVel.y**2 + finalVel.z**2);
        const verticalSpeed = Math.abs(finalVel.z);
        
        // Criterion 1: Reached space boundary (Karman line)
        if (altitude > 100000) {
            console.log('[MISSION SUCCESS] Reached space boundary (>100km)');
            return true;
        }
        
        // Criterion 2: High orbital/suborbital speed
        if (speed > 1000) {
            console.log('[MISSION SUCCESS] High velocity achieved (>1000 m/s)');
            return true;
        }
        
        // Criterion 3: Safe landing (near ground, low vertical speed)
        if (altitude > 0 && altitude < 100 && verticalSpeed < 50) {
            console.log('[MISSION SUCCESS] Safe landing (altitude:', altitude.toFixed(1), 'm, v_z:', verticalSpeed.toFixed(1), 'm/s)');
            return true;
        }
        
        // Criterion 4: Stable mid-altitude flight
        if (altitude > 100 && altitude < 50000 && verticalSpeed < 100) {
            console.log('[MISSION SUCCESS] Stable flight at altitude:', altitude.toFixed(1), 'm');
            return true;
        }
        
        // If no criterion is met, mission is partial
        console.log('[MISSION PARTIAL] altitude:', altitude.toFixed(1), 'm, speed:', speed.toFixed(1), 'm/s, v_z:', verticalSpeed.toFixed(1), 'm/s');
        return false;
    },
    
    /**
     * Calculate realism score
     */
    calculateRealismScore(data, isGeocentric = false) {
        let score = 100;
        
        if (data.rows.length < 10) score -= 20;
        
        const speeds = data.rows.map(row => {
            const vx = row[6] || 0;
            const vy = row[7] || 0;
            const vz = row[8] || 0;
            return Math.sqrt(vx**2 + vy**2 + vz**2);
        });
        
        const maxSpeed = Math.max(...speeds);
        if (maxSpeed > 10000) score -= 10;
        if (maxSpeed < 0.1) score -= 20;
        
        // Convert altitudes correctly
        const EARTH_RADIUS = 6378137.0;
        const altitudes = data.rows.map(row => {
            if (isGeocentric) {
                const posX = row[3] || 0;
                const posY = row[4] || 0;
                const posZ = row[5] || 0;
                const r = Math.sqrt(posX**2 + posY**2 + posZ**2);
                return r - EARTH_RADIUS;
            } else {
                return row[5] || 0;
            }
        });
        
        const negativeAltitudes = altitudes.filter(a => a < 0).length;
        if (negativeAltitudes > altitudes.length * 0.1) score -= 30;
        
        return Math.max(0, Math.min(100, score));
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

// Export
window.Results = Results;
