$ErrorActionPreference = 'Stop'
$baseline = (Get-Content -Raw vcpkg.json | ConvertFrom-Json).'builtin-baseline'
if ($baseline -notmatch '^[0-9a-f]{40}$') { throw 'Invalid vcpkg baseline' }
if (-not (Test-Path vcpkg)) {
    git clone https://github.com/microsoft/vcpkg.git vcpkg
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
git -C vcpkg checkout --detach $baseline
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not (Test-Path vcpkg/vcpkg.exe)) {
    & ./vcpkg/bootstrap-vcpkg.bat -disableMetrics
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
