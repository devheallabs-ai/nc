"""
NC Runtime — Python wrapper around the NC binary or libncrt shared library.

Supports two modes:
  1. Subprocess mode (default) — calls the `nc` binary
  2. FFI mode — loads libncrt.so/dylib via ctypes (faster, no process overhead)
"""

import os
import subprocess
import shutil
import tempfile
from dataclasses import dataclass
from typing import Optional


@dataclass
class NCResult:
    """Result of evaluating NC code."""
    ok: bool
    output: str
    error: str
    exit_code: int


class NC:
    """NC runtime instance for evaluating NC code from Python."""

    def __init__(self, binary_path: Optional[str] = None,
                 providers_path: Optional[str] = None):
        self._binary = binary_path or self._find_nc_binary()
        self._env = dict(os.environ)
        if providers_path:
            self._env["NC_AI_CONFIG_FILE"] = providers_path

    @staticmethod
    def _find_nc_binary() -> str:
        found = shutil.which("nc")
        if found:
            return found
        candidates = [
            os.path.join(os.path.dirname(__file__), "bin", "nc"),
            os.path.expanduser("~/.local/bin/nc"),
            "/usr/local/bin/nc",
            "/opt/homebrew/bin/nc",
        ]
        if os.name == "nt":
            candidates += [
                os.path.join(os.environ.get("LOCALAPPDATA", ""), "nc", "nc.exe"),
                os.path.join(os.environ.get("PROGRAMFILES", ""), "nc", "nc.exe"),
            ]
        for c in candidates:
            if os.path.isfile(c) and os.access(c, os.X_OK):
                return c
        return "nc"

    def set_env(self, key: str, value: str):
        """Set an environment variable for NC execution."""
        self._env[key] = value
        return self

    def set_provider(self, adapter: str, key: Optional[str] = None,
                     model: Optional[str] = None):
        """Configure AI provider shorthand."""
        self._env["NC_AI_ADAPTER"] = adapter
        if key:
            self._env["NC_AI_KEY"] = key
        if model:
            self._env["NC_AI_MODEL"] = model
        return self

    @staticmethod
    def _escape_nc_string(value: str) -> str:
        """Escape a value for safe embedding in an NC string literal."""
        return (str(value)
                .replace('\\', '\\\\')
                .replace('"', '\\"')
                .replace('\n', '\\n')
                .replace('\r', '\\r')
                .replace('\t', '\\t'))

    def run(self, source: str, **variables) -> NCResult:
        """
        Evaluate NC source code with optional context variables.

        Variables are injected as `set <name> to "<value>"` at the top
        of the source before execution.
        """
        full_source = ""
        for name, value in variables.items():
            escaped = self._escape_nc_string(value)
            full_source += f'set {name} to "{escaped}"\n'
        full_source += source

        return self._exec_source(full_source)

    def run_file(self, filename: str) -> NCResult:
        """Evaluate an NC file."""
        try:
            proc = subprocess.run(
                [self._binary, "run", filename],
                capture_output=True, text=True, timeout=120,
                env=self._env
            )
            return NCResult(
                ok=proc.returncode == 0,
                output=proc.stdout.strip(),
                error=proc.stderr.strip(),
                exit_code=proc.returncode
            )
        except FileNotFoundError:
            return NCResult(
                ok=False, output="",
                error=f"NC binary not found at '{self._binary}'. Install NC or set binary_path.",
                exit_code=127
            )
        except subprocess.TimeoutExpired:
            return NCResult(
                ok=False, output="",
                error="NC execution timed out (120s)",
                exit_code=124
            )

    def call(self, source: str, behavior: str, **args) -> NCResult:
        """Call a specific behavior in the source with named arguments."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.nc', delete=False) as f:
            f.write(source)
            f.flush()
            try:
                proc = subprocess.run(
                    [self._binary, "run", f.name, "-b", behavior],
                    capture_output=True, text=True, timeout=120,
                    env=self._env
                )
                return NCResult(
                    ok=proc.returncode == 0,
                    output=proc.stdout.strip(),
                    error=proc.stderr.strip(),
                    exit_code=proc.returncode
                )
            finally:
                os.unlink(f.name)

    def validate(self, source: str) -> NCResult:
        """Validate NC source without executing."""
        return self._exec_cmd("validate", source)

    def _exec_source(self, source: str) -> NCResult:
        """Execute NC source via temp file (cross-platform)."""
        tmp_path = None
        try:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.nc',
                                             delete=False) as f:
                f.write(source)
                f.flush()
                tmp_path = f.name
            proc = subprocess.run(
                [self._binary, "run", tmp_path],
                capture_output=True, text=True,
                timeout=120, env=self._env
            )
            return NCResult(
                ok=proc.returncode == 0,
                output=proc.stdout.strip(),
                error=proc.stderr.strip(),
                exit_code=proc.returncode
            )
        except FileNotFoundError:
            return NCResult(
                ok=False, output="",
                error=f"NC binary not found at '{self._binary}'. Install NC or set binary_path.",
                exit_code=127
            )
        except subprocess.TimeoutExpired:
            return NCResult(
                ok=False, output="",
                error="NC execution timed out (120s)",
                exit_code=124
            )
        finally:
            if tmp_path:
                try:
                    os.unlink(tmp_path)
                except OSError:
                    pass

    def _exec_cmd(self, cmd: str, source: str) -> NCResult:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.nc', delete=False) as f:
            f.write(source)
            f.flush()
            try:
                proc = subprocess.run(
                    [self._binary, cmd, f.name],
                    capture_output=True, text=True, timeout=30,
                    env=self._env
                )
                return NCResult(
                    ok=proc.returncode == 0,
                    output=proc.stdout.strip(),
                    error=proc.stderr.strip(),
                    exit_code=proc.returncode
                )
            finally:
                os.unlink(f.name)
