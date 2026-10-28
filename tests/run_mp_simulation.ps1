param(
    [string]$Mode = "arcade",
    [int]$NumPlayers = 3,
    [int]$LevelId = 0,
    [int]$RaceFrames = 60,
    [int]$BasePort = 32010
)

$exe = "build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Executable not found at $exe"
    exit 1
}

Write-Host "=================================================="
Write-Host "Starting MP Simulation: Mode=$Mode, Players=$NumPlayers, Level=$LevelId, Frames=$RaceFrames, Port=$BasePort"
Write-Host "=================================================="

# Start Host process
$hostArgs = @("--mp-port", "$BasePort", "--mp-test-host", "$Mode", "$NumPlayers", "$LevelId", "3", "$RaceFrames")
$hostProc = Start-Process -FilePath $exe -ArgumentList $hostArgs -PassThru -NoNewWindow

Start-Sleep -Milliseconds 800

# Start Clients
$clientProcs = @()
for ($i = 1; $i -lt $NumPlayers; $i++) {
    $clientArgs = @("--mp-port", "$BasePort", "--mp-ip", "127.0.0.1", "--mp-test-client", "$Mode", "$i", "$RaceFrames")
    $clientProc = Start-Process -FilePath $exe -ArgumentList $clientArgs -PassThru -NoNewWindow
    $clientProcs += $clientProc
    Start-Sleep -Milliseconds 300
}

# Wait for Host to complete (timeout 45s)
$timeoutSeconds = 45
try {
    Wait-Process -Id $hostProc.Id -Timeout $timeoutSeconds -ErrorAction Stop
} catch {
    Write-Host "HOST TIMED OUT! Killing processes..."
    Stop-Process -Id $hostProc.Id -Force -ErrorAction SilentlyContinue
    foreach ($cp in $clientProcs) {
        Stop-Process -Id $cp.Id -Force -ErrorAction SilentlyContinue
    }
    exit 1
}

$hostProc.Refresh()
$hostExitCode = $hostProc.ExitCode

# Wait for clients
foreach ($cp in $clientProcs) {
    try {
        Wait-Process -Id $cp.Id -Timeout 10 -ErrorAction SilentlyContinue
    } catch {}
    $cp.Refresh()
    if (-not $cp.HasExited) {
        Stop-Process -Id $cp.Id -Force -ErrorAction SilentlyContinue
    }
}

if ($hostProc.HasExited) {
    Write-Host "SIMULATION PASSED!"
    exit 0
} else {
    Write-Host "SIMULATION FAILED! (Host exit code: $hostExitCode)"
    exit 1
}
