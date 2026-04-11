param(
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [switch]$ExcludeProprietaryAI
)

$ErrorActionPreference = "Stop"

$sourceRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$destinationPath = [System.IO.Path]::GetFullPath($Destination)

if (-not (Test-Path $destinationPath)) {
    New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
}

$excludeDirs = @(
    ".git",
    "engine\\build",
    "engine\\build_local",
    "editor\\vscode\\node_modules",
    "__pycache__"
)

$robocopyArgs = @(
    $sourceRoot.Path,
    $destinationPath,
    "/E",
    "/XD"
) + $excludeDirs + @(
    "/XF", "*.o", "*.obj", "*.exe", "*.dll", "*.so", "*.dylib", "*.bin"
)

& robocopy @robocopyArgs | Out-Null
if ($LASTEXITCODE -gt 7) {
    throw "robocopy failed with exit code $LASTEXITCODE"
}

if ($ExcludeProprietaryAI) {
    $privateFiles = @(
        "engine\\src\\nc_nova.c",
        "engine\\src\\nc_nova.h",
        "engine\\src\\nc_cortex.c",
        "engine\\src\\nc_cortex.h",
        "engine\\src\\nc_training.c",
        "engine\\src\\nc_training.h",
        "engine\\src\\nc_model.h",
        "engine\\src\\nc_tokenizer.c",
        "engine\\src\\nc_ai_enterprise.c",
        "engine\\nc_ai_model.bin",
        "engine\\nc_ai_model_prod.bin",
        "engine\\nc_ai_tokenizer.bin"
    )

    foreach ($relative in $privateFiles) {
        $target = Join-Path $destinationPath $relative
        if (Test-Path $target) {
            Remove-Item -LiteralPath $target -Force
        }
    }
}

Write-Host ""
Write-Host "Standalone nc-lang repo copy created at:" -ForegroundColor Cyan
Write-Host "  $destinationPath" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. cd $destinationPath"
Write-Host "  2. git init"
Write-Host "  3. review OPEN_SOURCE_SCOPE.md and STANDALONE_REPO_BLUEPRINT.md"
Write-Host "  4. push to https://github.com/devheallabs-ai/nc-lang"
