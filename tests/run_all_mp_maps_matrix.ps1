param(
    [int]$RaceFrames = 90
)

$tracks = @(
    @{ Id = 0;  Name = "Crash Cove";      Players = 3; Mode = "arcade" },
    @{ Id = 1;  Name = "Roo's Tubes";     Players = 2; Mode = "vs"     },
    @{ Id = 2;  Name = "Mystery Caves";   Players = 4; Mode = "arcade" },
    @{ Id = 3;  Name = "Sewer Speedway";  Players = 3; Mode = "vs"     },
    @{ Id = 5;  Name = "Coco Park";       Players = 2; Mode = "arcade" },
    @{ Id = 6;  Name = "Tiny Arena";      Players = 4; Mode = "vs"     },
    @{ Id = 8;  Name = "Blizzard Bluff";  Players = 3; Mode = "arcade" },
    @{ Id = 10; Name = "Polar Pass";      Players = 2; Mode = "vs"     },
    @{ Id = 11; Name = "Cortex Castle";   Players = 4; Mode = "arcade" },
    @{ Id = 12; Name = "N. Gin Labs";     Players = 3; Mode = "vs"     },
    @{ Id = 13; Name = "Hot Air Skyway";  Players = 2; Mode = "arcade" },
    @{ Id = 14; Name = "Oxide Station";   Players = 4; Mode = "vs"     },
    @{ Id = 15; Name = "Slide Coliseum";  Players = 3; Mode = "arcade" },
    @{ Id = 16; Name = "Turbo Track";     Players = 4; Mode = "vs"     }
)

$basePort = 32100
$passed = 0
$failed = 0
$failedList = @()

Write-Host "============================================================"
Write-Host "Starting Matrix Tests across Tracks, Modes, and Player Counts"
Write-Host "============================================================"

foreach ($t in $tracks) {
    $port = $basePort + $t.Id
    Write-Host ""
    Write-Host ">>> Running test on [$($t.Name)] (Id=$($t.Id)) with $($t.Players) players in $($t.Mode.ToUpper()) mode (Port=$port)..."
    
    $res = powershell -ExecutionPolicy Bypass -File tests/run_mp_simulation.ps1 -Mode $t.Mode -NumPlayers $t.Players -LevelId $t.Id -RaceFrames $RaceFrames -BasePort $port
    
    $hasPassed = ($res -match "SIMULATION PASSED!")
    if ($hasPassed) {
        Write-Host ">>> [PASS] $($t.Name) ($($t.Players)P $($t.Mode))" -ForegroundColor Green
        $passed++
    } else {
        Write-Host ">>> [FAIL] $($t.Name) ($($t.Players)P $($t.Mode))" -ForegroundColor Red
        $failed++
        $failedList += "$($t.Name) ($($t.Players)P $($t.Mode))"
        Write-Host ($res -join "`n")
    }
    
    Start-Sleep -Milliseconds 500
}

Write-Host ""
Write-Host "============================================================"
Write-Host "MATRIX TEST SUMMARY:"
Write-Host "Passed: $passed / $($tracks.Count)"
Write-Host "Failed: $failed / $($tracks.Count)"
if ($failed -gt 0) {
    Write-Host "Failed runs: $($failedList -join ', ')" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL SIMULATION TESTS PASSED WITH ZERO CRASHES!" -ForegroundColor Green
    exit 0
}
