param(
    [string]$Prompt = "inventory dashboard",
    [string]$OutputDir,
    [string]$NcBin,
    [string]$ModelPath,
    [string]$TokenizerPath,
    [string]$ArtifactsDir,
    [switch]$TemplateFallback,
    [switch]$CopyModel,
    [switch]$Force,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$SetupScript = Join-Path $RepoRoot "scripts\setup-local-ai-model.ps1"

if (-not (Test-Path $SetupScript -PathType Leaf)) {
    throw "Local model setup script not found: $SetupScript"
}

if (-not $NcBin) {
    $SearchPaths = @(
        "engine\build\nc.exe",
        "engine\build\nc_ready.exe",
        "engine\build\nc_new.exe",
        "engine\build\Release\nc.exe",
        "engine\build-msvc\Release\nc.exe"
    )

    foreach ($path in $SearchPaths) {
        $candidate = Join-Path $RepoRoot $path
        if (Test-Path $candidate -PathType Leaf) {
            $NcBin = $candidate
            break
        }
    }
}

if (-not $NcBin -and $env:NC_BIN -and (Test-Path $env:NC_BIN -PathType Leaf)) {
    $NcBin = $env:NC_BIN
}

if (-not $NcBin) {
    throw "nc.exe not found. Build the engine first or pass -NcBin."
}

$NcBin = (Resolve-Path $NcBin).Path
$TrainingModel = Join-Path $RepoRoot "training_data\nova_model.bin"
$TrainingTokenizer = Join-Path $RepoRoot "training_data\nc_ai_tokenizer.bin"

$needsSetup = $Force -or
    $ModelPath -or
    $TokenizerPath -or
    $ArtifactsDir -or
    -not (Test-Path $TrainingModel -PathType Leaf) -or
    -not (Test-Path $TrainingTokenizer -PathType Leaf)

if ($needsSetup) {
    $setupParams = @{}
    if ($ModelPath) { $setupParams.ModelPath = $ModelPath }
    if ($TokenizerPath) { $setupParams.TokenizerPath = $TokenizerPath }
    if ($ArtifactsDir) { $setupParams.ArtifactsDir = $ArtifactsDir }
    if ($CopyModel) { $setupParams.Copy = $true }
    if ($Force) { $setupParams.Force = $true }

    & $SetupScript @setupParams
}

function Convert-ToSlug {
    param([string]$Value)

    $slug = $Value.ToLowerInvariant() -replace "[^a-z0-9]+", "_"
    $slug = $slug.Trim("_")
    if ([string]::IsNullOrWhiteSpace($slug)) {
        return "prompt"
    }

    return $slug
}

$PromptSlug = Convert-ToSlug $Prompt
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot ("engine\build\ai_create_smoke_" + $PromptSlug)
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot $OutputDir
}

if (Test-Path $OutputDir) {
    if (-not $Force) {
        throw "Output directory already exists: $OutputDir. Re-run with -Force or pass -OutputDir."
    }

    Remove-Item $OutputDir -Recurse -Force
}

$ExpectedFiles = @(
    "service.nc",
    "app.ncui",
    "test_app.nc",
    "README.md"
)

$BundleFiles = @(
    "dist\app.html",
    "app.html"
)

Write-Host ""
Write-Host "  NC AI Smoke Test (Windows)" -ForegroundColor Cyan
Write-Host "  Binary: $NcBin" -ForegroundColor DarkGray
Write-Host "  Prompt: $Prompt" -ForegroundColor DarkGray
Write-Host "  Output: $OutputDir" -ForegroundColor DarkGray
Write-Host ""

Push-Location $RepoRoot
try {
    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $CreateArgs = @("ai", "create", $Prompt, "-o", $OutputDir)
        if ($TemplateFallback) {
            $CreateArgs += "--template-fallback"
        }
        $CommandOutput = & $NcBin @CreateArgs 2>&1 | ForEach-Object { "$_" }
        $ExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
} finally {
    Pop-Location
}

if ($Verbose -and $CommandOutput) {
    Write-Host ($CommandOutput -join [Environment]::NewLine)
}

if ($ExitCode -ne 0) {
    if ($CommandOutput) {
        Write-Host ($CommandOutput -join [Environment]::NewLine) -ForegroundColor DarkGray
    }
    throw "nc ai create failed with exit code $ExitCode"
}

$MissingFiles = @()
foreach ($relativePath in $ExpectedFiles) {
    if (-not (Test-Path (Join-Path $OutputDir $relativePath) -PathType Leaf)) {
        $MissingFiles += $relativePath
    }
}

$BundleFound = $false
foreach ($relativePath in $BundleFiles) {
    if (Test-Path (Join-Path $OutputDir $relativePath) -PathType Leaf) {
        $BundleFound = $true
        break
    }
}

if ($MissingFiles.Count -gt 0) {
    throw "Smoke output is incomplete. Missing: $($MissingFiles -join ', ')"
}
if (-not $BundleFound) {
    throw "Smoke output is missing the frontend bundle (dist\\app.html or app.html)."
}

$Highlights = @()
foreach ($line in $CommandOutput) {
    if ($line -match "Validation:" -or
        $line -match "Successfully compiled" -or
        $line -match "Project created:" -or
        $line -match "source files generated") {
        $Highlights += $line
    }
}

Write-Host "  Smoke passed." -ForegroundColor Green
foreach ($line in $Highlights) {
    Write-Host "  $line" -ForegroundColor Green
}
Write-Host ""
Write-Host "  Verified files:" -ForegroundColor Cyan
foreach ($relativePath in $ExpectedFiles) {
    Write-Host "    $relativePath" -ForegroundColor White
}
Write-Host "    dist\\app.html or app.html" -ForegroundColor White
Write-Host ""