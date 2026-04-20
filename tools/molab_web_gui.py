#!/usr/bin/env python3
"""
MoLab Web GUI - Local Web Interface
A web-based GUI that runs locally and works in any browser
"""

import http.server
import socketserver
import json
import subprocess
import threading
import os
import sys
import webbrowser
from pathlib import Path
import datetime
import urllib.parse
import time
import platform

class MoLabWebHandler(http.server.SimpleHTTPRequestHandler):
    # Class-level simulation status (shared across all request instances)
    _sim_lock = threading.Lock()
    _sim_status = {"state": "idle"}  # idle | running | completed | failed

    def __init__(self, *args, **kwargs):
        self.project_root = Path(__file__).parent.parent
        self.build_dir = self.project_root / "build"
        self.config_dir = self.project_root / "data" / "config"
        self.output_dir = self.build_dir / "output"
        # Create config directory if it doesn't exist
        self.config_dir.mkdir(parents=True, exist_ok=True)
        super().__init__(*args, **kwargs)

    def do_GET(self):
        """Handle GET requests"""
        # Parse path without query parameters
        from urllib.parse import urlparse
        parsed_path = urlparse(self.path).path
        
        if parsed_path == "/" or parsed_path == "/index.html":
            self.serve_main_page()
        elif parsed_path == "/api/config":
            self.serve_config()
        elif parsed_path == "/api/plugins":
            self.serve_plugins()
        elif parsed_path == "/api/status":
            self.serve_status()
        elif parsed_path == "/api/results":
            self.serve_results()
        elif parsed_path.startswith("/api/results/"):
            # Extract filename from path
            filename = parsed_path.split("/")[-1]
            self.serve_result_data(filename)
        elif parsed_path.startswith("/api/results"):
            self.serve_results()
        elif parsed_path == "/api/plots":
            self.serve_plots_list()
        elif parsed_path.startswith("/api/plots/"):
            filename = parsed_path.split("/")[-1]
            self.serve_plot_file(filename)
        else:
            self.send_error(404)
    
    def do_POST(self):
        """Handle POST requests"""
        if self.path == "/api/run":
            self.handle_run_simulation()
        elif self.path == "/api/save":
            self.handle_save_config()
        elif self.path == "/api/load":
            self.handle_load_config()
        else:
            self.send_error(404)
    
    def serve_main_page(self):
        """Serve the main HTML page"""
        html = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MoLab Aerospace Simulator</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: #333;
        }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .header { text-align: center; color: white; margin-bottom: 30px; }
        .header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
        .main-content {
            background: white; border-radius: 15px; padding: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        .tabs { display: flex; margin-bottom: 30px; border-bottom: 2px solid #eee; }
        .tab {
            padding: 15px 25px; cursor: pointer; border: none; background: none;
            font-size: 16px; font-weight: 500; color: #666; transition: all 0.3s;
        }
        .tab.active { color: #667eea; border-bottom: 3px solid #667eea; }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .form-group { margin-bottom: 20px; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: 500; color: #333; }
        .form-group input, .form-group select {
            width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 8px;
            font-size: 14px; transition: border-color 0.3s;
        }
        .form-group input:focus, .form-group select:focus { outline: none; border-color: #667eea; }
        .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .btn {
            padding: 12px 24px; border: none; border-radius: 8px; font-size: 16px;
            font-weight: 500; cursor: pointer; transition: all 0.3s; margin-right: 10px; margin-bottom: 10px;
        }
        .btn-primary { background: #667eea; color: white; }
        .btn-primary:hover { background: #5a6fd8; transform: translateY(-2px); }
        .btn-success { background: #28a745; color: white; }
        .btn-info { background: #17a2b8; color: white; }
        .status {
            padding: 15px; border-radius: 8px; margin-bottom: 20px; font-weight: 500;
        }
        .status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .status.info { background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }
        .plugin-card {
            padding: 15px; border: 2px solid #eee; border-radius: 8px; margin-bottom: 10px;
            transition: border-color 0.3s; cursor: pointer;
        }
        .plugin-card.enabled { border-color: #28a745; background: #f8fff9; }
        .plugin-header {
            display: flex; justify-content: space-between; align-items: center;
        }
        .plugin-description {
            font-size: 14px; color: #666; margin-bottom: 5px;
        }
        .plugin-type {
            font-size: 12px; color: #999; margin-bottom: 5px;
        }
        .plugin-parameters {
            padding: 15px; border: 1px solid #ddd; border-radius: 8px; margin-top: 10px;
        }
        .param-group {
            margin-bottom: 15px;
        }
        .param-group label {
            display: block; margin-bottom: 5px; font-weight: 500; color: #333;
        }
        .param-group input[type="number"], .param-group input[type="text"] {
            width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 8px;
            font-size: 14px; transition: border-color 0.3s;
        }
        .param-group input[type="number"]:focus, .param-group input[type="text"]:focus { outline: none; border-color: #667eea; }
        .control-panel {
            background: #f8f9fa; padding: 20px; border-radius: 8px; margin-top: 30px; text-align: center;
        }
        .results-list {
            max-height: 300px; overflow-y: auto; border: 1px solid #ddd; border-radius: 8px; padding: 15px;
        }
        .result-item {
            padding: 15px; border: 1px solid #eee; border-radius: 8px; margin-bottom: 10px;
            background: #f8f9fa; transition: all 0.3s;
        }
        .result-item:hover { background: #e9ecef; transform: translateY(-2px); }
        .result-item h4 { color: #333; margin-bottom: 5px; }
        .result-item p { color: #666; margin-bottom: 10px; font-size: 14px; }
        .chart-container {
            background: white; border-radius: 8px; padding: 20px; margin-top: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .chart-wrapper { position: relative; width: 100%; height: 400px; }
        .chart-canvas { width: 100% !important; height: 100% !important; }
        .stats-container {
            display: flex;
            flex-direction: column;
            gap: 30px;
            margin-top: 20px;
        }
        .stats-row {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
        }
        .stats-row.primary {
            grid-template-columns: repeat(3, 1fr);
        }
        .stats-row.secondary {
            grid-template-columns: repeat(2, 1fr);
        }
        .stat-section {
            background: linear-gradient(135deg, #ffffff 0%, #f8f9fa 100%);
            border: 1px solid #e9ecef;
            border-radius: 16px;
            padding: 24px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.08);
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }
        .stat-section::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 4px;
            background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
        }
        .stat-section:hover {
            transform: translateY(-4px);
            box-shadow: 0 8px 25px rgba(0,0,0,0.12);
        }
        .stat-section h3 {
            margin: 0 0 20px 0;
            color: #343a40;
            font-size: 1.3em;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .stat-cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
            gap: 15px;
        }
        .stat-card {
            background: rgba(255,255,255,0.8);
            border: 1px solid #dee2e6;
            border-radius: 12px;
            padding: 16px;
            text-align: center;
            transition: all 0.3s ease;
            backdrop-filter: blur(10px);
        }
        .stat-card h4 { 
            color: #495057; 
            margin-bottom: 8px; 
            font-size: 0.9em;
        }
        .stat-card .value { 
            font-size: 1.4em; 
            font-weight: bold; 
            color: #667eea; 
            margin-bottom: 4px;
        }
        .stat-card .unit { 
            color: #6c757d; 
            font-size: 0.8em; 
        }
        .stat-card .subtext {
            color: #6c757d;
            font-size: 0.7em;
            margin-top: 4px;
        }
        .stat-card.multi-value {
            background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
        }
        .stat-card.multi-value .value {
            font-size: 1.0em;
            margin: 2px 0;
        }
        .value.positive {
            color: #28a745 !important;
        }
        .value.negative {
            color: #dc3545 !important;
        }
        .value.neutral {
            color: #ffc107 !important;
        }
        .value.warning {
            color: #fd7e14 !important;
        }
        .stat-card.highlight {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
        }
        .stat-card.highlight h4 {
            color: #ffffff !important;
        }
        .stat-card.highlight .value {
            color: #ffffff !important;
            text-shadow: 0 1px 2px rgba(0,0,0,0.3);
        }
        .stat-card.highlight .unit {
            color: #e8e8e8 !important;
        }
        .stat-card.highlight:hover {
            transform: scale(1.05);
            box-shadow: 0 6px 20px rgba(102, 126, 234, 0.6);
        }
        .stat-card:hover {
            transform: scale(1.05);
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.2);
        }
        /* Responsive design */
        @media (max-width: 1200px) {
            .stats-row.primary {
                grid-template-columns: repeat(2, 1fr);
            }
        }
        @media (max-width: 768px) {
            .stats-row.primary,
            .stats-row.secondary {
                grid-template-columns: 1fr;
            }
            .stat-cards {
                grid-template-columns: repeat(2, 1fr);
            }
        }
        /* Chart Navigation Styles */
        .chart-tabs {
            display: flex;
            margin-bottom: 20px;
            border-bottom: 2px solid #eee;
        }
        .chart-tab {
            padding: 15px 25px; cursor: pointer; border: none; background: none;
            font-size: 16px; font-weight: 500; color: #666; transition: all 0.3s;
        }
        .chart-tab.active { color: #667eea; border-bottom: 3px solid #667eea; }
        .chart-panel { display: none; }
        .chart-panel.active { display: block; }
        
        /* Results Controls and Comparison Styles */
        .results-controls {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }
        .comparison-panel {
            background: #f8f9fa;
            border: 2px solid #667eea;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .comparison-panel h4 {
            color: #667eea;
            margin-bottom: 15px;
        }
        .selected-results {
            background: white;
            border: 1px solid #dee2e6;
            border-radius: 8px;
            padding: 15px;
            margin: 15px 0;
            min-height: 60px;
        }
        .selected-results p {
            background: #667eea;
            color: white;
            padding: 8px 12px;
            border-radius: 6px;
            margin: 5px 0;
            display: inline-block;
        }
        .result-item {
            position: relative;
        }
        .result-item.selected {
            border: 2px solid #667eea;
            background: #f0f4ff;
        }
        
        /* Performance Alerts */
        .performance-alerts-section {
            margin-bottom: 30px;
        }
        .performance-alert {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);
            color: white;
            padding: 15px;
            border-radius: 8px;
            margin: 10px 0;
            animation: pulse 2s infinite;
            max-width: 300px;
        }
        .performance-alert.warning {
            background: linear-gradient(135deg, #feca57 0%, #ff9ff3 100%);
        }
        .performance-alert.success {
            background: linear-gradient(135deg, #48dbfb 0%, #0abde3 100%);
        }
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.8; }
            100% { opacity: 1; }
        }
        
        /* Mission Templates Styles */
        .mission-templates {
            background: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 25px;
        }
        .mission-templates h4 {
            color: #495057;
            margin-bottom: 15px;
            text-align: center;
        }
        .template-cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .template-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
            text-align: center;
            border: none;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.3);
        }
        .template-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 8px 25px rgba(102, 126, 234, 0.5);
        }
        .template-card h5 {
            margin: 0 0 10px 0;
            font-size: 1.1em;
            font-weight: 600;
        }
        .template-card p {
            margin: 0;
            font-size: 0.9em;
            opacity: 0.9;
        }
        
        /* Progress Section Styles */
        .progress-section {
            margin-top: 20px;
            padding: 20px;
            border: 1px solid #ddd;
            border-radius: 8px;
            background: #f8f9fa;
        }
        .progress-bar {
            width: 100%;
            height: 10px;
            background: #ddd;
            border-radius: 8px;
            overflow: hidden;
        }
        .progress-fill {
            width: 0%;
            height: 100%;
            background: #667eea;
            transition: width 0.3s;
        }
        .progress-text {
            font-size: 14px;
            font-weight: 500;
            color: #333;
            margin-top: 10px;
        }
        .live-stats {
            margin-top: 20px;
        }
        .stat-item {
            margin-bottom: 10px;
        }
        .stat-label {
            font-weight: 500;
            color: #333;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1> MoLab</h1>
            <p>Web Interface for Realistic Aerospace Simulations</p>
        </div>
        
        <div class="main-content">
            <div id="status" class="status info">
                 Loading system status... 
                <button onclick="forceRefresh()" class="btn btn-secondary" style="margin-left: 10px;"> Force Refresh</button>
            </div>
            
            <div class="tabs">
                <button class="tab active" onclick="showTab('simulation', event)"> Simulation</button>
                <button class="tab" onclick="showTab('physics', event)"> Physics</button>
                <button class="tab" onclick="showTab('plugins', event)"> Plugins</button>
                <button class="tab" onclick="showTab('output', event)"> Output</button>
                <button class="tab" onclick="showTab('results', event)"> Results</button>
                <button class="tab" onclick="showTab('earth', event)"> Earth View</button>
                <button class="tab" onclick="showTab('analysis', event)"> Physics Analysis</button>
            </div>
            
            <div id="simulation-tab" class="tab-content active">
                <h3> Simulation Configuration</h3>
                
                <!-- Mission Templates -->
                <div class="mission-templates">
                    <h4> Mission Templates</h4>
                    <div class="template-cards">
                        <div class="template-card" onclick="loadTemplate('suborbital')">
                            <h5> Suborbital Flight</h5>
                            <p>High altitude trajectory with parabolic arc</p>
                        </div>
                        <div class="template-card" onclick="loadTemplate('atmospheric')">
                            <h5> Atmospheric Test</h5>
                            <p>Low altitude flight with atmospheric effects</p>
                        </div>
                        <div class="template-card" onclick="loadTemplate('landing')">
                            <h5> Landing Simulation</h5>
                            <p>Controlled descent and landing sequence</p>
                        </div>
                        <div class="template-card" onclick="loadTemplate('orbital')">
                            <h5> Orbital Insertion</h5>
                            <p>Multi-stage ascent to orbital velocity</p>
                        </div>
                    </div>
                </div>
                
                <div class="form-row">
                    <div class="form-group">
                        <label for="time-step">Time Step (seconds)</label>
                        <input type="number" id="time-step" min="0.001" max="1.0" step="0.001" value="0.1">
                    </div>
                    <div class="form-group">
                        <label for="duration">Duration (seconds)</label>
                        <input type="number" id="duration" min="1" max="1000" value="10">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="log-level">Log Level</label>
                        <select id="log-level">
                            <option value="INFO" selected>INFO</option>
                            <option value="DEBUG">DEBUG</option>
                            <option value="WARNING">WARNING</option>
                            <option value="ERROR">ERROR</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="enable-logging" checked> Enable Logging</label>
                    </div>
                </div>
            </div>
            
            <div id="physics-tab" class="tab-content">
                <h3> Physics Configuration</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label><input type="checkbox" id="enable-gravity" checked> Enable Gravity</label>
                    </div>
                    <div class="form-group">
                        <label for="gravity-magnitude">Gravity (m/s²)</label>
                        <input type="number" id="gravity-magnitude" min="0" max="50" step="0.01" value="9.81">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="integrator-type">Integrator</label>
                        <select id="integrator-type">
                            <option value="euler">Euler</option>
                            <option value="runge_kutta_4" selected>Runge-Kutta 4</option>
                            <option value="verlet">Verlet</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="enable-drag" checked> Atmospheric Drag</label>
                    </div>
                </div>
                <h3 style="margin-top:20px;">🛰️ Vehicle</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="vehicle-mass">Dry Mass (kg)</label>
                        <input type="number" id="vehicle-mass" min="1" max="1000000" step="10" value="1000">
                    </div>
                    <div class="form-group">
                        <label for="drag-coefficient">Drag Coefficient (Cd)</label>
                        <input type="number" id="drag-coefficient" min="0" max="5" step="0.01" value="0.3">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="reference-area">Reference Area (m²)</label>
                        <input type="number" id="reference-area" min="0.01" max="1000" step="0.1" value="1.0">
                    </div>
                </div>
                <h3 style="margin-top:20px;"> Propulsion Plugin</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label><input type="checkbox" id="enable-propulsion" checked> Enable Propulsion</label>
                    </div>
                    <div class="form-group">
                        <label for="engine-type">Engine Type</label>
                        <select id="engine-type">
                            <option value="0" selected>Solid Rocket</option>
                            <option value="1">Liquid Rocket</option>
                            <option value="2">Hybrid Rocket</option>
                            <option value="3">Ion Thruster</option>
                        </select>
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="sea-level-thrust">Sea-Level Thrust (N)</label>
                        <input type="number" id="sea-level-thrust" min="0" max="10000000" step="1000" value="50000">
                    </div>
                    <div class="form-group">
                        <label for="vacuum-thrust">Vacuum Thrust (N)</label>
                        <input type="number" id="vacuum-thrust" min="0" max="10000000" step="1000" value="55000">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="isp-sea-level">Isp Sea-Level (s)</label>
                        <input type="number" id="isp-sea-level" min="10" max="5000" step="5" value="250">
                    </div>
                    <div class="form-group">
                        <label for="isp-vacuum">Isp Vacuum (s)</label>
                        <input type="number" id="isp-vacuum" min="10" max="5000" step="5" value="280">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="fuel-mass">Fuel Mass (kg)</label>
                        <input type="number" id="fuel-mass" min="0" max="1000000" step="10" value="500">
                    </div>
                    <div class="form-group">
                        <label for="throttle-setting">Throttle (0–1)</label>
                        <input type="number" id="throttle-setting" min="0" max="1" step="0.05" value="1.0">
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="thrust-vector-angle">Thrust Vector Angle (rad)</label>
                        <input type="number" id="thrust-vector-angle" min="-1.57" max="1.57" step="0.01" value="0">
                    </div>
                    <div class="form-group">
                        <label for="nozzle-exit-area">Nozzle Exit Area (m²)</label>
                        <input type="number" id="nozzle-exit-area" min="0.001" max="100" step="0.01" value="0.1">
                    </div>
                </div>
            </div>
            
            <div id="plugins-tab" class="tab-content">
                <h3>🔌 Plugin Configuration</h3>
                <div id="plugins-list"><p>Loading plugins...</p></div>
            </div>
            
            <div id="output-tab" class="tab-content">
                <h3>📊 Output Configuration</h3>
                <div class="form-group">
                    <label>Output Formats:</label>
                    <div>
                        <label><input type="checkbox" id="enable-csv" checked> CSV Format</label><br>
                        <label><input type="checkbox" id="enable-json"> JSON Format</label>
                    </div>
                </div>
                <div class="form-row">
                    <div class="form-group">
                        <label for="output-interval">Output Interval</label>
                        <input type="number" id="output-interval" min="1" value="1">
                    </div>
                    <div class="form-group">
                        <label for="output-directory">Output Directory</label>
                        <input type="text" id="output-directory" value="output">
                    </div>
                </div>
            </div>
            
            <div id="results-tab" class="tab-content">
                <h3> Simulation Results</h3>
                <div class="results-controls">
                    <button class="btn btn-info" onclick="loadResults()"> Refresh Results</button>
                    <button class="btn btn-secondary" onclick="toggleComparisonMode()"> Compare Mode</button>
                    <button class="btn btn-success" onclick="exportReport()" style="display: none;" id="export-btn"> Export Report</button>
                </div>
                
                <div id="comparison-panel" class="comparison-panel" style="display: none;">
                    <h4> Comparison Mode</h4>
                    <p>Select up to 3 simulations to compare:</p>
                    <div id="selected-results" class="selected-results"></div>
                    <button class="btn btn-primary" onclick="compareSelected()"> Compare Selected</button>
                    <button class="btn btn-secondary" onclick="clearComparison()"> Clear Selection</button>
                </div>
                
                <div id="results-list" class="results-list">
                    <p>Click "Refresh Results" to load available simulation results.</p>
                </div>
                
                <div id="plots-gallery" style="display: none; margin-top: 20px;">
                    <h3 style="margin-bottom: 10px;"> Generated Plots</h3>
                    <div id="plots-grid" style="display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 16px; margin-bottom: 24px;"></div>
                </div>
                
                <div id="chart-container" class="chart-container" style="display: none;">
                    <div id="summary-stats" class="stats-container"></div>
                    
                    <!-- Chart Navigation Tabs -->
                    <div class="chart-tabs">
                        <button class="chart-tab active" onclick="showChart('trajectory')"> Trajectory</button>
                        <button class="chart-tab" onclick="showChart('velocity')"> Velocity</button>
                        <button class="chart-tab" onclick="showChart('altitude')"> Altitude</button>
                        <button class="chart-tab" onclick="showChart('speed')"> Speed</button>
                        <button class="chart-tab" onclick="showChart('earth')"> Atmosphere</button>
                        <button class="chart-tab" onclick="showChart('physics')"> Physics</button>
                        <button class="chart-tab" onclick="showChart('comparison')" id="comparison-tab" style="display: none;"> Comparison</button>
                    </div>
                    
                    <!-- Chart Containers -->
                    <div id="trajectory-chart-container" class="chart-panel active">
                        <h4> 3D Trajectory Analysis</h4>
                        <div class="chart-wrapper"><canvas id="trajectory-chart"></canvas></div>
                    </div>
                    
                    <div id="velocity-chart-container" class="chart-panel">
                        <h4> Velocity Components</h4>
                        <div class="chart-wrapper"><canvas id="velocity-chart"></canvas></div>
                    </div>
                    
                    <div id="altitude-chart-container" class="chart-panel">
                        <h4> Altitude Profile</h4>
                        <div class="chart-wrapper"><canvas id="altitude-chart"></canvas></div>
                    </div>
                    
                    <div id="speed-chart-container" class="chart-panel">
                        <h4> Speed Analysis</h4>
                        <div class="chart-wrapper"><canvas id="speed-chart"></canvas></div>
                    </div>
                    
                    <div id="earth-chart-container" class="chart-panel">
                        <h4> Atmospheric Conditions</h4>
                        <div class="chart-wrapper"><canvas id="earth-chart"></canvas></div>
                    </div>
                    
                    <div id="physics-chart-container" class="chart-panel">
                        <h4> Gravity, Energy & Pressure</h4>
                        <div class="chart-wrapper"><canvas id="physics-chart"></canvas></div>
                    </div>
                    
                    <div id="comparison-chart-container" class="chart-panel">
                        <h4> Multi-Simulation Comparison</h4>
                        <div class="chart-wrapper"><canvas id="comparison-chart"></canvas></div>
                    </div>
                </div>
            </div>
            
            <div id="earth-tab" class="tab-content">
                <h3> Earth View</h3>
                <div id="earth-view"></div>
            </div>
            
            <div id="analysis-tab" class="tab-content">
                <h3> Physics Analysis</h3>
                <div id="physics-analysis"></div>
            </div>
            
            <div class="control-panel">
                <h3>Control Panel</h3>
                
                <!-- Real-time Progress Section -->
                <div id="progress-section" class="progress-section" style="display: none;">
                    <h4> Simulation Progress</h4>
                    <div class="progress-bar">
                        <div id="progress-fill" class="progress-fill"></div>
                    </div>
                    <div id="progress-text" class="progress-text">0% - Initializing...</div>
                    <div id="live-stats" class="live-stats">
                        <div class="stat-item">
                            <span class="stat-label">Tick:</span>
                            <span id="current-tick">0</span>/<span id="total-ticks">0</span>
                        </div>
                        <div class="stat-item">
                            <span class="stat-label">Position:</span>
                            <span id="current-position">X: 0, Y: 0, Z: 0</span>
                        </div>
                        <div class="stat-item">
                            <span class="stat-label">Velocity:</span>
                            <span id="current-velocity">X: 0, Y: 0, Z: 0</span>
                        </div>
                    </div>
                </div>
                
                <div class="control-buttons">
                    <button class="btn btn-info" onclick="saveConfig()"> Save Config</button>
                    <button class="btn btn-primary" onclick="runSimulation()" id="run-btn"> Run Simulation</button>
                    <button class="btn btn-success" onclick="loadResults()"> View Results</button>
                    <button class="btn btn-warning" onclick="stopSimulation()" id="stop-btn" style="display: none;">⏹ Stop Simulation</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let availablePlugins = [];
        let currentChart = null;
        let comparisonMode = false;
        let selectedResults = [];
        
        document.addEventListener('DOMContentLoaded', function() {
            console.log('DOM loaded, initializing...');
            loadStatus();
            loadPlugins();
            
            // Auto-retry status loading if it fails
            setTimeout(() => {
                const statusDiv = document.getElementById('status');
                if (statusDiv.innerHTML.includes('Loading system status')) {
                    console.log('Status still loading after 10s, retrying...');
                    loadStatus();
                }
            }, 10000);
        });
        
        async function loadStatus() {
            const statusDiv = document.getElementById('status');
            console.log('Loading status...');
            try {
                const controller = new AbortController();
                const timeoutId = setTimeout(() => controller.abort(), 5000);
                
                const response = await fetch('/api/status?t=' + Date.now(), { 
                    signal: controller.signal,
                    cache: 'no-cache'
                });
                clearTimeout(timeoutId);
                
                console.log('Status response:', response.status);
                
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }
                
                const status = await response.json();
                console.log('Status data:', status);
                
                if (status.simulator_built) {
                    statusDiv.className = 'status success';
                    statusDiv.innerHTML = ' System ready - Simulator built and available';
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.innerHTML = '❌ Simulator not built - Please build the project first';
                }
            } catch (error) {
                console.error('Status loading error:', error);
                statusDiv.className = 'status error';
                if (error.name === 'AbortError') {
                    statusDiv.innerHTML = '⏱️ Status loading timed out - Server may be starting up. <button onclick="loadStatus()" class="btn btn-info">🔄 Retry</button>';
                } else {
                    statusDiv.innerHTML = `❌ Error loading status: ${error.message} <button onclick="loadStatus()" class="btn btn-info">🔄 Retry</button>`;
                }
            }
        }
        
        function forceRefresh() {
            console.log('Force refresh triggered');
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status info';
            statusDiv.innerHTML = '🔄 Loading system status... <button onclick="forceRefresh()" class="btn btn-secondary" style="margin-left: 10px;">🔄 Force Refresh</button>';
            
            // Clear any caches and reload status
            if ('caches' in window) {
                caches.keys().then(function(names) {
                    names.forEach(function(name) {
                        caches.delete(name);
                    });
                });
            }
            
            setTimeout(() => {
                loadStatus();
                loadPlugins();
            }, 100);
        }
        
        function showTab(tabName, event) {
            document.querySelectorAll('.tab-content').forEach(tab => tab.classList.remove('active'));
            document.querySelectorAll('.tab').forEach(tab => tab.classList.remove('active'));
            document.getElementById(tabName + '-tab').classList.add('active');
            
            // If called programmatically, find the tab button by onclick attribute
            if (event && event.target) {
                event.target.classList.add('active');
            } else {
                // Find the tab button by its onclick attribute (updated to match new syntax)
                const tabButton = document.querySelector(`[onclick*="showTab('${tabName}')"]`);
                if (tabButton) {
                    tabButton.classList.add('active');
                }
            }
        }
        
        async function loadPlugins() {
            try {
                const response = await fetch('/api/plugins');
                availablePlugins = await response.json();
                updatePluginsList();
            } catch (error) {
                console.error('Error loading plugins:', error);
            }
        }
        
        function updatePluginsList() {
            const pluginsList = document.getElementById('plugins-list');
            
            if (availablePlugins.length === 0) {
                pluginsList.innerHTML = '<p>❌ No plugins found. Build the project first.</p>';
                return;
            }
            
            let html = '';
            availablePlugins.forEach((plugin, index) => {
                const enabledClass = plugin.enabled ? 'enabled' : '';
                html += `
                    <div class="plugin-card ${enabledClass}">
                        <div class="plugin-header" onclick="togglePlugin(${index})">
                            <h4>${plugin.name} ${plugin.enabled ? '✅' : '❌'}</h4>
                            <p class="plugin-description">${plugin.description}</p>
                            <span class="plugin-type">Type: ${plugin.type === 0 ? 'Sequential' : 'Parallel Physics'}</span>
                        </div>
                        ${plugin.enabled && plugin.parameters ? generateParametersForm(plugin, index) : ''}
                    </div>
                `;
            });
            pluginsList.innerHTML = html;
        }
        
        function generateParametersForm(plugin, index) {
            if (!plugin.parameters) return '';
            
            let html = '<div class="plugin-parameters">';
            html += '<h5>⚙️ Parameters:</h5>';
            
            Object.entries(plugin.parameters).forEach(([key, value]) => {
                const inputId = `plugin-${index}-${key}`;
                if (typeof value === 'boolean') {
                    html += `
                        <div class="param-group">
                            <label>
                                <input type="checkbox" id="${inputId}" ${value ? 'checked' : ''} 
                                       onchange="updatePluginParameter(${index}, '${key}', this.checked)">
                                ${key.replace(/_/g, ' ').replace(/\\b\\w/g, l => l.toUpperCase())}
                            </label>
                        </div>
                    `;
                } else if (typeof value === 'number') {
                    html += `
                        <div class="param-group">
                            <label for="${inputId}">${key.replace(/_/g, ' ').replace(/\\b\\w/g, l => l.toUpperCase())}</label>
                            <input type="number" id="${inputId}" value="${value}" step="any"
                                   onchange="updatePluginParameter(${index}, '${key}', parseFloat(this.value))">
                        </div>
                    `;
                } else {
                    html += `
                        <div class="param-group">
                            <label for="${inputId}">${key.replace(/_/g, ' ').replace(/\\b\\w/g, l => l.toUpperCase())}</label>
                            <input type="text" id="${inputId}" value="${value}"
                                   onchange="updatePluginParameter(${index}, '${key}', this.value)">
                        </div>
                    `;
                }
            });
            
            html += '</div>';
            return html;
        }
        
        function updatePluginParameter(pluginIndex, paramKey, value) {
            if (!availablePlugins[pluginIndex].parameters) {
                availablePlugins[pluginIndex].parameters = {};
            }
            availablePlugins[pluginIndex].parameters[paramKey] = value;
        }
        
        function togglePlugin(index) {
            availablePlugins[index].enabled = !availablePlugins[index].enabled;
            updatePluginsList();
        }
        
        function getConfigFromForm() {
            return {
                "simulation": {
                    "time_step": parseFloat(document.getElementById('time-step').value),
                    "duration": parseFloat(document.getElementById('duration').value),
                    "max_iterations": 1000,
                    "enable_logging": document.getElementById('enable-logging').checked,
                    "log_level": document.getElementById('log-level').value,
                    "log_file": "logs/molab_web.log"
                },
                "physics": {
                    "enable_gravity": document.getElementById('enable-gravity').checked,
                    "gravity_magnitude": parseFloat(document.getElementById('gravity-magnitude').value),
                    "enable_atmospheric_drag": document.getElementById('enable-drag').checked,
                    "integration_tolerance": 1e-6,
                    "integrator_type": document.getElementById('integrator-type').value,
                    "vehicle_mass": parseFloat(document.getElementById('vehicle-mass').value),
                    "drag_coefficient": parseFloat(document.getElementById('drag-coefficient').value),
                    "reference_area": parseFloat(document.getElementById('reference-area').value)
                },
                "plugins": (function() {
                    const plugins = availablePlugins.filter(p => p.enabled);
                    // Inject propulsion plugin from form controls
                    const enableProp = document.getElementById('enable-propulsion').checked;
                    if (enableProp) {
                        // Remove any existing propulsion plugin entry
                        const filtered = plugins.filter(p => p.name !== 'propulsion');
                        filtered.push({
                            "name": "propulsion",
                            "type": 1,
                            "library_path": "build/lib/propulsion",
                            "enabled": true,
                            "parameters": {
                                "engine_type": parseInt(document.getElementById('engine-type').value),
                                "sea_level_thrust": parseFloat(document.getElementById('sea-level-thrust').value),
                                "vacuum_thrust": parseFloat(document.getElementById('vacuum-thrust').value),
                                "specific_impulse_sl": parseFloat(document.getElementById('isp-sea-level').value),
                                "specific_impulse_vac": parseFloat(document.getElementById('isp-vacuum').value),
                                "initial_fuel_mass": parseFloat(document.getElementById('fuel-mass').value),
                                "throttle_setting": parseFloat(document.getElementById('throttle-setting').value),
                                "thrust_vector_angle": parseFloat(document.getElementById('thrust-vector-angle').value),
                                "nozzle_exit_area": parseFloat(document.getElementById('nozzle-exit-area').value),
                                "engine_on": true,
                                "enable_altitude_compensation": true,
                                "enable_fuel_consumption": true,
                                "enable_thrust_vectoring": parseFloat(document.getElementById('thrust-vector-angle').value) !== 0
                            }
                        });
                        return filtered;
                    }
                    return plugins.filter(p => p.name !== 'propulsion');
                })(),
                "output": {
                    "enable_csv": document.getElementById('enable-csv').checked,
                    "enable_json": document.getElementById('enable-json').checked,
                    "enable_binary": false,
                    "output_interval": parseInt(document.getElementById('output-interval').value),
                    "output_directory": document.getElementById('output-directory').value
                }
            };
        }
        
        async function runSimulation() {
            const config = getConfigFromForm();
            const statusDiv = document.getElementById('status');
            const progressSection = document.getElementById('progress-section');
            const runBtn = document.getElementById('run-btn');
            const stopBtn = document.getElementById('stop-btn');
            
            // Show progress section and update UI
            progressSection.style.display = 'block';
            runBtn.style.display = 'none';
            stopBtn.style.display = 'inline-block';
            
            statusDiv.className = 'status info';
            statusDiv.innerHTML = '🚀 Starting simulation...';
            
            // Initialize progress
            updateProgress(0, 'Initializing simulation...');
            
            try {
                const response = await fetch('/api/run', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(config)
                });
                
                const result = await response.json();
                if (result.success) {
                    statusDiv.className = 'status success';
                    statusDiv.innerHTML = '✅ Simulation running...';
                    
                    // Start monitoring simulation progress
                    await monitorSimulationProgress(config);
                    
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.innerHTML = '❌ Simulation failed: ' + result.error;
                    resetSimulationUI();
                }
            } catch (error) {
                statusDiv.className = 'status error';
                statusDiv.innerHTML = '❌ Error: ' + error.message;
                resetSimulationUI();
            }
        }
        
        async function monitorSimulationProgress(config) {
            const totalTicks = Math.ceil(config.simulation.duration / config.simulation.time_step);
            document.getElementById('total-ticks').textContent = totalTicks;
            document.getElementById('current-tick').textContent = '...';
            document.getElementById('current-position').textContent = 'Computing...';
            document.getElementById('current-velocity').textContent = 'Computing...';
            
            const statusDiv = document.getElementById('status');
            let dots = 0;
            
            const monitorInterval = setInterval(async () => {
                try {
                    const resp = await fetch('/api/status');
                    const status = await resp.json();
                    const sim = status.simulation || {};
                    
                    if (sim.state === 'running') {
                        dots = (dots + 1) % 4;
                        const dotStr = '.'.repeat(dots);
                        const msg = sim.message || 'Running';
                        updateProgress(50, msg + dotStr);
                        statusDiv.className = 'status info';
                        statusDiv.innerHTML = '🚀 ' + msg + dotStr;
                    
                    } else if (sim.state === 'completed') {
                        clearInterval(monitorInterval);
                        window.currentSimulationInterval = null;
                        updateProgress(100, 'Simulation completed!');
                        statusDiv.className = 'status success';
                        statusDiv.innerHTML = '✅ Loading results...';
                        
                        await loadResults();
                        await loadPlots();
                        showTab('results');
                        statusDiv.innerHTML = '✅ Simulation completed successfully!';
                        resetSimulationUI();
                    
                    } else if (sim.state === 'failed') {
                        clearInterval(monitorInterval);
                        window.currentSimulationInterval = null;
                        const err = sim.message || 'Unknown error';
                        updateProgress(0, 'Failed: ' + err);
                        statusDiv.className = 'status error';
                        statusDiv.innerHTML = '❌ ' + err;
                        resetSimulationUI();
                    }
                } catch (error) {
                    console.error('Error polling status:', error);
                }
            }, 500);
            
            window.currentSimulationInterval = monitorInterval;
        }
        
        async function loadPlots() {
            try {
                const response = await fetch('/api/plots');
                const plots = await response.json();
                
                const gallery = document.getElementById('plots-gallery');
                const grid = document.getElementById('plots-grid');
                
                if (!plots || plots.length === 0) {
                    gallery.style.display = 'none';
                    return;
                }
                
                const plotLabels = {
                    'trajectory_3d.png': '3D Trajectory',
                    'altitude_vs_time.png': 'Altitude vs Time',
                    'speed_vs_time.png': 'Speed vs Time',
                    'velocity_components.png': 'Velocity Components',
                    'atmospheric_conditions.png': 'Atmospheric Conditions',
                    'trajectory_2d_profile.png': '2D Flight Profile'
                };
                
                grid.innerHTML = '';
                
                const cacheBust = Date.now();
                plots.forEach(plot => {
                    const label = plotLabels[plot.filename] || plot.filename;
                    const url = plot.url + '?t=' + cacheBust;
                    grid.innerHTML += `
                        <div style="background: #f8f9fa; border: 1px solid #ddd; border-radius: 8px; overflow: hidden;">
                            <a href="${url}" target="_blank" style="display: block;">
                                <img src="${url}" alt="${label}" 
                                     style="width: 100%; height: 200px; object-fit: contain; background: #fff; padding: 8px;" 
                                     loading="lazy">
                            </a>
                            <div style="padding: 10px; display: flex; justify-content: space-between; align-items: center;">
                                <span style="font-weight: 500; font-size: 0.9em;">${label}</span>
                                <a href="${plot.url}" download="${plot.filename}" 
                                   class="btn btn-primary" 
                                   style="padding: 4px 12px; font-size: 0.8em; text-decoration: none;">
                                   ⬇️ Download
                                </a>
                            </div>
                        </div>
                    `;
                });
                
                gallery.style.display = 'block';
                
            } catch (error) {
                console.error('Error loading plots:', error);
            }
        }
        
        function updateProgress(percentage, message) {
            const progressFill = document.getElementById('progress-fill');
            const progressText = document.getElementById('progress-text');
            
            progressFill.style.width = percentage + '%';
            progressText.textContent = `${percentage.toFixed(0)}% - ${message}`;
        }
        
        function stopSimulation() {
            if (window.currentSimulationInterval) {
                clearInterval(window.currentSimulationInterval);
            }
            
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status warning';
            statusDiv.innerHTML = '⏹️ Simulation stopped by user';
            
            resetSimulationUI();
        }
        
        function resetSimulationUI() {
            const progressSection = document.getElementById('progress-section');
            const runBtn = document.getElementById('run-btn');
            const stopBtn = document.getElementById('stop-btn');
            
            progressSection.style.display = 'none';
            runBtn.style.display = 'inline-block';
            stopBtn.style.display = 'none';
            
            // Reset progress
            updateProgress(0, 'Ready to start...');
            document.getElementById('current-tick').textContent = '0';
            document.getElementById('current-position').textContent = 'X: 0, Y: 0, Z: 0';
            document.getElementById('current-velocity').textContent = 'X: 0, Y: 0, Z: 0';
        }
        
        async function saveConfig() {
            const filename = prompt('Configuration name:', 'web_config');
            if (!filename) return;
            
            const config = getConfigFromForm();
            try {
                const response = await fetch('/api/save', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({filename, config})
                });
                
                const result = await response.json();
                const statusDiv = document.getElementById('status');
                if (result.success) {
                    statusDiv.className = 'status success';
                    statusDiv.innerHTML = '✅ Configuration saved!';
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.innerHTML = '❌ Save failed: ' + result.error;
                }
            } catch (error) {
                document.getElementById('status').innerHTML = '❌ Error saving';
            }
        }
        
        async function loadResults() {
            try {
                const response = await fetch('/api/results');
                const results = await response.json();
                
                const resultsList = document.getElementById('results-list');
                
                if (results.length === 0) {
                    resultsList.innerHTML = '<p>❌ No simulation results found. Run a simulation first.</p>';
                    return;
                }
                
                let html = '';
                results.forEach(result => {
                    const date = new Date(result.modified * 1000).toLocaleString();
                    const size = (result.size / 1024).toFixed(1);
                    html += `
                        <div class="result-item">
                            <h4>📊 ${result.filename}</h4>
                            <p>📅 Modified: ${date} | 📁 Size: ${size} KB</p>
                            <button class="btn btn-primary" onclick="visualizeResult('${result.filename}')">📈 Visualize</button>
                            ${comparisonMode ? `<button class="btn btn-secondary" onclick="selectResult('${result.filename}')">📊 Select for Comparison</button>` : ''}
                        </div>
                    `;
                });
                
                resultsList.innerHTML = html;
                
                // Auto-load the most recent result
                if (results.length > 0) {
                    visualizeResult(results[0].filename);
                }
                
            } catch (error) {
                document.getElementById('results-list').innerHTML = '❌ Error loading results: ' + error.message;
            }
        }
        
        async function visualizeResult(filename) {
            try {
                const response = await fetch(`/api/results/${filename}`);
                const data = await response.json();
                
                if (data.error) {
                    alert('Error loading result data: ' + data.error);
                    return;
                }
                
                // Show chart container
                document.getElementById('chart-container').style.display = 'block';
                
                // Update summary statistics
                updateSummaryStats(data);
                
                // Create visualization
                createChart(data);
                
                showTab('results');
                
            } catch (error) {
                alert('Error visualizing results: ' + error.message);
            }
        }
        
        function updateSummaryStats(data) {
            const statsDiv = document.getElementById('summary-stats');
            
            const durationValue = data.summary.duration || 0;
            const duration = durationValue.toFixed(2);
            const totalPoints = data.summary.total_points || 0;
            const finalPos = data.summary.final_position || {x: 0, y: 0, z: 0};
            const finalVel = data.summary.final_velocity || {x: 0, y: 0, z: 0};
            const initialPos = data.summary.initial_position || {x: 0, y: 0, z: 0};
            const initialVel = data.summary.initial_velocity || {x: 0, y: 0, z: 0};
            const distance = data.summary.distance_traveled ? data.summary.distance_traveled.toFixed(2) : '0';
            
            // Calculate advanced aerospace metrics
            const initialSpeed = Math.sqrt(initialVel.x**2 + initialVel.y**2 + initialVel.z**2);
            const finalSpeed = Math.sqrt(finalVel.x**2 + finalVel.y**2 + finalVel.z**2);
            const speedChange = finalSpeed - initialSpeed;
            const altitudeChange = finalPos.z - initialPos.z;
            const horizontalDistance = Math.sqrt((finalPos.x - initialPos.x)**2 + (finalPos.y - initialPos.y)**2);
            
            // Advanced calculations
            const avgAcceleration = durationValue > 0 ? speedChange / durationValue : 0;
            const maxAltitude = Math.max(initialPos.z, finalPos.z);
            const avgSpeed = distance > 0 && durationValue > 0 ? distance / durationValue : 0;
            
            // Energy calculations (simplified)
            const initialKineticEnergy = 0.5 * initialSpeed**2; // assuming unit mass
            const finalKineticEnergy = 0.5 * finalSpeed**2;
            const energyChange = finalKineticEnergy - initialKineticEnergy;
            
            // Flight efficiency metrics
            const straightLineDistance = Math.sqrt((finalPos.x - initialPos.x)**2 + (finalPos.y - initialPos.y)**2 + (finalPos.z - initialPos.z)**2);
            const flightEfficiency = straightLineDistance > 0 ? (straightLineDistance / distance * 100) : 0;
            
            // Trajectory characteristics
            const ascentRate = durationValue > 0 ? altitudeChange / durationValue : 0;
            const horizontalVelocity = Math.sqrt(finalVel.x*finalVel.x + finalVel.y*finalVel.y);
            const verticalVelocity = Math.abs(finalVel.z);
            
            // Performance Analysis and Alerts
            const performanceAlerts = analyzePerformance({
                flightEfficiency, energyChange, avgAcceleration, ascentRate, 
                finalSpeed, altitudeChange, durationValue
            });
            
            // Aerospace Physics Metrics
            const maxMach = Math.max(...data.rows.map(row => {
                const speed = Math.sqrt((row[5]||0)**2 + (row[6]||0)**2 + (row[7]||0)**2);
                return speed / 343.0; // Speed of sound at sea level
            }));
            const maxGForce = Math.max(...data.rows.map(row => {
                const ax = row[8] || 0;
                const ay = row[9] || 0; 
                const az = row[10] || 0;
                return Math.sqrt(ax*ax + ay*ay + az*az) / 9.81;
            }));
            const orbitalVelocityPercent = (finalSpeed / 7840) * 100; // 7.84 km/s orbital velocity
            
            // Mission Classification
            const flightType = getFlightType(finalPos, finalVel);
            const flightTypeEmoji = getFlightTypeEmoji(flightType);
            const flightPhase = getFlightPhase(finalPos, finalVel);
            const flightPhaseEmoji = getFlightPhaseEmoji(flightPhase);
            const missionSuccess = isMissionSuccessful(finalPos, finalVel);
            const realismScore = calculateRealismScore(data);
            
            statsDiv.innerHTML = `
                <div class="stats-row primary">
                    <div class="stat-section">
                        <h3> Mission Overview</h3>
                        <div class="stat-cards">
                            <div class="stat-card highlight">
                                <h4>⏱ Flight Duration</h4>
                                <div class="value">${duration}</div>
                                <div class="unit">seconds</div>
                            </div>
                            <div class="stat-card">
                                <h4> Data Points</h4>
                                <div class="value">${totalPoints}</div>
                                <div class="unit">samples</div>
                            </div>
                            <div class="stat-card highlight">
                                <h4> Total Distance</h4>
                                <div class="value">${distance}</div>
                                <div class="unit">meters</div>
                            </div>
                            <div class="stat-card">
                                <h4> Straight Distance</h4>
                                <div class="value">${straightLineDistance.toFixed(1)}</div>
                                <div class="unit">meters</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="stat-section">
                        <h3> Trajectory Analysis</h3>
                        <div class="stat-cards">
                            <div class="stat-card">
                                <h4> Altitude Change</h4>
                                <div class="value ${altitudeChange >= 0 ? 'positive' : 'negative'}">${altitudeChange >= 0 ? '+' : ''}${altitudeChange.toFixed(1)}</div>
                                <div class="unit">meters</div>
                            </div>
                            <div class="stat-card">
                                <h4> Horizontal Distance</h4>
                                <div class="value">${horizontalDistance.toFixed(1)}</div>
                                <div class="unit">meters</div>
                            </div>
                            <div class="stat-card highlight">
                                <h4> Maximum Altitude</h4>
                                <div class="value">${maxAltitude.toFixed(1)}</div>
                                <div class="unit">meters</div>
                            </div>
                            <div class="stat-card">
                                <h4> Flight Efficiency</h4>
                                <div class="value ${flightEfficiency >= 80 ? 'positive' : flightEfficiency >= 60 ? 'neutral' : 'negative'}">${flightEfficiency.toFixed(1)}</div>
                                <div class="unit">%</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="stat-section">
                        <h3> Performance Metrics</h3>
                        <div class="stat-cards">
                            <div class="stat-card">
                                <h4> Initial Speed</h4>
                                <div class="value">${initialSpeed.toFixed(1)}</div>
                                <div class="unit">m/s</div>
                            </div>
                            <div class="stat-card">
                                <h4> Final Speed</h4>
                                <div class="value">${finalSpeed.toFixed(1)}</div>
                                <div class="unit">m/s</div>
                            </div>
                            <div class="stat-card">
                                <h4> Speed Change</h4>
                                <div class="value ${speedChange >= 0 ? 'positive' : 'negative'}">${speedChange >= 0 ? '+' : ''}${speedChange.toFixed(1)}</div>
                                <div class="unit">m/s</div>
                            </div>
                            <div class="stat-card">
                                <h4> Avg Acceleration</h4>
                                <div class="value ${Math.abs(avgAcceleration) > 9.81 ? 'highlight' : ''}">${avgAcceleration.toFixed(2)}</div>
                                <div class="unit">m/s²</div>
                            </div>
                        </div>
                    </div>
                </div>
                
                <div class="stats-row secondary">
                    <div class="stat-section">
                        <h3> Advanced Analysis</h3>
                        <div class="stat-cards">
                            <div class="stat-card">
                                <h4> Average Speed</h4>
                                <div class="value">${avgSpeed.toFixed(1)}</div>
                                <div class="unit">m/s</div>
                            </div>
                            <div class="stat-card">
                                <h4> Ascent Rate</h4>
                                <div class="value ${ascentRate >= 0 ? 'positive' : 'negative'}">${ascentRate >= 0 ? '+' : ''}${ascentRate.toFixed(1)}</div>
                                <div class="unit">m/s</div>
                            </div>
                            <div class="stat-card">
                                <h4> Energy Change</h4>
                                <div class="value ${energyChange >= 0 ? 'positive' : 'negative'}">${energyChange >= 0 ? '+' : ''}${energyChange.toFixed(1)}</div>
                                <div class="unit">J/kg</div>
                            </div>
                            <div class="stat-card">
                                <h4> Final H/V Ratio</h4>
                                <div class="value">${verticalVelocity > 0 ? (horizontalVelocity/verticalVelocity).toFixed(2) : 'N/A'}</div>
                                <div class="unit">ratio</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="stat-section">
                        <h3> Velocity Analysis</h3>
                        <div class="stat-cards">
                            <div class="stat-card multi-value">
                                <h4> Final Horizontal</h4>
                                <div class="value">X: ${finalVel.x.toFixed(1)} m/s</div>
                                <div class="value">Y: ${finalVel.y.toFixed(1)} m/s</div>
                                <div class="value">Total: ${horizontalVelocity.toFixed(1)} m/s</div>
                            </div>
                            <div class="stat-card multi-value">
                                <h4> Final Vertical</h4>
                                <div class="value">Z: ${finalVel.z.toFixed(1)} m/s</div>
                                <div class="value">|Z|: ${verticalVelocity.toFixed(1)} m/s</div>
                                <div class="value">${finalVel.z >= 0 ? '⬆️ Ascending' : '⬇️ Descending'}</div>
                            </div>
                        </div>
                    </div>
                </div>
                
                <div class="stats-row primary">
                    <div class="stat-section">
                        <h3>📍 Position Details</h3>
                        <div class="stat-cards">
                            <div class="stat-card multi-value">
                                <h4> Initial Position</h4>
                                <div class="value">X: ${initialPos.x.toFixed(1)}m</div>
                                <div class="value">Y: ${initialPos.y.toFixed(1)}m</div>
                                <div class="value">Z: ${initialPos.z.toFixed(1)}m</div>
                            </div>
                            <div class="stat-card multi-value">
                                <h4> Final Position</h4>
                                <div class="value">X: ${finalPos.x.toFixed(1)}m</div>
                                <div class="value">Y: ${finalPos.y.toFixed(1)}m</div>
                                <div class="value">Z: ${finalPos.z.toFixed(1)}m</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="stat-section">
                        <h3> Aerospace Physics</h3>
                        <div class="stat-cards">
                            <div class="stat-card">
                                <h4>🌌 Max Altitude</h4>
                                <div class="value ${maxAltitude > 100000 ? 'highlight' : ''}">${(maxAltitude/1000).toFixed(1)}</div>
                                <div class="unit">km</div>
                                <div class="subtext">${maxAltitude > 100000 ? 'Space boundary reached!' : 'Atmospheric flight'}</div>
                            </div>
                            <div class="stat-card">
                                <h4>💨 Max Mach Number</h4>
                                <div class="value ${maxMach > 1 ? 'highlight' : ''}">${maxMach.toFixed(2)}</div>
                                <div class="unit">Mach</div>
                                <div class="subtext">${maxMach > 1 ? 'Supersonic achieved!' : 'Subsonic flight'}</div>
                            </div>
                            <div class="stat-card">
                                <h4>⚡ G-Force Peak</h4>
                                <div class="value ${maxGForce > 3 ? 'warning' : ''}">${maxGForce.toFixed(1)}</div>
                                <div class="unit">g</div>
                                <div class="subtext">${maxGForce > 6 ? 'Extreme stress!' : maxGForce > 3 ? 'High stress' : 'Normal'}</div>
                            </div>
                            <div class="stat-card">
                                <h4>🛰️ Orbital Velocity</h4>
                                <div class="value ${orbitalVelocityPercent > 80 ? 'positive' : orbitalVelocityPercent > 50 ? 'warning' : 'negative'}">${orbitalVelocityPercent.toFixed(1)}</div>
                                <div class="unit">%</div>
                                <div class="subtext">${orbitalVelocityPercent > 90 ? 'Orbital capable!' : 'Sub-orbital'}</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="stat-section">
                        <h3> Mission Classification</h3>
                        <div class="stat-cards">
                            <div class="stat-card">
                                <h4> Flight Type</h4>
                                <div class="value">${flightType}</div>
                                <div class="unit">${flightTypeEmoji}</div>
                            </div>
                            <div class="stat-card">
                                <h4>⏱ Flight Phase</h4>
                                <div class="value">${flightPhase}</div>
                                <div class="unit">${flightPhaseEmoji}</div>
                            </div>
                            <div class="stat-card">
                                <h4> Mission Success</h4>
                                <div class="value ${missionSuccess ? 'positive' : 'negative'}">${missionSuccess ? 'SUCCESS' : 'PARTIAL'}</div>
                                <div class="unit">${missionSuccess ? '✅' : '⚠️'}</div>
                            </div>
                            <div class="stat-card">
                                <h4> Realism Score</h4>
                                <div class="value ${realismScore > 80 ? 'positive' : realismScore > 60 ? 'warning' : 'negative'}">${realismScore.toFixed(0)}</div>
                                <div class="unit">%</div>
                                <div class="subtext">${realismScore > 80 ? 'Highly realistic' : realismScore > 60 ? 'Moderately realistic' : 'Check physics'}</div>
                            </div>
                        </div>
                    </div>
                </div>
                
                <div class="performance-alerts-section">
                    ${performanceAlerts}
                </div>
            `;
        }
        
        function analyzePerformance(metrics) {
            const alerts = [];
            
            // Flight Efficiency Analysis
            if (metrics.flightEfficiency < 50) {
                alerts.push({
                    type: 'error',
                    title: ' Low Flight Efficiency',
                    message: `Flight efficiency is ${metrics.flightEfficiency.toFixed(1)}%. Consider optimizing trajectory for more direct path.`
                });
            } else if (metrics.flightEfficiency > 90) {
                alerts.push({
                    type: 'success',
                    title: ' Excellent Flight Efficiency',
                    message: `Outstanding flight efficiency of ${metrics.flightEfficiency.toFixed(1)}%. Very direct trajectory achieved.`
                });
            }
            
            // Energy Analysis
            if (metrics.energyChange > 1000) {
                alerts.push({
                    type: 'warning',
                    title: ' High Energy Gain',
                    message: `Significant energy increase of ${metrics.energyChange.toFixed(1)} J/kg detected. Check propulsion systems.`
                });
            } else if (metrics.energyChange < -1000) {
                alerts.push({
                    type: 'warning',
                    title: ' High Energy Loss',
                    message: `Significant energy loss of ${Math.abs(metrics.energyChange).toFixed(1)} J/kg detected. Check for excessive drag.`
                });
            }
            
            // Acceleration Analysis
            if (Math.abs(metrics.avgAcceleration) > 20) {
                alerts.push({
                    type: 'error',
                    title: '🚨 Extreme Acceleration',
                    message: `Average acceleration of ${metrics.avgAcceleration.toFixed(2)} m/s² exceeds safe limits. Review mission parameters.`
                });
            }
            
            // Ascent Rate Analysis
            if (metrics.ascentRate > 50) {
                alerts.push({
                    type: 'warning',
                    title: '📈 Rapid Ascent',
                    message: `High ascent rate of ${metrics.ascentRate.toFixed(1)} m/s detected. Monitor structural loads.`
                });
            } else if (metrics.ascentRate < -50) {
                alerts.push({
                    type: 'warning',
                    title: '📉 Rapid Descent',
                    message: `High descent rate of ${Math.abs(metrics.ascentRate).toFixed(1)} m/s detected. Check landing systems.`
                });
            }
            
            // Speed Analysis
            if (metrics.finalSpeed > 200) {
                alerts.push({
                    type: 'warning',
                    title: '🚀 High Terminal Speed',
                    message: `Final speed of ${metrics.finalSpeed.toFixed(1)} m/s is very high. Consider deceleration procedures.`
                });
            }
            
            // Mission Duration Analysis
            if (metrics.duration && metrics.duration < 1) {
                alerts.push({
                    type: 'warning',
                    title: '⏱️ Short Mission Duration',
                    message: `Mission duration of ${metrics.duration.toFixed(2)}s is very short. Consider extending simulation time.`
                });
            }
            
            // Generate alerts HTML
            if (alerts.length === 0) {
                return `<div class="performance-alert success">
                    <h4>✅ All Systems Nominal</h4>
                    <p>Mission parameters are within acceptable ranges. No performance issues detected.</p>
                </div>`;
            }
            
            return alerts.map(alert => `
                <div class="performance-alert ${alert.type}">
                    <h4>${alert.title}</h4>
                    <p>${alert.message}</p>
                </div>
            `).join('');
        }
        
        function getFlightType(finalPos, finalVel) {
            const altitude = finalPos.z;
            const speed = Math.sqrt(finalVel.x*finalVel.x + finalVel.y*finalVel.y + finalVel.z*finalVel.z);
            
            if (altitude > 100000 && speed > 7000) {
                return 'Orbital';
            } else if (altitude > 100000) {
                return 'Suborbital';
            } else if (finalVel.z > 0) {
                return 'Ascending';
            } else if (finalVel.z < -50) {
                return 'Landing';
            } else {
                return 'Atmospheric';
            }
        }
        
        function getFlightPhase(finalPos, finalVel) {
            const altitude = finalPos.z;
            
            if (altitude > 400000) {
                return 'Deep Space';
            } else if (altitude > 100000) {
                return 'Space';
            } else if (altitude > 50000) {
                return 'Upper Atmosphere';
            } else if (altitude > 10000) {
                return 'Stratosphere';
            } else {
                return 'Ground Level';
            }
        }
        
        function isMissionSuccessful(finalPos, finalVel) {
            const altitude = finalPos.z;
            const speed = Math.sqrt(finalVel.x*finalVel.x + finalVel.y*finalVel.y + finalVel.z*finalVel.z);
            
            // Success criteria based on mission type
            if (altitude > 100000 && speed > 7000) {
                return true; // Orbital mission success
            } else if (altitude > 50000) {
                return true; // Suborbital mission success
            } else if (altitude < 100 && Math.abs(finalVel.z) < 10) {
                return true; // Landing mission success
            } else if (speed > 100) {
                return true; // General flight success
            }
            
            return false; // Mission incomplete or failed
        }
        
        function calculateRealismScore(data) {
            let score = 100;
            
            // Check for unrealistic accelerations
            const accelerations = data.rows.map(row => {
                if (row.length > 10) {
                    return Math.abs(row[10] || 0); // G-force column
                }
                return 0;
            });
            
            const maxAccel = Math.max(...accelerations);
            if (maxAccel > 20) score -= 30; // Extreme acceleration penalty
            else if (maxAccel > 10) score -= 15; // High acceleration penalty
            
            // Check for realistic velocities
            const velocities = data.rows.map(row => {
                const vx = row[5] || 0;
                const vy = row[6] || 0;
                const vz = row[7] || 0;
                return Math.sqrt(vx*vx + vy*vy + vz*vz);
            });
            
            const maxVel = Math.max(...velocities);
            if (maxVel > 15000) score -= 20; // Unrealistic velocity penalty
            else if (maxVel > 11000) score -= 10; // High velocity penalty
            
            // Check for realistic altitude progression
            const altitudes = data.rows.map(row => row[4] || 0);
            const maxAlt = Math.max(...altitudes);
            const minAlt = Math.min(...altitudes);
            
            if (minAlt < -1000) score -= 25; // Underground penalty
            if (maxAlt > 1000000) score -= 15; // Too high penalty
            
            // Bonus for realistic physics
            if (maxAlt > 50000 && maxVel > 1000) score += 10; // Space mission bonus
            if (data.rows.length > 100) score += 5; // Long simulation bonus
            
            return Math.max(0, Math.min(100, score));
        }
        
        function getFlightTypeEmoji(flightType) {
            switch (flightType) {
                case 'Orbital':
                    return '🛰️';
                case 'Suborbital':
                    return '🚀';
                case 'Ascending':
                    return '⬆️';
                case 'Landing':
                    return '⬇️';
                case 'Atmospheric':
                    return '🌍';
                default:
                    return '';
            }
        }
        
        function getFlightPhaseEmoji(flightPhase) {
            switch (flightPhase) {
                case 'Deep Space':
                    return '🚀';
                case 'Space':
                    return '🛰️';
                case 'Upper Atmosphere':
                    return '☁️';
                case 'Stratosphere':
                    return '🌌';
                case 'Ground Level':
                    return '🏔️';
                default:
                    return '';
            }
        }
        
        // Helper: map column names to indices dynamically
        function colIdx(data, name) {
            const idx = data.headers.indexOf(name);
            return idx >= 0 ? idx : -1;
        }
        function colVal(row, data, name) {
            const i = colIdx(data, name);
            return i >= 0 ? row[i] : 0;
        }
        // Format large numbers for axis ticks
        function fmtNum(v) {
            if (Math.abs(v) >= 1e6) return (v/1e6).toFixed(2) + 'M';
            if (Math.abs(v) >= 1e3) return (v/1e3).toFixed(1) + 'k';
            return v.toFixed(2);
        }
        const chartColors = {
            blue:    {border: '#3b82f6', bg: 'rgba(59,130,246,0.15)'},
            red:     {border: '#ef4444', bg: 'rgba(239,68,68,0.15)'},
            green:   {border: '#22c55e', bg: 'rgba(34,197,94,0.15)'},
            orange:  {border: '#f97316', bg: 'rgba(249,115,22,0.15)'},
            purple:  {border: '#a855f7', bg: 'rgba(168,85,247,0.15)'},
            cyan:    {border: '#06b6d4', bg: 'rgba(6,182,212,0.15)'},
            pink:    {border: '#ec4899', bg: 'rgba(236,72,153,0.15)'},
            amber:   {border: '#f59e0b', bg: 'rgba(245,158,11,0.15)'},
        };
        const defaultTooltip = {
            mode: 'index',
            intersect: false,
            callbacks: {
                label: ctx => `${ctx.dataset.label}: ${fmtNum(ctx.parsed.y)}`
            }
        };
        const defaultFont = { size: 13, weight: 'bold' };

        function createChart(data) {
            createTrajectoryChart(data);
            createVelocityChart(data);
            createAltitudeChart(data);
            createSpeedChart(data);
            if (document.getElementById('earth-chart')) createEarthChart(data);
            if (document.getElementById('physics-chart')) createPhysicsChart(data);
        }
        
        function createTrajectoryChart(data) {
            const ctx = document.getElementById('trajectory-chart').getContext('2d');
            if (window.trajectoryChart) window.trajectoryChart.destroy();

            const t = colIdx(data, 'simulation_time');
            const pz = colIdx(data, 'position_z');
            const px = colIdx(data, 'position_x');
            const py = colIdx(data, 'position_y');
            if (t < 0 || pz < 0) return;

            const datasets = [];

            // Altitude (Position Z) — main series
            datasets.push({
                label: 'Altitude / Position Z (m)',
                data: data.rows.map(row => ({x: row[t], y: row[pz]})),
                borderColor: chartColors.blue.border,
                backgroundColor: chartColors.blue.bg,
                fill: true,
                tension: 0.3,
                pointRadius: 3,
                pointBackgroundColor: chartColors.blue.border,
                borderWidth: 2,
                yAxisID: 'y'
            });

            // Downrange distance sqrt(X²+Y²) if meaningful
            if (px >= 0 && py >= 0) {
                const hasMovement = data.rows.some(r => Math.abs(r[px]) > 1 || Math.abs(r[py]) > 1);
                if (hasMovement) {
                    datasets.push({
                        label: 'Downrange (m)',
                        data: data.rows.map(row => ({x: row[t], y: Math.sqrt(row[px]**2 + row[py]**2)})),
                        borderColor: chartColors.orange.border,
                        backgroundColor: chartColors.orange.bg,
                        fill: false,
                        tension: 0.3,
                        pointRadius: 2,
                        borderWidth: 2,
                        borderDash: [6, 3],
                        yAxisID: 'y1'
                    });
                }
            }

            const hasY1 = datasets.length > 1;
            window.trajectoryChart = new Chart(ctx, {
                type: 'line',
                data: { datasets },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Trajectory — Altitude & Downrange vs Time', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: defaultTooltip
                    },
                    scales: {
                        x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                              ticks: { callback: v => v.toFixed(2) + 's' } },
                        y: { type: 'linear', position: 'left',
                             title: { display: true, text: 'Altitude (m)', font: { size: 12 } },
                             ticks: { callback: v => fmtNum(v) } },
                        ...(hasY1 ? { y1: { type: 'linear', position: 'right',
                             title: { display: true, text: 'Downrange (m)', font: { size: 12 } },
                             ticks: { callback: v => fmtNum(v) },
                             grid: { drawOnChartArea: false } } } : {})
                    }
                }
            });
        }
        
        function createVelocityChart(data) {
            const ctx = document.getElementById('velocity-chart').getContext('2d');
            if (window.velocityChart) window.velocityChart.destroy();

            const t  = colIdx(data, 'simulation_time');
            const vx = colIdx(data, 'velocity_x');
            const vy = colIdx(data, 'velocity_y');
            const vz = colIdx(data, 'velocity_z');
            if (t < 0 || vz < 0) return;

            const datasets = [];

            // Only show components that have meaningful values
            const hasVx = vx >= 0 && data.rows.some(r => Math.abs(r[vx]) > 0.1);
            const hasVy = vy >= 0 && data.rows.some(r => Math.abs(r[vy]) > 0.1);

            if (hasVx) {
                datasets.push({
                    label: 'Vx (m/s)', data: data.rows.map(r => ({x: r[t], y: r[vx]})),
                    borderColor: chartColors.red.border, backgroundColor: chartColors.red.bg,
                    fill: false, tension: 0.3, pointRadius: 2, borderWidth: 1.5
                });
            }
            if (hasVy) {
                datasets.push({
                    label: 'Vy (m/s)', data: data.rows.map(r => ({x: r[t], y: r[vy]})),
                    borderColor: chartColors.green.border, backgroundColor: chartColors.green.bg,
                    fill: false, tension: 0.3, pointRadius: 2, borderWidth: 1.5
                });
            }

            // Vz always shown
            datasets.push({
                label: 'Vz — Vertical (m/s)', data: data.rows.map(r => ({x: r[t], y: r[vz]})),
                borderColor: chartColors.blue.border, backgroundColor: chartColors.blue.bg,
                fill: true, tension: 0.3, pointRadius: 3, borderWidth: 2
            });

            // Total speed
            if (vx >= 0 && vy >= 0) {
                datasets.push({
                    label: 'Total Speed (m/s)',
                    data: data.rows.map(r => {
                        const sx = r[vx]||0, sy = r[vy]||0, sz = r[vz]||0;
                        return {x: r[t], y: Math.sqrt(sx*sx + sy*sy + sz*sz)};
                    }),
                    borderColor: chartColors.red.border, backgroundColor: 'transparent',
                    fill: false, tension: 0.3, pointRadius: 0, borderWidth: 2.5, borderDash: [8, 4]
                });
            }

            window.velocityChart = new Chart(ctx, {
                type: 'line', data: { datasets },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Velocity Components vs Time', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: defaultTooltip
                    },
                    scales: {
                        x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                              ticks: { callback: v => v.toFixed(2) + 's' } },
                        y: { title: { display: true, text: 'Velocity (m/s)', font: { size: 12 } },
                             ticks: { callback: v => fmtNum(v) } }
                    }
                }
            });
        }
        
        function createAltitudeChart(data) {
            const ctx = document.getElementById('altitude-chart').getContext('2d');
            if (window.altitudeChart) window.altitudeChart.destroy();

            const t  = colIdx(data, 'simulation_time');
            const pz = colIdx(data, 'position_z');
            const vz = colIdx(data, 'velocity_z');
            if (t < 0 || pz < 0) return;

            const datasets = [{
                label: 'Altitude (m)', yAxisID: 'y',
                data: data.rows.map(r => ({x: r[t], y: r[pz]})),
                borderColor: chartColors.green.border, backgroundColor: chartColors.green.bg,
                fill: true, tension: 0.3, pointRadius: 3, borderWidth: 2
            }];

            if (vz >= 0) {
                datasets.push({
                    label: 'Vertical Velocity (m/s)', yAxisID: 'y1',
                    data: data.rows.map(r => ({x: r[t], y: r[vz]})),
                    borderColor: chartColors.cyan.border, backgroundColor: 'transparent',
                    fill: false, tension: 0.3, pointRadius: 2, borderWidth: 2, borderDash: [6, 3]
                });
            }

            window.altitudeChart = new Chart(ctx, {
                type: 'line', data: { datasets },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Altitude Profile & Vertical Velocity', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: defaultTooltip
                    },
                    scales: {
                        x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                              ticks: { callback: v => v.toFixed(2) + 's' } },
                        y: { position: 'left', title: { display: true, text: 'Altitude (m)', font: { size: 12 } },
                             ticks: { callback: v => fmtNum(v) } },
                        y1: { position: 'right', title: { display: true, text: 'Vertical Vel. (m/s)', font: { size: 12 } },
                              ticks: { callback: v => fmtNum(v) }, grid: { drawOnChartArea: false } }
                    }
                }
            });
        }
        
        function createSpeedChart(data) {
            const ctx = document.getElementById('speed-chart').getContext('2d');
            if (window.speedChart) window.speedChart.destroy();

            const t  = colIdx(data, 'simulation_time');
            const vxi = colIdx(data, 'velocity_x');
            const vyi = colIdx(data, 'velocity_y');
            const vzi = colIdx(data, 'velocity_z');
            if (t < 0 || vzi < 0) return;

            const datasets = [];

            // Total speed — always shown, prominent
            datasets.push({
                label: 'Total Speed (m/s)',
                data: data.rows.map(r => {
                    const sx = vxi >= 0 ? r[vxi] : 0, sy = vyi >= 0 ? r[vyi] : 0, sz = r[vzi];
                    return {x: r[t], y: Math.sqrt(sx*sx + sy*sy + sz*sz)};
                }),
                borderColor: chartColors.red.border, backgroundColor: chartColors.red.bg,
                fill: true, tension: 0.3, pointRadius: 3, borderWidth: 2.5
            });

            // Vertical speed (absolute)
            datasets.push({
                label: '|Vertical Speed| (m/s)',
                data: data.rows.map(r => ({x: r[t], y: Math.abs(r[vzi])})),
                borderColor: chartColors.amber.border, backgroundColor: 'transparent',
                fill: false, tension: 0.3, pointRadius: 2, borderWidth: 2, borderDash: [6, 3]
            });

            // Horizontal only if meaningful
            if (vxi >= 0 && vyi >= 0) {
                const hasHoriz = data.rows.some(r => Math.abs(r[vxi]) > 0.1 || Math.abs(r[vyi]) > 0.1);
                if (hasHoriz) {
                    datasets.push({
                        label: 'Horizontal Speed (m/s)',
                        data: data.rows.map(r => ({x: r[t], y: Math.sqrt(r[vxi]**2 + r[vyi]**2)})),
                        borderColor: chartColors.purple.border, backgroundColor: 'transparent',
                        fill: false, tension: 0.3, pointRadius: 2, borderWidth: 1.5
                    });
                }
            }

            window.speedChart = new Chart(ctx, {
                type: 'line', data: { datasets },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Speed Analysis', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: defaultTooltip
                    },
                    scales: {
                        x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                              ticks: { callback: v => v.toFixed(2) + 's' } },
                        y: { title: { display: true, text: 'Speed (m/s)', font: { size: 12 } },
                             ticks: { callback: v => fmtNum(v) }, beginAtZero: true }
                    }
                }
            });
        }
        
        function createEarthChart(data) {
            const ctx = document.getElementById('earth-chart').getContext('2d');
            if (window.earthChart) window.earthChart.destroy();

            const t    = colIdx(data, 'simulation_time');
            const rho  = colIdx(data, 'atm_density');
            const pres = colIdx(data, 'atm_pressure');
            const temp = colIdx(data, 'atm_temperature');
            if (t < 0) return;

            const datasets = [];

            if (rho >= 0) {
                datasets.push({
                    label: 'Air Density (kg/m³)', yAxisID: 'y',
                    data: data.rows.map(r => ({x: r[t], y: r[rho]})),
                    borderColor: chartColors.purple.border, backgroundColor: chartColors.purple.bg,
                    fill: true, tension: 0.3, pointRadius: 3, borderWidth: 2
                });
            }
            if (temp >= 0) {
                datasets.push({
                    label: 'Temperature (K)', yAxisID: 'y1',
                    data: data.rows.map(r => ({x: r[t], y: r[temp]})),
                    borderColor: chartColors.red.border, backgroundColor: 'transparent',
                    fill: false, tension: 0.3, pointRadius: 2, borderWidth: 2, borderDash: [6, 3]
                });
            }

            const scales = {
                x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                      ticks: { callback: v => v.toFixed(2) + 's' } }
            };
            if (rho >= 0) {
                scales.y = { position: 'left',
                    title: { display: true, text: 'Density (kg/m³)', font: { size: 12 } },
                    ticks: { callback: v => v.toFixed(4) } };
            }
            if (temp >= 0) {
                scales.y1 = { position: 'right',
                    title: { display: true, text: 'Temperature (K)', font: { size: 12 } },
                    ticks: { callback: v => v.toFixed(1) + ' K' },
                    grid: { drawOnChartArea: false } };
            }

            window.earthChart = new Chart(ctx, {
                type: 'line', data: { datasets },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Atmospheric Conditions vs Time', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: {
                            mode: 'index', intersect: false,
                            callbacks: {
                                label: ctx => {
                                    const v = ctx.parsed.y;
                                    if (ctx.dataset.label.includes('Density')) return `Density: ${v.toFixed(4)} kg/m³`;
                                    if (ctx.dataset.label.includes('Temp')) return `Temp: ${v.toFixed(1)} K (${(v - 273.15).toFixed(1)} °C)`;
                                    return `${ctx.dataset.label}: ${fmtNum(v)}`;
                                }
                            }
                        }
                    },
                    scales
                }
            });
        }
        
        function createPhysicsChart(data) {
            const ctx = document.getElementById('physics-chart').getContext('2d');
            if (window.physicsChart) window.physicsChart.destroy();

            const t   = colIdx(data, 'simulation_time');
            const gx  = colIdx(data, 'gravity_x');
            const gy  = colIdx(data, 'gravity_y');
            const gz  = colIdx(data, 'gravity_z');
            const vxi = colIdx(data, 'velocity_x');
            const vyi = colIdx(data, 'velocity_y');
            const vzi = colIdx(data, 'velocity_z');
            const pres = colIdx(data, 'atm_pressure');
            if (t < 0) return;

            const datasets = [];

            // Gravity magnitude
            if (gz >= 0) {
                datasets.push({
                    label: 'Gravity (m/s²)', yAxisID: 'y',
                    data: data.rows.map(r => {
                        const x = gx >= 0 ? r[gx] : 0, y = gy >= 0 ? r[gy] : 0, z = r[gz];
                        return {x: r[t], y: Math.sqrt(x*x + y*y + z*z)};
                    }),
                    borderColor: chartColors.orange.border, backgroundColor: chartColors.orange.bg,
                    fill: true, tension: 0.3, pointRadius: 3, borderWidth: 2
                });
            }

            // Kinetic energy (½mv², m=1000 kg)
            if (vzi >= 0) {
                const mass = 1000;
                datasets.push({
                    label: 'Kinetic Energy (kJ)', yAxisID: 'y1',
                    data: data.rows.map(r => {
                        const sx = vxi >= 0 ? r[vxi] : 0, sy = vyi >= 0 ? r[vyi] : 0, sz = r[vzi];
                        return {x: r[t], y: 0.5 * mass * (sx*sx + sy*sy + sz*sz) / 1000};
                    }),
                    borderColor: chartColors.cyan.border, backgroundColor: 'transparent',
                    fill: false, tension: 0.3, pointRadius: 2, borderWidth: 2, borderDash: [6, 3]
                });
            }

            // Atmospheric pressure if available
            if (pres >= 0) {
                datasets.push({
                    label: 'Pressure (kPa)', yAxisID: 'y1',
                    data: data.rows.map(r => ({x: r[t], y: r[pres] / 1000})),
                    borderColor: chartColors.pink.border, backgroundColor: 'transparent',
                    fill: false, tension: 0.3, pointRadius: 1, borderWidth: 1.5, borderDash: [3, 3]
                });
            }

            window.physicsChart = new Chart(ctx, {
                type: 'line', data: { datasets },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        title: { display: true, text: 'Physics — Gravity, Energy & Pressure', font: defaultFont },
                        legend: { display: true, position: 'top', labels: { usePointStyle: true, padding: 16 } },
                        tooltip: defaultTooltip
                    },
                    scales: {
                        x: { type: 'linear', title: { display: true, text: 'Time (s)', font: { size: 12 } },
                              ticks: { callback: v => v.toFixed(2) + 's' } },
                        y: { position: 'left',
                             title: { display: true, text: 'Gravity (m/s²)', font: { size: 12 } },
                             ticks: { callback: v => v.toFixed(4) } },
                        y1: { position: 'right',
                              title: { display: true, text: 'Energy (kJ) / Pressure (kPa)', font: { size: 12 } },
                              ticks: { callback: v => fmtNum(v) },
                              grid: { drawOnChartArea: false } }
                    }
                }
            });
        }
        
        function showChart(chartName) {
            document.querySelectorAll('.chart-panel').forEach(panel => panel.classList.remove('active'));
            document.querySelectorAll('.chart-tab').forEach(tab => tab.classList.remove('active'));
            document.getElementById(chartName + '-chart-container').classList.add('active');
            document.querySelector(`[onclick*="showChart('${chartName}')"]`).classList.add('active');
            
            // Trigger resize so Chart.js recalculates within the fixed wrapper
            const chartMap = {
                'trajectory': window.trajectoryChart,
                'velocity': window.velocityChart,
                'altitude': window.altitudeChart,
                'speed': window.speedChart,
                'earth': window.earthChart,
                'physics': window.physicsChart,
                'comparison': window.comparisonChart
            };
            const chart = chartMap[chartName];
            if (chart) {
                setTimeout(() => chart.resize(), 50);
            }
        }
        
        function toggleComparisonMode() {
            comparisonMode = !comparisonMode;
            const comparisonPanel = document.getElementById('comparison-panel');
            const toggleButton = document.querySelector('button[onclick="toggleComparisonMode()"]');
            
            if (comparisonMode) {
                comparisonPanel.style.display = 'block';
                toggleButton.textContent = '📊 Exit Comparison Mode';
                toggleButton.className = 'btn btn-warning';
                
                // Reload results to add comparison buttons
                loadResults();
            } else {
                comparisonPanel.style.display = 'none';
                toggleButton.textContent = '📊 Compare Simulations';
                toggleButton.className = 'btn btn-info';
                selectedResults = [];
                updateComparisonChart();
                
                // Reload results to remove comparison buttons
                loadResults();
            }
        }
        
        function selectResult(filename) {
            if (selectedResults.length < 3) {
                selectedResults.push(filename);
                const selectedResultsDiv = document.getElementById('selected-results');
                selectedResultsDiv.innerHTML += `<p>${filename}</p>`;
            } else {
                alert('Maximum 3 results can be selected for comparison');
            }
        }
        
        function clearComparison() {
            selectedResults = [];
            const selectedResultsDiv = document.getElementById('selected-results');
            selectedResultsDiv.innerHTML = '';
        }
        
        function compareSelected() {
            if (selectedResults.length > 1) {
                const comparisonChartContainer = document.getElementById('comparison-chart-container');
                comparisonChartContainer.style.display = 'block';
                document.getElementById('comparison-tab').style.display = 'inline-block';
                
                // Create comparison chart
                const ctx = document.getElementById('comparison-chart').getContext('2d');
                if (window.comparisonChart) {
                    window.comparisonChart.destroy();
                }
                
                const datasets = [];
                
                selectedResults.forEach((result, index) => {
                    const color = `hsl(${(index * 137.5) % 360}, 100%, 50%)`;
                    datasets.push({
                        label: result,
                        data: [],
                        borderColor: color,
                        backgroundColor: color,
                        fill: false,
                        tension: 0.4
                    });
                });
                
                // Load data for each result
                Promise.all(selectedResults.map(result => fetch(`/api/results/${result}`).then(response => response.json()))).then(data => {
                    const maxLength = Math.max(...data.map(d => d.rows.length));
                    
                    for (let i = 0; i < maxLength; i++) {
                        data.forEach((result, index) => {
                            const row = result.rows[i];
                            if (row) {
                                datasets[index].data.push({x: row[1], y: row[5]});
                            } else {
                                datasets[index].data.push({x: i, y: null});
                            }
                        });
                    }
                    
                    window.comparisonChart = new Chart(ctx, {
                        type: 'line',
                        data: { datasets: datasets },
                        options: {
                            responsive: true,
                            maintainAspectRatio: false,
                            plugins: {
                                title: {
                                    display: true,
                                    text: '📊 Multi-Simulation Comparison'
                                }
                            },
                            scales: {
                                x: {
                                    title: { display: true, text: '⏱️ Time (seconds)' }
                                },
                                y: {
                                    title: { display: true, text: '🏔️ Altitude (meters)' }
                                }
                            }
                        }
                    });
                });
            } else {
                alert('Please select at least 2 results for comparison');
            }
        }
        
        function updateComparisonChart() {
            if (window.comparisonChart) {
                window.comparisonChart.destroy();
            }
        }
        
        function exportReport() {
            // TO DO: implement report export functionality
            alert('Report export is not implemented yet');
        }
        
        function loadTemplate(templateName) {
            const templates = {
                suborbital: {
                    name: "Suborbital Flight",
                    description: "High altitude trajectory with parabolic arc",
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
                        plugins: [
                            {
                                name: "propulsion",
                                type: 1,
                                enabled: true,
                                parameters: {
                                    sea_level_thrust: 50000.0,
                                    specific_impulse_sl: 280.0,
                                    engine_on: true,
                                    throttle_setting: 0.8
                                }
                            }
                        ]
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
                        plugins: [
                            {
                                name: "aerodynamics",
                                type: 2,
                                enabled: true,
                                parameters: {
                                    reference_area: 10.0,
                                    drag_coefficient: 0.3,
                                    enable_drag: true
                                }
                            }
                        ]
                    }
                },
                landing: {
                    name: "Landing Simulation",
                    description: "Controlled descent and landing sequence",
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
                        plugins: [
                            {
                                name: "propulsion",
                                type: 1,
                                enabled: true,
                                parameters: {
                                    sea_level_thrust: 15000.0,
                                    specific_impulse_sl: 320.0,
                                    engine_on: true,
                                    throttle_setting: 0.4
                                }
                            }
                        ]
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
                        plugins: [
                            {
                                name: "propulsion",
                                type: 1,
                                enabled: true,
                                parameters: {
                                    sea_level_thrust: 100000.0,
                                    specific_impulse_sl: 350.0,
                                    engine_on: true,
                                    throttle_setting: 1.0
                                }
                            }
                        ]
                    }
                }
            };
            
            const template = templates[templateName];
            if (!template) {
                alert('Template not found: ' + templateName);
                return;
            }
            
            // Apply template configuration
            applyTemplateConfig(template.config);
            
            // Show confirmation
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status success';
            statusDiv.innerHTML = `✅ Loaded template: ${template.name}`;
            
            // Auto-hide status after 3 seconds
            setTimeout(() => {
                statusDiv.className = 'status';
                statusDiv.innerHTML = '';
            }, 3000);
        }
        
        function applyTemplateConfig(config) {
            // Apply simulation parameters
            if (config.simulation) {
                document.getElementById('time-step').value = config.simulation.time_step || 0.1;
                document.getElementById('duration').value = config.simulation.duration || 10.0;
            }
            
            // Apply physics parameters
            if (config.physics) {
                document.getElementById('enable-gravity').checked = config.physics.enable_gravity !== false;
                document.getElementById('gravity-magnitude').value = config.physics.gravity_magnitude || 9.81;
                document.getElementById('enable-drag').checked = config.physics.enable_atmospheric_drag !== false;
                document.getElementById('integrator-type').value = config.physics.integrator_type || 'runge_kutta_4';
                document.getElementById('vehicle-mass').value = config.physics.vehicle_mass || 1000;
                document.getElementById('drag-coefficient').value = config.physics.drag_coefficient || 0.3;
                document.getElementById('reference-area').value = config.physics.reference_area || 1.0;
            }
            
            // Apply propulsion plugin parameters
            if (config.propulsion) {
                document.getElementById('enable-propulsion').checked = config.propulsion.enabled !== false;
                document.getElementById('engine-type').value = config.propulsion.engine_type || 0;
                document.getElementById('sea-level-thrust').value = config.propulsion.sea_level_thrust || 50000;
                document.getElementById('vacuum-thrust').value = config.propulsion.vacuum_thrust || 55000;
                document.getElementById('isp-sea-level').value = config.propulsion.specific_impulse_sl || 250;
                document.getElementById('isp-vacuum').value = config.propulsion.specific_impulse_vac || 280;
                document.getElementById('fuel-mass').value = config.propulsion.initial_fuel_mass || 500;
                document.getElementById('throttle-setting').value = config.propulsion.throttle_setting || 1.0;
                document.getElementById('thrust-vector-angle').value = config.propulsion.thrust_vector_angle || 0;
                document.getElementById('nozzle-exit-area').value = config.propulsion.nozzle_exit_area || 0.1;
            }
            
            // Apply plugin configurations
            if (config.plugins && Array.isArray(config.plugins)) {
                // Reset all plugins to disabled first
                availablePlugins.forEach(plugin => {
                    plugin.enabled = false;
                });
                
                // Enable and configure specified plugins
                config.plugins.forEach(templatePlugin => {
                    const plugin = availablePlugins.find(p => p.name === templatePlugin.name);
                    if (plugin) {
                        plugin.enabled = templatePlugin.enabled;
                        if (templatePlugin.parameters) {
                            Object.assign(plugin.parameters, templatePlugin.parameters);
                        }
                    }
                });
                
                // Refresh plugin display
                loadPlugins();
            }
        }
    </script>
</body>
</html>'''
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(html.encode())
    
    def serve_config(self):
        """Serve current configuration"""
        config = {
            "simulation": {
                "time_step": 0.1,
                "duration": 10.0,
                "max_iterations": 100,
                "enable_logging": True,
                "log_level": "INFO"
            },
            "physics": {
                "enable_gravity": True,
                "gravity_magnitude": 9.81,
                "enable_atmospheric_drag": False,
                "integration_tolerance": 1e-6,
                "integrator_type": "runge_kutta_4"
            },
            "plugins": [],
            "output": {
                "enable_csv": True,
                "enable_json": False,
                "enable_binary": False,
                "output_interval": 1,
                "output_directory": "output"
            }
        }
        self.send_json_response(config)
    
    def serve_plugins(self):
        """Serve available plugins"""
        plugins = []
        
        # Check build/plugins, build/lib, and build/bin directories
        plugins_dirs = [
            self.build_dir / "plugins",
            self.build_dir / "lib",
            self.build_dir / "bin"
        ]
        
        plugin_definitions = {
            "example_plugin": {
                "name": "Example Plugin",
                "type": 1,
                "description": "Example plugin for testing and demonstration",
                "enabled": False
            },
            "aerodynamics": {
                "name": "Aerodynamics",
                "type": 1,
                "description": "Real aerodynamic forces: drag, lift, compressibility effects",
                "enabled": False,
                "parameters": {
                    "reference_area": 0.785,
                    "drag_coefficient": 0.3,
                    "enable_drag": True,
                    "enable_altitude_effects": True
                }
            },
            "propulsion": {
                "name": "Propulsion",
                "type": 1,
                "description": "Rocket/jet propulsion with fuel consumption and altitude compensation",
                "enabled": False,
                "parameters": {
                    "sea_level_thrust": 50000.0,
                    "specific_impulse_sl": 250.0,
                    "engine_on": True,
                    "throttle_setting": 0.8
                }
            },
            "structures": {
                "name": "Structures",
                "type": 0,
                "description": "Mass tracking, center of gravity, and structural analysis",
                "enabled": False,
                "parameters": {
                    "enable_mass_tracking": True,
                    "enable_inertia_calculation": True,
                    "enable_structural_analysis": True
                }
            },
            "environment": {
                "name": "Environment",
                "type": 1,
                "description": "Atmospheric model, wind effects, and gravity variation",
                "enabled": False,
                "parameters": {
                    "enable_atmospheric_model": True,
                    "enable_wind_effects": True,
                    "enable_gravity_variation": True
                }
            },
            "programming": {
                "name": "Programming",
                "type": 0,
                "description": "Programmable logic and control sequences",
                "enabled": False
            }
        }
        
        # Determine plugin extension based on platform
        if platform.system() == "Windows":
            plugin_pattern = "lib*.dll"
        elif platform.system() == "Darwin":
            plugin_pattern = "lib*.dylib"
        else:  # Linux
            plugin_pattern = "lib*.so"
        
        for plugins_dir in plugins_dirs:
            if plugins_dir.exists():
                for plugin_file in plugins_dir.glob(plugin_pattern):
                    # Extract plugin name from filename
                    plugin_name = plugin_file.stem.replace("lib", "")
                    
                    if plugin_name in plugin_definitions:
                        plugin_def = plugin_definitions[plugin_name].copy()
                        plugin_def["library_path"] = str(plugin_file.absolute())
                        plugins.append(plugin_def)
        
        self.send_json_response(plugins)

    def get_simulator_path(self):
        """Return the correct simulator binary path for the current platform."""
        bin_dir = self.build_dir / "bin"
        if platform.system() == "Windows":
            sim_path = bin_dir / "simulator.exe"
        else:
            sim_path = bin_dir / "simulator"
        return sim_path

    def serve_status(self):
        """Serve system status including simulation state"""
        simulator_path = self.get_simulator_path()
        simulator_exists = simulator_path.exists()
        with MoLabWebHandler._sim_lock:
            sim = dict(MoLabWebHandler._sim_status)
        status = {
            "simulator_built": simulator_exists,
            "simulator_path": str(simulator_path),
            "project_root": str(self.project_root),
            "timestamp": datetime.datetime.now().isoformat(),
            "simulation": sim
        }
        self.send_json_response(status)
    
    def serve_results(self):
        """Serve simulation results"""
        results = []
        
        # Search in multiple output directories
        output_dirs = [
            self.output_dir,  # build/output
            self.project_root / "output",  # output
            self.project_root / "tools" / "output",  # tools/output
            self.project_root / "results"  # results
        ]
        
        for output_dir in output_dirs:
            if output_dir.exists():
                for result_file in output_dir.glob("*.csv"):
                    results.append({
                        "filename": result_file.name,
                        "path": str(result_file),
                        "size": result_file.stat().st_size,
                        "modified": result_file.stat().st_mtime
                    })
        
        # Sort by modification time (newest first)
        results.sort(key=lambda x: x['modified'], reverse=True)
        self.send_json_response(results)
    
    def serve_result_data(self, filename):
        """Serve specific result file data for visualization"""
        try:
            # Search in multiple output directories
            output_dirs = [
                self.output_dir,  # build/output
                self.project_root / "output",  # output
                self.project_root / "tools" / "output",  # tools/output
                self.project_root / "results"  # results
            ]
            
            result_file = None
            for output_dir in output_dirs:
                potential_file = output_dir / filename
                if potential_file.exists():
                    result_file = potential_file
                    break
            
            if not result_file:
                self.send_json_response({"error": "File not found"})
                return
            
            # Read CSV data
            import csv
            data = {
                "headers": [],
                "rows": [],
                "summary": {}
            }
            
            with open(result_file, 'r') as f:
                reader = csv.reader(f)
                headers = next(reader)
                data["headers"] = headers
                
                rows = []
                for row in reader:
                    # Convert numeric values
                    numeric_row = []
                    for i, value in enumerate(row):
                        try:
                            numeric_row.append(float(value))
                        except ValueError:
                            numeric_row.append(value)
                    rows.append(numeric_row)
                
                data["rows"] = rows
            
            # Generate summary statistics
            if rows:
                # Calculate distance traveled
                total_distance = 0
                if len(rows) > 1:
                    for i in range(1, len(rows)):
                        if len(rows[i]) >= 6 and len(rows[i-1]) >= 6:
                            dx = rows[i][3] - rows[i-1][3]  # position_x
                            dy = rows[i][4] - rows[i-1][4]  # position_y
                            dz = rows[i][5] - rows[i-1][5]  # position_z
                            total_distance += (dx**2 + dy**2 + dz**2)**0.5
                
                data["summary"] = {
                    "total_points": len(rows),
                    "duration": rows[-1][1] - rows[0][1] if len(rows) > 1 else 0,  # simulation_time
                    "initial_position": {
                        "x": rows[0][3] if len(rows[0]) > 3 else 0,  # position_x
                        "y": rows[0][4] if len(rows[0]) > 4 else 0,  # position_y
                        "z": rows[0][5] if len(rows[0]) > 5 else 0   # position_z
                    } if len(rows) > 0 else {"x": 0, "y": 0, "z": 0},
                    "final_position": {
                        "x": rows[-1][3] if len(rows[-1]) > 3 else 0,  # position_x
                        "y": rows[-1][4] if len(rows[-1]) > 4 else 0,  # position_y
                        "z": rows[-1][5] if len(rows[-1]) > 5 else 0   # position_z
                    } if len(rows) > 0 else {"x": 0, "y": 0, "z": 0},
                    "initial_velocity": {
                        "x": rows[0][6] if len(rows[0]) > 6 else 0,  # velocity_x
                        "y": rows[0][7] if len(rows[0]) > 7 else 0,  # velocity_y
                        "z": rows[0][8] if len(rows[0]) > 8 else 0   # velocity_z
                    } if len(rows) > 0 else {"x": 0, "y": 0, "z": 0},
                    "final_velocity": {
                        "x": rows[-1][6] if len(rows[-1]) > 6 else 0,  # velocity_x
                        "y": rows[-1][7] if len(rows[-1]) > 7 else 0,  # velocity_y
                        "z": rows[-1][8] if len(rows[-1]) > 8 else 0   # velocity_z
                    } if len(rows) > 0 else {"x": 0, "y": 0, "z": 0},
                    "distance_traveled": total_distance
                }
            
            self.send_json_response(data)
            
        except Exception as e:
            self.send_json_response({"error": str(e)})
    
    def serve_plots_list(self):
        """List available plot images"""
        plots = []
        output_dirs = [
            self.output_dir,
            self.project_root / "output"
        ]
        
        for output_dir in output_dirs:
            plots_dir = output_dir / "plots"
            if plots_dir.exists():
                for plot_file in plots_dir.glob("*.png"):
                    plots.append({
                        "filename": plot_file.name,
                        "size": plot_file.stat().st_size,
                        "modified": plot_file.stat().st_mtime,
                        "url": f"/api/plots/{plot_file.name}"
                    })
        
        plots.sort(key=lambda x: x['modified'], reverse=True)
        self.send_json_response(plots)
    
    def serve_plot_file(self, filename):
        """Serve a specific plot PNG file"""
        import os
        # Sanitize filename
        filename = os.path.basename(filename)
        if not filename.endswith('.png'):
            self.send_error(400, "Only PNG files supported")
            return
        
        output_dirs = [
            self.output_dir,
            self.project_root / "output"
        ]
        
        for output_dir in output_dirs:
            plot_path = output_dir / "plots" / filename
            if plot_path.exists():
                try:
                    with open(plot_path, 'rb') as f:
                        content = f.read()
                    self.send_response(200)
                    self.send_header('Content-type', 'image/png')
                    self.send_header('Content-Length', str(len(content)))
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()
                    self.wfile.write(content)
                    return
                except Exception as e:
                    self.send_error(500, str(e))
                    return
        
        self.send_error(404, "Plot not found")
    
    def handle_run_simulation(self):
        """Handle simulation run request"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        
        try:
            config = json.loads(post_data.decode())
            
            # Create temporary config file
            temp_config = self.config_dir / f"temp_web_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            config["initial_state_file"] = str(self.project_root / "data" / "defaults" / "default_state.json")
            
            with open(temp_config, 'w') as f:
                json.dump(config, f, indent=2)
            
            # Run simulation in background
            simulator_path = self.get_simulator_path()
            if not simulator_path.exists():
                self.send_json_response({"success": False, "error": "Simulator not built"})
                return
            
            ticks = int(config['simulation']['duration'] / config['simulation']['time_step'])
            
            project_root = self.project_root
            output_dir = self.output_dir

            # Mark running BEFORE starting thread (prevents frontend from seeing stale state)
            with MoLabWebHandler._sim_lock:
                MoLabWebHandler._sim_status = {"state": "running", "message": "Simulator starting..."}

            def run_sim():
                cls = MoLabWebHandler
                try:
                    with cls._sim_lock:
                        cls._sim_status = {"state": "running", "message": "Simulator running..."}

                    result = subprocess.run([
                        str(simulator_path),
                        "--config", str(temp_config),
                        "--ticks", str(ticks)
                    ], capture_output=True, text=True, timeout=300,
                    cwd=str(project_root))

                    if result.returncode == 0 and temp_config.exists():
                        temp_config.unlink()

                    if result.returncode != 0:
                        err = result.stderr[:500] if result.stderr else ""
                        if not err and result.stdout:
                            # Simulator logs to stdout; grab last meaningful lines
                            lines = [l for l in result.stdout.strip().split('\n') if l.strip()]
                            err = '\n'.join(lines[-5:])[:500]
                        if not err:
                            err = f"exit code {result.returncode}"
                        print(f"[SIM] Simulator failed (rc={result.returncode}):\nSTDOUT: {result.stdout[-1000:] if result.stdout else ''}\nSTDERR: {result.stderr[-500:] if result.stderr else ''}")
                        with cls._sim_lock:
                            cls._sim_status = {"state": "failed", "message": f"Simulator error: {err}"}
                        return

                    # Generate visualization plots
                    with cls._sim_lock:
                        cls._sim_status = {"state": "running", "message": "Generating plots..."}

                    try:
                        visualizer = project_root / "tools" / "visualize_results.py"
                        if visualizer.exists():
                            out_dir = project_root / "output"
                            if not out_dir.exists():
                                out_dir = output_dir
                            plots_dir = out_dir / "plots"
                            viz_result = subprocess.run([
                                sys.executable, str(visualizer),
                                "--output-dir", str(out_dir),
                                "--plots-dir", str(plots_dir),
                                "--no-show"
                            ], capture_output=True, text=True, timeout=60,
                            cwd=str(project_root))
                            if viz_result.returncode == 0:
                                print(f"[VIZ] Plots generated in {plots_dir}")
                            else:
                                print(f"[VIZ] Error (rc={viz_result.returncode}): {viz_result.stderr}")
                            if viz_result.stdout:
                                print(f"[VIZ] {viz_result.stdout}")
                    except Exception as viz_err:
                        print(f"[VIZ] Plot generation warning: {viz_err}")

                    with cls._sim_lock:
                        cls._sim_status = {"state": "completed", "message": "Simulation completed"}

                except Exception as e:
                    print(f"Simulation error: {e}")
                    with cls._sim_lock:
                        cls._sim_status = {"state": "failed", "message": str(e)}

            sim_thread = threading.Thread(target=run_sim)
            sim_thread.daemon = True
            sim_thread.start()

            self.send_json_response({"success": True, "message": "Simulation started"})
            
        except Exception as e:
            self.send_json_response({"success": False, "error": str(e)})

    
    def handle_save_config(self):
        """Handle save configuration request"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        
        try:
            data = json.loads(post_data.decode())
            filename = data.get('filename', 'web_config')
            config = data.get('config', {})
            
            config_file = self.config_dir / f"{filename}.json"
            with open(config_file, 'w') as f:
                json.dump(config, f, indent=2)
            
            self.send_json_response({"success": True, "message": f"Config saved as {filename}.json"})
            
        except Exception as e:
            self.send_json_response({"success": False, "error": str(e)})
    
    def handle_load_config(self):
        """Handle load configuration request"""
        configs = []
        if self.config_dir.exists():
            for config_file in self.config_dir.glob("*.json"):
                try:
                    with open(config_file, 'r') as f:
                        config_data = json.load(f)
                    configs.append({
                        "filename": config_file.stem,
                        "config": config_data
                    })
                except:
                    pass
        self.send_json_response(configs)
    
    def send_json_response(self, data):
        """Send JSON response"""
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

def main():
    """Start the web server"""
    port = 8082
    
    print(f"[WEB] Starting MoLab Web Interface...")
    print(f"[WEB] Server will be available at: http://localhost:{port}")
    print(f"[WEB] Project root: {Path(__file__).parent.parent}")
    
    try:
        with socketserver.TCPServer(("", port), MoLabWebHandler) as httpd:
            print(f"[OK] Server started on port {port}")
            print(f"[WEB] Opening browser...")
            
            # Try to open browser
            try:
                webbrowser.open(f'http://localhost:{port}')
            except:
                print("[WARN] Could not open browser automatically")
                print(f"   Please open: http://localhost:{port}")
            
            print(f"[INFO] Press Ctrl+C to stop the server")
            httpd.serve_forever()
            
    except KeyboardInterrupt:
        print(f"\n[INFO] Server stopped")
    except Exception as e:
        print(f"[ERROR] Error starting server: {e}")

if __name__ == "__main__":
    main()
