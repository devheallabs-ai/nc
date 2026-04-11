CC ?= cc
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

CFLAGS = -Wall -Wextra -O2 -std=c11 -I include \
         -Wno-unused-function -Wno-unused-variable \
         -fomit-frame-pointer -DNDEBUG \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1

# Platform detection: check uname FIRST (MSYS2/MinGW), then OS env var.
# When running "cmd /c ... && make" with MSYS2 in PATH, OS=Windows_NT
# but uname returns MINGW64_NT or MSYS_NT. MSYS2 make uses sh (not cmd)
# for recipe commands, so we must use sh-compatible syntax everywhere.

ifneq ($(findstring MINGW,$(UNAME_S)),)
  ifeq ($(CC),cc)
    CC = gcc
  endif
  # MSYS2 MinGW (most common Windows build)
  # REPL uses basic fgets fallback (no readline needed)
  CFLAGS += -DNC_WINDOWS
  LDFLAGS = -lm -lwinhttp -lws2_32
  BIN = build/nc.exe
  UNIT_BIN = build/test_nc.exe
  MKDIR = mkdir -p build
else ifneq ($(findstring MSYS,$(UNAME_S)),)
  ifeq ($(CC),cc)
    CC = gcc
  endif
  # MSYS2 MSYS shell
  CFLAGS += -DNC_WINDOWS
  LDFLAGS = -lm -lwinhttp -lws2_32
  BIN = build/nc.exe
  UNIT_BIN = build/test_nc.exe
  MKDIR = mkdir -p build
else ifeq ($(UNAME_S),Windows)
  ifeq ($(CC),cc)
    CC = gcc
  endif
  # Native Windows without uname (nmake or similar)
  CFLAGS += -DNC_WINDOWS
  LDFLAGS = -lm -lwinhttp -lws2_32
  BIN = build/nc.exe
  UNIT_BIN = build/test_nc.exe
  MKDIR = mkdir -p build
else
  # POSIX (Linux / macOS)
  CFLAGS += -D_GNU_SOURCE
  LDFLAGS = -lm -lcurl -lpthread -ldl
  # Apple Accelerate + Metal GPU acceleration (macOS)
  ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DACCELERATE_NEW_LAPACK
    LDFLAGS += -framework Accelerate -framework Metal -framework MetalPerformanceShaders -framework Foundation
  else
    # Linux: try to link OpenBLAS for BLAS acceleration (10-100x faster matmul)
    OPENBLAS_CHECK := $(shell echo '\#include <cblas.h>' | $(CC) -x c -E - 2>/dev/null && echo yes)
    ifeq ($(OPENBLAS_CHECK),yes)
      LDFLAGS += -lopenblas
    endif
  endif
  BIN = build/nc
  UNIT_BIN = build/test_nc
  MKDIR = mkdir -p build
  # Try to link libedit (macOS) or readline (Linux) for REPL
  EDIT_CHECK := $(shell echo '\#include <editline/readline.h>' | $(CC) -x c -E - 2>/dev/null && echo yes)
  READLINE_CHECK := $(shell echo '\#include <readline/readline.h>' | $(CC) -x c -E - 2>/dev/null && echo yes)
  ifeq ($(EDIT_CHECK),yes)
    LDFLAGS += -ledit
  else ifeq ($(READLINE_CHECK),yes)
    LDFLAGS += -lreadline
  else
    CFLAGS += -DNC_NO_REPL
  endif
endif

# NOTE: Proprietary NOVA engine files (nc_nova.c, nc_training.c, nc_model.c, etc.)
# are not compiled in the open-source build. They ship as precompiled binaries
# in official NC releases. See NOVA_ARCHITECTURE.md for details.
SRC = src/main.c src/nc_value.c src/nc_lexer.c src/nc_vm.c \
      src/nc_parser.c src/nc_interp.c src/nc_json.c src/nc_http.c \
      src/nc_compiler.c src/nc_llvm.c \
      src/nc_pkg.c src/nc_debug.c src/nc_lsp.c \
      src/nc_semantic.c src/nc_gc.c src/nc_repl.c src/nc_stdlib.c \
      src/nc_module.c src/nc_optimizer.c src/nc_polyglot.c src/nc_migrate.c \
      src/nc_jit.c src/nc_async.c src/nc_enterprise.c \
      src/nc_server.c src/nc_database.c src/nc_websocket.c src/nc_middleware.c \
      src/nc_suggestions.c src/nc_plugin.c src/nc_embed.c \
      src/nc_ai_router.c \
      src/nc_ai_enterprise.c \
      src/nc_ai_efficient.c src/nc_build.c src/nc_wasm.c \
      src/nc_ui_compiler.c src/nc_ui_vm.c src/nc_ui_html.c \
      src/nc_terminal_ui.c src/nc_cuda.c \
      src/nc_generate_stubs.c

OBJ = $(SRC:src/%.c=build/%.o)

# Metal GPU: Objective-C on macOS only if nc_metal.m is present (proprietary).
# Falls back to no-op stub for source builds and on Linux/Windows.
ifeq ($(UNAME_S),Darwin)
  ifneq ($(wildcard src/nc_metal.m),)
    OBJ += build/nc_metal.o
  else
    OBJ += build/nc_metal_noop.o
  endif
else
  OBJ += build/nc_metal_noop.o
endif
UNIT_SRC = $(filter-out src/main.c,$(SRC))
COV_CFLAGS = -Wall -Wextra -O0 -g --coverage -std=c11 -I include \
             -Wno-unused-function -Wno-unused-variable -D_GNU_SOURCE
COV_LDFLAGS = --coverage -lm -lcurl -lpthread -ledit -ldl

.PHONY: all clean test test-unit install static debug coverage

all: $(BIN)

$(BIN): $(OBJ)
	@$(MKDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  NC built successfully"
	@echo "  Binary: $(BIN)"
	@echo ""

# Objective-C rule for Metal GPU (macOS)
build/nc_metal.o: src/nc_metal.m src/nc_metal.h
	@$(MKDIR)
	$(CC) -x objective-c $(CFLAGS) -c -o $@ $<

build/%.o: src/%.c include/nc.h
	@$(MKDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Static build for Alpine/Docker (musl libc, fully self-contained)
static:
	$(MAKE) CFLAGS="$(CFLAGS) -static" LDFLAGS="-static -lm -lcurl -lpthread -lnghttp2 -lssl -lcrypto -lz -lbrotlidec -lbrotlicommon" 2>/dev/null || \
	$(MAKE) CFLAGS="$(CFLAGS) -static" LDFLAGS="-static -lm -lcurl -lpthread" 2>/dev/null || \
	$(MAKE)

# Debug build
debug:
	$(MAKE) CFLAGS="-Wall -Wextra -O0 -g -std=c11 -I include -Wno-unused-function -Wno-unused-variable" LDFLAGS="-lm -lcurl -lpthread"

test: $(BIN)
	@./$(BIN) test

$(UNIT_BIN): tests/test_nc.c $(filter-out build/main.o,$(OBJ))
	@$(MKDIR)
	$(CC) $(CFLAGS) -o $@ tests/test_nc.c $(filter-out build/main.o,$(OBJ)) $(LDFLAGS)

test-unit: $(UNIT_BIN)
	@./$(UNIT_BIN)

test-vulnerabilities: tests/test_vulnerabilities.c $(UNIT_SRC)
	@$(MKDIR)
	$(CC) $(CFLAGS) -o build/test_vuln.exe tests/test_vulnerabilities.c $(UNIT_SRC) $(LDFLAGS)
	@./build/test_vuln.exe

install: $(BIN)
	@mkdir -p /usr/local/bin
	cp $(BIN) /usr/local/bin/nc
	@echo "  Installed: /usr/local/bin/nc"
	@echo "  Run 'nc version' to verify."

uninstall:
	rm -f /usr/local/bin/nc
	@echo "  Uninstalled NC from /usr/local/bin/nc"

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(COV_CFLAGS)" LDFLAGS="$(COV_LDFLAGS)" all
	$(MAKE) CFLAGS="$(COV_CFLAGS)" LDFLAGS="$(COV_LDFLAGS)" test-unit
	@cd .. && ./tests/run_tests.sh lang
	@cd .. && ./tests/run_tests.sh integration
	@cd .. && python3 tests/coverage_report.py --min-line "$${NC_COVERAGE_MIN:-100}"

clean:
	rm -rf build/*.o build/*.gcda build/*.gcno build/nc build/nc.exe build/test_nc build/test_nc.exe *.ll
