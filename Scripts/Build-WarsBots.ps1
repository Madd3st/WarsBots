[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$ExperimentalSpawner,
    [switch]$Fresh,
    [ValidateRange(1, 64)][int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$warsRoot = Split-Path $PSScriptRoot -Parent
$warsVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $warsVswhere)) { throw 'Install Visual Studio Build Tools with C++ and CMake first.' }
$warsVs = & $warsVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $warsVs) { throw 'No complete MSVC x86/x64 installation found.' }
$warsArch = if ($Architecture -eq 'x64') { 'amd64' } else { 'x86' }
& (Join-Path $warsVs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch $warsArch -HostArch amd64 -SkipAutomaticLocation
# Keep /showIncludes stable across Windows locales for Ninja dependency discovery.
$env:VSLANG = '1033'
$warsBin = if ($Architecture -eq 'x64') { 'Bin64' } else { 'Bin32' }
$warsPreset = "$($Configuration.ToLowerInvariant())-$($warsBin.ToLowerInvariant())"
$warsVariant = if ($ExperimentalSpawner) { "$warsPreset-probe" } else { $warsPreset }
$warsOutput = Join-Path $warsRoot "build/$warsVariant/Mods/WarsBots/$warsBin"
$warsLogs = Join-Path $warsRoot "build/$warsVariant/logs"
$warsFlag = if ($ExperimentalSpawner) { 'ON' } else { 'OFF' }
New-Item -ItemType Directory -Force -Path $warsLogs | Out-Null
Push-Location $warsRoot
try {
    Get-Command cl, cmake, ninja | Select-Object Name, Source | Format-Table
    & cmake --version
    & ninja --version
    $warsConfigureArgs = @('--preset', $warsPreset, '-B', $warsOutput, "-DWARSBOTS_EXPERIMENTAL_SPAWNER=$warsFlag")
    if ($Fresh) { $warsConfigureArgs += '--fresh' }
    & cmake @warsConfigureArgs 2>&1 | Tee-Object (Join-Path $warsLogs 'configure.log')
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE). See $warsLogs" }
    & cmake --build $warsOutput --parallel $Jobs 2>&1 | Tee-Object (Join-Path $warsLogs 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). See $warsLogs" }
    $warsDll = Join-Path $warsOutput 'WarsBots.dll'
    if (-not (Test-Path -LiteralPath $warsDll)) { throw 'Build returned success but WarsBots.dll is missing.' }
    & dumpbin /headers /exports $warsDll 2>&1 | Tee-Object (Join-Path $warsLogs 'dll-inspection.log')
    if ($LASTEXITCODE -ne 0) { throw 'DLL inspection failed.' }
    Get-FileHash -LiteralPath $warsDll -Algorithm SHA256
} finally {
    Pop-Location
}
