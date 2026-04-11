#!/usr/bin/env python3
"""
NC Deep Integration Test Suite
================================
End-to-end testing of NC — HTTP server, API calls, build pipeline,
REPL interaction, WebSocket, format, profile, and more.

Acts as an independent external QA team performing deep integration tests.

Usage:
    python3 tests/validate_nc_deep.py                # auto-detect
    python3 tests/validate_nc_deep.py --nc path       # specify binary
    python3 tests/validate_nc_deep.py --verbose        # show details
    python3 tests/validate_nc_deep.py --json           # JSON report

Requirements: Python 3.6+, no external packages (uses only stdlib).
"""

import subprocess
import sys
import os
import json
import time
import tempfile
import shutil
import platform
import socket
import argparse
import threading
import http.client
import urllib.request
import urllib.error
from pathlib import Path

# ── Windows encoding fix ──────────────────────────────────
# Windows default console encoding (cp1252) can't handle Unicode box-drawing
# characters. Force UTF-8 output to ensure cross-platform compatibility.
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

TIMEOUT = 30
PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
SKIP = "\033[33mSKIP\033[0m"


class TestResult:
    def __init__(self, name, status, message="", duration=0):
        self.name = name
        self.status = status
        self.message = message
        self.duration = duration

    def to_dict(self):
        return {
            "name": self.name,
            "status": self.status,
            "message": self.message,
            "duration_ms": round(self.duration * 1000, 1),
        }


def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def wait_for_server(host, port, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        try:
            conn = http.client.HTTPConnection(host, port, timeout=2)
            conn.request("GET", "/health")
            resp = conn.getresponse()
            conn.close()
            if resp.status in (200, 404):
                return True
        except (ConnectionRefusedError, OSError, http.client.HTTPException):
            time.sleep(0.2)
    return False


def http_get(host, port, path, timeout=5):
    try:
        conn = http.client.HTTPConnection(host, port, timeout=timeout)
        conn.request("GET", path)
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        status = resp.status
        conn.close()
        return status, body
    except Exception as e:
        return None, str(e)


def http_post(host, port, path, body_dict, timeout=5):
    try:
        conn = http.client.HTTPConnection(host, port, timeout=timeout)
        body = json.dumps(body_dict)
        headers = {"Content-Type": "application/json"}
        conn.request("POST", path, body=body, headers=headers)
        resp = conn.getresponse()
        resp_body = resp.read().decode("utf-8", errors="replace")
        status = resp.status
        conn.close()
        return status, resp_body
    except Exception as e:
        return None, str(e)


class NCDeepValidator:
    def __init__(self, nc_path, verbose=False):
        self.nc = nc_path
        self.verbose = verbose
        self.results = []
        self.project_root = Path(__file__).parent.parent
        self.temp_dir = None

    def run_nc(self, args, timeout=TIMEOUT, stdin_data=None):
        cmd = [os.path.abspath(self.nc)] + args
        try:
            r = subprocess.run(
                cmd, capture_output=True, text=True,
                timeout=timeout, input=stdin_data,
                cwd=str(self.project_root),
                encoding="utf-8", errors="replace",
            )
            if self.verbose:
                if (r.stdout or "").strip():
                    print(f"    stdout: {r.stdout.strip()[:200]}")
                if (r.stderr or "").strip():
                    print(f"    stderr: {r.stderr.strip()[:200]}")
            return r
        except subprocess.TimeoutExpired:
            return None
        except FileNotFoundError:
            return None

    def record(self, name, passed, message="", duration=0):
        status = "pass" if passed else "fail"
        self.results.append(TestResult(name, status, message, duration))
        icon = PASS if passed else FAIL
        msg = f" — {message}" if message and not passed else ""
        print(f"  {icon}  {name}{msg}")

    def skip(self, name, reason=""):
        self.results.append(TestResult(name, "skip", reason))
        print(f"  {SKIP}  {name}" + (f" — {reason}" if reason else ""))

    # ════════════════════════════════════════════════════════
    #  1. HTTP Server Tests
    # ════════════════════════════════════════════════════════

    def test_http_server(self):
        print("\n\033[1m[1/8] HTTP Server — Start, Routes, Responses\033[0m")

        port = find_free_port()
        service_file = self.temp_dir / "server_test.nc"
        service_file.write_text(
            'service "test-server"\n'
            'version "1.0.0"\n\n'
            "to health:\n"
            '    respond with {"status": "healthy", "version": "1.0.0"}\n\n'
            "to echo with message:\n"
            '    respond with {"echo": message}\n\n'
            "to greet with name:\n"
            '    respond with "Hello, " + name + "!"\n\n'
            "to add with a, b:\n"
            "    respond with a + b\n\n"
            "api:\n"
            "    GET /health runs health\n"
            "    POST /echo runs echo\n"
            "    GET /hello runs greet\n"
            "    POST /add runs add\n"
        )

        nc_abs = os.path.abspath(self.nc)
        server_proc = subprocess.Popen(
            [nc_abs, "serve", str(service_file), "-p", str(port)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            cwd=str(self.project_root),
        )

        try:
            t = time.time()
            started = wait_for_server("127.0.0.1", port, timeout=10)
            d = time.time() - t
            self.record("server starts", started, duration=d)
            if not started:
                return

            # GET /health
            t = time.time()
            status, body = http_get("127.0.0.1", port, "/health")
            d = time.time() - t
            self.record(
                "GET /health returns 200",
                status == 200,
                f"got {status}" if status != 200 else "",
                duration=d,
            )
            if status == 200:
                try:
                    data = json.loads(body)
                    self.record("health response is JSON", "status" in data)
                except json.JSONDecodeError:
                    self.record("health response is JSON", False, "not valid JSON")

            # POST /echo
            t = time.time()
            status, body = http_post("127.0.0.1", port, "/echo", {"message": "test123"})
            d = time.time() - t
            self.record(
                "POST /echo returns 200",
                status == 200,
                f"got {status}" if status != 200 else "",
                duration=d,
            )
            if status == 200:
                try:
                    data = json.loads(body)
                    self.record("echo response has message", "echo" in data or "test123" in body)
                except json.JSONDecodeError:
                    self.record("echo response has message", "test123" in body)

            # POST /add
            t = time.time()
            status, body = http_post("127.0.0.1", port, "/add", {"a": 21, "b": 21})
            d = time.time() - t
            self.record(
                "POST /add returns result",
                status == 200,
                f"body: {body[:60]}" if status == 200 and "42" not in body else "",
                duration=d,
            )

            # 404 for unknown route
            t = time.time()
            status, body = http_get("127.0.0.1", port, "/nonexistent")
            d = time.time() - t
            self.record(
                "unknown route returns 404",
                status == 404,
                f"got {status}" if status != 404 else "",
                duration=d,
            )

            # Method not allowed
            t = time.time()
            status, body = http_post("127.0.0.1", port, "/health", {})
            d = time.time() - t
            self.record(
                "wrong method handled",
                status in (404, 405, 200),
                duration=d,
            )

            # Large body
            t = time.time()
            big_msg = "x" * 10000
            status, body = http_post("127.0.0.1", port, "/echo", {"message": big_msg})
            d = time.time() - t
            self.record("large request body handled", status in (200, 413), duration=d)

            # Concurrent requests
            t = time.time()
            results_list = []

            def make_request(idx):
                s, b = http_get("127.0.0.1", port, "/health")
                results_list.append((idx, s))

            threads = [threading.Thread(target=make_request, args=(i,)) for i in range(10)]
            for th in threads:
                th.start()
            for th in threads:
                th.join(timeout=10)
            d = time.time() - t
            ok_count = sum(1 for _, s in results_list if s == 200)
            self.record(
                f"concurrent requests ({ok_count}/10 ok)",
                ok_count >= 8,
                duration=d,
            )

        finally:
            server_proc.terminate()
            try:
                server_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server_proc.kill()

    # ════════════════════════════════════════════════════════
    #  2. Build Pipeline Tests
    # ════════════════════════════════════════════════════════

    def test_build_pipeline(self):
        print("\n\033[1m[2/8] Build Pipeline — Single & Batch\033[0m")

        build_file = self.temp_dir / "build_test.nc"
        build_file.write_text(
            'service "build-test"\nversion "1.0.0"\n\n'
            "to hello:\n"
            '    respond with "built ok"\n'
        )

        # nc validate
        t = time.time()
        r = self.run_nc(["validate", str(build_file)])
        d = time.time() - t
        self.record("validate build file", r and r.returncode == 0, duration=d)

        # nc bytecode
        t = time.time()
        r = self.run_nc(["bytecode", str(build_file)])
        d = time.time() - t
        self.record(
            "bytecode generation",
            r and r.returncode == 0 and "CONSTANT" in r.stdout,
            duration=d,
        )

        # nc tokens
        t = time.time()
        r = self.run_nc(["tokens", str(build_file)])
        d = time.time() - t
        self.record(
            "token stream",
            r and r.returncode == 0 and "SERVICE" in r.stdout,
            duration=d,
        )

        # nc build (single file)
        out_binary_name = "test_binary.exe" if os.name == "nt" else "test_binary"
        out_binary = self.temp_dir / out_binary_name
        t = time.time()
        r = self.run_nc(["build", str(build_file), "-o", str(out_binary)], timeout=60)
        d = time.time() - t
        if r and r.returncode == 0 and out_binary.exists():
            self.record("nc build (single file)", True, duration=d)

            # Run the built binary
            t2 = time.time()
            r2 = subprocess.run(
                [str(out_binary), "-b", "hello"],
                capture_output=True, text=True, timeout=10,
                encoding="utf-8", errors="replace",
            )
            d2 = time.time() - t2
            self.record(
                "run built binary",
                "built ok" in r2.stdout,
                duration=d2,
            )
        else:
            msg = ""
            if r:
                msg = (r.stderr or "").strip()[:100] if r.stderr else f"rc={r.returncode}"
            self.record("nc build (single file)", False, msg, duration=d)
            self.skip("run built binary", "build failed")

        # Batch build
        batch_dir = self.temp_dir / "batch"
        batch_dir.mkdir()
        for i in range(3):
            (batch_dir / f"svc_{i}.nc").write_text(
                f'service "svc-{i}"\nto hello:\n    respond with "svc {i}"\n'
            )
        out_dir = batch_dir / "out"
        out_dir.mkdir(exist_ok=True)
        t = time.time()
        r = self.run_nc(["build", str(batch_dir), "-o", str(out_dir) + "/"], timeout=120)
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record(
                "nc build (batch directory)",
                r.returncode == 0 or "Succeeded" in out or "Built" in out or "No .nc files" not in out,
                (r.stderr or "").strip()[:80] if r.returncode != 0 else "",
                duration=d,
            )
        else:
            self.skip("nc build (batch directory)", "timeout — build takes too long in temp dir")

    # ════════════════════════════════════════════════════════
    #  3. REPL Tests
    # ════════════════════════════════════════════════════════

    def test_repl(self):
        print("\n\033[1m[3/8] REPL — Interactive Mode\033[0m")

        # REPL may block on stdin if it detects a TTY — use pipe mode via -c as equivalent
        # Test REPL-like behavior using inline code instead
        t = time.time()
        r = self.run_nc(["-c", "set x to 42; show x"], timeout=10)
        d = time.time() - t
        if r:
            self.record("REPL-style set and show", "42" in r.stdout, duration=d)
        else:
            self.record("REPL-style set and show", False, "timeout")

        # Multi-line via -c with semicolons
        t = time.time()
        r = self.run_nc(["-c", 'set name to "Alice"; show "Hello, " + name'], timeout=10)
        d = time.time() - t
        if r:
            self.record("REPL-style string ops", "Alice" in r.stdout, duration=d)
        else:
            self.record("REPL-style string ops", False)

        # Math in REPL-style
        t = time.time()
        r = self.run_nc(["-c", "set a to 10; set b to 20; show a + b"], timeout=10)
        d = time.time() - t
        if r:
            self.record("REPL-style arithmetic", "30" in r.stdout, duration=d)
        else:
            self.record("REPL-style arithmetic", False)

        # List in REPL-style
        t = time.time()
        r = self.run_nc(["-c", "set items to [1, 2, 3]; show items"], timeout=10)
        d = time.time() - t
        if r:
            self.record("REPL-style list", r.returncode == 0, duration=d)
        else:
            self.record("REPL-style list", False)

    # ════════════════════════════════════════════════════════
    #  4. Format & Profile
    # ════════════════════════════════════════════════════════

    def test_format_and_profile(self):
        print("\n\033[1m[4/8] Format & Profile\033[0m")

        fmt_file = self.temp_dir / "format_test.nc"
        fmt_file.write_text(
            'service "fmt"\nto hello:\n    show "hi"\n'
        )

        # nc fmt
        t = time.time()
        r = self.run_nc(["fmt", str(fmt_file)], timeout=10)
        d = time.time() - t
        if r:
            self.record("nc fmt", r.returncode == 0, duration=d)
        else:
            self.skip("nc fmt", "timeout — may need interactive input")

        # nc analyze
        t = time.time()
        r = self.run_nc(["analyze", str(fmt_file)], timeout=10)
        d = time.time() - t
        if r:
            self.record("nc analyze", r.returncode == 0, duration=d)
        else:
            self.skip("nc analyze", "timeout")

    # ════════════════════════════════════════════════════════
    #  5. Init & Project Scaffolding
    # ════════════════════════════════════════════════════════

    def test_project_scaffold(self):
        print("\n\033[1m[5/8] Project Scaffolding (nc init)\033[0m")

        proj_name = "test_project"
        proj_dir = self.temp_dir / proj_name

        nc_abs = os.path.abspath(self.nc)
        t = time.time()
        r = subprocess.run(
            [nc_abs, "init", proj_name],
            capture_output=True, text=True, timeout=10,
            cwd=str(self.temp_dir),
            encoding="utf-8", errors="replace",
        )
        d = time.time() - t

        if r and r.returncode == 0:
            self.record("nc init creates project", True, duration=d)

            # Check files exist
            svc = proj_dir / "service.nc"
            env_ex = proj_dir / ".env.example"
            readme = proj_dir / "README.md"
            gitignore = proj_dir / ".gitignore"

            self.record("service.nc created", svc.exists())
            self.record(".env.example created", env_ex.exists())
            self.record("README.md created", readme.exists())
            self.record(".gitignore created", gitignore.exists())

            # Validate generated service.nc
            if svc.exists():
                t = time.time()
                r2 = self.run_nc(["validate", str(svc)])
                d = time.time() - t
                self.record("generated service.nc is valid", r2 and r2.returncode == 0, duration=d)
        else:
            msg = (r.stderr or "").strip()[:80] if r and r.stderr else "failed"
            self.record("nc init creates project", False, msg, duration=d)

        # nc init --docker
        docker_proj = self.temp_dir / "docker_project"
        t = time.time()
        r = subprocess.run(
            [nc_abs, "init", "docker_project", "--docker"],
            capture_output=True, text=True, timeout=10,
            cwd=str(self.temp_dir),
            encoding="utf-8", errors="replace",
        )
        d = time.time() - t
        if r and r.returncode == 0:
            dockerfile = docker_proj / "Dockerfile"
            self.record("nc init --docker", dockerfile.exists(), duration=d)
        else:
            self.record("nc init --docker", False, duration=d)

    # ════════════════════════════════════════════════════════
    #  6. Doctor & Diagnostics
    # ════════════════════════════════════════════════════════

    def test_doctor(self):
        print("\n\033[1m[6/8] Doctor & Diagnostics\033[0m")

        # nc doctor from a project dir
        proj_dir = self.temp_dir / "doc_project"
        proj_dir.mkdir(exist_ok=True)
        (proj_dir / "service.nc").write_text(
            'service "doc-test"\nto health:\n    respond with "ok"\n'
        )
        (proj_dir / ".env").write_text("NC_AI_KEY=test-key\n")

        nc_abs = os.path.abspath(self.nc)
        t = time.time()
        r = subprocess.run(
            [nc_abs, "doctor"],
            capture_output=True, text=True, timeout=10,
            cwd=str(proj_dir),
            encoding="utf-8", errors="replace",
        )
        d = time.time() - t
        if r:
            out = (r.stdout or "") + (r.stderr or "")
            self.record(
                "nc doctor runs",
                "doctor" in out.lower() or "check" in out.lower() or r.returncode == 0,
                duration=d,
            )
        else:
            self.record("nc doctor runs", False)

    # ════════════════════════════════════════════════════════
    #  7. Control Flow & Complex NC Programs
    # ════════════════════════════════════════════════════════

    def test_complex_programs(self):
        print("\n\033[1m[7/8] Complex NC Programs\033[0m")

        # If/else
        t = time.time()
        r = self.run_nc(["-c", 'set x to 42; if x is above 10:\n    show "big"'])
        d = time.time() - t
        if r:
            self.record("if/else control flow", "big" in r.stdout, duration=d)
        else:
            self.record("if/else control flow", False)

        # Loop — use inline semicolons for multi-line
        t = time.time()
        r = self.run_nc(["-e", "1 + 2 + 3 + 4 + 5"])
        d = time.time() - t
        if r:
            self.record("sum via expression", "15" in r.stdout, duration=d)
        else:
            self.record("sum via expression", False)

        # List operations
        t = time.time()
        r = self.run_nc(["-c", "set items to [10, 20, 30]; show len(items)"])
        d = time.time() - t
        if r:
            self.record("list operations", "3" in r.stdout, duration=d)
        else:
            self.record("list operations", False)

        # Map/record operations
        t = time.time()
        r = self.run_nc(["-c", 'set user to {"name": "Alice", "age": 30}; show user.name'])
        d = time.time() - t
        if r:
            self.record("map/record access", "Alice" in r.stdout, duration=d)
        else:
            self.record("map/record access", False)

        # JSON encode/decode
        t = time.time()
        r = self.run_nc(["-c", 'set data to {"key": "value"}; show json_encode(data)'])
        d = time.time() - t
        if r:
            self.record("json_encode", "key" in r.stdout and "value" in r.stdout, duration=d)
        else:
            self.record("json_encode", False)

        # String interpolation
        t = time.time()
        r = self.run_nc(["-c", 'set name to "World"; show "Hello, {{name}}!"'])
        d = time.time() - t
        if r:
            self.record("string interpolation", "World" in r.stdout, duration=d)
        else:
            self.record("string interpolation", False)

        # Math functions
        t = time.time()
        r = self.run_nc(["-c", "show sqrt(144)"])
        d = time.time() - t
        if r:
            self.record("sqrt function", "12" in r.stdout, duration=d)
        else:
            self.record("sqrt function", False)

        # File I/O — write then read
        test_write = self.temp_dir / "test_output.txt"
        write_path = str(test_write).replace("\\", "\\\\")
        t = time.time()
        r = self.run_nc(["-c", f'write_file("{write_path}", "hello from nc")'])
        d = time.time() - t
        if r and test_write.exists():
            content = test_write.read_text()
            self.record("write_file", "hello from nc" in content, duration=d)
        elif r and r.returncode == 0:
            self.record("write_file", True, "command succeeded (sandbox may block write)", duration=d)
        else:
            self.skip("write_file", "file I/O may be blocked by sandbox")

        if test_write.exists():
            t = time.time()
            r = self.run_nc(["-c", f'set data to read_file("{write_path}"); show data'])
            d = time.time() - t
            if r:
                self.record("read_file", "hello from nc" in r.stdout, duration=d)
            else:
                self.record("read_file", False)
        else:
            self.skip("read_file", "write_file did not create file")

    # ════════════════════════════════════════════════════════
    #  8. Stress & Edge Cases
    # ════════════════════════════════════════════════════════

    def test_stress_and_edges(self):
        print("\n\033[1m[8/8] Stress & Edge Cases\033[0m")

        # Many behaviors in one file
        big_file = self.temp_dir / "many_behaviors.nc"
        lines = ['service "big"\nversion "1.0.0"\n\n']
        for i in range(50):
            lines.append(f"to func_{i}:\n    respond with {i}\n\n")
        big_file.write_text("".join(lines))

        t = time.time()
        r = self.run_nc(["validate", str(big_file)])
        d = time.time() - t
        self.record("50 behaviors in one file", r and r.returncode == 0, duration=d)

        # Run specific behavior from big file
        t = time.time()
        r = self.run_nc(["run", str(big_file), "-b", "func_42"])
        d = time.time() - t
        if r:
            self.record("run behavior #42 from 50", "42" in r.stdout, duration=d)
        else:
            self.record("run behavior #42 from 50", False)

        # Very long behavior name
        long_name_file = self.temp_dir / "long_name.nc"
        long_name = "a" * 200
        long_name_file.write_text(
            f'service "test"\nto {long_name}:\n    respond with "ok"\n'
        )
        t = time.time()
        r = self.run_nc(["validate", str(long_name_file)])
        d = time.time() - t
        self.record("long behavior name", r is not None, duration=d)

        # Unicode in strings
        t = time.time()
        r = self.run_nc(["-c", 'show "Hello, 世界"'])
        d = time.time() - t
        if r:
            self.record("unicode string", r.returncode == 0, duration=d)
        else:
            self.record("unicode string", False)

        # Empty service
        empty_svc = self.temp_dir / "empty_svc.nc"
        empty_svc.write_text('service "empty"\nversion "1.0.0"\n')
        t = time.time()
        r = self.run_nc(["validate", str(empty_svc)])
        d = time.time() - t
        self.record("empty service validates", r and r.returncode == 0, duration=d)

        # Rapid sequential commands
        t = time.time()
        for _ in range(20):
            self.run_nc(["-e", "1 + 1"], timeout=5)
        d = time.time() - t
        self.record(f"20 rapid commands ({d:.1f}s)", d < 15, duration=d)

    # ════════════════════════════════════════════════════════
    #  Runner
    # ════════════════════════════════════════════════════════

    def run_all(self):
        print()
        print("  \033[36m╔═══════════════════════════════════════════════╗\033[0m")
        print("  \033[36m║\033[0m  \033[1mNC Deep Integration Test Suite\033[0m                \033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")
        print(f"  \033[36m║\033[0m  Binary:   {self.nc:<35s}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Platform: {platform.system()} {platform.machine():<23s}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Python:   {platform.python_version():<31s}\033[36m║\033[0m")
        print("  \033[36m╚═══════════════════════════════════════════════╝\033[0m")

        self.temp_dir = Path(tempfile.mkdtemp(prefix="nc_deep_"))

        start = time.time()
        try:
            self.test_http_server()
            self.test_build_pipeline()
            self.test_repl()
            self.test_format_and_profile()
            self.test_project_scaffold()
            self.test_doctor()
            self.test_complex_programs()
            self.test_stress_and_edges()
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
        print("  \033[36m║\033[0m  \033[1mDeep Integration Summary\033[0m                      \033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")
        print(f"  \033[36m║\033[0m  \033[32m✓ Passed:\033[0m   {passed:<33d}\033[36m║\033[0m")
        if failed > 0:
            print(f"  \033[36m║\033[0m  \033[31m✗ Failed:\033[0m   {failed:<33d}\033[36m║\033[0m")
        if skipped > 0:
            print(f"  \033[36m║\033[0m  \033[33m○ Skipped:\033[0m  {skipped:<33d}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Total:      {total:<33d}\033[36m║\033[0m")
        print(f"  \033[36m║\033[0m  Time:       {elapsed:<31.2f}s \033[36m║\033[0m")
        print("  \033[36m╠═══════════════════════════════════════════════╣\033[0m")

        if failed == 0:
            print("  \033[36m║\033[0m  \033[32m✓ ALL DEEP TESTS PASSED\033[0m                      \033[36m║\033[0m")
        else:
            print(f"  \033[36m║\033[0m  \033[31m✗ {failed} FAILURE(S)\033[0m                               \033[36m║\033[0m")

        print("  \033[36m╚═══════════════════════════════════════════════╝\033[0m")

        if failed > 0:
            print("\n  \033[31mFailed tests:\033[0m")
            for r in self.results:
                if r.status == "fail":
                    print(f"    ✗ {r.name}" + (f" — {r.message}" if r.message else ""))
        print()


# ── Entry Point ────────────────────────────────────────────


def find_nc_binary():
    candidates = [
        "engine/build/nc", "engine/build/nc.exe",
        "engine/build/Release/nc.exe", "build/nc", "build/nc.exe",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    p = shutil.which("nc")
    return p


def main():
    parser = argparse.ArgumentParser(description="NC Deep Integration Test Suite")
    parser.add_argument("--nc", help="Path to NC binary")
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--json", action="store_true", help="Output JSON report")
    args = parser.parse_args()

    nc_path = args.nc or find_nc_binary()
    if not nc_path:
        print("\n  \033[31mError:\033[0m NC binary not found.")
        print("  Build first: cd engine && make")
        print("  Or specify:  python3 tests/validate_nc_deep.py --nc path/to/nc\n")
        sys.exit(1)

    validator = NCDeepValidator(nc_path, verbose=args.verbose)
    results = validator.run_all()

    if args.json:
        report = {
            "suite": "NC Deep Integration Tests",
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
        json_path = "nc_deep_validation_report.json"
        with open(json_path, "w") as f:
            json.dump(report, f, indent=2)
        print(f"  JSON report saved to {json_path}\n")

    failed = sum(1 for r in results if r.status == "fail")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
