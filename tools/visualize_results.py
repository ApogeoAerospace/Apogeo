#!/usr/bin/env python3
"""
MoLab Simulation Results Visualizer

This script loads simulation output CSV files and generates a standardized
set of key plots for quick visual analysis of simulation results.

Usage:
    python visualize_results.py
    python visualize_results.py --file output/molab_simulation_20251126_222234.csv
    python visualize_results.py --no-show
    python visualize_results.py --plots-dir output/my_plots
"""
import sys
import os
import argparse
import json
import warnings
from pathlib import Path

import pandas as pd
import numpy as np
import matplotlib
# Backend must be set BEFORE importing pyplot (critical for subprocess/headless)
if "--no-show" in sys.argv:
    matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="MoLab Simulation Results Visualizer — generates key plots from CSV output."
    )
    parser.add_argument(
        "--file", type=str, default=None,
        help="Path to a specific CSV file. If omitted, the most recent CSV in --output-dir is used."
    )
    parser.add_argument(
        "--output-dir", type=str, default="output",
        help="Directory where simulation CSVs are stored (default: output)."
    )
    parser.add_argument(
        "--plots-dir", type=str, default="output/plots",
        help="Directory where plot images will be saved (default: output/plots)."
    )
    parser.add_argument(
        "--no-show", action="store_true",
        help="Do not display plots on screen; only save PNG files."
    )
    return parser.parse_args()


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_csv_data(file_path: str = None, output_dir: str = "output") -> pd.DataFrame:
    """Load simulation CSV data into a DataFrame.

    If *file_path* is provided, that file is loaded directly.
    Otherwise the most recent ``molab_simulation_*.csv`` in *output_dir* is used.
    """
    if file_path is not None:
        csv_path = Path(file_path)
        if not csv_path.exists():
            print(f"Error: file '{file_path}' not found.")
            sys.exit(1)
    else:
        out = Path(output_dir)
        if not out.exists():
            print(f"Error: output directory '{output_dir}' not found.")
            sys.exit(1)
        csv_files = sorted(out.glob("molab_simulation_*.csv"))
        if not csv_files:
            print(f"Error: no CSV files found in '{output_dir}'.")
            sys.exit(1)
        csv_path = csv_files[-1]

    print(f"Loading: {csv_path}")
    df = pd.read_csv(csv_path)

    if df.empty:
        print("Error: CSV file is empty (no data rows).")
        sys.exit(1)

    return df, csv_path


def load_metadata(csv_path: Path) -> dict:
    """Load JSON metadata associated with a CSV file.

    Returns the ``metadata`` dict or an empty dict on failure.
    """
    json_path = csv_path.with_suffix(".json")
    if not json_path.exists():
        return {}
    try:
        with open(json_path, "r") as f:
            data = json.load(f)
        return data.get("metadata", {})
    except (json.JSONDecodeError, KeyError):
        return {}


# ---------------------------------------------------------------------------
# Derived columns
# ---------------------------------------------------------------------------

def compute_derived_columns(df: pd.DataFrame) -> pd.DataFrame:
    """Add computed columns: speed, downrange, altitude."""
    if {"velocity_x", "velocity_y", "velocity_z"}.issubset(df.columns):
        df["speed"] = np.sqrt(
            df["velocity_x"] ** 2 + df["velocity_y"] ** 2 + df["velocity_z"] ** 2
        )

    if {"position_x", "position_y"}.issubset(df.columns):
        df["downrange"] = np.sqrt(df["position_x"] ** 2 + df["position_y"] ** 2)

    if "position_z" in df.columns:
        df["altitude"] = df["position_z"]

    return df


# ---------------------------------------------------------------------------
# Plot helpers
# ---------------------------------------------------------------------------

def _time_column(df: pd.DataFrame) -> str:
    """Return the name of the time column present in the DataFrame."""
    if "simulation_time" in df.columns:
        return "simulation_time"
    if "time" in df.columns:
        return "time"
    return "tick"


def _apply_style():
    """Set a consistent matplotlib style with a safe fallback."""
    for style in ("seaborn-v0_8-whitegrid", "seaborn-whitegrid", "ggplot"):
        try:
            plt.style.use(style)
            return
        except OSError:
            continue


def _title_prefix(metadata: dict) -> str:
    """Build a title prefix from metadata."""
    name = metadata.get("run_name", "")
    return f"{name} — " if name else "MoLab — "


def _save(fig: plt.Figure, plots_dir: str, filename: str):
    """Save a figure to *plots_dir/filename*."""
    path = Path(plots_dir) / filename
    fig.savefig(path, dpi=150, bbox_inches="tight")
    print(f"  Saved: {path}")


# ---------------------------------------------------------------------------
# Individual plot functions
# ---------------------------------------------------------------------------

def plot_trajectory_3d(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 1 — 3-D trajectory coloured by simulation time."""
    required = {"position_x", "position_y", "position_z"}
    if not required.issubset(df.columns):
        warnings.warn(f"Skipping 3D trajectory: missing columns {required - set(df.columns)}")
        return None

    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection="3d")

    tcol = _time_column(df)
    sc = ax.scatter(
        df["position_x"], df["position_y"], df["position_z"],
        c=df[tcol], cmap="viridis", s=8, depthshade=True,
    )
    ax.plot(
        df["position_x"], df["position_y"], df["position_z"],
        color="gray", alpha=0.3, linewidth=0.5,
    )

    ax.scatter(*[df.iloc[0][c] for c in ("position_x", "position_y", "position_z")],
               color="green", s=80, marker="o", label="Start")
    ax.scatter(*[df.iloc[-1][c] for c in ("position_x", "position_y", "position_z")],
               color="red", s=80, marker="X", label="End")

    ax.set_xlabel("Position X (m)")
    ax.set_ylabel("Position Y (m)")
    ax.set_zlabel("Position Z (m)")
    ax.set_title(f"{_title_prefix(metadata)}3D Trajectory")
    ax.legend()
    fig.colorbar(sc, ax=ax, label=f"{tcol} (s)", shrink=0.6)

    _save(fig, plots_dir, "trajectory_3d.png")
    return fig


def plot_altitude_vs_time(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 2 — Altitude vs simulation time."""
    if "altitude" not in df.columns:
        warnings.warn("Skipping altitude plot: 'altitude' column missing.")
        return None

    tcol = _time_column(df)
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.plot(df[tcol], df["altitude"], linewidth=1.5, color="#1f77b4")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Altitude (m)")
    ax.set_title(f"{_title_prefix(metadata)}Altitude vs Time")
    ax.grid(True)

    _save(fig, plots_dir, "altitude_vs_time.png")
    return fig


def plot_speed_vs_time(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 3 — Speed magnitude vs simulation time."""
    if "speed" not in df.columns:
        warnings.warn("Skipping speed plot: 'speed' column missing.")
        return None

    tcol = _time_column(df)
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.plot(df[tcol], df["speed"], linewidth=1.5, color="#ff7f0e")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Speed (m/s)")
    ax.set_title(f"{_title_prefix(metadata)}Speed vs Time")
    ax.grid(True)

    _save(fig, plots_dir, "speed_vs_time.png")
    return fig


def plot_velocity_components(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 4 — Velocity components (x, y, z) vs simulation time."""
    required = {"velocity_x", "velocity_y", "velocity_z"}
    if not required.issubset(df.columns):
        warnings.warn(f"Skipping velocity components: missing columns {required - set(df.columns)}")
        return None

    tcol = _time_column(df)
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.plot(df[tcol], df["velocity_x"], linewidth=1.2, color="#d62728", label="Vx")
    ax.plot(df[tcol], df["velocity_y"], linewidth=1.2, color="#2ca02c", label="Vy")
    ax.plot(df[tcol], df["velocity_z"], linewidth=1.2, color="#1f77b4", label="Vz")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Velocity (m/s)")
    ax.set_title(f"{_title_prefix(metadata)}Velocity Components vs Time")
    ax.legend()
    ax.grid(True)

    _save(fig, plots_dir, "velocity_components.png")
    return fig


def plot_atmospheric(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 5 — Atmospheric conditions (density, pressure, temperature) vs time."""
    required = {"atm_density", "atm_pressure", "atm_temperature"}
    if not required.issubset(df.columns):
        warnings.warn(f"Skipping atmospheric plot: missing columns {required - set(df.columns)}")
        return None

    tcol = _time_column(df)
    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

    axes[0].plot(df[tcol], df["atm_density"], linewidth=1.2, color="#9467bd")
    axes[0].set_ylabel("Density (kg/m³)")
    axes[0].set_title(f"{_title_prefix(metadata)}Atmospheric Conditions")
    axes[0].grid(True)

    axes[1].plot(df[tcol], df["atm_pressure"], linewidth=1.2, color="#8c564b")
    axes[1].set_ylabel("Pressure (Pa)")
    axes[1].grid(True)

    axes[2].plot(df[tcol], df["atm_temperature"], linewidth=1.2, color="#e377c2")
    axes[2].set_ylabel("Temperature (K)")
    axes[2].set_xlabel("Time (s)")
    axes[2].grid(True)

    fig.tight_layout()
    _save(fig, plots_dir, "atmospheric_conditions.png")
    return fig


def plot_trajectory_2d(df: pd.DataFrame, metadata: dict, plots_dir: str) -> plt.Figure:
    """Plot 6 — 2-D flight profile (downrange vs altitude)."""
    if "downrange" not in df.columns or "altitude" not in df.columns:
        warnings.warn("Skipping 2D trajectory: 'downrange' or 'altitude' column missing.")
        return None

    fig, ax = plt.subplots(figsize=(12, 7))
    ax.plot(df["downrange"], df["altitude"], linewidth=1.5, color="#17becf")
    ax.plot(df["downrange"].iloc[0], df["altitude"].iloc[0],
            "go", markersize=10, label="Start")
    ax.plot(df["downrange"].iloc[-1], df["altitude"].iloc[-1],
            "rX", markersize=12, label="End")
    ax.set_xlabel("Downrange (m)")
    ax.set_ylabel("Altitude (m)")
    ax.set_title(f"{_title_prefix(metadata)}2D Flight Profile")
    ax.legend()
    ax.grid(True)

    _save(fig, plots_dir, "trajectory_2d_profile.png")
    return fig


# ---------------------------------------------------------------------------
# Orchestrator
# ---------------------------------------------------------------------------

def generate_all_plots(df: pd.DataFrame, metadata: dict, plots_dir: str, no_show: bool):
    """Generate all plots, save them, and optionally display them."""
    os.makedirs(plots_dir, exist_ok=True)

    print(f"\nGenerating plots in: {plots_dir}")
    print("-" * 50)

    plot_functions = [
        plot_trajectory_3d,
        plot_altitude_vs_time,
        plot_speed_vs_time,
        plot_velocity_components,
        plot_atmospheric,
        plot_trajectory_2d,
    ]

    generated = 0
    for plot_fn in plot_functions:
        fig = plot_fn(df, metadata, plots_dir)
        if fig is not None:
            generated += 1

    print("-" * 50)
    print(f"Generated {generated}/{len(plot_functions)} plots.")

    if not no_show:
        plt.show()
    else:
        plt.close("all")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    args = parse_arguments()

    _apply_style()

    df, csv_path = load_csv_data(file_path=args.file, output_dir=args.output_dir)
    metadata = load_metadata(csv_path)
    df = compute_derived_columns(df)

    print(f"Data points: {len(df)}")
    tcol = _time_column(df)
    print(f"Time range:  {df[tcol].iloc[0]:.3f}s — {df[tcol].iloc[-1]:.3f}s")
    if "speed" in df.columns:
        print(f"Speed range: {df['speed'].min():.3f} — {df['speed'].max():.3f} m/s")

    generate_all_plots(df, metadata, args.plots_dir, args.no_show)


if __name__ == "__main__":
    main()
