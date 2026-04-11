$packageName = 'nc'
Uninstall-BinFile -Name 'nc'
$installDir = Join-Path (Get-ToolsLocation) $packageName
if (Test-Path $installDir) { Remove-Item $installDir -Recurse -Force }
Write-Host "  NC uninstalled." -ForegroundColor Yellow
