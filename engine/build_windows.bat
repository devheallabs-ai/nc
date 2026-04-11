@echo off
setlocal
REM NC Engine Windows Build Script
REM Auto-detects w64devkit or common MSYS2 installs.

set "NC_GCC=gcc"
set "NC_TOOLCHAIN="

if defined W64DEVKIT_PATH if exist "%W64DEVKIT_PATH%\bin\gcc.exe" (
    set "PATH=%W64DEVKIT_PATH%\bin;%PATH%"
    set "NC_TOOLCHAIN=w64devkit"
)

if not defined NC_TOOLCHAIN if defined MSYS2_ROOT if exist "%MSYS2_ROOT%\mingw64\bin\gcc.exe" (
    set "PATH=%MSYS2_ROOT%\mingw64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"
    set "NC_TOOLCHAIN=msys2-mingw64"
)

if not defined NC_TOOLCHAIN if defined MSYS2_ROOT if exist "%MSYS2_ROOT%\ucrt64\bin\gcc.exe" (
    set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"
    set "NC_TOOLCHAIN=msys2-ucrt64"
)

if not defined NC_TOOLCHAIN if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%"
    set "NC_TOOLCHAIN=msys2-mingw64"
)

if not defined NC_TOOLCHAIN if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
    set "NC_TOOLCHAIN=msys2-ucrt64"
)

if not defined NC_TOOLCHAIN if exist "C:\tools\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\tools\msys64\mingw64\bin;C:\tools\msys64\usr\bin;%PATH%"
    set "NC_TOOLCHAIN=msys2-mingw64"
)

where %NC_GCC% >nul 2>&1 || (
    echo ERROR: gcc not found. Set W64DEVKIT_PATH, MSYS2_ROOT, or install MSYS2 to C:\msys64.
    exit /b 1
)

if defined NC_TOOLCHAIN (
    echo Using toolchain: %NC_TOOLCHAIN%
) else (
    echo Using toolchain: gcc from PATH
)

cd /d "%~dp0"
if not exist build mkdir build

echo [1/2] Compiling...
for %%f in (src\*.c) do (
    %NC_GCC% -c -Iinclude -O2 -DNDEBUG -DNC_NO_REPL -std=c11 -w %%f -o build\%%~nf.o
    if errorlevel 1 (
        echo COMPILE FAILED: %%f
        exit /b 1
    )
)
echo [1/2] Compile done.

echo [2/2] Linking...
%NC_GCC% -o build\nc.exe build\main.o build\nc_value.o build\nc_lexer.o build\nc_vm.o build\nc_parser.o build\nc_interp.o build\nc_json.o build\nc_http.o build\nc_compiler.o build\nc_llvm.o build\nc_tensor.o build\nc_pkg.o build\nc_debug.o build\nc_lsp.o build\nc_semantic.o build\nc_gc.o build\nc_repl.o build\nc_stdlib.o build\nc_module.o build\nc_autograd.o build\nc_optimizer.o build\nc_polyglot.o build\nc_migrate.o build\nc_jit.o build\nc_async.o build\nc_distributed.o build\nc_enterprise.o build\nc_server.o build\nc_database.o build\nc_websocket.o build\nc_middleware.o build\nc_suggestions.o build\nc_plugin.o build\nc_embed.o build\nc_model.o build\nc_training.o build\nc_tokenizer.o build\nc_cortex.o build\nc_nova.o build\nc_crypto.o build\nc_generate.o build\nc_ai_router.o build\nc_ai_benchmark.o build\nc_ai_enterprise.o build\nc_nova_reasoning.o build\nc_ai_efficient.o build\nc_build.o build\nc_wasm.o build\nc_dataset.o build\nc_ui_compiler.o build\nc_ui_vm.o build\nc_ui_html.o build\nc_terminal_ui.o build\nc_metal_noop.o -lws2_32 -lwinhttp -lm

if exist build\nc.exe (
    echo.
    echo ============================
    echo  NC built successfully!
    echo  Binary: build\nc.exe
    echo ============================
    build\nc.exe version
) else (
    echo.
    echo BUILD FAILED - nc.exe not produced
    exit /b 1
)
