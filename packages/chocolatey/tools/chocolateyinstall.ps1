$ErrorActionPreference = 'Stop'

$packageName = 'nc'
$version = '1.3.0'
$repo = 'devheallabs-ai/nc'

$arch = if ([Environment]::Is64BitOperatingSystem) { 'x86_64' } else { 'x86' }
$url = "https://github.com/$repo/releases/download/v$version/nc-windows-$arch.exe"

$installDir = Join-Path (Get-ToolsLocation) $packageName
if (!(Test-Path $installDir)) { New-Item -ItemType Directory -Path $installDir -Force | Out-Null }

$ncExe = Join-Path $installDir 'nc.exe'

Get-ChocolateyWebFile -PackageName $packageName `
    -FileFullPath $ncExe `
    -Url $url `
    -Url64bit $url

Install-BinFile -Name 'nc' -Path $ncExe

Write-Host ""
Write-Host "  NC installed successfully!" -ForegroundColor Green
Write-Host "  Run: nc version" -ForegroundColor Cyan
Write-Host ""
