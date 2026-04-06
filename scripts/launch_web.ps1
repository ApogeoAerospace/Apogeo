# MoLab Web Interface Launcher (Windows-native)

$ErrorActionPreference = 'Stop'

Set-Location (Join-Path $PSScriptRoot '..')

function Get-PythonCommand {
    if (Get-Command py -ErrorAction SilentlyContinue) { return @('py', '-3') }
    if (Get-Command python -ErrorAction SilentlyContinue) { return @('python') }
    if (Get-Command python3 -ErrorAction SilentlyContinue) { return @('python3') }
    return $null
}

$pythonCmd = Get-PythonCommand
if (-not $pythonCmd) {
    Write-Host "❌ Python 3 not found"
    exit 1
}

if (-not (Test-Path 'build')) {
    Write-Host "⚠️  Build directory not found. Configuring project..."
    & cmake -S . -B build -G Ninja
}

$simulator = 'build/bin/simulator.exe'
if (-not (Test-Path $simulator)) {
    Write-Host "⚠️  Simulator not found. Building project..."
    & cmake -S . -B build -G Ninja
    & cmake --build build --parallel
}

if (-not (Test-Path $simulator)) {
    Write-Host "❌ Could not find simulator binary after build: $simulator"
    exit 1
}

Write-Host "🌐 Starting MoLab web interface"
Write-Host "   URL: http://localhost:8082"
Write-Host "   Stop with Ctrl+C"

if ($pythonCmd.Length -gt 1) {
    & $pythonCmd[0] $pythonCmd[1] 'tools/molab_web_gui_v2.py'
} else {
    & $pythonCmd[0] 'tools/molab_web_gui_v2.py'
}
