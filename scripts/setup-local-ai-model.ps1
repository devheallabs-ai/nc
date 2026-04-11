param(
    [string]$ModelPath,
    [string]$TokenizerPath,
    [string]$ArtifactsDir,
    [switch]$Copy,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$EngineDir = Join-Path $RepoRoot "engine"
$BuildDir = if ($ArtifactsDir) {
    if (-not (Test-Path $ArtifactsDir -PathType Container)) {
        throw "Artifacts directory not found: $ArtifactsDir"
    }
    (Resolve-Path $ArtifactsDir).Path
} else {
    Join-Path $EngineDir "build"
}

$TargetDir = Join-Path $RepoRoot "training_data"
$ModelTarget = Join-Path $TargetDir "nova_model.bin"
$TokenizerTarget = Join-Path $TargetDir "nc_ai_tokenizer.bin"
$EngineRootModel = Join-Path $EngineDir "nova_model.bin"
$EngineRootTokenizer = Join-Path $EngineDir "nc_ai_tokenizer.bin"

function Resolve-FirstFile {
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

function Get-ResolvedPathOrNull {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return $null
    }
    if (-not (Test-Path $PathValue)) {
        return $null
    }

    return (Resolve-Path $PathValue).Path
}

function Same-File {
    param(
        [string]$Left,
        [string]$Right
    )

    $leftResolved = Get-ResolvedPathOrNull $Left
    $rightResolved = Get-ResolvedPathOrNull $Right
    return $leftResolved -and $rightResolved -and $leftResolved -eq $rightResolved
}

function Stage-Artifact {
    param(
        [string]$Source,
        [string]$Target,
        [string]$Label,
        [switch]$PreferMove
    )

    if (Same-File $Source $Target) {
        Write-Host "  $Label already staged: $Target" -ForegroundColor DarkGray
        return
    }

    if (Test-Path $Target -PathType Leaf) {
        if (-not $Force) {
            $sourceItem = Get-Item $Source
            $targetItem = Get-Item $Target
            if ($sourceItem.Length -eq $targetItem.Length) {
                Write-Host "  Reusing existing ${Label}: $Target" -ForegroundColor DarkGray
                return
            }
            throw "Target already exists for ${Label}: $Target. Re-run with -Force to replace it."
        }

        Remove-Item $Target -Force
    }

    if ($PreferMove) {
        Move-Item -LiteralPath $Source -Destination $Target
        Write-Host "  Moved $Label to $Target" -ForegroundColor Green
        return
    }

    if (-not $Copy) {
        try {
            New-Item -ItemType HardLink -Path $Target -Target $Source -Force | Out-Null
            Write-Host "  Linked $Label to $Target" -ForegroundColor Green
            return
        } catch {
        }
    }

    Copy-Item -LiteralPath $Source -Destination $Target -Force
    Write-Host "  Copied $Label to $Target" -ForegroundColor Green
}

$ModelCandidates = @(
    $ModelPath
    if ($ArtifactsDir) { Join-Path $BuildDir "nova_model.bin" }
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_ai_model_prod.bin" }
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_model.bin" }
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_model_1024.bin" }
    $EngineRootModel
    Join-Path $EngineDir "nc_ai_model_prod.bin"
    Join-Path $BuildDir "nova_model.bin"
    Join-Path $BuildDir "nc_ai_model_prod.bin"
    Join-Path $BuildDir "nc_model.bin"
    Join-Path $BuildDir "nc_model_1024.bin"
    Join-Path $BuildDir "nc_model_v1_release.bin"
)

$TokenizerCandidates = @(
    $TokenizerPath
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_ai_tokenizer.bin" }
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_model_tokenizer.bin" }
    if ($ArtifactsDir) { Join-Path $BuildDir "nc_model_1024_tokenizer.bin" }
    $EngineRootTokenizer
    Join-Path $BuildDir "nc_ai_tokenizer.bin"
    Join-Path $BuildDir "nc_model_tokenizer.bin"
    Join-Path $BuildDir "nc_model_1024_tokenizer.bin"
    Join-Path $BuildDir "nc_model_v1_release_tokenizer.bin"
)

$ModelSource = Resolve-FirstFile $ModelCandidates
$TokenizerSource = Resolve-FirstFile $TokenizerCandidates

if (-not $ModelSource) {
    throw "Could not find a local model artifact. Pass -ModelPath or point -ArtifactsDir at a build directory that contains nc_model.bin or nova_model.bin."
}
if (-not $TokenizerSource) {
    throw "Could not find a tokenizer artifact. Pass -TokenizerPath or point -ArtifactsDir at a build directory that contains nc_model_tokenizer.bin or nc_ai_tokenizer.bin."
}

New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null

$MoveModel = (Same-File $ModelSource $EngineRootModel)
$MoveTokenizer = (Same-File $TokenizerSource $EngineRootTokenizer)

Write-Host ""
Write-Host "  Local AI Model Setup" -ForegroundColor Cyan
Write-Host "  Repo Root:  $RepoRoot" -ForegroundColor DarkGray
Write-Host "  Model Src:  $ModelSource" -ForegroundColor DarkGray
Write-Host "  Token Src:  $TokenizerSource" -ForegroundColor DarkGray
Write-Host "  Target Dir: $TargetDir" -ForegroundColor DarkGray
Write-Host ""

Stage-Artifact -Source $ModelSource -Target $ModelTarget -Label "model" -PreferMove:$MoveModel
Stage-Artifact -Source $TokenizerSource -Target $TokenizerTarget -Label "tokenizer" -PreferMove:$MoveTokenizer

Write-Host ""
Write-Host "  Local built-in model is ready:" -ForegroundColor Green
Write-Host "    training_data\\nova_model.bin" -ForegroundColor Green
Write-Host "    training_data\\nc_ai_tokenizer.bin" -ForegroundColor Green
Write-Host ""
Write-Host "  Next step:" -ForegroundColor Cyan
Write-Host "    powershell -ExecutionPolicy Bypass -File tests\\run_ai_smoke.ps1 -Prompt 'inventory dashboard'" -ForegroundColor White
Write-Host ""