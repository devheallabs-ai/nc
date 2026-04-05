<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="250">
</p>

# Contributing to NC

Thank you for your interest in contributing to NC!

## Quick Start for Contributors

```bash
# 1. Clone the repo
git clone https://github.com/YOUR_USERNAME/nc-lang.git
cd nc-lang

# 2. Build
cd engine && make
cd ..

# 3. Run tests
cd engine && make test-unit && cd ..
./engine/build/nc test                # 85 language tests (947 behaviors)
python3 tests/validate_nc.py          # 67 external validation tests
python3 tests/validate_nc_deep.py     # 45 deep integration tests (HTTP, build)
./engine/build/nc conformance         # language conformance

# 4. Make your changes

# 5. Rebuild and test
cd engine && make clean && make && make test-unit && cd ..
./engine/build/nc test
```

## Project Structure

```
engine/
├── include/nc.h      ← All type definitions and function declarations
├── src/              ← Implementation files (one per component)
└── tests/test_nc.c   ← Unit test suite
```

### Key Files to Know

| File | What It Does | Good First Contribution |
|------|-------------|------------------------|
| `nc_lexer.c` | Tokenizer | Add new keyword support |
| `nc_parser.c` | Parser | Add new statement types |
| `nc_compiler.c` | Bytecode compiler | Compile new statements to opcodes |
| `nc_vm.c` | Virtual machine | Add new opcodes |
| `nc_interp.c` | Tree-walker | Fix execution bugs |
| `nc_stdlib.c` | Standard library | Add built-in functions |
| `nc_json.c` | JSON parser | Fix edge cases |
| `nc_polyglot.c` | Language digestion | Add new language support |
| `tests/test_nc.c` | Tests | Add more tests! |

## What We Need Help With

### Easy (Good First Issue)
- Add more unit tests to `tests/test_nc.c`
- Fix warnings in compilation (`make` shows a few)
- Add more examples in `examples/`
- Improve error messages in `nc_parser.c`
- Add more built-in functions to `nc_stdlib.c`

### Medium
- Improve the cross-language digester (Python/JS → NC)
- Better template `{{variable}}` resolution in the VM
- Add `from "module" import behavior` to the module system
- Improve the REPL with history and tab completion
- Add more conformance tests to `nc_enterprise.c`

### Hard
- Wire libcurl for real AI API calls
- Full bytecode VM execution of all behavior patterns
- Implement proper loop iteration in the bytecode compiler
- LLVM IR generation for more statement types
- JIT compilation of hot paths

## Code Style

- C11 standard (`-std=c11`)
- 4-space indentation
- Function names: `nc_component_action` (e.g., `nc_lexer_tokenize`)
- Type names: `NcTypeName` (e.g., `NcValue`, `NcString`)
- Constants: `UPPER_CASE`
- Every source file has a header comment explaining its purpose
- No global mutable state where avoidable

## Testing

Every change should:
1. Not break existing tests: `make test-unit && ./build/nc test`
2. Add new tests for new functionality (C unit tests or NC language tests)
3. Pass with no new compiler warnings: `make`
4. Optionally run Python validation: `python3 tests/validate_nc.py`
5. For Windows: `powershell -File tests/run_tests.ps1`
6. For Docker Linux validation: `docker build -f Dockerfile.test -t nc-test . && docker run --rm nc-test`

### Adding a Test

In `tests/test_nc.c`:
```c
static void test_my_feature(void) {
    printf("  My Feature Tests:\n");

    NcValue result = nc_call_behavior(
        "to test:\n    respond with 42",
        "<test>", "test", nc_map_new()
    );
    ASSERT_EQ_INT(AS_INT(result), 42, "my feature works");
}
```

Then call it from `main()`:
```c
test_my_feature();
```

## Submitting Changes

1. Fork the repository
2. Create a branch: `git checkout -b my-feature`
3. Make your changes
4. Run tests: `make clean && make && make test-unit`
5. Commit: `git commit -m "Add feature X"`
6. Push: `git push origin my-feature`
7. Open a Pull Request

## Reporting Bugs

Open an issue with:
- The `.nc` file that causes the bug
- The command you ran
- What you expected
- What actually happened
- Your OS and compiler version (`cc --version`)

---

NC is created by **Nuckala Sai Narender**, Founder of **[DevHeal Labs AI](https://devheallabs.in)**.
For questions, reach out at support@devheallabs.in.
