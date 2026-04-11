cmake_minimum_required(VERSION 3.14)
project(nc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(NC_NO_REPL "Build without REPL (readline/libedit)" OFF)

# Source files — core NC language engine
set(NC_SOURCES
    src/main.c
    src/nc_value.c
    src/nc_lexer.c
    src/nc_vm.c
    src/nc_parser.c
    src/nc_interp.c
    src/nc_json.c
    src/nc_http.c
    src/nc_compiler.c
    src/nc_llvm.c
    src/nc_tensor.c
    src/nc_pkg.c
    src/nc_debug.c
    src/nc_lsp.c
    src/nc_semantic.c
    src/nc_gc.c
    src/nc_repl.c
    src/nc_stdlib.c
    src/nc_module.c
    src/nc_autograd.c
    src/nc_optimizer.c
    src/nc_polyglot.c
    src/nc_migrate.c
    src/nc_jit.c
    src/nc_async.c
    src/nc_distributed.c
    src/nc_enterprise.c
    src/nc_server.c
    src/nc_database.c
    src/nc_websocket.c
    src/nc_middleware.c
    src/nc_suggestions.c
    src/nc_embed.c
    src/nc_plugin.c
    # AI / ML engine (NC-AI + NOVA)
    src/nc_model.c
    src/nc_training.c
    src/nc_tokenizer.c
    src/nc_cortex.c
    src/nc_nova.c
    src/nc_nova_reasoning.c
    src/nc_crypto.c
    src/nc_generate.c
    src/nc_ai_router.c
    src/nc_ai_benchmark.c
    src/nc_ai_enterprise.c
    src/nc_ai_efficient.c
    src/nc_build.c
    src/nc_wasm.c
    src/nc_dataset.c
    src/nc_terminal_ui.c
    src/nc_ui_compiler.c
    src/nc_ui_vm.c
)

add_executable(nc ${NC_SOURCES})
target_include_directories(nc PRIVATE include)

# macOS: Metal GPU acceleration + Accelerate BLAS
if(APPLE)
    # Compile Metal Objective-C file
    set_source_files_properties(src/nc_metal.m PROPERTIES LANGUAGE C)
    target_sources(nc PRIVATE src/nc_metal.m)
    target_compile_definitions(nc PRIVATE ACCELERATE_NEW_LAPACK)
    target_link_libraries(nc PRIVATE
        "-framework Accelerate"
        "-framework Metal"
        "-framework MetalPerformanceShaders"
        "-framework Foundation"
    )
else()
    # Non-macOS: Metal no-op stub
    target_sources(nc PRIVATE src/nc_metal_noop.c)
endif()

# Platform-specific configuration
if(WIN32)
    target_link_libraries(nc PRIVATE ws2_32)
    target_compile_definitions(nc PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(NC_NO_REPL)
        target_compile_definitions(nc PRIVATE NC_NO_REPL)
    endif()
    # On Windows, we use the basic fgets fallback in nc_repl.c if readline is missing.
    # No need to auto-disable the REPL here.
    find_library(READLINE_LIB readline)
    if(READLINE_LIB)
        target_link_libraries(nc PRIVATE ${READLINE_LIB})
    endif()
else()
    target_compile_definitions(nc PRIVATE _GNU_SOURCE)
    target_link_libraries(nc PRIVATE m pthread dl)

    if(NC_NO_REPL)
        target_compile_definitions(nc PRIVATE NC_NO_REPL)
    else()
        find_library(EDIT_LIB edit)
        find_library(READLINE_LIB readline)
        if(EDIT_LIB)
            target_link_libraries(nc PRIVATE ${EDIT_LIB})
        elseif(READLINE_LIB)
            target_link_libraries(nc PRIVATE ${READLINE_LIB})
        else()
            message(STATUS "Neither libedit nor readline found — disabling REPL")
            target_compile_definitions(nc PRIVATE NC_NO_REPL)
        endif()
    endif()
endif()

# HTTP backend: WinHTTP on Windows (zero DLLs), libcurl elsewhere
if(WIN32)
    target_link_libraries(nc PRIVATE winhttp)
else()
    find_package(CURL REQUIRED)
    target_link_libraries(nc PRIVATE CURL::libcurl)
endif()

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(nc PRIVATE
        -Wall -Wextra -O2
        -Wno-unused-function -Wno-unused-variable
        -fomit-frame-pointer
    )
elseif(MSVC)
    target_compile_options(nc PRIVATE /W4 /O2 /wd4996 /wd4244 /wd4267)
endif()

# Unit tests
set(NC_UNIT_SOURCES ${NC_SOURCES})
list(REMOVE_ITEM NC_UNIT_SOURCES src/main.c)
list(APPEND NC_UNIT_SOURCES tests/test_nc.c)

add_executable(test_nc ${NC_UNIT_SOURCES})
target_include_directories(test_nc PRIVATE include)
if(WIN32)
    target_link_libraries(test_nc PRIVATE ws2_32 winhttp)
    target_compile_definitions(test_nc PRIVATE _CRT_SECURE_NO_WARNINGS NC_NO_REPL)
else()
    find_package(CURL REQUIRED)
    target_link_libraries(test_nc PRIVATE CURL::libcurl)
    target_compile_definitions(test_nc PRIVATE _GNU_SOURCE NC_NO_REPL)
    target_link_libraries(test_nc PRIVATE m pthread dl)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(test_nc PRIVATE
        -Wall -Wextra -O2
        -Wno-unused-function -Wno-unused-variable
    )
elseif(MSVC)
    target_compile_options(test_nc PRIVATE /W4 /O2 /wd4996 /wd4244 /wd4267)
endif()

# Install target
install(TARGETS nc RUNTIME DESTINATION bin)
