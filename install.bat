@echo off
REM â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
REM  NC — Windows Batch Installer
REM
REM  Usage:
REM    install.bat
REM
REM  This script:
REM    1. Checks for GCC (w64devkit or MSYS2)
REM    2. Downloads w64devkit automatically if not found
REM    3. Builds NC from source
REM    4. Installs to %LOCALAPPDATA%\nc\bin and adds to PATH
REM    5. Verifies installation
REM
REM  Set NC_ACCEPT_LICENSE=1 before running to skip license prompt.
REM â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
setlocal enabledelayedexpansion

set "NC_VERSION=1.0.0"
set "INSTALL_DIR=%LOCALAPPDATA%\nc"
set "BIN_DIR=%INSTALL_DIR%\bin"
set "LIB_DIR=%INSTALL_DIR%\Lib"
set "W64DEVKIT_VER=2.6.0"
set "W64DEVKIT_URL=https://github.com/skeeto/w64devkit/releases/download/v%W64DEVKIT_VER%/w64devkit-x64-%W64DEVKIT_VER%.7z.exe"
set "W64DEVKIT_DIR=%INSTALL_DIR%\w64devkit"

echo.
echo   =============================================
echo    NC Installer for Windows v%NC_VERSION%
echo    The Notation Language Compiler
echo   =============================================
echo.

REM â”€â”€ License acceptance â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if "%NC_ACCEPT_LICENSE%"=="1" (
    echo   [+] License accepted via NC_ACCEPT_LICENSE
    goto :license_done
)
if "%NC_ACCEPT_LICENSE%"=="yes" (
    echo   [+] License accepted via NC_ACCEPT_LICENSE
    goto :license_done
)

echo   License Agreement
echo   â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo   NC is licensed under the Apache License 2.0.
echo   By installing NC, you agree to the license terms.
echo   Review: https://github.com/devheallabs-ai/nc-lang/blob/main/LICENSE
echo.
set /p "ACCEPT=  Type 'yes' to accept and continue: "
if /i not "%ACCEPT%"=="yes" (
    if /i not "%ACCEPT%"=="y" (
        echo   [x] Installation cancelled. License not accepted.
        goto :eof
    )
)
echo   [+] License accepted
:license_done
echo.

REM â”€â”€ Create directories â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"

REM â”€â”€ Detect source directory â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
set "SOURCE_DIR="
set "SCRIPT_DIR=%~dp0"
set "ENGINE_SUBDIR=engine"
if exist "%SCRIPT_DIR%engine\Makefile" (
    set "SOURCE_DIR=%SCRIPT_DIR%"
    set "ENGINE_SUBDIR=engine"
    echo   [*] Source code found at %SCRIPT_DIR%engine
) else if exist "%SCRIPT_DIR%nc\Makefile" (
    set "SOURCE_DIR=%SCRIPT_DIR%"
    set "ENGINE_SUBDIR=nc"
    echo   [*] Source code found at %SCRIPT_DIR%nc
)

REM â”€â”€ Strategy 1: Try downloading pre-built binary â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo   ==^> Checking for pre-built binary
set "BINARY_URL=https://github.com/devheallabs-ai/nc-lang/releases/download/v%NC_VERSION%/nc-windows-x86_64.exe"
set "DOWNLOADED=0"

where curl >nul 2>&1
if %errorlevel%==0 (
    curl -fsSL -o "%BIN_DIR%\nc.exe" "%BINARY_URL%" 2>nul
    if exist "%BIN_DIR%\nc.exe" (
        for %%A in ("%BIN_DIR%\nc.exe") do if %%~zA GTR 10000 (
            echo   [+] Downloaded pre-built binary
            set "DOWNLOADED=1"
        )
    )
)

if "%DOWNLOADED%"=="1" goto :install_path

REM â”€â”€ Check for C compiler â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo.
echo   ==^> Checking for C compiler
set "FOUND_CC="

REM Check MSYS2 MinGW GCC
if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "FOUND_CC=C:\msys64\mingw64\bin\gcc.exe"
    echo   [+] Found MSYS2 MinGW GCC
    goto :have_compiler
)

REM Check w64devkit
if exist "%W64DEVKIT_DIR%\bin\gcc.exe" (
    set "FOUND_CC=%W64DEVKIT_DIR%\bin\gcc.exe"
    echo   [+] Found w64devkit GCC
    goto :have_compiler
)

REM Check PATH
where gcc >nul 2>&1
if %errorlevel%==0 (
    for /f "tokens=*" %%i in ('where gcc') do set "FOUND_CC=%%i"
    echo   [+] Found GCC in PATH
    goto :have_compiler
)

where cl >nul 2>&1
if %errorlevel%==0 (
    echo   [+] Found MSVC cl.exe
    echo   [!] MSVC builds require CMake. Attempting w64devkit download instead...
    goto :download_w64devkit
)

REM â”€â”€ No compiler found: download w64devkit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
:download_w64devkit
echo   [!] No suitable C compiler found
echo.
echo   ==^> Downloading w64devkit (portable GCC for Windows)
echo   [*] URL: %W64DEVKIT_URL%

set "DL_FILE=%TEMP%\w64devkit.7z.exe"

where curl >nul 2>&1
if %errorlevel%==0 (
    echo   [*] Downloading with curl...
    curl -fSL -o "%DL_FILE%" "%W64DEVKIT_URL%"
    if %errorlevel% neq 0 (
        echo   [x] Download failed.
        goto :install_failed
    )
    goto :extract_w64devkit
)

where powershell >nul 2>&1
if %errorlevel%==0 (
    echo   [*] Downloading with PowerShell...
    powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%W64DEVKIT_URL%' -OutFile '%DL_FILE%' -UseBasicParsing"
    if %errorlevel% neq 0 (
        echo   [x] Download failed.
        goto :install_failed
    )
    goto :extract_w64devkit
)

echo   [x] Cannot download: need curl or PowerShell
goto :install_failed

:extract_w64devkit
echo   [*] Extracting w64devkit (self-extracting archive)...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

REM w64devkit is a self-extracting 7z archive
"%DL_FILE%" -o"%INSTALL_DIR%" -y >nul 2>&1

del "%DL_FILE%" 2>nul

if exist "%W64DEVKIT_DIR%\bin\gcc.exe" (
    set "FOUND_CC=%W64DEVKIT_DIR%\bin\gcc.exe"
    echo   [+] w64devkit installed at %W64DEVKIT_DIR%
) else (
    echo   [x] w64devkit extraction failed
    goto :install_failed
)

:have_compiler
echo.

REM â”€â”€ Check for make â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
set "FOUND_MAKE="

REM Derive make path from compiler path
for %%F in ("%FOUND_CC%") do set "CC_DIR=%%~dpF"
if exist "%CC_DIR%make.exe" (
    set "FOUND_MAKE=%CC_DIR%make.exe"
) else if exist "%CC_DIR%mingw32-make.exe" (
    set "FOUND_MAKE=%CC_DIR%mingw32-make.exe"
) else (
    where make >nul 2>&1
    if %errorlevel%==0 (
        for /f "tokens=*" %%i in ('where make') do set "FOUND_MAKE=%%i"
    ) else (
        where mingw32-make >nul 2>&1
        if %errorlevel%==0 (
            for /f "tokens=*" %%i in ('where mingw32-make') do set "FOUND_MAKE=%%i"
        )
    )
)

if "%FOUND_MAKE%"=="" (
    echo   [x] make not found. Install MSYS2 and run:
    echo       pacman -S make
    goto :install_failed
)

echo   [+] Make: %FOUND_MAKE%

REM â”€â”€ Build from source â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if "%SOURCE_DIR%"=="" (
    echo   [x] Source code not found. Clone the repo first:
    echo       git clone https://github.com/devheallabs-ai/nc-lang.git
    echo       cd nc-lang ^&^& install.bat
    goto :install_failed
)

echo.
echo   ==^> Building NC from source

REM Add compiler dir to PATH for the build
for %%F in ("%FOUND_CC%") do set "CC_BIN_DIR=%%~dpF"
set "PATH=%CC_BIN_DIR%;%PATH%"

pushd "%SOURCE_DIR%%ENGINE_SUBDIR%"
if not exist build mkdir build

REM Direct GCC compilation (most reliable on Windows)
echo   [*] Compiling with GCC...
gcc -o build\nc.exe -Iinclude -O2 -DNDEBUG -DNC_NO_REPL -w src\main.c src\nc_lexer.c src\nc_parser.c src\nc_compiler.c src\nc_vm.c src\nc_value.c src\nc_gc.c src\nc_json.c src\nc_http.c src\nc_server.c src\nc_stdlib.c src\nc_middleware.c src\nc_enterprise.c src\nc_async.c src\nc_module.c src\nc_optimizer.c src\nc_semantic.c src\nc_suggestions.c src\nc_migrate.c src\nc_repl.c src\nc_debug.c src\nc_lsp.c src\nc_tensor.c src\nc_autograd.c src\nc_llvm.c src\nc_jit.c src\nc_embed.c src\nc_plugin.c src\nc_pkg.c src\nc_distributed.c src\nc_database.c src\nc_websocket.c src\nc_polyglot.c src\nc_interp.c src\nc_crypto.c src\nc_generate.c src\nc_ai_router.c src\nc_ai_benchmark.c src\nc_ai_enterprise.c src\nc_nova_reasoning.c src\nc_ai_efficient.c src\nc_build.c src\nc_wasm.c src\nc_dataset.c src\nc_ui_compiler.c src\nc_ui_vm.c src\nc_model.c src\nc_training.c src\nc_tokenizer.c src\nc_cortex.c src\nc_nova.c -lws2_32 -lwinhttp
if exist "build\nc.exe" (
    copy /y "build\nc.exe" "%BIN_DIR%\nc.exe" >nul
    echo   [+] Build successful
) else (
    echo   [x] Build failed - nc.exe not produced
    popd
    goto :install_failed
)
popd

REM Bundle runtime DLLs if from MinGW
for %%F in ("%FOUND_CC%") do set "CC_BIN_DIR=%%~dpF"
for %%D in (libgcc_s_seh-1.dll libwinpthread-1.dll libstdc++-6.dll) do (
    if exist "%CC_BIN_DIR%%%D" (
        if not exist "%BIN_DIR%\%%D" (
            copy /y "%CC_BIN_DIR%%%D" "%BIN_DIR%\%%D" >nul
        )
    )
)

REM Copy standard library
if exist "%SOURCE_DIR%Lib" (
    xcopy /s /y /q "%SOURCE_DIR%Lib\*" "%LIB_DIR%\" >nul 2>&1
    echo   [+] Standard library installed
)

REM â”€â”€ Add to PATH â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
:install_path
echo.
echo   ==^> Configuring PATH

REM Check if already in user PATH
set "IN_PATH=0"
for /f "tokens=2*" %%A in ('reg query "HKCU\Environment" /v PATH 2^>nul') do (
    echo %%B | findstr /i /c:"%BIN_DIR%" >nul 2>&1
    if !errorlevel!==0 set "IN_PATH=1"
)

if "%IN_PATH%"=="0" (
    REM Add to user PATH permanently via registry
    for /f "tokens=2*" %%A in ('reg query "HKCU\Environment" /v PATH 2^>nul') do set "USER_PATH=%%B"
    if defined USER_PATH (
        setx PATH "%BIN_DIR%;%USER_PATH%" >nul 2>&1
    ) else (
        setx PATH "%BIN_DIR%" >nul 2>&1
    )
    set "PATH=%BIN_DIR%;%PATH%"
    echo   [+] Added %BIN_DIR% to user PATH (permanent)
) else (
    echo   [+] Already in PATH
)

REM Set NC_LIB_PATH
setx NC_LIB_PATH "%LIB_DIR%" >nul 2>&1

REM Write consent marker
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
(
    echo accepted=1
    echo license=Apache-2.0
    echo product=nc
    echo version=%NC_VERSION%
) > "%INSTALL_DIR%\license.accepted"

REM â”€â”€ Verify â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo.
echo   ==^> Verifying installation

set "NC_ACCEPT_LICENSE=1"
"%BIN_DIR%\nc.exe" version >nul 2>&1
if %errorlevel%==0 (
    echo   [+] Verification passed
) else (
    "%BIN_DIR%\nc.exe" --version >nul 2>&1
    if %errorlevel%==0 (
        echo   [+] Verification passed
    ) else (
        echo   [!] Binary exists but version check failed (may still work)
    )
)

REM â”€â”€ Success â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo.
echo   =============================================
echo    NC installed successfully!
echo   =============================================
echo.
echo   Location:  %BIN_DIR%\nc.exe
echo.
echo   Get started (restart terminal first):
echo     nc version                Show version info
echo     nc "show 42 + 8"         Run inline code
echo     nc run hello.nc           Run a program
echo     nc serve app.nc           Start HTTP server
echo.
echo   Other install methods:
echo     pip install nc-lang       Python package
echo     choco install nc          Chocolatey
echo     docker pull nc-lang/nc    Docker
echo.
goto :eof

:install_failed
echo.
echo   =============================================
echo    Installation failed
echo   =============================================
echo.
echo   Try one of these alternatives:
echo.
echo     Option 1 - Python package:
echo       pip install nc-lang
echo.
echo     Option 2 - Install MSYS2 and build:
echo       winget install MSYS2.MSYS2
echo       :: In MSYS2 terminal:
echo       pacman -S mingw-w64-x86_64-gcc make
echo       cd nc ^&^& make
echo.
echo     Option 3 - Use w64devkit:
echo       Download from https://github.com/skeeto/w64devkit/releases
echo       Extract, open w64devkit.exe, then:
echo       cd nc ^&^& make
echo.
exit /b 1

