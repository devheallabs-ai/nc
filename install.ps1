# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#  NC â€” Windows PowerShell Installer (Zero-Config, like Python/Rust)
#
#  One-line install:
#    irm https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.ps1 | iex
#
#  Or:
#    powershell -ExecutionPolicy Bypass -File install.ps1
#
#  Environment variables:
#    $env:NC_ACCEPT_LICENSE = "1"    Skip license prompt
#    $env:NC_INSTALL_DIR = "<path>"  Custom install directory
#
#  After install, nc works from ANY terminal (CMD, PowerShell, Windows Terminal).
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

$ErrorActionPreference = "Continue"

$NC_VERSION     = "1.0.0"
$REPO           = "DevHealLabs/nc-lang"
$INSTALL_DIR    = if ($env:NC_INSTALL_DIR) { $env:NC_INSTALL_DIR } else { "$env:LOCALAPPDATA\nc" }
$BIN_DIR        = "$INSTALL_DIR\bin"
$LIB_DIR        = "$INSTALL_DIR\Lib"
$W64DEVKIT_VER  = "2.6.0"
$W64DEVKIT_URL  = "https://github.com/skeeto/w64devkit/releases/download/v$W64DEVKIT_VER/w64devkit-x64-$W64DEVKIT_VER.7z.exe"
$W64DEVKIT_DIR  = "$INSTALL_DIR\w64devkit"

# â”€â”€ Colored output helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Write-Step    { param([string]$msg) Write-Host "  ==> $msg" -ForegroundColor Blue }
function Write-Info    { param([string]$msg) Write-Host "  [*] $msg" -ForegroundColor Cyan }
function Write-Ok      { param([string]$msg) Write-Host "  [+] $msg" -ForegroundColor Green }
function Write-Warn    { param([string]$msg) Write-Host "  [!] $msg" -ForegroundColor Yellow }
function Write-Err     { param([string]$msg) Write-Host "  [x] $msg" -ForegroundColor Red }

# â”€â”€ Banner â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Show-Banner {
    Write-Host ""
    Write-Host "  ==============================================" -ForegroundColor Cyan
    Write-Host "    NC Installer for Windows v$NC_VERSION"        -ForegroundColor Cyan
    Write-Host "    The Notation Language Compiler"               -ForegroundColor Cyan
    Write-Host "  ==============================================" -ForegroundColor Cyan
    Write-Host ""
    $arch = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { "x86" }
    Write-Info "Platform: Windows $arch"
    Write-Host ""
}

# â”€â”€ License acceptance â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Confirm-License {
    if ($env:NC_ACCEPT_LICENSE -in @("1", "yes", "true", "TRUE", "YES")) {
        Write-Ok "License accepted via NC_ACCEPT_LICENSE"
        return
    }

    $title = "NC License Agreement"
    $message = @"
NC is licensed under the Apache License 2.0.

By installing NC, you agree to the license terms and notices shipped with this software.

Review:
  https://github.com/$REPO/blob/main/LICENSE
  https://github.com/$REPO/blob/main/NOTICE

Select Yes to continue installation.
"@

    # Try GUI dialog first
    try {
        Add-Type -AssemblyName PresentationFramework -ErrorAction Stop
        $result = [System.Windows.MessageBox]::Show(
            $message, $title,
            [System.Windows.MessageBoxButton]::YesNo,
            [System.Windows.MessageBoxImage]::Information
        )
        if ($result -eq [System.Windows.MessageBoxResult]::Yes) {
            Write-Ok "License accepted"
            return
        }
        Write-Err "Installation cancelled. License not accepted."
        exit 1
    } catch {
        # Fall through to console
    }

    # Console fallback
    Write-Host "  License Agreement" -ForegroundColor Cyan
    Write-Host "  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€" -ForegroundColor Cyan
    Write-Host "  NC is licensed under the Apache License 2.0."
    Write-Host "  By installing NC, you agree to the license terms."
    Write-Host "  Review:" -ForegroundColor Gray
    Write-Host "    https://github.com/$REPO/blob/main/LICENSE" -ForegroundColor Gray
    Write-Host "    https://github.com/$REPO/blob/main/NOTICE" -ForegroundColor Gray
    Write-Host ""

    try {
        $reply = Read-Host "  Type 'yes' to accept and continue"
    } catch {
        Write-Err "Non-interactive install. Set `$env:NC_ACCEPT_LICENSE='1' to accept."
        exit 1
    }

    if ($reply -notin @("yes", "YES", "y", "Y")) {
        Write-Err "Installation cancelled. License not accepted."
        exit 1
    }
    Write-Ok "License accepted"
}

# â”€â”€ Write consent marker â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Write-ConsentMarker {
    $consentDir = if ($env:LOCALAPPDATA) { "$env:LOCALAPPDATA\nc" } elseif ($env:APPDATA) { "$env:APPDATA\nc" } else { return }
    if (!(Test-Path $consentDir)) { New-Item -ItemType Directory -Path $consentDir -Force | Out-Null }
    @(
        "accepted=1",
        "license=Apache-2.0",
        "product=nc",
        "version=$NC_VERSION",
        "date=$(Get-Date -Format 'yyyy-MM-ddTHH:mm:ssZ')"
    ) | Set-Content -Path "$consentDir\license.accepted" -Encoding ASCII
}

# â”€â”€ Check for C compiler â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Find-Compiler {
    # Check MSYS2 MinGW
    $msys2Gcc = @(
        "C:\msys64\mingw64\bin\gcc.exe",
        "$env:USERPROFILE\msys64\mingw64\bin\gcc.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ($msys2Gcc) {
        Write-Ok "Found MSYS2 MinGW GCC: $msys2Gcc"
        return @{ CC = $msys2Gcc; Type = "msys2" }
    }

    # Check w64devkit (already installed)
    if (Test-Path "$W64DEVKIT_DIR\bin\gcc.exe") {
        Write-Ok "Found w64devkit GCC"
        return @{ CC = "$W64DEVKIT_DIR\bin\gcc.exe"; Type = "w64devkit" }
    }

    # Check PATH
    $pathGcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($pathGcc) {
        Write-Ok "Found GCC in PATH: $($pathGcc.Source)"
        return @{ CC = $pathGcc.Source; Type = "path" }
    }

    # Check cl.exe (MSVC)
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    if ($cl) {
        Write-Warn "Found MSVC cl.exe (not ideal for NC build, prefer GCC)"
    }

    return $null
}

# â”€â”€ Check for make â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Find-Make {
    param([string]$CompilerDir)

    # Check alongside compiler
    if ($CompilerDir) {
        $dir = Split-Path $CompilerDir -Parent
        foreach ($name in @("make.exe", "mingw32-make.exe")) {
            $path = Join-Path $dir $name
            if (Test-Path $path) {
                return $path
            }
        }
    }

    # Check PATH
    foreach ($name in @("make", "mingw32-make")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }

    return $null
}

# â”€â”€ Download w64devkit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Install-W64Devkit {
    Write-Step "Downloading w64devkit (portable GCC for Windows)"
    Write-Info "URL: $W64DEVKIT_URL"
    Write-Info "This is a one-time download (~60 MB)..."

    $zipFile = "$env:TEMP\w64devkit.zip"

    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'  # Speed up download
        Invoke-WebRequest -Uri $W64DEVKIT_URL -OutFile $zipFile -UseBasicParsing -TimeoutSec 120
    } catch {
        Write-Err "Download failed: $_"
        return $null
    }

    if (!(Test-Path $zipFile) -or (Get-Item $zipFile).Length -lt 1000000) {
        Write-Err "Download incomplete or corrupted"
        Remove-Item $zipFile -ErrorAction SilentlyContinue
        return $null
    }

    Write-Info "Extracting (self-extracting archive)..."
    try {
        if (!(Test-Path $INSTALL_DIR)) { New-Item -ItemType Directory -Path $INSTALL_DIR -Force | Out-Null }
        & $zipFile "-o$INSTALL_DIR" "-y" 2>&1 | Out-Null
    } catch {
        Write-Err "Extraction failed: $_"
        Remove-Item $zipFile -ErrorAction SilentlyContinue
        return $null
    }
    Remove-Item $zipFile -ErrorAction SilentlyContinue

    if (Test-Path "$W64DEVKIT_DIR\bin\gcc.exe") {
        Write-Ok "w64devkit installed at $W64DEVKIT_DIR"
        return @{ CC = "$W64DEVKIT_DIR\bin\gcc.exe"; Type = "w64devkit" }
    }

    Write-Err "w64devkit extraction failed (gcc.exe not found)"
    return $null
}

# â”€â”€ Try downloading pre-built binary â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Try-PrebuiltBinary {
    $arch = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { "x86" }
    $assetName = "nc-windows-$arch.exe"
    $url = "https://github.com/$REPO/releases/download/v$NC_VERSION/$assetName"

    Write-Step "Checking for pre-built binary"
    Write-Info "Looking for $assetName..."

    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile "$BIN_DIR\nc.exe" -UseBasicParsing -TimeoutSec 30 -ErrorAction Stop

        if ((Test-Path "$BIN_DIR\nc.exe") -and (Get-Item "$BIN_DIR\nc.exe").Length -gt 10000) {
            Write-Ok "Downloaded pre-built binary"
            return $true
        }
        Remove-Item "$BIN_DIR\nc.exe" -ErrorAction SilentlyContinue
    } catch {
        Write-Warn "No pre-built release available"
    }
    return $false
}

# â”€â”€ Build from source â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Build-FromSource {
    param(
        [hashtable]$Compiler,
        [string]$MakePath,
        [string]$SourceDir
    )

    Write-Step "Building NC from source"

    $ncSrc = Join-Path $SourceDir "nc"
    if (!(Test-Path (Join-Path $ncSrc "Makefile"))) {
        Write-Err "Makefile not found at $ncSrc"
        return $false
    }

    # Add compiler directory to PATH for the build
    $ccDir = Split-Path $Compiler.CC -Parent
    $env:PATH = "$ccDir;$env:PATH"

    # For MSYS2, use bash to build
    if ($Compiler.Type -eq "msys2") {
        $msysBash = @(
            "C:\msys64\usr\bin\bash.exe",
            "$env:USERPROFILE\msys64\usr\bin\bash.exe"
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1

        if ($msysBash) {
            Write-Info "Building with MSYS2 bash..."
            $ncPath = ($ncSrc -replace '\\', '/')
            & $msysBash -lc "cd '$ncPath' && make clean 2>/dev/null; make 2>&1" | ForEach-Object {
                Write-Host "    $_" -ForegroundColor DarkGray
            }
        } else {
            Write-Warn "MSYS2 bash not found, trying direct make..."
            Push-Location $ncSrc
            & $MakePath clean 2>$null
            & $MakePath CC="$($Compiler.CC)" 2>&1 | ForEach-Object {
                Write-Host "    $_" -ForegroundColor DarkGray
            }
            Pop-Location
        }
    } else {
        Write-Info "Compiling (this may take a moment)..."
        Push-Location $ncSrc
        & $MakePath clean 2>$null
        & $MakePath CC="$($Compiler.CC)" 2>&1 | ForEach-Object {
            Write-Host "    $_" -ForegroundColor DarkGray
        }
        Pop-Location
    }

    # Find built binary
    $builtExe = Join-Path $ncSrc "build\nc.exe"
    if (!(Test-Path $builtExe)) {
        Write-Err "Build failed: nc.exe not produced"
        return $false
    }

    Copy-Item $builtExe "$BIN_DIR\nc.exe" -Force
    Write-Ok "Build successful"

    # Bundle runtime DLLs
    $bundled = 0
    foreach ($dll in @("libgcc_s_seh-1.dll", "libwinpthread-1.dll", "libstdc++-6.dll")) {
        $src = Join-Path $ccDir $dll
        $dst = Join-Path $BIN_DIR $dll
        if ((Test-Path $src) -and !(Test-Path $dst)) {
            Copy-Item $src $dst -Force
            $bundled++
        }
    }
    if ($bundled -gt 0) {
        Write-Ok "Bundled $bundled runtime DLLs"
    }

    return $true
}

# â”€â”€ Add to PATH â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Set-NcPath {
    Write-Step "Configuring PATH"

    $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    if (!$userPath -or $userPath -notlike "*$BIN_DIR*") {
        if ($userPath) {
            [Environment]::SetEnvironmentVariable("PATH", "$BIN_DIR;$userPath", "User")
        } else {
            [Environment]::SetEnvironmentVariable("PATH", $BIN_DIR, "User")
        }
        $env:PATH = "$BIN_DIR;$env:PATH"
        Write-Ok "Added $BIN_DIR to user PATH (permanent)"
    } else {
        Write-Ok "Already in PATH"
    }

    [Environment]::SetEnvironmentVariable("NC_LIB_PATH", $LIB_DIR, "User")
}

# â”€â”€ Verify installation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Test-Installation {
    Write-Step "Verifying installation"

    $env:NC_ACCEPT_LICENSE = "1"
    $ncExe = "$BIN_DIR\nc.exe"

    if (!(Test-Path $ncExe)) {
        Write-Err "Binary not found at $ncExe"
        return $false
    }

    try {
        $verOutput = & $ncExe version 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -or $verOutput -match "NC|Notation|$NC_VERSION") {
            Write-Ok "Verification passed"
            return $true
        }
    } catch {}

    try {
        $verOutput = & $ncExe --version 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) {
            Write-Ok "Verification passed"
            return $true
        }
    } catch {}

    Write-Warn "Binary exists but version check returned non-zero (may still work)"
    return $true
}

# â”€â”€ Print success â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Show-Success {
    Write-Host ""
    Write-Host "  ==============================================" -ForegroundColor Green
    Write-Host "    NC installed successfully!"                    -ForegroundColor Green
    Write-Host "  ==============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Location:  $BIN_DIR\nc.exe"
    Write-Host ""
    Write-Host "  Get started (restart terminal first):" -ForegroundColor Cyan
    Write-Host '    nc version                Show version info'
    Write-Host '    nc "show 42 + 8"          Run inline code'
    Write-Host '    nc run hello.nc            Run a program'
    Write-Host '    nc serve app.nc            Start HTTP server'
    Write-Host '    nc repl                    Interactive REPL'
    Write-Host ""
    Write-Host "  Other install methods:" -ForegroundColor Gray
    Write-Host "    pip install nc-lang        Python package"
    Write-Host "    choco install nc           Chocolatey"
    Write-Host "    docker pull nc-lang/nc     Docker"
    Write-Host ""
    Write-Warn "Restart your terminal for PATH changes to take effect."
    Write-Host ""
}

# â”€â”€ Print failure â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
function Show-Failure {
    Write-Host ""
    Write-Err "Installation failed"
    Write-Host ""
    Write-Host "  Try one of these alternatives:" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Option 1 - Python package:"
    Write-Host "    pip install nc-lang" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Option 2 - Chocolatey:"
    Write-Host "    choco install nc" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Option 3 - Install MSYS2 and build manually:"
    Write-Host "    winget install MSYS2.MSYS2" -ForegroundColor Yellow
    Write-Host "    # In MSYS2 terminal:" -ForegroundColor Gray
    Write-Host "    pacman -S mingw-w64-x86_64-gcc make" -ForegroundColor Yellow
    Write-Host "    cd engine && make" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Option 4 - Download w64devkit manually:"
    Write-Host "    https://github.com/skeeto/w64devkit/releases" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#  Main
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

Show-Banner
Confirm-License
Write-ConsentMarker
Write-Host ""

# Create directories
foreach ($dir in @($INSTALL_DIR, $BIN_DIR, $LIB_DIR)) {
    if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
}

# Detect source directory
$SOURCE_DIR = $null
if ($PSScriptRoot) {
    $ncMakefile = Join-Path $PSScriptRoot "nc\Makefile"
    if (Test-Path $ncMakefile) {
        $SOURCE_DIR = $PSScriptRoot
        Write-Info "Source code found at $SOURCE_DIR"
    }
}

$installed = $false

# Strategy 1: Pre-built binary
if (Try-PrebuiltBinary) {
    $installed = $true
}

# Strategy 2: Build from source
if (!$installed) {
    Write-Host ""

    # Check prerequisites
    Write-Step "Checking prerequisites"
    $compiler = Find-Compiler

    if (!$compiler) {
        # Try downloading w64devkit
        Write-Warn "No C compiler found"
        $compiler = Install-W64Devkit
        if (!$compiler) {
            Show-Failure
        }
    }

    $makePath = Find-Make -CompilerDir $compiler.CC
    if (!$makePath) {
        Write-Err "make not found"
        Write-Host ""
        Write-Host "  Install make:" -ForegroundColor Cyan
        if ($compiler.Type -eq "msys2") {
            Write-Host "    Open MSYS2 terminal and run: pacman -S make" -ForegroundColor Yellow
        } else {
            Write-Host "    make should be included with w64devkit or MSYS2" -ForegroundColor Yellow
        }
        Show-Failure
    }
    Write-Ok "Make: $makePath"
    Write-Host ""

    if (!$SOURCE_DIR) {
        # Try to clone
        Write-Info "Source code not found locally, attempting download..."
        $git = Get-Command git -ErrorAction SilentlyContinue
        if ($git) {
            $tmpDir = "$env:TEMP\nc-build-$(Get-Random)"
            & git clone --depth 1 "https://github.com/$REPO.git" $tmpDir 2>&1 | Out-Null
            if (Test-Path "$tmpDir\nc\Makefile") {
                $SOURCE_DIR = $tmpDir
                Write-Ok "Downloaded source code"
            }
        }
    }

    if (!$SOURCE_DIR) {
        Write-Err "Source code not found. Clone the repo first:"
        Write-Host "    git clone https://github.com/$REPO.git" -ForegroundColor Yellow
        Write-Host "    cd nc && powershell -File install.ps1" -ForegroundColor Yellow
        Show-Failure
    }

    if (Build-FromSource -Compiler $compiler -MakePath $makePath -SourceDir $SOURCE_DIR) {
        $installed = $true
    }
}

if (!$installed) {
    Show-Failure
}

# Install standard library
if ($SOURCE_DIR) {
    $libSource = Join-Path $SOURCE_DIR "Lib"
    if (Test-Path $libSource) {
        Write-Info "Installing standard library..."
        Copy-Item -Path "$libSource\*" -Destination $LIB_DIR -Recurse -Force -ErrorAction SilentlyContinue
        Write-Ok "Standard library installed"
    }
}

# Configure PATH and verify
Write-Host ""
Set-NcPath
Write-Host ""
Test-Installation
Show-Success

