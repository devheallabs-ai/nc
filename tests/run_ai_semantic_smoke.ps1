param(
    [string]$Prompt = "multi-tenant operations dashboard with role based access analytics alert center approvals and dark theme",
    [string]$OutputDir,
    [string]$NcBin,
    [string]$ModelPath,
    [string]$TokenizerPath,
    [string]$ArtifactsDir,
    [switch]$CopyModel,
    [switch]$Force,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$SmokeScript = Join-Path $RepoRoot "tests\run_ai_smoke.ps1"

if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot "engine\build\ai_create_semantic_ops_dashboard"
}

$smokeParams = @{
    Prompt = $Prompt
    OutputDir = $OutputDir
    TemplateFallback = $true
}
if ($NcBin) { $smokeParams.NcBin = $NcBin }
if ($ModelPath) { $smokeParams.ModelPath = $ModelPath }
if ($TokenizerPath) { $smokeParams.TokenizerPath = $TokenizerPath }
if ($ArtifactsDir) { $smokeParams.ArtifactsDir = $ArtifactsDir }
if ($CopyModel) { $smokeParams.CopyModel = $true }
if ($Force) { $smokeParams.Force = $true }
if ($Verbose) { $smokeParams.Verbose = $true }

& $SmokeScript @smokeParams

$serviceText = Get-Content (Join-Path $OutputDir 'service.nc') -Raw
$pageText = Get-Content (Join-Path $OutputDir 'app.ncui') -Raw

function Assert-AllPatterns {
    param(
        [string]$Name,
        [string]$Text,
        [string[]]$Patterns
    )

    foreach ($pattern in $Patterns) {
        if ($Text -notmatch $pattern) {
            throw "Semantic smoke failed: $Name is missing pattern '$pattern'"
        }
    }

    Write-Host ("  [PASS] " + $Name) -ForegroundColor Green
}

Write-Host "  Semantic checks" -ForegroundColor Cyan
Assert-AllPatterns 'tenant scope in service' $serviceText @('(?is)list_tenants', '(?is)/api/v1/tenants')
Assert-AllPatterns 'role access in service' $serviceText @('(?is)list_roles', '(?is)/api/v1/roles', '(?is)permission')
Assert-AllPatterns 'analytics in service' $serviceText @('(?is)analytics_overview', '(?is)/api/v1/analytics/overview')
Assert-AllPatterns 'approval workflow in service' $serviceText @('(?is)list_approvals', '(?is)approve_request', '(?is)reject_request')
Assert-AllPatterns 'alert center in service' $serviceText @('(?is)list_alerts', '(?is)/api/v1/alerts')

Assert-AllPatterns 'dashboard section in page' $pageText @('(?is)Dashboard')
Assert-AllPatterns 'tenant section in page' $pageText @('(?is)Tenants')
Assert-AllPatterns 'roles section in page' $pageText @('(?is)Roles')
Assert-AllPatterns 'analytics section in page' $pageText @('(?is)Analytics')
Assert-AllPatterns 'approvals section in page' $pageText @('(?is)Approvals')
Assert-AllPatterns 'alerts section in page' $pageText @('(?is)Alerts')

Write-Host ""
Write-Host "  Semantic smoke passed." -ForegroundColor Green
Write-Host ("  Output: " + $OutputDir) -ForegroundColor White
Write-Host ""