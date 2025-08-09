#!/usr/bin/env python3
"""
MoLab Simulation Results Analyzer

This script analyzes the output from MoLab simulations and provides
comprehensive summaries of the simulation results.

Usage:
    python analyze_results.py [output_directory]
"""
import sys
import os
import json
from pathlib import Path
import csv
import math

def load_simulation_data(output_dir="output"):
    """Load the most recent simulation data"""
    if not os.path.exists(output_dir):
        print(f"Output directory '{output_dir}' not found!")
        return None, None
    
    # Find the most recent CSV file
    csv_files = list(Path(output_dir).glob("*.csv"))
    if not csv_files:
        print(f"No CSV files found in '{output_dir}'!")
        return None, None
    
    # Get the most recent file
    latest_csv = max(csv_files, key=os.path.getctime)
    print(f"Loading data from: {latest_csv}")
    
    # Load CSV data
    data = []
    with open(latest_csv, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for key in row:
                if key == 'tick':
                    try:
                        row[key] = int(row[key])
                    except ValueError:
                        pass
                else:
                    try:
                        row[key] = float(row[key])
                    except ValueError:
                        pass
            data.append(row)
    
    # Load JSON metadata if available
    metadata = None
    json_file = latest_csv.with_suffix('.json')
    if json_file.exists():
        try:
            with open(json_file, 'r') as f:
                json_data = json.load(f)
                metadata = json_data.get('metadata', {})
        except json.JSONDecodeError as e:
            print(f"Warning: Could not parse JSON file: {e}")
            metadata = {}
    
    return data, metadata

def calculate_speed(vel_x, vel_y, vel_z):
    """Calculate speed from velocity components"""
    return math.sqrt(vel_x**2 + vel_y**2 + vel_z**2)

def calculate_distance(x1, y1, z1, x2, y2, z2):
    """Calculate distance between two points"""
    return math.sqrt((x2-x1)**2 + (y2-y1)**2 + (z2-z1)**2)

def print_summary(data, metadata):
    """Print comprehensive summary of simulation results"""
    if not data:
        print("No data to analyze")
        return
    
    first_row = data[0]
    last_row = data[-1]
    
    print("============================================================")
    print("MOLAB SIMULATION RESULTS SUMMARY")
    print("============================================================")
    print(f"Run Name: {metadata.get('run_name', 'Unknown')}")
    print(f"Timestamp: {metadata.get('timestamp', 'Unknown')}")
    print(f"Output Interval: {metadata.get('output_interval', 1)}")
    print()
    print(f"Data Points: {len(data)}")
    print(f"Simulation Ticks: {first_row['tick']} to {last_row['tick']}")
    
    # Handle both old and new CSV formats
    time_col = 'simulation_time' if 'simulation_time' in data[0] else 'time'
    print(f"Time Range: {first_row[time_col]:.3f}s to {last_row[time_col]:.3f}s")
    
    # Show UTC time if available
    if 'utc_time' in data[0]:
        print(f"UTC Time Range: {first_row['utc_time']:.3f} to {last_row['utc_time']:.3f}")
    
    print()
    print(f"Initial Position: ({first_row['position_x']:.3f}, {first_row['position_y']:.3f}, {first_row['position_z']:.3f})")
    print(f"Final Position:   ({last_row['position_x']:.3f}, {last_row['position_y']:.3f}, {last_row['position_z']:.3f})")
    print()
    print(f"Initial Velocity: ({first_row['velocity_x']:.3f}, {first_row['velocity_y']:.3f}, {first_row['velocity_z']:.3f})")
    print(f"Final Velocity:   ({last_row['velocity_x']:.3f}, {last_row['velocity_y']:.3f}, {last_row['velocity_z']:.3f})")
    print()
    
    # Calculate distance traveled
    distance = ((last_row['position_x'] - first_row['position_x'])**2 + 
               (last_row['position_y'] - first_row['position_y'])**2 + 
               (last_row['position_z'] - first_row['position_z'])**2)**0.5
    print(f"Distance Traveled: {distance:.3f} units")
    print()
    
    # Speed statistics
    speeds = [calculate_speed(row['velocity_x'], row['velocity_y'], row['velocity_z']) for row in data]
    print("Speed Statistics:")
    print(f"  Initial: {speeds[0]:.3f}")
    print(f"  Final:   {speeds[-1]:.3f}")
    print(f"  Average: {sum(speeds) / len(speeds):.3f}")
    print(f"  Max:     {max(speeds):.3f}")
    print(f"  Min:     {min(speeds):.3f}")
    print()
    
    # Environmental conditions
    print("Atmospheric Conditions:")
    print(f"  Density:     {first_row['atm_density']:.3f} kg/m³")
    print(f"  Pressure:    {first_row['atm_pressure']:.1f} Pa")
    print(f"  Temperature: {first_row['atm_temperature']:.2f} K")
    print()
    
    print("Gravity Vector:")
    print(f"  ({first_row['gravity_x']:.3f}, {first_row['gravity_y']:.3f}, {first_row['gravity_z']:.3f}) m/s²")
    print()
    
    print("Wind Vector:")
    print(f"  ({first_row['wind_speed_x']:.3f}, {first_row['wind_speed_y']:.3f}, {first_row['wind_speed_z']:.3f}) m/s")
    print()
    
    # Show trajectory evolution (first few and last few points)
    print("Trajectory Evolution:")
    print("Tick | Time    | Position (x, y, z)           | Velocity (x, y, z)           | Speed")
    print("-" * 90)
    
    show_rows = min(5, len(data))
    for i in range(show_rows):
        row = data[i]
        print(f"{row['tick']:4.0f} | {row[time_col]:7.3f} | "
              f"({row['position_x']:8.2f}, {row['position_y']:8.2f}, {row['position_z']:8.2f}) | "
              f"({row['velocity_x']:8.2f}, {row['velocity_y']:8.2f}, {row['velocity_z']:8.2f}) | "
              f"{calculate_speed(row['velocity_x'], row['velocity_y'], row['velocity_z']):7.3f}")
    
    if len(data) > 10:
        print("  ...    ...     ...                            ...                            ...")
        for i in range(max(show_rows, len(data) - 3), len(data)):
            row = data[i]
            print(f"{row['tick']:4.0f} | {row[time_col]:7.3f} | "
                  f"({row['position_x']:8.2f}, {row['position_y']:8.2f}, {row['position_z']:8.2f}) | "
                  f"({row['velocity_x']:8.2f}, {row['velocity_y']:8.2f}, {row['velocity_z']:8.2f}) | "
                  f"{calculate_speed(row['velocity_x'], row['velocity_y'], row['velocity_z']):7.3f}")
    
    print()
    print("============================================================")
    print("Analysis complete!")
    print("For more detailed analysis, you can:")
    print("1. Open the CSV file in Excel or Google Sheets")
    print("2. Use Python with matplotlib for plotting")
    print("3. Import the JSON file into your favorite analysis tool")
    print("============================================================")

def main():
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "output"
    
    # Load data
    data, metadata = load_simulation_data(output_dir)
    if data is None:
        return
    
    # Print summary
    print_summary(data, metadata)

if __name__ == "__main__":
    main()
