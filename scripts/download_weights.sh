#!/usr/bin/env bash
# download_weights.sh — Download NC AI model weights from GitHub Releases
#
# Usage:
#   bash nc-lang/scripts/download_weights.sh
#   bash nc-lang/scripts/download_weights.sh --version v1.1.0
#   bash nc-lang/scripts/download_weights.sh --dir ~/.nc/models
#
# Environment variables:
#   NC_MODEL_PATH     — override destination for model .bin file
#   GITHUB_TOKEN      — optional, increases API rate limit to 5000 req/hr

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────
REPO="devheallabs/nc"
VERSION="latest"
DEST_DIR="${NC_MODEL_DIR:-$(pwd)}"
MODEL_FILE="nc_ai_model_prod.bin"
TOKEN_FILE="nc_ai_model_prod.nctok"
VERIFY=true

# ── Colours ───────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

info()  { echo -e "${CYAN}[NC]${RESET}  $*"; }
ok()    { echo -e "${GREEN}[OK]${RESET}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET} $*"; }
die()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; exit 1; }

# ── Parse arguments ───────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)  VERSION="$2"; shift 2 ;;
        --dir)      DEST_DIR="$2"; shift 2 ;;
        --no-verify) VERIFY=false; shift ;;
        -h|--help)
            echo "Usage: $0 [--version v1.1.0] [--dir /path/to/dir] [--no-verify]"
            exit 0
            ;;
        *) die "Unknown option: $1" ;;
    esac
done

# ── Check dependencies ────────────────────────────────────────────
for dep in curl jq; do
    if ! command -v "$dep" &>/dev/null; then
        die "$dep is required but not installed. Install it and retry."
    fi
done

# ── Resolve release URL ───────────────────────────────────────────
GITHUB_API="https://api.github.com/repos/$REPO"
AUTH_HEADER=""
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    AUTH_HEADER="Authorization: Bearer $GITHUB_TOKEN"
fi

info "Fetching release info from GitHub ($VERSION)..."

if [[ "$VERSION" == "latest" ]]; then
    RELEASE_URL="$GITHUB_API/releases/latest"
else
    RELEASE_URL="$GITHUB_API/releases/tags/$VERSION"
fi

if [[ -n "$AUTH_HEADER" ]]; then
    RELEASE_JSON=$(curl -fsSL -H "$AUTH_HEADER" "$RELEASE_URL") \
        || die "Failed to fetch release info. Check your network or GITHUB_TOKEN."
else
    RELEASE_JSON=$(curl -fsSL "$RELEASE_URL") \
        || die "Failed to fetch release info. Rate limit? Set GITHUB_TOKEN to increase limit."
fi

RELEASE_TAG=$(echo "$RELEASE_JSON" | jq -r '.tag_name')
if [[ -z "$RELEASE_TAG" || "$RELEASE_TAG" == "null" ]]; then
    die "Release '$VERSION' not found. Check available releases at: https://github.com/$REPO/releases"
fi

info "Found release: $RELEASE_TAG"

# ── Find asset URLs ───────────────────────────────────────────────
get_asset_url() {
    local name="$1"
    echo "$RELEASE_JSON" | jq -r ".assets[] | select(.name == \"$name\") | .browser_download_url"
}

MODEL_URL=$(get_asset_url "$MODEL_FILE")
TOKEN_URL=$(get_asset_url "$TOKEN_FILE")
MODEL_SHA_URL=$(get_asset_url "${MODEL_FILE}.sha256")
TOKEN_SHA_URL=$(get_asset_url "${TOKEN_FILE}.sha256")

if [[ -z "$MODEL_URL" || "$MODEL_URL" == "null" ]]; then
    die "Model weights not found in release $RELEASE_TAG. Assets may not be attached yet."
fi
if [[ -z "$TOKEN_URL" || "$TOKEN_URL" == "null" ]]; then
    die "Tokenizer file not found in release $RELEASE_TAG."
fi

# ── Create destination directory ──────────────────────────────────
mkdir -p "$DEST_DIR"

MODEL_DEST="$DEST_DIR/$MODEL_FILE"
TOKEN_DEST="$DEST_DIR/$TOKEN_FILE"

# ── Download model weights ────────────────────────────────────────
if [[ -f "$MODEL_DEST" ]]; then
    warn "Model file already exists: $MODEL_DEST"
    warn "Delete it first or use a different --dir to re-download."
else
    info "Downloading model weights (~20 MB)..."
    curl -fL --progress-bar -o "$MODEL_DEST" "$MODEL_URL" \
        || die "Download failed: $MODEL_URL"
    ok "Downloaded: $MODEL_DEST"
fi

# ── Download tokenizer ────────────────────────────────────────────
if [[ -f "$TOKEN_DEST" ]]; then
    warn "Tokenizer already exists: $TOKEN_DEST"
else
    info "Downloading tokenizer..."
    curl -fL --progress-bar -o "$TOKEN_DEST" "$TOKEN_URL" \
        || die "Download failed: $TOKEN_URL"
    ok "Downloaded: $TOKEN_DEST"
fi

# ── Verify checksums ──────────────────────────────────────────────
if [[ "$VERIFY" == "true" ]]; then
    info "Verifying checksums..."

    verify_checksum() {
        local file="$1"
        local sha_url="$2"
        local label="$3"

        if [[ -z "$sha_url" || "$sha_url" == "null" ]]; then
            warn "No checksum file found for $label — skipping verification"
            return 0
        fi

        local expected_sha
        expected_sha=$(curl -fsSL "$sha_url" | awk '{print $1}')

        if command -v sha256sum &>/dev/null; then
            local actual_sha
            actual_sha=$(sha256sum "$file" | awk '{print $1}')
        elif command -v shasum &>/dev/null; then
            local actual_sha
            actual_sha=$(shasum -a 256 "$file" | awk '{print $1}')
        else
            warn "No sha256sum or shasum found — skipping checksum verification"
            return 0
        fi

        if [[ "$actual_sha" == "$expected_sha" ]]; then
            ok "Checksum verified: $label"
        else
            die "Checksum MISMATCH for $label!\n  Expected: $expected_sha\n  Got:      $actual_sha\nDelete the file and retry."
        fi
    }

    verify_checksum "$MODEL_DEST" "$MODEL_SHA_URL" "$MODEL_FILE"
    verify_checksum "$TOKEN_DEST" "$TOKEN_SHA_URL" "$TOKEN_FILE"
fi

# ── Done ──────────────────────────────────────────────────────────
echo ""
ok "NC AI model weights ready ($RELEASE_TAG)"
echo ""
echo "  Model:     $MODEL_DEST"
echo "  Tokenizer: $TOKEN_DEST"
echo ""
echo "  Run:  nc ai status    (verify model is loaded)"
echo "  Run:  nc ai generate  (start generating)"
echo ""
echo "  To use a custom path:"
echo "  export NC_MODEL_PATH=$MODEL_DEST"
echo ""
