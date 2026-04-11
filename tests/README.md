# NC Test Suite

**Version:** v1.0.0

Tests for the NC language.

## Metrics

| Category | Count |
|----------|-------|
| Language test files (`tests/lang/`) | 56 |
| Test behaviors (across all test files) | 648 |
| C unit test assertions (`nc/tests/test_nc.c`) | 296 |

## Test Categories

- **values** — Literals, types, numbers
- **strings** — Concatenation, templates, stdlib
- **lists** — List operations, indexing, slicing
- **maps** — Records, map variables, dot access
- **lexer** — Tokenization
- **parser** — Grammar, AST
- **compiler+VM** — Bytecode, compilation
- **interpreter** — Execution
- **JSON** — JSON encode/decode, JSON edge cases
- **stdlib** — Built-in functions
- **GC** — Garbage collection
- **middleware auth** — Authentication middleware
- **rate limiting** — Rate limiter
- **platform abstractions** — Cross-platform I/O
- **async/thread pool** — Concurrency
- **JSON edge cases** — Boundary conditions

## Directory Structure

```
tests/
├── run_tests.sh              # Main test runner
├── run_tests_xplat.sh         # Cross-platform test runner (Linux/macOS/Windows)
├── test_developer_workflow.nc # Developer workflow test (11 behaviors)
├── lang/                      # Language tests (written in NC) — 85 files
│   ├── test_literals.nc       # Integer, float, string, bool, nothing
│   ├── test_math.nc           # +, -, *, /, precedence, compound
│   ├── test_strings.nc        # Concatenation, templates, mixed types
│   ├── test_variables.nc      # set/to, overwrite, multiple vars
│   ├── test_if.nc             # if/otherwise, comparisons, nested, and/or
│   ├── test_loops.nc          # repeat for each, accumulation, fibonacci
│   ├── test_match.nc          # match/when, otherwise
│   ├── test_lists.nc          # List literals, len(), mixed types
│   ├── test_types.nc          # define/as type declarations
│   ├── test_try.nc            # try/on error
│   ├── test_api.nc            # API route declarations (GET/POST/PUT/DELETE)
│   └── ...                    # (46 more language test files)
└── nc/tests/
    └── test_nc.c              # 296 C unit test assertions covering values,
                               # strings, lists, maps, lexer, parser, compiler,
                               # VM, interpreter, JSON, stdlib, GC, etc.
```

### Developer Workflow Test

`tests/test_developer_workflow.nc` contains **11 behaviors** that validate the full developer workflow:

1. Variables & data types
2. String operations
3. List operations
4. Control flow
5. JSON operations
6. Math functions
7. File I/O
8. Environment variables
9. Error handling
10. Time functions
11. Main runner (`run_all_tests`)

## Running Tests

```bash
# Run ALL tests
./tests/run_tests.sh

# Cross-platform runner (Linux, macOS, Windows/Git Bash)
./tests/run_tests_xplat.sh

# Run only language tests (.nc files)
./tests/run_tests.sh lang

# Run only C unit tests (runtime internals)
./tests/run_tests.sh unit

# Run only integration tests (full pipeline)
./tests/run_tests.sh integration

# Run C unit tests directly via make
cd engine && make test-unit

# Run developer workflow test
nc run tests/test_developer_workflow.nc
```

## Terminal UX Cross-Platform Contract

The test runners now validate terminal output behavior on Linux/macOS/Windows:

- `NC_NO_ANIM=1` disables spinner/progress animations and uses plain output.
- `NO_COLOR=1` disables ANSI colors for logs, banners, and status output.
- Non-interactive terminals automatically fall back to plain text output.

Quick checks:

```bash
NC_NO_ANIM=1 ./engine/build/nc ai status
NO_COLOR=1 ./engine/build/nc version
```

```powershell
$env:NC_NO_ANIM="1"; .\engine\build\nc.exe ai status
$env:NO_COLOR="1"; .\engine\build\nc.exe version
```

On Windows, if `engine/build/nc.exe` is locked by a running process, the test runners automatically fall back to `engine/build/nc_new.exe`. You can also override the binary explicitly with `NC_BIN`.

## Writing New Tests

### Language Tests (`.nc` files)
Add a new `test_*.nc` file in `tests/lang/`. The test runner will:
1. Validate it (lexer + parser)
2. Compile and run it (compiler + VM)
3. Generate bytecode
4. Run each behavior individually

### C Unit Tests
Add assertions to `nc/tests/test_nc.c` using the `ASSERT_*` macros.

### Integration Tests
Add new commands to `tests/run_tests.sh` in the `run_integration_tests()` function.
