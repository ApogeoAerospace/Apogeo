# Local CI smoke pipeline (Windows-native)

$ErrorActionPreference = 'Stop'

Set-Location (Join-Path $PSScriptRoot '..')

$buildDir = 'build'
$toolchainFile = Join-Path (Get-Location) 'vcpkg/scripts/buildsystems/vcpkg.cmake'

Write-Host "=== Configure (Release) ==="
$configureArgs = @('-B', $buildDir, '-S', '.', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
if (Test-Path $toolchainFile) {
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
} else {
    Write-Host "⚠️  vcpkg toolchain not found. Running CMake without explicit toolchain file."
}
& cmake @configureArgs

Write-Host "=== Build (Release) ==="
& cmake --build $buildDir --config Release --parallel

if (-not (Test-Path 'build/bin/simulator.exe')) {
    Write-Host "❌ simulator binary not found: build/bin/simulator.exe"
    exit 1
}

Write-Host "=== Configure tests (Debug) ==="
$testArgs = @('-B', $buildDir, '-S', '.', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DENABLE_TESTING=ON')
if (Test-Path $toolchainFile) {
    $testArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
}
& cmake @testArgs

Write-Host "=== Build tests (Debug) ==="
& cmake --build $buildDir --config Debug --parallel

Write-Host "=== Run unit tests ==="
& ctest --test-dir $buildDir --output-on-failure

Write-Host "=== Local CI smoke pipeline completed ==="
