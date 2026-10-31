# Build the Windows GUI and mesh tool from an MSYS2 installation.
#
# This is a convenience wrapper around build.sh. Run it from PowerShell:
#
#   .\build-windows.ps1
#   .\build-windows.ps1 -Jobs 8
#   .\build-windows.ps1 -Deploy
#
# MSYS2 must be installed with the MINGW64 toolchain and Qt 5; see
# docs/WINDOWS.md. The default location is C:\msys64. Pass -Msys2Root if
# yours is elsewhere.

param(
    [string]$Msys2Root = "C:\msys64",
    [int]$Jobs = 0,
    [switch]$Deploy
)

$ErrorActionPreference = "Stop"

$bash = Join-Path $Msys2Root "usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    throw "bash.exe was not found under $Msys2Root. Install MSYS2 or pass -Msys2Root."
}

$repo = (Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
if ($repo -notmatch '^[A-Za-z]:\\') {
    throw "The project path must be a normal Windows drive path: $repo"
}
if ($repo -match '\s') {
    throw "The project path contains a space. Move it to a path without spaces, for example C:\hazeron-shipgen: $repo"
}

# C:\Users\me\project -> /c/Users/me/project
$unixRepo = "/" + $repo.Substring(0, 1).ToLowerInvariant() + $repo.Substring(2).Replace('\', '/')
$unixRepo = $unixRepo.Replace("'", "'\''")

$command = "cd '$unixRepo' && ./build.sh"
if ($Jobs -gt 0) {
    $command += " -j$Jobs"
}
if ($Deploy) {
    $command += " && ./deploy-windows.sh"
}

$env:MSYSTEM = "MINGW64"
Write-Host "Running in the MSYS2 MINGW64 environment..."
& $bash -lc $command
exit $LASTEXITCODE
