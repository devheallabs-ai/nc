#!/usr/bin/env bash
# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
#
#  Install from the web:
#    curl -sSL https://nc.devheallabs.in/install.sh | bash
#    wget -qO- https://nc.devheallabs.in/install.sh | bash
#
#  Install from source checkout:
#    git clone https://github.com/devheallabs-ai/nc.git && cd nc && bash install.sh
#
#  Environment variables:
#    NC_ACCEPT_LICENSE=1    Skip license prompt
#    NC_INSTALL_DIR=<path>  Custom binary install directory
#    NC_LIB_DIR=<path>      Custom standard library directory
# ----------------------------------------------------------------------
set -euo pipefail

NC_VERSION="1.3.0"
REPO="devheallabs-ai/nc"
INSTALL_DIR="${NC_INSTALL_DIR:-/usr/local/bin}"
LIB_DIR="${NC_LIB_DIR:-/usr/local/lib/nc}"

# ----------------------------------------------------------------------
if [ -t 1 ] && command -v tput >/dev/null 2>&1 && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    BOLD=$(tput bold)
    RED=$(tput setaf 1)
    GREEN=$(tput setaf 2)
    YELLOW=$(tput setaf 3)
    BLUE=$(tput setaf 4)
    CYAN=$(tput setaf 6)
    DIM=$(tput setaf 8 2>/dev/null || tput dim 2>/dev/null || echo "")
    RESET=$(tput sgr0)
else
    BOLD="" RED="" GREEN="" YELLOW="" BLUE="" CYAN="" DIM="" RESET=""
fi

info()    { echo "${CYAN}  [*]${RESET} $*"; }
success() { echo "${GREEN}  [+]${RESET} $*"; }
warn()    { echo "${YELLOW}  [!]${RESET} $*"; }
error()   { echo "${RED}  [x]${RESET} $*"; }
step()    { echo "${BOLD}${BLUE}  ==> ${RESET}${BOLD}$*${RESET}"; }

# ----------------------------------------------------------------------
detect_platform() {
    local uname_s
    uname_s="$(uname -s 2>/dev/null || echo Unknown)"
    ARCH="$(uname -m 2>/dev/null || echo unknown)"

    case "$ARCH" in
        x86_64|amd64)   ARCH="x86_64" ;;
        aarch64|arm64)  ARCH="arm64" ;;
        armv7l)         ARCH="armv7" ;;
        i686|i386)      ARCH="x86" ;;
    esac

    case "$uname_s" in
        Darwin*)
            PLATFORM="macos"
            DISTRO="macos"
            BINARY_EXT=""
            ;;
        Linux*)
            PLATFORM="linux"
            BINARY_EXT=""
            # Detect distro
            if [ -f /etc/os-release ]; then
                . /etc/os-release
                case "$ID" in
                    ubuntu|debian|pop|linuxmint|elementary|zorin|kali)
                        DISTRO="debian" ;;
                    fedora|rhel|centos|rocky|alma|ol)
                        DISTRO="fedora" ;;
                    arch|manjaro|endeavouros|garuda)
                        DISTRO="arch" ;;
                    opensuse*|sles)
                        DISTRO="suse" ;;
                    alpine)
                        DISTRO="alpine" ;;
                    void)
                        DISTRO="void" ;;
                    *)
                        DISTRO="unknown" ;;
                esac
            elif [ -f /etc/debian_version ]; then
                DISTRO="debian"
            elif [ -f /etc/redhat-release ]; then
                DISTRO="fedora"
            else
                DISTRO="unknown"
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            PLATFORM="windows"
            DISTRO="msys2"
            BINARY_EXT=".exe"
            # Override install dir for Windows
            if [ -z "${NC_INSTALL_DIR:-}" ]; then
                INSTALL_DIR="${USERPROFILE:-$HOME}/bin"
            fi
            ;;
        *)
            PLATFORM="unknown"
            DISTRO="unknown"
            BINARY_EXT=""
            ;;
    esac
}

# ----------------------------------------------------------------------
print_banner() {
    echo ""
    echo "${BOLD}${CYAN}  +--------------------------------------+${RESET}"
    echo "${BOLD}${CYAN}  |         NC Installer v${NC_VERSION}          |${RESET}"
    echo "${BOLD}${CYAN}  |   The Notation Language Compiler     |${RESET}"
    echo "${BOLD}${CYAN}  +--------------------------------------+${RESET}"
    echo ""

    info "Platform: ${BOLD}${PLATFORM}/${ARCH}${RESET} (${DISTRO})"
    echo ""
}

# ----------------------------------------------------------------------
require_license_acceptance() {
    if [ "${NC_ACCEPT_LICENSE:-}" = "1" ] || [ "${NC_ACCEPT_LICENSE:-}" = "yes" ] || [ "${NC_ACCEPT_LICENSE:-}" = "true" ]; then
        success "License accepted via NC_ACCEPT_LICENSE"
        return 0
    fi

    # GUI dialog on macOS
    if [ "$PLATFORM" = "macos" ] && command -v osascript >/dev/null 2>&1; then
        local msg="NC is licensed under the Apache License 2.0.\n\nBy installing NC, you agree to the license terms.\n\nReview: https://github.com/${REPO}/blob/main/LICENSE"
        if osascript -e "display dialog \"$msg\" buttons {\"Decline\", \"Accept\"} default button \"Accept\" with title \"NC License Agreement\"" >/dev/null 2>&1; then
            success "License accepted"
            return 0
        fi
        error "Installation cancelled. License not accepted."
        exit 1
    fi

    # GUI dialog on Linux
    if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
        if command -v zenity >/dev/null 2>&1; then
            if zenity --question --title="NC License Agreement" --width=520 \
                --ok-label="Accept" --cancel-label="Decline" \
                --text="NC is licensed under the Apache License 2.0.\n\nBy installing NC, you agree to the license terms.\n\nReview:\nhttps://github.com/${REPO}/blob/main/LICENSE" 2>/dev/null; then
                success "License accepted"
                return 0
            fi
            error "Installation cancelled."
            exit 1
        fi
    fi

    # Terminal fallback
    echo "  ${BOLD}License Agreement${RESET}"
    echo "  --------------------------------------"
    echo "  NC is licensed under the Apache License 2.0."
    echo "  By installing NC, you agree to the license terms."
    echo "  Review: ${DIM}https://github.com/${REPO}/blob/main/LICENSE${RESET}"
    echo ""

    if [ -r /dev/tty ]; then
        printf "  Type 'yes' to accept and continue: " > /dev/tty
        read -r reply < /dev/tty || true
        case "$reply" in
            yes|YES|y|Y)
                success "License accepted"
                return 0 ;;
            *)
                error "Installation cancelled."
                exit 1 ;;
        esac
    fi

    error "Non-interactive install detected. Set NC_ACCEPT_LICENSE=1 to accept."
    exit 1
}

# ----------------------------------------------------------------------
write_consent_marker() {
    local consent_dir=""
    if [ -n "${XDG_STATE_HOME:-}" ]; then
        consent_dir="${XDG_STATE_HOME}/nc"
    elif [ -n "${HOME:-}" ]; then
        consent_dir="${HOME}/.config/nc"
    else
        return 0
    fi
    mkdir -p "$consent_dir" 2>/dev/null || true
    printf "accepted=1\nlicense=Apache-2.0\nproduct=nc\nversion=%s\ndate=%s\n" \
        "$NC_VERSION" "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || echo unknown)" \
        > "$consent_dir/license.accepted" 2>/dev/null || true
}

# ----------------------------------------------------------------------
check_compiler() {
    for cc in cc gcc clang; do
        if command -v "$cc" >/dev/null 2>&1; then
            FOUND_CC="$cc"
            return 0
        fi
    done
    FOUND_CC=""
    return 1
}

check_make() {
    if command -v make >/dev/null 2>&1; then
        FOUND_MAKE="make"
        return 0
    fi
    if command -v gmake >/dev/null 2>&1; then
        FOUND_MAKE="gmake"
        return 0
    fi
    FOUND_MAKE=""
    return 1
}

check_curl_or_wget() {
    command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1
}

print_install_instructions() {
    echo ""
    error "Missing build tools. Install them first:"
    echo ""
    case "$DISTRO" in
        macos)
            echo "  ${BOLD}macOS:${RESET}"
            echo "    ${CYAN}xcode-select --install${RESET}"
            echo ""
            echo "  Or with Homebrew:"
            echo "    ${CYAN}brew install gcc make${RESET}"
            ;;
        debian)
            echo "  ${BOLD}Ubuntu / Debian:${RESET}"
            echo "    ${CYAN}sudo apt update && sudo apt install -y build-essential libcurl4-openssl-dev${RESET}"
            ;;
        fedora)
            echo "  ${BOLD}Fedora / RHEL / CentOS:${RESET}"
            echo "    ${CYAN}sudo dnf install -y gcc make libcurl-devel${RESET}"
            ;;
        arch)
            echo "  ${BOLD}Arch Linux:${RESET}"
            echo "    ${CYAN}sudo pacman -S base-devel curl${RESET}"
            ;;
        suse)
            echo "  ${BOLD}openSUSE:${RESET}"
            echo "    ${CYAN}sudo zypper install -y gcc make libcurl-devel${RESET}"
            ;;
        alpine)
            echo "  ${BOLD}Alpine Linux:${RESET}"
            echo "    ${CYAN}apk add build-base curl-dev${RESET}"
            ;;
        void)
            echo "  ${BOLD}Void Linux:${RESET}"
            echo "    ${CYAN}sudo xbps-install -S base-devel libcurl-devel${RESET}"
            ;;
        msys2)
            echo "  ${BOLD}Windows (MSYS2):${RESET}"
            echo "    ${CYAN}pacman -S mingw-w64-x86_64-gcc make${RESET}"
            echo ""
            echo "  ${BOLD}Windows (Git Bash, no MSYS2):${RESET}"
            echo "    Download w64devkit: ${CYAN}https://github.com/skeeto/w64devkit/releases${RESET}"
            echo "    Extract and add to PATH, then re-run this script."
            ;;
        *)
            echo "  Install a C compiler (gcc or clang) and make for your system."
            ;;
    esac
    echo ""
}

check_prerequisites() {
    step "Checking prerequisites"
    local missing=0

    if check_compiler; then
        success "C compiler: ${FOUND_CC} ($($FOUND_CC --version 2>&1 | head -1))"
    else
        error "C compiler: not found (need gcc, clang, or cc)"
        missing=1
    fi

    if check_make; then
        success "Build tool: ${FOUND_MAKE} ($($FOUND_MAKE --version 2>&1 | head -1))"
    else
        error "Build tool: make not found"
        missing=1
    fi

    if [ "$PLATFORM" != "windows" ]; then
        if pkg-config --exists libcurl 2>/dev/null || [ -f /usr/include/curl/curl.h ] || [ -f /usr/local/include/curl/curl.h ]; then
            success "libcurl: found"
        else
            warn "libcurl: not found (build may fail without it)"
        fi
    fi

    if [ "$missing" -eq 1 ]; then
        print_install_instructions
        exit 1
    fi
    echo ""
}

# ----------------------------------------------------------------------
# Compute sha256 of a file using whichever tool is available.
# Prints the hex digest to stdout; returns non-zero if no tool is found.
compute_sha256() {
    local f="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$f" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$f" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$f" | awk '{print $NF}'
    else
        return 1
    fi
}

# ----------------------------------------------------------------------
try_prebuilt() {
    local asset_name="nc-${PLATFORM}-${ARCH}${BINARY_EXT}"
    local url="https://github.com/${REPO}/releases/download/v${NC_VERSION}/${asset_name}"
    local sums_url="https://github.com/${REPO}/releases/download/v${NC_VERSION}/SHA256SUMS"

    step "Checking for pre-built binary"
    info "Looking for ${asset_name}..."

    local tmp_bin="/tmp/nc-installer-$$${BINARY_EXT}"
    local tmp_sums="/tmp/nc-installer-$$-SHA256SUMS"
    local http_code=""

    if command -v curl >/dev/null 2>&1; then
        http_code=$(curl -fsSL -o "$tmp_bin" -w "%{http_code}" "$url" 2>/dev/null) || true
    elif command -v wget >/dev/null 2>&1; then
        if wget -q -O "$tmp_bin" "$url" 2>/dev/null; then
            http_code="200"
        fi
    fi

    if [ "${http_code:-}" != "200" ] || [ ! -s "$tmp_bin" ]; then
        rm -f "$tmp_bin"
        warn "No pre-built binary available, will build from source"
        echo ""
        return 1
    fi

    # ----- SHA256 verification --------------------------------------------
    # Fetch the SHA256SUMS sibling file and verify the binary's hash matches
    # the expected value before we ever chmod +x or execute it. If the sums
    # file is missing (older releases), refuse to run the binary unless the
    # user explicitly opts out with NC_SKIP_CHECKSUM=1.
    local sums_code=""
    if command -v curl >/dev/null 2>&1; then
        sums_code=$(curl -fsSL -o "$tmp_sums" -w "%{http_code}" "$sums_url" 2>/dev/null) || true
    elif command -v wget >/dev/null 2>&1; then
        if wget -q -O "$tmp_sums" "$sums_url" 2>/dev/null; then
            sums_code="200"
        fi
    fi

    if [ "${sums_code:-}" = "200" ] && [ -s "$tmp_sums" ]; then
        local expected actual
        expected=$(grep -E "(^| )${asset_name}\$" "$tmp_sums" | awk '{print $1}' | head -n1)
        actual=$(compute_sha256 "$tmp_bin" 2>/dev/null || true)
        rm -f "$tmp_sums"
        if [ -z "$expected" ]; then
            warn "SHA256SUMS found but has no entry for ${asset_name}"
            rm -f "$tmp_bin"
            return 1
        fi
        if [ -z "$actual" ]; then
            warn "No sha256 tool available (sha256sum/shasum/openssl). Cannot verify download."
            if [ "${NC_SKIP_CHECKSUM:-}" != "1" ]; then
                rm -f "$tmp_bin"
                error "Refusing to run unverified binary. Install a checksum tool or set NC_SKIP_CHECKSUM=1 to override."
                return 1
            fi
        elif [ "$actual" != "$expected" ]; then
            rm -f "$tmp_bin"
            error "Checksum mismatch for ${asset_name}"
            error "  expected: ${expected}"
            error "  actual:   ${actual}"
            error "Refusing to install. The download may be corrupted or tampered with."
            return 1
        else
            success "SHA256 verified (${actual:0:16}...)"
        fi
    else
        rm -f "$tmp_sums"
        if [ "${NC_SKIP_CHECKSUM:-}" = "1" ]; then
            warn "SHA256SUMS not published for v${NC_VERSION}. Skipping verification (NC_SKIP_CHECKSUM=1)."
        else
            rm -f "$tmp_bin"
            error "SHA256SUMS not found at ${sums_url}"
            error "Refusing to run an unverified binary. Build from source, or set NC_SKIP_CHECKSUM=1 to override."
            return 1
        fi
    fi
    # ---------------------------------------------------------------------

    chmod +x "$tmp_bin"
    if "$tmp_bin" version >/dev/null 2>&1; then
        success "Downloaded pre-built binary"
        INSTALL_BINARY="$tmp_bin"
        return 0
    fi

    rm -f "$tmp_bin"
    warn "Pre-built binary failed smoke test, will build from source"
    echo ""
    return 1
}

# ----------------------------------------------------------------------
build_from_source() {
    step "Building NC from source"

    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd)"
    local build_dir=""
    local src_dir=""
    local need_cleanup=false

    # Check if we're in a source checkout
    if [ -f "$script_dir/engine/Makefile" ]; then
        build_dir="$script_dir/engine"
        src_dir="$script_dir"
        info "Using local source at ${DIM}${build_dir}${RESET}"
    elif [ -f "$script_dir/nc/Makefile" ]; then
        build_dir="$script_dir/nc"
        src_dir="$script_dir"
        info "Using local source at ${DIM}${build_dir}${RESET}"
    else
        # Download source
        info "Downloading NC source code..."
        local tmpdir
        tmpdir="$(mktemp -d)"
        need_cleanup=true

        if command -v git >/dev/null 2>&1; then
            git clone --depth 1 "https://github.com/${REPO}.git" "$tmpdir/nc" 2>/dev/null
        elif command -v curl >/dev/null 2>&1; then
            curl -sSL "https://github.com/${REPO}/archive/main.tar.gz" | tar xz -C "$tmpdir"
            mv "$tmpdir"/nc-*main* "$tmpdir/nc" 2>/dev/null || true
        elif command -v wget >/dev/null 2>&1; then
            wget -qO- "https://github.com/${REPO}/archive/main.tar.gz" | tar xz -C "$tmpdir"
            mv "$tmpdir"/nc-*main* "$tmpdir/nc" 2>/dev/null || true
        else
            error "Cannot download source: need git, curl, or wget"
            exit 1
        fi

        build_dir="$tmpdir/nc/engine"
        src_dir="$tmpdir/nc"

        # Fallback to nc/ subdirectory for older repo layouts
        if [ ! -f "$build_dir/Makefile" ]; then
            build_dir="$tmpdir/nc/nc"
        fi

        if [ ! -f "$build_dir/Makefile" ]; then
            error "Makefile not found after download"
            rm -rf "$tmpdir"
            exit 1
        fi
    fi

    # Build
    info "Compiling (this may take a moment)..."
    cd "$build_dir"
    make clean >/dev/null 2>&1 || true

    if make 2>&1 | while IFS= read -r line; do
        # Show last few lines to indicate progress
        printf "  ${DIM}  %s${RESET}\n" "$line"
    done; then
        true
    fi

    local built_bin="build/nc${BINARY_EXT}"
    if [ ! -f "$built_bin" ]; then
        error "Build failed: ${built_bin} not produced"
        if $need_cleanup; then rm -rf "$tmpdir"; fi
        exit 1
    fi

    success "Build successful"

    local tmp_bin="/tmp/nc-installer-$$${BINARY_EXT}"
    cp "$built_bin" "$tmp_bin"
    chmod +x "$tmp_bin"
    INSTALL_BINARY="$tmp_bin"

    # Copy standard library
    if [ -d "$src_dir/Lib" ]; then
        INSTALL_LIB_SRC="$src_dir/Lib"
    fi

    if $need_cleanup; then
        # Keep the lib source around until install finishes
        CLEANUP_DIR="$tmpdir"
    fi
    echo ""
}

# ----------------------------------------------------------------------
install_binary() {
    step "Installing NC"

    if [ -z "${INSTALL_BINARY:-}" ] || [ ! -f "${INSTALL_BINARY}" ]; then
        error "No binary to install"
        exit 1
    fi

    # Create install directory
    if [ ! -d "$INSTALL_DIR" ]; then
        if [ -w "$(dirname "$INSTALL_DIR")" ]; then
            mkdir -p "$INSTALL_DIR"
        else
            info "Creating ${INSTALL_DIR} (requires sudo)"
            sudo mkdir -p "$INSTALL_DIR"
        fi
    fi

    # Copy binary
    local dest="${INSTALL_DIR}/nc${BINARY_EXT}"
    if [ -w "$INSTALL_DIR" ]; then
        cp "$INSTALL_BINARY" "$dest"
    else
        info "Installing to ${INSTALL_DIR} (requires sudo)"
        sudo cp "$INSTALL_BINARY" "$dest"
        sudo chmod +x "$dest"
    fi
    success "Binary installed: ${dest}"

    # Copy standard library
    if [ -n "${INSTALL_LIB_SRC:-}" ] && [ -d "$INSTALL_LIB_SRC" ]; then
        if [ -w "$(dirname "$LIB_DIR")" ] || [ -w "$LIB_DIR" ] 2>/dev/null; then
            mkdir -p "$LIB_DIR"
            cp -r "$INSTALL_LIB_SRC/"* "$LIB_DIR/" 2>/dev/null || true
        else
            sudo mkdir -p "$LIB_DIR"
            sudo cp -r "$INSTALL_LIB_SRC/"* "$LIB_DIR/" 2>/dev/null || true
        fi
        success "Standard library installed: ${LIB_DIR}"
    fi

    # Windows/MSYS2: add to PATH via .bashrc or .bash_profile
    if [ "$PLATFORM" = "windows" ]; then
        local profile="${HOME}/.bashrc"
        if ! grep -q "nc.*bin" "$profile" 2>/dev/null; then
            echo "" >> "$profile"
            echo "# NC language" >> "$profile"
            echo "export PATH=\"${INSTALL_DIR}:\$PATH\"" >> "$profile"
            success "Added ${INSTALL_DIR} to PATH in ~/.bashrc"
        fi
    fi

    # Cleanup temp binary
    rm -f "$INSTALL_BINARY"
    if [ -n "${CLEANUP_DIR:-}" ]; then
        rm -rf "$CLEANUP_DIR"
    fi

    echo ""
}

# ----------------------------------------------------------------------
verify_installation() {
    step "Verifying installation"

    export NC_ACCEPT_LICENSE=1
    local nc_bin="${INSTALL_DIR}/nc${BINARY_EXT}"

    if [ ! -f "$nc_bin" ]; then
        error "Binary not found at ${nc_bin}"
        exit 1
    fi

    if "$nc_bin" version >/dev/null 2>&1; then
        local ver_output
        ver_output="$("$nc_bin" version 2>&1 | head -1)"
        success "Verification passed: ${ver_output}"
    elif "$nc_bin" --version >/dev/null 2>&1; then
        local ver_output
        ver_output="$("$nc_bin" --version 2>&1 | head -1)"
        success "Verification passed: ${ver_output}"
    else
        warn "Binary exists but version check returned non-zero (may still work)"
    fi
    echo ""
}

# ----------------------------------------------------------------------
print_success() {
    echo "${GREEN}  +--------------------------------------+${RESET}"
    echo "${GREEN}  |    NC installed successfully!        |${RESET}"
    echo "${GREEN}  +--------------------------------------+${RESET}"
    echo ""
    echo "  ${BOLD}Location:${RESET}  ${INSTALL_DIR}/nc${BINARY_EXT}"
    echo "  ${BOLD}Platform:${RESET}  ${PLATFORM}/${ARCH}"
    echo ""
    echo "  ${BOLD}Get started:${RESET}"
    echo "    ${CYAN}nc version${RESET}                Show version info"
    echo "    ${CYAN}nc \"show 42 + 8\"${RESET}          Run inline code"
    echo "    ${CYAN}nc run hello.nc${RESET}            Run a program"
    echo "    ${CYAN}nc serve app.nc${RESET}            Start HTTP server"
    echo "    ${CYAN}nc repl${RESET}                    Interactive REPL"
    echo ""
    echo "  ${BOLD}Other install methods:${RESET}"
    echo "    ${DIM}pip install nc-lang${RESET}        Python package"
    echo "    ${DIM}brew install nc${RESET}            macOS Homebrew"
    echo "    ${DIM}docker pull nc-lang/nc${RESET}     Docker"
    echo ""

    if [ "$PLATFORM" = "windows" ]; then
        warn "Restart your terminal for PATH changes to take effect."
        echo ""
    fi
}

# ----------------------------------------------------------------------
#  Main
# ----------------------------------------------------------------------
main() {
    detect_platform
    print_banner
    require_license_acceptance
    write_consent_marker
    echo ""
    check_prerequisites

    # Try pre-built binary first, fall back to source build
    if ! try_prebuilt; then
        build_from_source
    fi

    install_binary
    verify_installation
    print_success
}

main "$@"

