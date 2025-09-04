#!/usr/bin/env pwsh
#
# Simple PowerShell script to record IQ files from KiwiSDR servers
# Uses the external kiwiclient project
#

param(
    [string]$Server = "wessex.zapto.org",
    [int]$Port = 8074,
    [double]$Freq = 7100.0,
    [string]$Mode = "amn",
    [int]$LowOffset = -2000,
    [int]$HighOffset = 2000,
    [int]$Duration = 30,
    [string]$Output,
    [switch]$Help
)

# Configuration
$KiwiRepo = "../kiwiclient"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Generate default output filename if not provided
if (-not $Output) {
    $Output = "kiwi_recording_${Timestamp}"
}

# Resolve data directory path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$DataDir = Join-Path $ProjectRoot "data"

function Show-Help {
    Write-Host "Usage: .\record_iq_from_kiwi.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Record IQ files from KiwiSDR servers using kiwiclient"
    Write-Host ""
    Write-Host "OPTIONS:"
    Write-Host "  -Server SERVER    KiwiSDR server (default: wessex.zapto.org)"
    Write-Host "  -Port PORT        Port number (default: 8074)"
    Write-Host "  -Freq FREQ        Frequency in kHz (default: 7100.0)"
    Write-Host "  -Mode MODE        Mode: iq, am, fm, etc. (default: amn)"
    Write-Host "  -LowOffset LOW    Low frequency offset in Hz (default: -2000)"
    Write-Host "  -HighOffset HIGH  High frequency offset in Hz (default: 2000)"
    Write-Host "  -Duration SEC     Recording duration in seconds (default: 30)"
    Write-Host "  -Output FILE      Output filename (default: auto-generated)"
    Write-Host "  -Help             Show this help"
    Write-Host ""
    Write-Host "EXAMPLES:"
    Write-Host "  .\record_iq_from_kiwi.ps1 -Server wessex.zapto.org -Freq 7100.0 -Duration 30"
    Write-Host "  .\record_iq_from_kiwi.ps1 -Server rx.kiwisdr.com -Freq 14100.0 -Mode iq -Duration 60"
    Write-Host ""
    Write-Host "REQUIREMENTS:"
    Write-Host "  - kiwiclient repository: git clone https://github.com/jks-prv/kiwiclient.git ../kiwiclient"
    Write-Host "  - Python dependencies: pip install mod-pywebsocket pyyaml"
    Write-Host ""
}

if ($Help) {
    Show-Help
    exit 0
}

# Check if kiwiclient repository exists
if (-not (Test-Path $KiwiRepo)) {
    Write-Host "ERROR: kiwiclient repository not found!" -ForegroundColor Red
    Write-Host "Expected location: $KiwiRepo"
    Write-Host ""
    Write-Host "Please clone it first:"
    Write-Host "  git clone https://github.com/jks-prv/kiwiclient.git ../kiwiclient" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Also install dependencies:"
    Write-Host "  pip install mod-pywebsocket"
    Write-Host "  pip install pyyaml  # optional" -ForegroundColor Yellow
    exit 1
}

# Check if kiwirecorder.py exists
$KiwiRecorderPath = Join-Path $KiwiRepo "kiwirecorder.py"
if (-not (Test-Path $KiwiRecorderPath)) {
    Write-Host "ERROR: kiwirecorder.py not found in kiwiclient repository!" -ForegroundColor Red
    Write-Host "Expected location: $KiwiRecorderPath"
    Write-Host "Please make sure the kiwiclient repository is properly cloned."
    exit 1
}

# Create data directory if it doesn't exist
if (-not (Test-Path $DataDir)) {
    New-Item -ItemType Directory -Path $DataDir -Force | Out-Null
}

# Full path for output file
$OutputPath = Join-Path $DataDir $Output

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "IQ Lab - KiwiSDR Recording Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Server:     $Server`:$Port"
Write-Host "Frequency:  $Freq kHz"
Write-Host "Mode:       $Mode"
Write-Host "Bandwidth:  $LowOffset to $HighOffset Hz"
Write-Host "Duration:   $Duration seconds"
Write-Host "Output:     $OutputPath"
Write-Host "==========================================" -ForegroundColor Cyan

# Check if Python is available
$PythonCmd = $null
if (Get-Command python3 -ErrorAction SilentlyContinue) {
    $PythonCmd = "python3"
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $PythonCmd = "python"
} else {
    Write-Host "ERROR: Python not found!" -ForegroundColor Red
    Write-Host "Please install Python 3.6+ from https://python.org"
    exit 1
}

# Test Python version and check for required modules
try {
    $pythonVersion = & $PythonCmd --version 2>&1
    Write-Host "Python version: $pythonVersion" -ForegroundColor Gray

    # Test if required modules are available
    $testModules = & $PythonCmd -c "import sys; print('Python path:', sys.executable)" 2>&1
    Write-Host "Python executable: $($testModules -replace 'Python path: ', '')" -ForegroundColor Gray
} catch {
    Write-Host "ERROR: Cannot execute Python properly" -ForegroundColor Red
    Write-Host "Please check your Python installation"
    exit 1
}

Write-Host "Starting recording..." -ForegroundColor Green

    # Build the command with frequency range parameters
    $KiwiRecorderPath = Join-Path $KiwiRepo "kiwirecorder.py"

    # Display frequency range information
    $LowFreq = $Freq + ($LowOffset / 1000.0)  # Convert Hz to kHz
    $HighFreq = $Freq + ($HighOffset / 1000.0)
    Write-Host "Frequency range: $LowFreq kHz to $HighFreq kHz" -ForegroundColor Cyan

    # Change to kiwiclient directory and run the recording
    Push-Location $KiwiRepo

    try {
        # Run the recording command with proper parameters
        $process = Start-Process -FilePath $PythonCmd -ArgumentList @(
            "kiwirecorder.py",
            "-s", $Server,
            "-p", $Port.ToString(),
            "--log_level=info",
            "-f", $Freq.ToString(),
            "-m", $Mode,
            "--tlimit", $Duration.ToString(),
            "--filename", $Output,
            "-d", $DataDir
        ) -NoNewWindow -Wait -PassThru

    # Check if recording was successful
    # Note: kiwirecorder creates .wav files, not .iq files
    $WavFilePath = $OutputPath + ".wav"
    if ($process.ExitCode -eq 0 -and (Test-Path $WavFilePath)) {
        $FileSize = (Get-Item $WavFilePath).Length
        $FileSizeMB = [math]::Round($FileSize / 1MB, 2)

        Write-Host ""
        Write-Host "✅ Recording completed successfully!" -ForegroundColor Green
        Write-Host "📁 File saved: $WavFilePath"
        Write-Host "📊 File size: $FileSizeMB MB"
        Write-Host ""
        Write-Host "You can now analyze this file with IQ Lab:" -ForegroundColor Cyan
        Write-Host "  .\iqinfo.exe --in $WavFilePath --format s16 --rate 12000" -ForegroundColor Yellow
        Write-Host "  .\generate_images.exe  # For spectrum/waterfall PNGs" -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "❌ Recording failed or file not created" -ForegroundColor Red
        Write-Host "Exit code: $($process.ExitCode)"
        Write-Host "Expected file: $WavFilePath"
        Write-Host "Check the error messages above"
        exit 1
    }
} finally {
    Pop-Location
}
