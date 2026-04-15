# MoLab Dependencies Checker (Windows-native)

$ErrorActionPreference = 'Stop'

Write-Host "🔧 MoLab dependency check (PowerShell)"
Write-Host ""

function Test-Tool {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Show-InstallHelp {
    param([string]$Package)
    Write-Host "   Install $Package with one of:"
    Write-Host "   - winget install Kitware.CMake / Python.Python.3 / Git.Git"
    Write-Host "   - choco install cmake python git ninja"
}

Write-Host "🔍 CMake"
if (Test-Tool 'cmake') {
    & cmake --version | Select-Object -First 1
} else {
    Write-Host "❌ CMake not found"
    Show-InstallHelp 'cmake'
}

Write-Host ""
Write-Host "🔍 Ninja"
if (Test-Tool 'ninja') {
    & ninja --version
} else {
    Write-Host "⚠️  Ninja not found (recommended generator)"
    Show-InstallHelp 'ninja'
}

Write-Host ""
Write-Host "🔍 Python 3"
if (Test-Tool 'py') {
    & py -3 --version
} elseif (Test-Tool 'python') {
    & python --version
} else {
    Write-Host "❌ Python 3 not found"
    Show-InstallHelp 'python'
}

Write-Host ""
Write-Host "🔍 C++ compiler"
if (Test-Tool 'cl') {
    Write-Host "✅ MSVC cl.exe detected"
} elseif (Test-Tool 'clang++') {
    & clang++ --version | Select-Object -First 1
} elseif (Test-Tool 'g++') {
    & g++ --version | Select-Object -First 1
} else {
    Write-Host "❌ No C++ compiler found"
    Write-Host "   Install Visual Studio Build Tools (Desktop development with C++)"
}

Write-Host ""
Write-Host "🔍 Git"
if (Test-Tool 'git') {
    & git --version
} else {
    Write-Host "❌ Git not found"
    Show-InstallHelp 'git'
}

Write-Host ""
Write-Host "✅ Dependency check finished"
Write-Host "Next steps:"
Write-Host "1) cmake -S . -B build -G Ninja"
Write-Host "2) cmake --build build --parallel"
Write-Host "3) .\scripts\launch_web.ps1"
