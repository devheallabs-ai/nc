"""
NC CLI wrapper.

Downloads the NC binary for the current platform on first run,
caches it in ~/.nc/bin/, and forwards all arguments to the native binary.
"""

import os
import sys
import subprocess

from nc_lang.installer import ensure_binary


def main():
    """Entry point for the `nc` command installed via pip."""
    # Respect NC_ACCEPT_LICENSE environment variable
    # Users must set NC_ACCEPT_LICENSE=yes to accept the license terms
    if os.environ.get("NC_ACCEPT_LICENSE", "").lower() not in ("yes", "1", "true"):
        # Allow version and help commands without license acceptance
        if len(sys.argv) > 1 and sys.argv[1] in ("version", "--version", "-v"):
            pass
        elif len(sys.argv) > 1 and sys.argv[1] in ("help", "--help", "-h"):
            pass
        else:
            _check_license_prompt()

    try:
        binary_path = ensure_binary()
    except Exception as e:
        print(f"nc-lang: failed to set up NC binary: {e}", file=sys.stderr)
        sys.exit(1)

    # Forward all arguments to the NC binary
    args = [binary_path] + sys.argv[1:]

    try:
        result = subprocess.run(args)
        sys.exit(result.returncode)
    except KeyboardInterrupt:
        sys.exit(130)
    except FileNotFoundError:
        print(
            f"nc-lang: binary not found at {binary_path}. "
            "Try reinstalling: pip install --force-reinstall nc-lang",
            file=sys.stderr,
        )
        sys.exit(1)


def _check_license_prompt():
    """Prompt the user to accept the license on first interactive run."""
    marker = os.path.join(os.path.expanduser("~"), ".nc", ".license_accepted")

    if os.path.exists(marker):
        return

    print("=" * 60)
    print("NC Language — Apache 2.0 License")
    print("Copyright (c) DevHeal Labs AI")
    print()
    print("By using NC, you agree to the Apache 2.0 License terms.")
    print("Full license: https://nc.devheallabs.in/license")
    print("=" * 60)

    if not sys.stdin.isatty():
        print(
            "\nnc-lang: set NC_ACCEPT_LICENSE=yes to accept non-interactively.",
            file=sys.stderr,
        )
        sys.exit(1)

    try:
        answer = input("\nDo you accept the license? [y/N] ").strip().lower()
    except EOFError:
        sys.exit(1)

    if answer in ("y", "yes"):
        os.makedirs(os.path.dirname(marker), exist_ok=True)
        with open(marker, "w") as f:
            f.write("accepted\n")
    else:
        print("License not accepted. Exiting.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
