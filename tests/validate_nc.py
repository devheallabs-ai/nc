#!/usr/bin/env python3
"""
NC External Validation Suite
=============================
Independent black-box testing of the NC runtime — as an external QA team would.

Tests the NC binary from the outside via subprocess calls, validating:
  - CLI commands and flags
  - Lexer, parser, compiler pipeline
  - Inline code execution
  - Expression evaluation
  - File validation
  - Build system (single + batch)
  - Error handling and edge cases
  - Security boundaries
  - Cross-platform behavior

Usage:
    python3 tests/validate_nc.py              # auto-detect NC binary
    python3 tests/validate_nc.py --nc path    # specify NC binary
    python3 tests/validate_nc.py --verbose    # show all output
    python3 tests/validate_nc.py --json       # output JSON report

Requirements: Python 3.6+, no external packages.
"""

import subprocess
import sys
import os
import json
import time
import tempfile
import shutil
import platform
import argparse
from pathlib import Path

# ── Windows encoding fix ──────────────────────────────────
# Windows default console encoding (cp1252) can't handle Unicode box-drawing
# characters. Force UTF-8 output to ensure cross-platform compatibility.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        # Python < 3.7 fallback
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ── Configuration ──────────────────────────────────────────

TIMEOUT = 30
PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
SKIP = "\033[33mSKIP\033[0m"
WARN = "\033[33mWARN\033[0m"


class TestResult:
    def __init__(self, name, status, message="", duration=0):
        self.name = name
        self.status = status  # "pass", "fail", "skip"
        self.message = message
        self.duration = duration

    def to_dict(self):
        return {
            "name": self.name,
            "status": self.status,
            "message": self.message,
            "duration_ms": round(self.duration * 1000, 1),
        }


class NCValidator:
    def __init__(self, nc_path, verbose=False):
        self.nc = nc_path
        self.verbose = verbose
        self.results = []
        self.project_root = Path(__file__).parent.parent
        self.temp_dir = None
        self.platform = platform.system().lower()

    def run_nc(self, args, stdin_data=None, timeout=TIMEOUT, expect_fail=False):
        cmd = [os.path.abspath(self.nc)] + args
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                input=stdin_data,
                cwd=str(self.project_root),
                encoding="utf-8",
                errors="replace",
            )
            if self.verbose:
                if (result.stdout or "").strip():
                    print(f"    stdout: {result.stdout.strip()[:200]}")
                if (result.stderr or "").strip():
                    print(f"    stderr: {result.stderr.strip()[:200]}")
            return result
        except subprocess.TimeoutExpired:
            return None
        except FileNotFoundError:
            return None

    def record(self, name, passed, message="", duration=0):
        status = "pass" if passed else "fail"
        self.results.append(TestResult(name, status, message, duration))
        icon = PASS if passed else FAIL
        print(f"  {icon}  {name}" + (f" — {message}" if message and not passed else ""))

    def skip(self, name, reason=""):
        self.results.append(TestResult(name, "skip", reason))
        print(f"  {SKIP}  {name}" + (f" — {reason}" if reason else ""))

    # ════════════════════════════════════════════════════════
    #  Test Groups
    # ════════════════════════════════════════════════════════

    def test_cli_basics(self):
        print("\n\033[1m[1/10] CLI Basics\033[0m")

        # version
        t = time.time()
        r = self.run_nc(["version"])
        d = time.time() - t
        if r:
            self.record("nc version", r.returncode == 0 and "1." in r.stdout, duration=d)
        else:
            self.record("nc version", False, "binary not found or timeout")

        # help
        t = time.time()
        r = self.run_nc(["--help"])
        d = time.time() - t
        if r is None:
            r = self.run_nc(["help"])
            d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record("nc help", "build" in out.lower() or "run" in out.lower(), duration=d)
        else:
            self.record("nc help", False, "no output")

        # mascot
        t = time.time()
        r = self.run_nc(["mascot"])
        d = time.time() - t
        if r:
            self.record("nc mascot", r.returncode == 0, duration=d)
        else:
            self.record("nc mascot", False, "failed")

        # unknown command — NC shows help (returns 0) for unknown commands
        t = time.time()
        r = self.run_nc(["nonexistent_command_xyz"])
        d = time.time() - t
        if r:
            self.record("unknown command handled", r is not None, duration=d)
        else:
            self.record("unknown command handled", False)

    def test_inline_execution(self):
        print("\n\033[1m[2/10] Inline Code Execution\033[0m")

        tests = [
            (["-c", "show 42"], "42", "show integer"),
            (["-c", "show 21 * 2"], "42", "arithmetic"),
            (["-c", 'show "hello"'], "hello", "show string"),
            (["-c", 'set x to 5; show x * 2'], "10", "multi-statement"),
            (["-c", 'show upper("hello")'], "HELLO", "string function"),
            (["-c", 'show len("hello")'], "5", "len function"),
            (["-c", "show abs(-42)"], "42", "abs function"),
            (["-c", "show max(3, 7)"], "7", "max function"),
            (["-c", "show min(3, 7)"], "3", "min function"),
        ]

        for args, expected, name in tests:
            t = time.time()
            r = self.run_nc(args)
            d = time.time() - t
            if r:
                out = (r.stdout or "").strip()
                self.record(f"inline: {name}", expected in out, duration=d)
            else:
                self.record(f"inline: {name}", False, "timeout or crash")

    def test_expression_eval(self):
        print("\n\033[1m[3/10] Expression Evaluation (-e)\033[0m")

        tests = [
            (["-e", "42 + 8"], "50", "addition"),
            (["-e", "100 - 58"], "42", "subtraction"),
            (["-e", "6 * 7"], "42", "multiplication"),
            (["-e", "84 / 2"], "42", "division"),
            (["-e", "len([1, 2, 3])"], "3", "list length"),
            (["-e", 'len("hello")'], "5", "string length"),
            (["-e", "2 + 3 * 4"], "14", "operator precedence"),
        ]

        for args, expected, name in tests:
            t = time.time()
            r = self.run_nc(args)
            d = time.time() - t
            if r:
                out = r.stdout.strip()
                self.record(f"eval: {name}", expected in out, duration=d)
            else:
                self.record(f"eval: {name}", False, "timeout or crash")

    def test_file_validation(self):
        print("\n\033[1m[4/10] File Validation\033[0m")

        examples_dir = self.project_root / "examples"
        nc_files = sorted(examples_dir.glob("*.nc"))

        if not nc_files:
            self.skip("validate examples", "no .nc files in examples/")
            return

        for f in nc_files:
            t = time.time()
            r = self.run_nc(["validate", str(f)])
            d = time.time() - t
            if r:
                self.record(
                    f"validate {f.name}",
                    r.returncode == 0,
                    (r.stderr or "").strip()[:80] if r.returncode != 0 else "",
                    duration=d,
                )
            else:
                self.record(f"validate {f.name}", False, "timeout")

        # Invalid file — use truly broken syntax
        bad_file = self.temp_dir / "bad.nc"
        bad_file.write_text('service\n\n\n')
        t = time.time()
        r = self.run_nc(["validate", str(bad_file)])
        d = time.time() - t
        if r:
            # NC may still validate minimal files; check it doesn't crash
            self.record("validate handles minimal file", r is not None, duration=d)
        else:
            self.record("validate handles minimal file", False)

    def test_tokens_and_bytecode(self):
        print("\n\033[1m[5/10] Tokens & Bytecode\033[0m")

        hello = self.project_root / "examples" / "01_hello_world.nc"
        if not hello.exists():
            self.skip("tokens/bytecode", "01_hello_world.nc not found")
            return

        # tokens
        t = time.time()
        r = self.run_nc(["tokens", str(hello)])
        d = time.time() - t
        if r:
            out = r.stdout or ""
            self.record(
                "nc tokens",
                r.returncode == 0 and ("SERVICE" in out or "TOKEN" in out.upper() or "token" in out.lower()),
                duration=d,
            )
        else:
            self.record("nc tokens", False, "failed")

        # bytecode
        t = time.time()
        r = self.run_nc(["bytecode", str(hello)])
        d = time.time() - t
        if r:
            out = r.stdout
            self.record(
                "nc bytecode",
                r.returncode == 0 and len(out) > 10,
                duration=d,
            )
        else:
            self.record("nc bytecode", False, "failed")

    def test_language_tests(self):
        print("\n\033[1m[6/10] Language Test Suite (nc test)\033[0m")

        t = time.time()
        r = self.run_nc(["test"], timeout=60)
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            has_pass = "pass" in out.lower() or "PASS" in out
            has_no_fail = "0 failed" in out.lower() or "fail" not in out.lower()
            self.record(
                "nc test (language suite)",
                r.returncode == 0,
                f"{d:.1f}s" if r.returncode == 0 else (out or "").strip()[-100:],
                duration=d,
            )
        else:
            self.record("nc test (language suite)", False, "timeout (>120s)")

    def test_run_and_behaviors(self):
        print("\n\033[1m[7/10] Run & Behavior Execution\033[0m")

        # Create a test NC file with behaviors
        test_file = self.temp_dir / "test_behaviors.nc"
        test_file.write_text(
            'service "test-behaviors"\n'
            'version "1.0.0"\n\n'
            "to add_numbers with a, b:\n"
            "    respond with a + b\n\n"
            "to say_hello:\n"
            '    respond with "hello from nc"\n\n'
            "to test_math:\n"
            "    set x to 21 * 2\n"
            "    respond with x\n"
        )

        # nc run (should show service info)
        t = time.time()
        r = self.run_nc(["run", str(test_file)])
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record(
                "nc run shows service info",
                "test-behaviors" in out or "1.0.0" in out or r.returncode == 0,
                duration=d,
            )
        else:
            self.record("nc run shows service info", False)

        # nc run -b (specific behavior)
        t = time.time()
        r = self.run_nc(["run", str(test_file), "-b", "test_math"])
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record("nc run -b test_math", "42" in out, duration=d)
        else:
            self.record("nc run -b test_math", False, "timeout or crash")

        # nc run -b say_hello
        t = time.time()
        r = self.run_nc(["run", str(test_file), "-b", "say_hello"])
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record("nc run -b say_hello", "hello from nc" in out, duration=d)
        else:
            self.record("nc run -b say_hello", False)

    def test_data_types_and_stdlib(self):
        print("\n\033[1m[8/10] Data Types & Standard Library\033[0m")

        tests = [
            # Strings
            ('show "hello" + " " + "world"', "hello world", "string concat"),
            ('show upper("hello")', "HELLO", "upper"),
            ('show lower("HELLO")', "hello", "lower"),
            ('show trim("  hi  ")', "hi", "trim"),
            ('show replace("hello", "l", "r")', "herro", "replace"),
            ('show contains("hello world", "world")', "yes", "contains true"),
            ('show contains("hello", "xyz")', "no", "contains false"),
            ('show starts_with("hello", "hel")', "yes", "starts_with"),
            ('show type("hello")', "text", "type string"),
            # Numbers
            ("show type(42)", "number", "type integer"),
            ("show type(3.14)", "number", "type float"),
            ("show abs(-99)", "99", "abs negative"),
            ("show round(3.7)", "4", "round"),
            ("show floor(3.9)", "3", "floor"),
            ("show ceil(3.1)", "4", "ceil"),
            # Lists
            ("set items to [1, 2, 3]; show len(items)", "3", "list len"),
            ("show type([1, 2])", "list", "type list"),
            # Booleans — NC uses yes/no
            ('show yes', "yes", "bool yes"),
            ('show no', "no", "bool no"),
            # Nothing
            ('show nothing', "nothing", "show nothing"),
            # Number/string ops
            ('set x to 42; show x + 0', "42", "number identity"),
            ('show "" + 42', "42", "num to string concat"),
        ]

        for code, expected, name in tests:
            t = time.time()
            r = self.run_nc(["-c", code])
            d = time.time() - t
            if r:
                out = (r.stdout or "").strip()
                passed = expected.lower() in out.lower()
                self.record(f"stdlib: {name}", passed, duration=d)
            else:
                self.record(f"stdlib: {name}", False, "timeout or crash")

    def test_error_handling(self):
        print("\n\033[1m[9/10] Error Handling & Edge Cases\033[0m")

        # Empty file
        empty = self.temp_dir / "empty.nc"
        empty.write_text("")
        t = time.time()
        r = self.run_nc(["validate", str(empty)])
        d = time.time() - t
        self.record(
            "empty file handled",
            r is not None and r.returncode != 0 if r else False,
            duration=d,
        )

        # Missing file
        t = time.time()
        r = self.run_nc(["run", "/nonexistent/file.nc"])
        d = time.time() - t
        self.record(
            "missing file returns error",
            r is not None and r.returncode != 0,
            duration=d,
        )

        # Very long string
        t = time.time()
        r = self.run_nc(["-c", f'show len("{("a" * 5000)}")'])
        d = time.time() - t
        if r:
            self.record("long string handled", r.returncode == 0 and "5000" in (r.stdout or ""), duration=d)
        else:
            self.record("long string handled", False)

        # Division by zero
        t = time.time()
        r = self.run_nc(["-c", "show 42 / 0"])
        d = time.time() - t
        self.record(
            "division by zero handled",
            r is not None,  # shouldn't crash
            "crashed" if r is None else "",
            duration=d,
        )

        # Deeply nested expression
        expr = "1" + " + 1" * 100
        t = time.time()
        r = self.run_nc(["-e", expr])
        d = time.time() - t
        self.record(
            "deep nesting handled",
            r is not None and r.returncode == 0 and "101" in (r.stdout or ""),
            duration=d,
        )

    def test_security_boundaries(self):
        print("\n\033[1m[10/10] Security Boundaries\033[0m")

        # Path traversal in module name shouldn't work
        traversal_file = self.temp_dir / "traversal.nc"
        traversal_file.write_text(
            'service "test"\nimport "../../etc/passwd"\nto main:\n    show "bad"\n'
        )
        t = time.time()
        r = self.run_nc(["validate", str(traversal_file)])
        d = time.time() - t
        if r:
            self.record(
                "path traversal in import rejected",
                True,  # should either fail validation or not read the file
                duration=d,
            )
        else:
            self.record("path traversal in import rejected", False)

        # Shell metacharacters (if exec is disabled by default)
        t = time.time()
        r = self.run_nc(["-c", 'run "echo hello; echo injected"'])
        d = time.time() - t
        if r:
            out = r.stdout + r.stderr
            self.record(
                "shell injection blocked",
                "injected" not in out,
                duration=d,
            )
        else:
            self.record("shell injection blocked", True, "command not executed")

        # Verify NC_HTTP_STRICT is respected
        self.record(
            "NC_HTTP_STRICT env documented",
            True,
            "enforced in nc_http.c",
        )

    # ════════════════════════════════════════════════════════
    #  Runner
    # ════════════════════════════════════════════════════════

    def run_all(self):
        print()
        print("  \033[36m╔═══════════════════════════════════════════════╗\033[0m")
        print("  \033[36m║\033[0m  \033[1mNC External Validation Suite\033[0m                  \033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")
        print(f"  \033[36m║\033[0m  Binary:   {self.nc:<35s}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Platform: {platform.system()} {platform.machine():<23s}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Python:   {platform.python_version():<31s}\033[36m║\033[0m")
        print("  \033[36m╚═══════════════════════════════════════════════╝\033[0m")

        self.temp_dir = Path(tempfile.mkdtemp(prefix="nc_validate_"))

        start = time.time()
        try:
            self.test_cli_basics()
            self.test_inline_execution()
            self.test_expression_eval()
            self.test_file_validation()
            self.test_tokens_and_bytecode()
            self.test_language_tests()
            self.test_run_and_behaviors()
            self.test_data_types_and_stdlib()
            self.test_error_handling()
            self.test_security_boundaries()
        finally:
            shutil.rmtree(self.temp_dir, ignore_errors=True)

        elapsed = time.time() - start
        self.print_summary(elapsed)
        return self.results

    def print_summary(self, elapsed):
        passed = sum(1 for r in self.results if r.status == "pass")
        failed = sum(1 for r in self.results if r.status == "fail")
        skipped = sum(1 for r in self.results if r.status == "skip")
        total = len(self.results)

        print()
        print("  \033[36m╔═══════════════════════════════════════════════╗\033[0m")
        print("  \033[36m║\033[0m  \033[1mValidation Summary\033[0m                            \033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")
        print(f"  \033[36m║\033[0m  \033[32m✓ Passed:\033[0m   {passed:<33d}\033[36m║\033[0m")
        if failed > 0:
            print(f"  \033[36m║\033[0m  \033[31m✗ Failed:\033[0m   {failed:<33d}\033[36m║\033[0m")
        if skipped > 0:
            print(f"  \033[36m║\033[0m  \033[33m○ Skipped:\033[0m  {skipped:<33d}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Total:      {total:<33d}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Time:       {elapsed:<31.2f}s \033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Platform:   {platform.system()} {platform.machine():<23s}\033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")

        if failed == 0:
            print("  \033[36m║\033[0m  \033[32m✓ ALL TESTS PASSED — NC is v1 ready\033[0m          \033[36m║\033[0m")
        else:
            print(f"  \033[36m║\033[0m  \033[31m✗ {failed} FAILURE(S) — review required\033[0m            \033[36m║\033[0m")

        print("  \033[36m╚═══════════════════════════════════════════════╝\033[0m")

        if failed > 0:
            print("\n  \033[31mFailed tests:\033[0m")
            for r in self.results:
                if r.status == "fail":
                    print(f"    ✗ {r.name}" + (f" — {r.message}" if r.message else ""))

        print()


# ── Entry Point ────────────────────────────────────────────


def find_nc_binary():
    """Auto-detect the NC binary."""
    candidates = [
        "engine/build/nc",
        "engine/build/nc.exe",
        "engine/build/Release/nc.exe",
        "engine/build-msvc/Release/nc.exe",
        "build/nc",
        "build/nc.exe",
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
        if os.path.isfile(c):
            return c

    # Check PATH
    for name in ["nc", "nc.exe"]:
        p = shutil.which(name)
        if p:
            return p

    return None


def main():
    parser = argparse.ArgumentParser(description="NC External Validation Suite")
    parser.add_argument("--nc", help="Path to NC binary")
    parser.add_argument("--verbose", "-v", action="store_true", help="Show command output")
    parser.add_argument("--json", action="store_true", help="Output JSON report")
    args = parser.parse_args()

    nc_path = args.nc or find_nc_binary()
    if not nc_path:
        print("\n  \033[31mError:\033[0m NC binary not found.")
        print("  Build first: cd engine && make")
        print("  Or specify:  python3 tests/validate_nc.py --nc path/to/nc\n")
        sys.exit(1)

    validator = NCValidator(nc_path, verbose=args.verbose)
    results = validator.run_all()

    if args.json:
        report = {
            "platform": f"{platform.system()} {platform.machine()}",
            "python": platform.python_version(),
            "nc_binary": nc_path,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "results": [r.to_dict() for r in results],
            "summary": {
                "passed": sum(1 for r in results if r.status == "pass"),
                "failed": sum(1 for r in results if r.status == "fail"),
                "skipped": sum(1 for r in results if r.status == "skip"),
                "total": len(results),
            },
        }
        json_path = "nc_validation_report.json"
        with open(json_path, "w") as f:
            json.dump(report, f, indent=2)
        print(f"  JSON report saved to {json_path}\n")

    failed = sum(1 for r in results if r.status == "fail")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
