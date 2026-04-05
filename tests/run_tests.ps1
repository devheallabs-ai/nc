# run_tests.ps1 — NC cross-platform test runner for Windows
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
#   powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -Verbose

param(
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"

# Find the NC binary
$NC = $null
$envNcBin = $env:NC_BIN
if ($envNcBin -and (Test-Path $envNcBin)) {
    $NC = $envNcBin
}
$SearchPaths = @(
    "engine\build\nc.exe",
    "engine\build\nc_new.exe",
    "engine\build\Release\nc.exe",
    "engine\build-msvc\Release\nc.exe",
    "build\nc.exe"
)
if (-not $NC) {
    foreach ($p in $SearchPaths) {
        if (Test-Path $p) { $NC = $p; break }
    }
}
if (-not $NC) {
    Write-Host "  ERROR: nc.exe not found. Build first with 'cd engine && make' or 'cmake --build .'" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "  NC Test Runner (Windows)" -ForegroundColor Cyan
Write-Host "  Binary: $NC" -ForegroundColor DarkGray
Write-Host "  ----------------------------------------" -ForegroundColor DarkGray
Write-Host ""

$total = 0
$passed = 0
$failed = 0
$errors = @()

# 1. Version check
Write-Host "  [1] Version check..." -NoNewline
$ver = & $NC version 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "version check failed"
}
$total++

# 2. Language tests (nc test)
Write-Host "  [2] Language tests (nc test)..." -NoNewline
$testOut = & $NC test 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "nc test failed"
    if ($Verbose) { Write-Host $testOut -ForegroundColor DarkGray }
}
$total++

# 3. Terminal UX compatibility
Write-Host "  [3] Terminal UX (NC_NO_ANIM)..." -NoNewline
$oldNoAnim = $env:NC_NO_ANIM
$env:NC_NO_ANIM = "1"
$animOut = & $NC ai status 2>&1
$animExit = $LASTEXITCODE
if ($null -eq $oldNoAnim) { Remove-Item Env:NC_NO_ANIM -ErrorAction SilentlyContinue } else { $env:NC_NO_ANIM = $oldNoAnim }
if ($animExit -eq 0 -and ($animOut -join "`n") -match "Loading model|Model loaded") {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "NC_NO_ANIM compatibility failed"
}
$total++

Write-Host "  [4] Terminal UX (NO_COLOR)..." -NoNewline
$oldNoColor = $env:NO_COLOR
$env:NO_COLOR = "1"
$noColorOut = & $NC version 2>&1
$noColorExit = $LASTEXITCODE
if ($null -eq $oldNoColor) { Remove-Item Env:NO_COLOR -ErrorAction SilentlyContinue } else { $env:NO_COLOR = $oldNoColor }
$joinedNoColor = ($noColorOut -join "`n")
if ($noColorExit -eq 0 -and ($joinedNoColor -notmatch "`e\[")) {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "NO_COLOR compatibility failed"
}
$total++

# 5. Validate examples
Write-Host "  [5] Validate examples..." -NoNewline
$exampleFiles = Get-ChildItem -Path "examples\*.nc" -ErrorAction SilentlyContinue
$examplePassed = 0
$exampleFailed = 0
foreach ($f in $exampleFiles) {
    & $NC validate $f.FullName 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $examplePassed++
    } else {
        $exampleFailed++
        if ($Verbose) { Write-Host "    FAIL: $($f.Name)" -ForegroundColor Yellow }
    }
}
if ($exampleFailed -eq 0 -and $examplePassed -gt 0) {
    Write-Host " PASS ($examplePassed files)" -ForegroundColor Green
    $passed++
} elseif ($examplePassed -eq 0) {
    Write-Host " SKIP (no examples found)" -ForegroundColor Yellow
} else {
    Write-Host " FAIL ($exampleFailed/$($examplePassed + $exampleFailed) failed)" -ForegroundColor Red
    $failed++
    $errors += "$exampleFailed example validations failed"
}
$total++

# 6. Inline code execution
Write-Host "  [6] Inline code (nc -c)..." -NoNewline
$inlineOut = & $NC -c "set x to 42; show x" 2>&1
if ($LASTEXITCODE -eq 0 -and $inlineOut -match "42") {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "inline code execution failed"
}
$total++

# 7. Expression evaluation
Write-Host "  [7] Expression eval (nc -e)..." -NoNewline
$evalOut = & $NC -e "21 * 2" 2>&1
if ($LASTEXITCODE -eq 0 -and $evalOut -match "42") {
    Write-Host " PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " FAIL" -ForegroundColor Red
    $failed++
    $errors += "expression evaluation failed"
}
$total++

# 8. Build command (single file)
Write-Host "  [8] Build command..." -NoNewline
$testNcFile = "examples\01_hello_world.nc"
if (Test-Path $testNcFile) {
    $buildOut = & $NC validate $testNcFile 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host " PASS" -ForegroundColor Green
        $passed++
    } else {
        Write-Host " FAIL" -ForegroundColor Red
        $failed++
        $errors += "build validation failed"
    }
} else {
    Write-Host " SKIP (no test file)" -ForegroundColor Yellow
}
$total++

# Summary
Write-Host ""
Write-Host "  ----------------------------------------" -ForegroundColor DarkGray
Write-Host "  Results: $passed passed, $failed failed, $total total" -NoNewline
if ($failed -eq 0) {
    Write-Host " - ALL PASS" -ForegroundColor Green
} else {
    Write-Host " - FAILURES" -ForegroundColor Red
    foreach ($e in $errors) {
        Write-Host "    - $e" -ForegroundColor Red
    }
}
Write-Host ""

exit $failed
