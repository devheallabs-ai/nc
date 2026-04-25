"""
NC binary installer.

Downloads the NC binary from GitHub releases for the current platform,
verifies its checksum, and caches it in ~/.nc/bin/.
"""

import hashlib
import os
import platform
import stat
import sys
import tempfile

try:
    import requests
except ImportError:
    requests = None

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

GITHUB_REPO = "devheallabs-ai/nc"
VERSION = "1.3.0"
BASE_URL = f"https://github.com/{GITHUB_REPO}/releases/download/v{VERSION}"

# Map (system, machine) to the release asset name and optional SHA-256 digest.
# Checksums should be populated for each release.
PLATFORM_MAP = {
    ("Linux", "x86_64"): {
        "asset": "nc-linux-x86_64",
        "sha256": "",
    },
    ("Darwin", "x86_64"): {
        "asset": "nc-macos-x86_64",
        "sha256": "",
    },
    ("Darwin", "arm64"): {
        "asset": "nc-macos-arm64",
        "sha256": "",
    },
    ("Windows", "AMD64"): {
        "asset": "nc-windows-x86_64.exe",
        "sha256": "",
    },
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

NC_HOME = os.path.join(os.path.expanduser("~"), ".nc")
BIN_DIR = os.path.join(NC_HOME, "bin")


def _binary_name():
    """Return the expected binary filename for the current platform."""
    if platform.system() == "Windows":
        return "nc.exe"
    return "nc"


def _binary_path():
    """Return the full path where the cached binary should live."""
    return os.path.join(BIN_DIR, _binary_name())


def _resolve_platform():
    """Determine the platform key and asset info for the current system."""
    system = platform.system()
    machine = platform.machine()

    # Normalize machine names
    if machine in ("x86_64", "AMD64"):
        if system == "Windows":
            machine = "AMD64"
        else:
            machine = "x86_64"
    elif machine in ("arm64", "aarch64"):
        if system == "Darwin":
            machine = "arm64"
        else:
            machine = "aarch64"

    key = (system, machine)
    if key not in PLATFORM_MAP:
        raise RuntimeError(
            f"Unsupported platform: {system} {machine}. "
            "Prebuilt binaries are currently published for Linux x86_64, "
            "macOS x86_64/arm64, and Windows x86_64. "
            "Please build NC from source: https://nc.devheallabs.in/docs/install"
        )
    return PLATFORM_MAP[key]


def _download_with_progress(url, dest):
    """Download a URL to a file with a progress bar."""
    if requests is not None:
        _download_requests(url, dest)
    else:
        _download_urllib(url, dest)


def _download_requests(url, dest):
    """Download using the requests library (preferred — supports streaming)."""
    resp = requests.get(url, stream=True, timeout=120, allow_redirects=True)
    resp.raise_for_status()

    total = int(resp.headers.get("content-length", 0))
    downloaded = 0
    chunk_size = 1024 * 64  # 64 KiB

    with open(dest, "wb") as f:
        for chunk in resp.iter_content(chunk_size=chunk_size):
            f.write(chunk)
            downloaded += len(chunk)
            if total > 0:
                pct = downloaded * 100 // total
                bar_len = 30
                filled = bar_len * downloaded // total
                bar = "#" * filled + "-" * (bar_len - filled)
                print(
                    f"\r  [{bar}] {pct:3d}%  ({downloaded // 1024} KB)",
                    end="",
                    flush=True,
                )
    if total > 0:
        print()  # newline after progress bar


def _download_urllib(url, dest):
    """Fallback download using urllib (no external dependency)."""
    import urllib.request

    print("  Downloading (urllib fallback)...", flush=True)
    urllib.request.urlretrieve(url, dest)
    print("  Download complete.")


def _verify_checksum(path, expected_sha256):
    """Verify the SHA-256 checksum of a downloaded file."""
    if not expected_sha256:
        return  # No checksum provided for this release yet

    sha = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            block = f.read(1024 * 64)
            if not block:
                break
            sha.update(block)

    actual = sha.hexdigest()
    if actual != expected_sha256:
        os.unlink(path)
        raise RuntimeError(
            f"Checksum verification failed.\n"
            f"  Expected: {expected_sha256}\n"
            f"  Got:      {actual}\n"
            "The downloaded binary has been removed. Please try again."
        )


def _make_executable(path):
    """Set executable permission on Unix systems."""
    if platform.system() != "Windows":
        st = os.stat(path)
        os.chmod(path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


def ensure_binary():
    """
    Ensure the NC binary is available and return its path.

    Downloads the binary on first run, then reuses the cached copy.
    Set NC_BINARY to override the binary path entirely.
    """
    # Allow explicit override
    override = os.environ.get("NC_BINARY")
    if override:
        if not os.path.isfile(override):
            raise FileNotFoundError(f"NC_BINARY points to missing file: {override}")
        return override

    cached = _binary_path()

    # Return cached binary if it already exists
    if os.path.isfile(cached):
        return cached

    # Resolve platform and download
    info = _resolve_platform()
    asset = info["asset"]
    url = f"{BASE_URL}/{asset}"

    print(f"nc-lang: installing NC v{VERSION} for {platform.system()} {platform.machine()}...")
    print(f"  Source: {url}")

    os.makedirs(BIN_DIR, exist_ok=True)

    # Download to a temp file first, then move into place (atomic-ish)
    fd, tmp_path = tempfile.mkstemp(dir=BIN_DIR, prefix="nc-download-")
    os.close(fd)

    try:
        _download_with_progress(url, tmp_path)
        _verify_checksum(tmp_path, info["sha256"])
        _make_executable(tmp_path)

        # Atomic rename
        os.replace(tmp_path, cached)
    except Exception:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)
        raise

    print(f"  Installed: {cached}")
    return cached


def remove_binary():
    """Remove the cached NC binary."""
    cached = _binary_path()
    if os.path.isfile(cached):
        os.unlink(cached)
        print(f"Removed: {cached}")
    else:
        print("No cached binary found.")
