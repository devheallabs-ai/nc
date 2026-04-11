@echo off
REM ═══════════════════════════════════════════════════════════════════════
REM  NC (Notation-as-Code) — Windows Starter Script
REM
REM  Usage:
REM    start.bat              Interactive menu
REM    start.bat build        Build NC engine from source
REM    start.bat test         Run all tests
REM    start.bat install      Build + install to %LOCALAPPDATA%\nc\bin
REM    start.bat serve <f>    Run an NC service
REM    start.bat docker       Build Docker image
REM    start.bat docker-run   Run NC in Docker container
REM    start.bat clean        Clean build artifacts
REM    start.bat showcase     List and run showcase projects
REM    start.bat help         Show this help
REM ═══════════════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "ENGINE_DIR=%SCRIPT_DIR%engine"
set "BUILD_DIR=%ENGINE_DIR%\build"
set "TESTS_DIR=%SCRIPT_DIR%tests"
set "SHOWCASE_DIR=%SCRIPT_DIR%showcase"
set "NC_BIN=%BUILD_DIR%\nc.exe"

REM ── Route command ────────────────────────────────────────────────────
if "%~1"=="" goto :menu
if /i "%~1"=="build" goto :build
if /i "%~1"=="test" goto :test
if /i "%~1"=="install" goto :install
if /i "%~1"=="serve" goto :serve
if /i "%~1"=="docker" goto :docker
if /i "%~1"=="docker-run" goto :docker_run
if /i "%~1"=="clean" goto :clean
if /i "%~1"=="showcase" goto :showcase
if /i "%~1"=="help" goto :help
if /i "%~1"=="--help" goto :help
if /i "%~1"=="-h" goto :help
echo [ERROR] Unknown command: %~1
goto :help

REM ═══════════════════════════════════════════════════════════════════════
REM  BUILD
REM ═══════════════════════════════════════════════════════════════════════
:build
echo.
echo === Building NC Engine ===
echo.

where gcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] GCC not found. Install MinGW-w64 or MSYS2 and add gcc to PATH.
    echo   Download: https://www.msys2.org/
    echo   Then run: pacman -S mingw-w64-x86_64-gcc
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [INFO]  Compiling NC engine ...
cd /d "%ENGINE_DIR%"

gcc -o "%BUILD_DIR%\nc.exe" -Iinclude -O2 -DNDEBUG -DNC_NO_REPL -w ^
    src\main.c src\nc_lexer.c src\nc_parser.c src\nc_compiler.c ^
    src\nc_vm.c src\nc_value.c src\nc_gc.c src\nc_json.c ^
    src\nc_http.c src\nc_server.c src\nc_stdlib.c src\nc_middleware.c ^
    src\nc_enterprise.c src\nc_async.c src\nc_module.c ^
    src\nc_optimizer.c src\nc_semantic.c src\nc_suggestions.c ^
    src\nc_migrate.c src\nc_repl.c src\nc_debug.c src\nc_lsp.c ^
    src\nc_tensor.c src\nc_autograd.c src\nc_llvm.c src\nc_jit.c ^
    src\nc_embed.c src\nc_plugin.c src\nc_pkg.c src\nc_distributed.c ^
    src\nc_database.c src\nc_websocket.c src\nc_polyglot.c ^
    src\nc_interp.c -lws2_32 -lwinhttp

if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK]    Build complete: %NC_BIN%
echo.
"%NC_BIN%" version 2>nul
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  TEST
REM ═══════════════════════════════════════════════════════════════════════
:test
echo.
echo === Running NC Tests ===
echo.

if not exist "%NC_BIN%" (
    echo [ERROR] NC binary not found. Run: start.bat build
    exit /b 1
)

if exist "%TESTS_DIR%\run_tests.ps1" (
    echo [INFO]  Running test suite via run_tests.ps1 ...
    powershell -ExecutionPolicy Bypass -File "%TESTS_DIR%\run_tests.ps1"
) else (
    echo [INFO]  Running tests via nc test ...
    cd /d "%ENGINE_DIR%"
    "%NC_BIN%" test
)

set PASSED=0
set FAILED=0
set TOTAL=0

if exist "%TESTS_DIR%\lang" (
    echo [INFO]  Running language tests ...
    for %%f in ("%TESTS_DIR%\lang\*.nc") do (
        set /a TOTAL+=1
        "%NC_BIN%" run "%%f" >nul 2>&1
        if errorlevel 1 (
            set /a FAILED+=1
            echo [WARN]  FAIL: %%~nxf
        ) else (
            set /a PASSED+=1
        )
    )
    echo.
    if !FAILED!==0 (
        echo [OK]    All !TOTAL! tests passed.
    ) else (
        echo [ERROR] !FAILED!/!TOTAL! tests failed.
        exit /b 1
    )
)
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  INSTALL
REM ═══════════════════════════════════════════════════════════════════════
:install
echo.
echo === Installing NC ===
echo.
call :build
if errorlevel 1 exit /b 1

set "INSTALL_DIR=%LOCALAPPDATA%\nc\bin"

if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
copy /y "%NC_BIN%" "%INSTALL_DIR%\nc.exe" >nul

echo [OK]    Installed to %INSTALL_DIR%\nc.exe

REM Check if already in PATH
echo %PATH% | findstr /i /c:"%INSTALL_DIR%" >nul
if errorlevel 1 (
    echo [WARN]  %INSTALL_DIR% is not in your PATH.
    echo         Add it manually or run:
    echo         setx PATH "%%PATH%%;%INSTALL_DIR%"
)

echo [OK]    Run 'nc version' to verify.
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  SERVE
REM ═══════════════════════════════════════════════════════════════════════
:serve
if "%~2"=="" (
    echo [ERROR] Usage: start.bat serve ^<file.nc^>
    exit /b 1
)
if not exist "%NC_BIN%" (
    echo [ERROR] NC binary not found. Run: start.bat build
    exit /b 1
)

set "SERVE_FILE=%~2"
if not exist "%SERVE_FILE%" (
    if exist "%SCRIPT_DIR%%SERVE_FILE%" (
        set "SERVE_FILE=%SCRIPT_DIR%%SERVE_FILE%"
    ) else (
        echo [ERROR] File not found: %SERVE_FILE%
        exit /b 1
    )
)

echo.
echo === Serving NC Application ===
echo [INFO]  File: %SERVE_FILE%
echo.
"%NC_BIN%" serve "%SERVE_FILE%"
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  DOCKER
REM ═══════════════════════════════════════════════════════════════════════
:docker
echo.
echo === Building Docker Image ===
echo.

where docker >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed or not in PATH.
    exit /b 1
)

cd /d "%SCRIPT_DIR%"
docker build -t nc:latest .
if errorlevel 1 (
    echo [ERROR] Docker build failed.
    exit /b 1
)
echo [OK]    Docker image built: nc:latest
echo.
echo   Run:   docker run -it nc:latest version
echo   Serve: docker run -p 8080:8080 -v %cd%:/app nc:latest serve /app/service.nc
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  DOCKER-RUN
REM ═══════════════════════════════════════════════════════════════════════
:docker_run
echo.
echo === Running NC in Docker ===
echo.

where docker >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed or not in PATH.
    exit /b 1
)

if not "%~2"=="" (
    echo [INFO]  Serving %~2 in Docker ...
    docker run --rm -it -p 8080:8080 -v "%SCRIPT_DIR%:/app" nc:latest serve "/app/%~2"
) else (
    echo [INFO]  Starting NC container ...
    docker run --rm -it -p 8080:8080 -v "%SCRIPT_DIR%:/app" nc:latest version
)
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  CLEAN
REM ═══════════════════════════════════════════════════════════════════════
:clean
echo.
echo === Cleaning Build Artifacts ===
echo.
if exist "%BUILD_DIR%\nc.exe" del /q "%BUILD_DIR%\nc.exe"
if exist "%BUILD_DIR%\*.o" del /q "%BUILD_DIR%\*.o"
if exist "%BUILD_DIR%\*.obj" del /q "%BUILD_DIR%\*.obj"
echo [OK]    Build artifacts removed.
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  SHOWCASE
REM ═══════════════════════════════════════════════════════════════════════
:showcase
echo.
echo === NC Showcase Projects ===
echo.

if not exist "%SHOWCASE_DIR%" (
    echo [ERROR] Showcase directory not found.
    exit /b 1
)

set IDX=0
for /d %%d in ("%SHOWCASE_DIR%\*") do (
    set /a IDX+=1
    set "PROJECT_!IDX!=%%~nxd"
    echo   !IDX!^) %%~nxd
    if exist "%%d\README.md" (
        for /f "skip=1 tokens=*" %%l in (%%d\README.md) do (
            if not defined DESC_!IDX! (
                set "DESC_!IDX!=%%l"
                echo      %%l
            )
        )
    )
    for %%f in ("%%d\*.nc") do (
        echo      nc serve showcase\%%~nxd\%%~nxf
    )
    echo.
)

set /p "CHOICE=Enter a number to run, or press Enter to go back: "
if "%CHOICE%"=="" goto :eof

set "SELECTED=!PROJECT_%CHOICE%!"
if "%SELECTED%"=="" (
    echo [ERROR] Invalid choice.
    goto :eof
)

if not exist "%NC_BIN%" (
    echo [ERROR] NC binary not found. Run: start.bat build
    exit /b 1
)

for %%f in ("%SHOWCASE_DIR%\%SELECTED%\*.nc") do (
    echo [INFO]  Starting %SELECTED% ...
    "%NC_BIN%" serve "%%f"
    goto :eof
)
echo [ERROR] No .nc file found in showcase\%SELECTED%
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  HELP
REM ═══════════════════════════════════════════════════════════════════════
:help
echo.
echo NC (Notation-as-Code) -- Project Starter
echo.
echo   Usage: start.bat [command] [args]
echo.
echo   Commands:
echo     build          Build NC engine from source
echo     test           Run all tests
echo     install        Build + install to %%LOCALAPPDATA%%\nc\bin
echo     serve ^<file^>   Run an NC service
echo     docker         Build Docker image
echo     docker-run     Run NC in Docker container
echo     clean          Clean build artifacts
echo     showcase       List and run showcase projects
echo     help           Show this help
echo.
echo   Examples:
echo     start.bat build
echo     start.bat serve examples\01_hello_world.nc
echo     start.bat docker
echo     start.bat showcase
echo.
goto :eof

REM ═══════════════════════════════════════════════════════════════════════
REM  INTERACTIVE MENU
REM ═══════════════════════════════════════════════════════════════════════
:menu
echo.
echo   ███╗   ██╗ ██████╗
echo   ████╗  ██║██╔════╝
echo   ██╔██╗ ██║██║
echo   ██║╚██╗██║██║
echo   ██║ ╚████║╚██████╗
echo   ╚═╝  ╚═══╝ ╚═════╝
echo.
echo   Notation-as-Code -- The AI Programming Language
echo   Write AI backends in plain English
echo.
echo   Select an action:
echo.
echo     1) Build NC engine from source
echo     2) Run all tests
echo     3) Build + install
echo     4) Serve an NC file
echo     5) Build Docker image
echo     6) Run NC in Docker
echo     7) Clean build artifacts
echo     8) Browse showcase projects
echo     9) Show help
echo     0) Exit
echo.
set /p "CHOICE=  Choice [0-9]: "
echo.

if "%CHOICE%"=="1" goto :build
if "%CHOICE%"=="2" goto :test
if "%CHOICE%"=="3" goto :install
if "%CHOICE%"=="4" (
    set /p "SERVE_FILE=  Enter .nc file path: "
    "%NC_BIN%" serve "!SERVE_FILE!"
    goto :eof
)
if "%CHOICE%"=="5" goto :docker
if "%CHOICE%"=="6" (
    set /p "DOCKER_FILE=  Enter .nc file path (or Enter for version): "
    if "!DOCKER_FILE!"=="" (
        docker run --rm -it nc:latest version
    ) else (
        docker run --rm -it -p 8080:8080 -v "%SCRIPT_DIR%:/app" nc:latest serve "/app/!DOCKER_FILE!"
    )
    goto :eof
)
if "%CHOICE%"=="7" goto :clean
if "%CHOICE%"=="8" goto :showcase
if "%CHOICE%"=="9" goto :help
if "%CHOICE%"=="0" (
    echo   Bye!
    goto :eof
)
echo [ERROR] Invalid choice: %CHOICE%
goto :eof
