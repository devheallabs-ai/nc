param(
    [string]$Version = 'v1.0.0',
    [string]$NcBin,
    [switch]$AllowIncompleteTraining,
    [switch]$SkipTests,
    [switch]$SkipAiSmoke,
    [switch]$SkipMemorySmoke,
    [switch]$SkipSemanticSmoke
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $ScriptDir '..')).Path
$EngineBuild = Join-Path $ProjectRoot 'engine\build'
$ManifestPath = Join-Path $EngineBuild 'v1_release_manifest.json'

$ReportPath = Join-Path $EngineBuild 'nc_model_v1_release_report.txt'
$StatePath = Join-Path $EngineBuild 'nc_model_v1_release_training_state.json'
$ModelPath = Join-Path $EngineBuild 'nc_model_v1_release.bin'
$TokenizerPath = Join-Path $EngineBuild 'nc_model_v1_release_tokenizer.bin'
$OptimizerPath = Join-Path $EngineBuild 'nc_model_v1_release_optimizer.bin'
$BestModelPath = Join-Path $EngineBuild 'nc_model_v1_release_best.bin'
$BestTokenizerPath = Join-Path $EngineBuild 'nc_model_v1_release_best_tokenizer.bin'
$AiSmokeOutputDir = Join-Path $EngineBuild 'release_ai_smoke_inventory_dashboard'
$MemoryStorePath = Join-Path $EngineBuild 'release_memory_policy_smoke_store.json'
$PolicyStorePath = Join-Path $EngineBuild 'release_policy_smoke_store.json'

$checks = New-Object System.Collections.ArrayList
$blockers = New-Object System.Collections.ArrayList

function Add-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Details
    )

    $status = if ($Passed) { 'pass' } else { 'fail' }
    $msg = if ($Passed) { $Name + ' passed.' } else { $Details }
    [void]$checks.Add([pscustomobject]@{
        name = $Name
        status = $status
        details = $msg
    })

    if ($Passed) {
        Write-Host ('  [PASS] ' + $Name) -ForegroundColor Green
    } else {
        Write-Host ('  [FAIL] ' + $Name) -ForegroundColor Red
        [void]$blockers.Add($Details)
    }
}

function Resolve-FirstExistingFile {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path $candidate -PathType Leaf) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Get-ReportMap {
    param([string]$Path)

    $map = [ordered]@{}
    foreach ($line in Get-Content $Path) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        $parts = $line -split '=', 2
        if ($parts.Count -eq 2) {
            $map[$parts[0].Trim()] = $parts[1].Trim()
        }
    }

    return $map
}

function Get-Sha256 {
    param([string]$Path)

    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

if (-not $NcBin) {
    $NcBin = Resolve-FirstExistingFile @(
        (Join-Path $EngineBuild 'nc_release_ready.exe'),
        (Join-Path $EngineBuild 'nc_ready.exe'),
        (Join-Path $EngineBuild 'nc_new.exe'),
        (Join-Path $EngineBuild 'nc.exe'),
        (Join-Path $EngineBuild 'nc')
    )
}

Write-Host ''
Write-Host ('  NC v1 Release Preparation - ' + $Version) -ForegroundColor Cyan
Write-Host ('  Project: ' + $ProjectRoot) -ForegroundColor DarkGray
Write-Host ('  Binary:  ' + $NcBin) -ForegroundColor DarkGray
Write-Host ''

$requiredArtifacts = @(
    $ReportPath,
    $StatePath,
    $ModelPath,
    $TokenizerPath,
    $OptimizerPath,
    $BestModelPath,
    $BestTokenizerPath
)

foreach ($artifact in $requiredArtifacts) {
    Add-Check ('artifact present: ' + (Split-Path $artifact -Leaf)) (Test-Path $artifact -PathType Leaf) ('Missing required release artifact: ' + $artifact)
}

$ncBinaryFound = -not [string]::IsNullOrWhiteSpace($NcBin) -and (Test-Path $NcBin -PathType Leaf)
Add-Check 'release binary located' $ncBinaryFound 'NC binary not found. Build the release binary first.'

$reportMap = @{}
if (Test-Path $ReportPath -PathType Leaf) {
    $reportMap = Get-ReportMap $ReportPath
}

$state = $null
if (Test-Path $StatePath -PathType Leaf) {
    $state = Get-Content $StatePath | ConvertFrom-Json
}

if ($ncBinaryFound -and -not $SkipTests) {
    try {
        $oldNcBin = $env:NC_BIN
        $env:NC_BIN = $NcBin
        & powershell -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot 'tests\run_tests.ps1') | Out-Host
        Add-Check 'local test suite' $true 'Local test suite passed.'
    } catch {
        Add-Check 'local test suite' $false ('Local test suite failed: ' + $_.Exception.Message)
    } finally {
        if ($null -eq $oldNcBin) {
            Remove-Item Env:NC_BIN -ErrorAction SilentlyContinue
        } else {
            $env:NC_BIN = $oldNcBin
        }
    }
}

if ($ncBinaryFound -and -not $SkipAiSmoke) {
    try {
        & powershell -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot 'tests\run_ai_smoke.ps1') -Prompt 'inventory dashboard' -NcBin $NcBin -ArtifactsDir $EngineBuild -ModelPath $ModelPath -TokenizerPath $TokenizerPath -OutputDir $AiSmokeOutputDir -Force | Out-Host

        $expectedAiFiles = @(
            (Join-Path $AiSmokeOutputDir 'service.nc'),
            (Join-Path $AiSmokeOutputDir 'app.ncui'),
            (Join-Path $AiSmokeOutputDir 'test_app.nc'),
            (Join-Path $AiSmokeOutputDir 'README.md')
        )
        $bundleFound = (Test-Path (Join-Path $AiSmokeOutputDir 'dist\app.html') -PathType Leaf) -or (Test-Path (Join-Path $AiSmokeOutputDir 'app.html') -PathType Leaf)
        $allExpected = $bundleFound
        foreach ($expected in $expectedAiFiles) {
            if (-not (Test-Path $expected -PathType Leaf)) {
                $allExpected = $false
            }
        }

        Add-Check 'AI create smoke' $allExpected ('AI smoke output is incomplete under ' + $AiSmokeOutputDir)
    } catch {
        Add-Check 'AI create smoke' $false ('AI smoke failed: ' + $_.Exception.Message)
    }
}

if ($ncBinaryFound -and -not $SkipSemanticSmoke) {
    $SemanticSmokeOutputDir = Join-Path $EngineBuild 'release_semantic_smoke_enterprise'
    try {
        $semanticParams = @{
            Prompt = 'multi-tenant operations dashboard with role based access analytics alert center approvals and dark theme'
            OutputDir = $SemanticSmokeOutputDir
            NcBin = $NcBin
            ArtifactsDir = $EngineBuild
            ModelPath = $ModelPath
            TokenizerPath = $TokenizerPath
        }
        & powershell -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot 'tests\run_ai_semantic_smoke.ps1') @semanticParams -Force | Out-Host
        Add-Check 'enterprise semantic smoke' $true 'Enterprise semantic validation passed.'
    } catch {
        Add-Check 'enterprise semantic smoke' $false ('Enterprise semantic smoke failed: ' + $_.Exception.Message)
    }
}

if ($ncBinaryFound -and -not $SkipMemorySmoke) {
    try {
        Remove-Item $MemoryStorePath -ErrorAction SilentlyContinue
        Remove-Item $PolicyStorePath -ErrorAction SilentlyContinue

        $oldRead = $env:NC_ALLOW_FILE_READ
        $oldWrite = $env:NC_ALLOW_FILE_WRITE
        $env:NC_ALLOW_FILE_READ = '1'
        $env:NC_ALLOW_FILE_WRITE = '1'

        Push-Location $ProjectRoot
        try {
            & $NcBin run (Join-Path $ProjectRoot 'tests\release_memory_policy_smoke.nc') -b smoke | Out-Host
        } finally {
            Pop-Location
            if ($null -eq $oldRead) {
                Remove-Item Env:NC_ALLOW_FILE_READ -ErrorAction SilentlyContinue
            } else {
                $env:NC_ALLOW_FILE_READ = $oldRead
            }
            if ($null -eq $oldWrite) {
                Remove-Item Env:NC_ALLOW_FILE_WRITE -ErrorAction SilentlyContinue
            } else {
                $env:NC_ALLOW_FILE_WRITE = $oldWrite
            }
        }

        $memoryOk = $false
        $policyOk = $false
        if (Test-Path $MemoryStorePath -PathType Leaf) {
            $memoryJson = Get-Content $MemoryStorePath | ConvertFrom-Json
            $memoryOk = $memoryJson.type -eq 'nc_long_term_memory' -and $memoryJson.entries.Count -ge 2
        }
        if (Test-Path $PolicyStorePath -PathType Leaf) {
            $policyJson = Get-Content $PolicyStorePath | ConvertFrom-Json
            $policyOk = $policyJson.type -eq 'nc_policy_memory' -and $policyJson.actions.use_memory.avg_reward -gt $policyJson.actions.skip_memory.avg_reward
        }

        Add-Check 'core memory/policy smoke' ($memoryOk -and $policyOk) 'Memory or policy smoke artifacts are missing or malformed.'
    } catch {
        Add-Check 'core memory/policy smoke' $false ('Memory/policy smoke failed: ' + $_.Exception.Message)
    }
}

$releaseGatePassed = $false
if ($reportMap.Contains('release_gate_passed')) {
    $releaseGatePassed = $reportMap['release_gate_passed'] -eq '1'
}
Add-Check 'release gate passed' $releaseGatePassed ('Release gate is not passing in ' + $ReportPath)

$completed = $false
if ($state) {
    $completed = [int]$state.completed -eq 1
}

if ($completed) {
    Add-Check 'training run completed' $true 'Training run completed.'
} elseif ($AllowIncompleteTraining) {
    Add-Check 'training run still in progress (candidate mode allowed)' $true 'Training run incomplete but candidate mode is allowed.'
} else {
    $currentStep = if ($state) { $state.current_step } else { 'unknown' }
    $targetStep = if ($state) { $state.target_step } else { 'unknown' }
    Add-Check 'training run completed' $false ('Training state is incomplete: current_step=' + $currentStep + ' target_step=' + $targetStep)
}

$checksums = [ordered]@{}
foreach ($asset in @($NcBin, $ModelPath, $TokenizerPath, $OptimizerPath, $BestModelPath, $BestTokenizerPath)) {
    if (-not [string]::IsNullOrWhiteSpace($asset) -and (Test-Path $asset -PathType Leaf)) {
        $checksums[(Split-Path $asset -Leaf)] = Get-Sha256 $asset
    }
}

$readiness = 'blocked'
if ($blockers.Count -eq 0) {
    if ($completed) {
        $readiness = 'final'
    } else {
        $readiness = 'candidate'
    }
}

$summary = [pscustomobject]@{
    version = $Version
    prepared_at = (Get-Date).ToString('s')
    nc_binary = $NcBin
    report = $reportMap
    training_state = $state
    checks = $checks
    checksums = $checksums
    readiness = $readiness
    blockers = $blockers
}

$summary | ConvertTo-Json -Depth 8 | Set-Content -Path $ManifestPath

Write-Host ''
Write-Host '  Summary' -ForegroundColor Cyan
Write-Host ('  readiness: ' + $readiness)
if ($reportMap.Contains('best_eval_ppl')) {
    Write-Host ('  best_eval_ppl: ' + $reportMap['best_eval_ppl'])
}
if ($state) {
    Write-Host ('  current_step: ' + $state.current_step)
    Write-Host ('  target_step: ' + $state.target_step)
}
Write-Host ('  manifest: ' + $ManifestPath)

if ($blockers.Count -gt 0) {
    Write-Host ''
    Write-Host '  Blockers' -ForegroundColor Red
    foreach ($blocker in $blockers) {
        Write-Host ('  - ' + $blocker) -ForegroundColor Red
    }
}

Write-Host ''
Write-Host '  Next steps' -ForegroundColor Cyan
Write-Host '  1. Update CHANGELOG.md and release notes.'
Write-Host '  2. Tag and push once readiness is acceptable.'
Write-Host '  3. Upload platform binaries plus the versioned v1 model assets.'
Write-Host ''

if ($readiness -eq 'blocked') {
    exit 1
}

if ($readiness -eq 'candidate' -and -not $AllowIncompleteTraining) {
    exit 1
}

exit 0
