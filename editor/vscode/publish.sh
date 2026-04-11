#!/usr/bin/env bash
# ============================================================
# NC Language VS Code Extension — Build & Publish Script
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

EXT_NAME="nc-lang"
VERSION=$(node -p "require('./package.json').version")
VSIX_FILE="${EXT_NAME}-${VERSION}.vsix"

echo "=== NC Language VS Code Extension ==="
echo "Version: $VERSION"
echo ""

# ----------------------------------------------------------
# 1. Ensure vsce is installed
# ----------------------------------------------------------
if ! command -v vsce &>/dev/null; then
    echo "[*] Installing @vscode/vsce globally..."
    npm install -g @vscode/vsce
fi

# ----------------------------------------------------------
# 2. Pre-flight checks
# ----------------------------------------------------------
if [ ! -f "package.json" ]; then
    echo "[!] Error: package.json not found. Run this script from the extension directory."
    exit 1
fi

if [ ! -f "icon.png" ]; then
    echo "[!] Warning: icon.png not found. The extension will not have an icon on the marketplace."
    echo "    Add a 128x128 (or larger) PNG icon as icon.png."
fi

# ----------------------------------------------------------
# 3. Package as VSIX
# ----------------------------------------------------------
echo "[*] Packaging extension as VSIX..."
vsce package --out "$VSIX_FILE"
echo "[+] Created: $VSIX_FILE"
echo ""

# ----------------------------------------------------------
# 4. Next steps
# ----------------------------------------------------------
echo "=== Next Steps ==="
echo ""
echo "  Install locally for testing:"
echo "    code --install-extension $VSIX_FILE"
echo ""
echo "  Publish to VS Code Marketplace:"
echo "    1. Create a publisher at https://marketplace.visualstudio.com/manage"
echo "    2. Generate a Personal Access Token (PAT) from Azure DevOps"
echo "       Scopes: Marketplace > Manage"
echo "    3. Login:   vsce login devheal-labs-ai"
echo "    4. Publish:  vsce publish"
echo ""
echo "  Publish to Open VSX (for open-source editors):"
echo "    npx ovsx publish $VSIX_FILE -p <OPEN_VSX_TOKEN>"
echo ""
echo "Done."
