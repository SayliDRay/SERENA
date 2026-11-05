# Convenience for the portable tools provisioned during project verification.
# On another machine use PlatformIO IDE's terminal: pio run -e xiao_esp32c3
param([ValidateSet('xiao_esp32c3','xiao_sim7600_bench','xiao_jury_demo')][string]$Environment = 'xiao_jury_demo')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$workspaceRoot = Split-Path -Parent $projectRoot
$nativePython = Join-Path $workspaceRoot '.tools\python-native\python.exe'
if (-not (Test-Path -LiteralPath $nativePython)) {
  throw 'Portable Python is absent. Use PlatformIO IDE: pio run -e xiao_esp32c3'
}
$previousCore = $env:PLATFORMIO_CORE_DIR
$mappedDrive = $null
try {
  # ESP32 GCC can exceed Windows' child-process command-line limit when the
  # Arduino SDK's many include paths repeat a long workspace path. Map only
  # this workspace to a temporary unused drive, then always unmap it.
  $usedDrives = [System.IO.Directory]::GetLogicalDrives()
  foreach ($letter in 'ZYXWVUTSRQPONMLKJIH'.ToCharArray()) {
    $candidate = "${letter}:"
    if ($usedDrives -notcontains "$candidate\") { $mappedDrive = $candidate; break }
  }
  if (-not $mappedDrive) { throw 'No unused temporary drive letter available.' }
  subst $mappedDrive $workspaceRoot
  if ($LASTEXITCODE -ne 0) { $mappedDrive = $null; throw 'Cannot map temporary build drive.' }
  $env:PLATFORMIO_CORE_DIR = "$mappedDrive\.platformio-local"
  & "$mappedDrive\.tools\python-native\python.exe" -m platformio run -d "$mappedDrive\nora-embedded" -e $Environment
  if ($LASTEXITCODE -ne 0) { throw "PlatformIO failed (exit $LASTEXITCODE)" }
} finally {
  $env:PLATFORMIO_CORE_DIR = $previousCore
  if ($mappedDrive) { subst $mappedDrive /D }
}
