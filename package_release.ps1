# NWSpeech Release Package Script
# For building Release version and creating distributable package

param(
    [string]$QtDir = $env:QtDir,
    [switch]$SkipBuild
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "NWSpeech Release Package Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"
$ReleaseDir = Join-Path $BuildDir "Release"
$PackageDir = Join-Path $ScriptDir "NWSpeech_Release"
$ModelsDir = Join-Path $ScriptDir "models"

Write-Host "Working directory: $ScriptDir" -ForegroundColor Yellow
Write-Host "Build directory: $BuildDir" -ForegroundColor Yellow
Write-Host "Package directory: $PackageDir" -ForegroundColor Yellow
Write-Host ""

# Step 1: Check if models folder exists
Write-Host "[1/5] Checking models folder..." -ForegroundColor Green
if (-not (Test-Path $ModelsDir)) {
    Write-Host "Error: models folder not found: $ModelsDir" -ForegroundColor Red
    exit 1
}
Write-Host "  OK" -ForegroundColor Green
Write-Host ""

# Step 2: Build Release version
if (-not $SkipBuild) {
    Write-Host "[2/5] Building Release version..." -ForegroundColor Green
    
    if (Test-Path $BuildDir) {
        Write-Host "  Cleaning old build directory..." -ForegroundColor Gray
        Remove-Item -Path $BuildDir -Recurse -Force
    }
    Write-Host "  Creating build directory..." -ForegroundColor Gray
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    
    Set-Location $BuildDir
    Write-Host "  Running CMake configure..." -ForegroundColor Gray
    cmake .. -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: CMake configure failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "  Building Release..." -ForegroundColor Gray
    cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Build failed" -ForegroundColor Red
        exit 1
    }
    
    Set-Location $ScriptDir
    Write-Host "  OK" -ForegroundColor Green
} else {
    Write-Host "[2/5] Skipping build step" -ForegroundColor Yellow
}
Write-Host ""

# Step 3: Clean and create package directory
Write-Host "[3/5] Preparing package directory..." -ForegroundColor Green
if (Test-Path $PackageDir) {
    Write-Host "  Cleaning old package directory..." -ForegroundColor Gray
    Remove-Item -Path $PackageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $PackageDir | Out-Null
Write-Host "  OK" -ForegroundColor Green
Write-Host ""

# Step 4: Copy files
Write-Host "[4/5] Copying files..." -ForegroundColor Green

$ExePath = Join-Path $ReleaseDir "nwspeech.exe"
if (-not (Test-Path $ExePath)) {
    Write-Host "Error: Executable not found: $ExePath" -ForegroundColor Red
    exit 1
}

Write-Host "  Copying Release directory..." -ForegroundColor Gray
Copy-Item -Path "$ReleaseDir\*" -Destination $PackageDir -Recurse

Write-Host "  Copying models folder..." -ForegroundColor Gray
Copy-Item -Path $ModelsDir -Destination $PackageDir -Recurse

Write-Host "  OK" -ForegroundColor Green
Write-Host ""

# Step 5: Use windeployqt to handle Qt dependencies
Write-Host "[5/5] Processing Qt dependencies with windeployqt..." -ForegroundColor Green

if ([string]::IsNullOrEmpty($QtDir)) {
    Write-Host "Warning: QtDir environment variable not set, please specify -QtDir parameter" -ForegroundColor Yellow
    Write-Host "Trying to find windeployqt automatically..." -ForegroundColor Gray
    
    $WinDeployQt = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if (-not $WinDeployQt) {
        Write-Host "Error: windeployqt.exe not found, please ensure Qt is in PATH or set QtDir environment variable" -ForegroundColor Red
        exit 1
    }
    $WinDeployQtPath = $WinDeployQt.Source
} else {
    Write-Host "  Using Qt from: $QtDir" -ForegroundColor Gray
    $WinDeployQtPath = Join-Path $QtDir "bin\windeployqt.exe"
    if (-not (Test-Path $WinDeployQtPath)) {
        Write-Host "Error: windeployqt.exe not found: $WinDeployQtPath" -ForegroundColor Red
        exit 1
    }
}

Write-Host "  Using windeployqt: $WinDeployQtPath" -ForegroundColor Gray
& $WinDeployQtPath --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $PackageDir "nwspeech.exe")
if ($LASTEXITCODE -ne 0) {
    Write-Host "Warning: windeployqt failed, may need to copy dependencies manually" -ForegroundColor Yellow
} else {
    Write-Host "  OK" -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Package complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Package location: $PackageDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "Please test the package to ensure it works correctly!" -ForegroundColor Yellow
