#!/usr/bin/env bash
#
# deploy-websites.sh
# Deploys all 5 DevHeal Labs / NC websites to GitHub Pages.
#
# Usage:
#   ./scripts/deploy-websites.sh <github_org> [nc_org]
#   ./scripts/deploy-websites.sh --dry-run <github_org> [nc_org]
#
# Examples:
#   ./scripts/deploy-websites.sh DevHealLabs
#   ./scripts/deploy-websites.sh DevHealLabs nc-lang
#   ./scripts/deploy-websites.sh --dry-run DevHealLabs nc-lang
#

set -euo pipefail

# ---------------------------------------------------------------------------
# Colors
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
info()    { printf "${BLUE}[INFO]${NC}  %s\n" "$*"; }
success() { printf "${GREEN}[OK]${NC}    %s\n" "$*"; }
warn()    { printf "${YELLOW}[WARN]${NC}  %s\n" "$*"; }
error()   { printf "${RED}[ERROR]${NC} %s\n" "$*" >&2; }
header()  { printf "\n${BOLD}${CYAN}=== %s ===${NC}\n" "$*"; }

die() {
    error "$@"
    exit 1
}

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
DRY_RUN=false

if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
    shift
fi

GITHUB_ORG="${1:-}"
NC_ORG="${2:-$GITHUB_ORG}"

if [[ -z "$GITHUB_ORG" ]]; then
    echo "Usage: $0 [--dry-run] <github_org> [nc_org]"
    echo ""
    echo "  github_org   GitHub username or organization (e.g. DevHealLabs)"
    echo "  nc_org       GitHub org for the NC language repo (defaults to github_org)"
    echo "  --dry-run    Show what would happen without making changes"
    exit 1
fi

# ---------------------------------------------------------------------------
# Resolve repo root (so the script works from any cwd)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---------------------------------------------------------------------------
# Website definitions
# Each entry: local_dir | remote_repo | description
# ---------------------------------------------------------------------------
WEBSITES=(
    "devheallabs-website|${GITHUB_ORG}/${GITHUB_ORG}.github.io|DevHeal Labs main site"
    "nc-lang-website|${NC_ORG}/${NC_ORG}.github.io|NC Language site"
    "hiveant-website|${GITHUB_ORG}/hiveant|HiveANT site"
    "swarmops-website|${GITHUB_ORG}/swarmops|SwarmOps site"
    "neuraledge-website|${GITHUB_ORG}/neuraledge|NeuralEdge site"
)

# ---------------------------------------------------------------------------
# Dry-run wrapper: runs a command, or prints it if --dry-run is set
# ---------------------------------------------------------------------------
run() {
    if $DRY_RUN; then
        printf "  ${YELLOW}[dry-run]${NC} %s\n" "$*"
    else
        "$@"
    fi
}

# ---------------------------------------------------------------------------
# Deploy a single website
# ---------------------------------------------------------------------------
deploy_website() {
    local local_dir="$1"
    local remote_repo="$2"
    local description="$3"

    header "Deploying: $description"
    info "Source:  $REPO_ROOT/$local_dir"
    info "Target:  git@github.com:${remote_repo}.git"

    local src_path="$REPO_ROOT/$local_dir"

    # --- Validate source directory -----------------------------------------
    if [[ ! -d "$src_path" ]]; then
        warn "Directory '$src_path' does not exist. Skipping."
        return 0
    fi

    if [[ ! -f "$src_path/index.html" ]]; then
        warn "No index.html found in '$src_path'. Skipping."
        return 0
    fi

    # --- Create temp directory ---------------------------------------------
    local tmp_dir
    tmp_dir="$(mktemp -d)"
    info "Temp directory: $tmp_dir"

    # Ensure cleanup on exit from this function
    trap "rm -rf '$tmp_dir'" RETURN

    # --- Copy files --------------------------------------------------------
    info "Copying index.html ..."
    run cp "$src_path/index.html" "$tmp_dir/index.html"

    if [[ -f "$src_path/CNAME" ]]; then
        info "Copying CNAME ..."
        run cp "$src_path/CNAME" "$tmp_dir/CNAME"
    else
        warn "No CNAME file found in '$src_path'. Deploying without custom domain."
    fi

    # Copy any additional assets if present (css, js, images, etc.)
    for item in "$src_path"/*; do
        local basename
        basename="$(basename "$item")"
        [[ "$basename" == "index.html" || "$basename" == "CNAME" ]] && continue
        info "Copying $basename ..."
        run cp -r "$item" "$tmp_dir/$basename"
    done

    # --- Initialize git and push -------------------------------------------
    info "Initializing git repository ..."
    run git -C "$tmp_dir" init -b main
    run git -C "$tmp_dir" add -A
    run git -C "$tmp_dir" commit -m "Deploy $description to GitHub Pages"
    run git -C "$tmp_dir" remote add origin "git@github.com:${remote_repo}.git"

    info "Pushing to main branch (force) ..."
    run git -C "$tmp_dir" push -u origin main --force

    success "$description deployed successfully!"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    header "Website Deployment Script"
    info "GitHub org:  $GITHUB_ORG"
    info "NC org:      $NC_ORG"
    if $DRY_RUN; then
        warn "DRY RUN MODE -- no changes will be made."
    fi

    local failed=0

    for entry in "${WEBSITES[@]}"; do
        IFS='|' read -r local_dir remote_repo description <<< "$entry"
        if ! deploy_website "$local_dir" "$remote_repo" "$description"; then
            error "Failed to deploy: $description"
            failed=$((failed + 1))
        fi
    done

    echo ""
    if [[ $failed -eq 0 ]]; then
        success "All deployments completed."
    else
        error "$failed deployment(s) failed."
        exit 1
    fi

    if $DRY_RUN; then
        echo ""
        warn "This was a dry run. Re-run without --dry-run to actually deploy."
    fi
}

main
