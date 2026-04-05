"""
NC CLI wrapper — makes `nc` available after `pip install nc-lang`.

On first run, downloads the pre-built NC binary for the current platform
and caches it in the package directory. Subsequent runs use the cached binary.
"""

import os
import sys
import stat
import platform
import subprocess
import urllib.request
import shutil

NC_VERSION = "1.0.0"
REPO = "nc-lang/nc"


def _get_binary_name():
    system = platform.system().lower()
    machine = platform.machine().lower()

    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "arm64"
    else:
        arch = machine

    if system == "windows":
        return f"nc-windows-{arch}.exe"
    elif system == "darwin":
        return f"nc-macos-{arch}"
    else:
        return f"nc-linux-{arch}"


def _get_cache_dir():
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    cache = os.path.join(pkg_dir, ".bin")
    os.makedirs(cache, exist_ok=True)
    return cache


def _get_nc_path():
    cache = _get_cache_dir()
    ext = ".exe" if platform.system().lower() == "windows" else ""
    return os.path.join(cache, f"nc{ext}")


def _download_binary():
    nc_path = _get_nc_path()
    if os.path.exists(nc_path):
        return nc_path

    binary_name = _get_binary_name()
    url = f"https://github.com/{REPO}/releases/download/v{NC_VERSION}/{binary_name}"

    print(f"  Downloading NC v{NC_VERSION} for {platform.system()}...", file=sys.stderr)
    try:
        urllib.request.urlretrieve(url, nc_path)
        if platform.system().lower() != "windows":
            st = os.stat(nc_path)
            os.chmod(nc_path, st.st_mode | stat.S_IEXEC)
        print(f"  NC binary cached at {nc_path}", file=sys.stderr)
        return nc_path
    except Exception as e:
        if os.path.exists(nc_path):
            os.remove(nc_path)
        print(f"  Failed to download NC: {e}", file=sys.stderr)
        print(f"  URL: {url}", file=sys.stderr)
        print(f"  You can install NC manually: https://github.com/{REPO}", file=sys.stderr)
        return None


def main():
    nc = shutil.which("nc")
    if not nc:
        nc = _get_nc_path()
        if not os.path.exists(nc):
            nc = _download_binary()
            if not nc:
                sys.exit(1)

    try:
        result = subprocess.run([nc] + sys.argv[1:])
        sys.exit(result.returncode)
    except FileNotFoundError:
        nc = _download_binary()
        if nc:
            result = subprocess.run([nc] + sys.argv[1:])
            sys.exit(result.returncode)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
