#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
#  NC (Notation-as-Code) — Project Starter Script
#
#  Usage:
#    ./start.sh              Interactive menu
#    ./start.sh build        Build NC engine from source
#    ./start.sh test         Run all tests
#    ./start.sh install      Build + install to /usr/local/bin
#    ./start.sh serve <f>    Run an NC service
#    ./start.sh docker       Build Docker image
#    ./start.sh docker-run   Run NC in Docker container
#    ./start.sh clean        Clean build artifacts
#    ./start.sh showcase     List and run showcase projects
#    ./start.sh help         Show this help
# ═══════════════════════════════════════════════════════════════════════

set -euo pipefail

# ── Colors ────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
RESET='\033[0m'

# ── Paths ─────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$SCRIPT_DIR/engine"
BUILD_DIR="$ENGINE_DIR/build"
TESTS_DIR="$SCRIPT_DIR/tests"
SHOWCASE_DIR="$SCRIPT_DIR/showcase"
NC_BIN="$BUILD_DIR/nc"

# ── Helpers ───────────────────────────────────────────────────────────
info()    { echo -e "${BLUE}[INFO]${RESET}  $*"; }
success() { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
error()   { echo -e "${RED}[ERROR]${RESET} $*"; }
header()  { echo -e "\n${BOLD}${CYAN}═══ $* ═══${RESET}\n"; }

check_compiler() {
    if command -v gcc &>/dev/null; then
        return 0
    elif command -v clang &>/dev/null; then
        return 0
    else
        error "No C compiler found. Install gcc or clang first."
        echo "  macOS:  xcode-select --install"
        echo "  Ubuntu: sudo apt install gcc"
        echo "  Fedora: sudo dnf install gcc"
        exit 1
    fi
}

check_nc_binary() {
    if [ ! -f "$NC_BIN" ]; then
        error "NC binary not found at $NC_BIN"
        echo "  Run: ./start.sh build"
        exit 1
    fi
}

# ── Commands ──────────────────────────────────────────────────────────

cmd_build() {
    header "Building NC Engine"
    check_compiler
    info "Compiling from $ENGINE_DIR ..."
    cd "$ENGINE_DIR"
    make clean 2>/dev/null || true
    make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    success "Build complete: $NC_BIN"
    echo ""
    "$NC_BIN" version 2>/dev/null || true
}

cmd_test() {
    header "Running NC Tests"
    check_nc_binary

    if [ -f "$TESTS_DIR/run_tests.sh" ]; then
        info "Running test suite via run_tests.sh ..."
        cd "$SCRIPT_DIR"
        bash "$TESTS_DIR/run_tests.sh"
    else
        info "Running tests via nc test ..."
        cd "$ENGINE_DIR"
        "$NC_BIN" test
    fi

    # Run lang tests if directory exists
    if [ -d "$TESTS_DIR/lang" ]; then
        info "Running language tests ..."
        PASSED=0
        FAILED=0
        TOTAL=0
        for f in "$TESTS_DIR/lang"/*.nc; do
            [ -f "$f" ] || continue
            TOTAL=$((TOTAL + 1))
            NAME=$(basename "$f")
            if "$NC_BIN" run "$f" &>/dev/null; then
                PASSED=$((PASSED + 1))
            else
                FAILED=$((FAILED + 1))
                warn "FAIL: $NAME"
            fi
        done
        echo ""
        if [ "$FAILED" -eq 0 ]; then
            success "All $TOTAL tests passed."
        else
            error "$FAILED/$TOTAL tests failed."
            exit 1
        fi
    fi
}

cmd_install() {
    header "Installing NC"
    cmd_build
    echo ""
    info "Installing to /usr/local/bin/nc ..."
    if [ -w /usr/local/bin ]; then
        cp "$NC_BIN" /usr/local/bin/nc
    else
        info "Requires sudo for /usr/local/bin ..."
        sudo cp "$NC_BIN" /usr/local/bin/nc
    fi
    success "Installed. Run 'nc version' to verify."
}

cmd_serve() {
    local FILE="${1:-}"
    if [ -z "$FILE" ]; then
        error "Usage: ./start.sh serve <file.nc>"
        exit 1
    fi
    if [ ! -f "$FILE" ]; then
        # Try relative to script dir
        if [ -f "$SCRIPT_DIR/$FILE" ]; then
            FILE="$SCRIPT_DIR/$FILE"
        else
            error "File not found: $FILE"
            exit 1
        fi
    fi
    check_nc_binary
    header "Serving NC Application"
    info "File: $FILE"
    "$NC_BIN" serve "$FILE"
}

cmd_docker() {
    header "Building Docker Image"
    if ! command -v docker &>/dev/null; then
        error "Docker is not installed or not in PATH."
        exit 1
    fi
    info "Building nc:latest ..."
    cd "$SCRIPT_DIR"
    docker build -t nc:latest .
    success "Docker image built: nc:latest"
    echo ""
    echo -e "  ${DIM}Run:  docker run -it nc:latest version${RESET}"
    echo -e "  ${DIM}Serve: docker run -p 8080:8080 -v \$(pwd):/app nc:latest serve /app/service.nc${RESET}"
}

cmd_docker_run() {
    header "Running NC in Docker"
    if ! command -v docker &>/dev/null; then
        error "Docker is not installed or not in PATH."
        exit 1
    fi
    local FILE="${1:-}"
    if [ -n "$FILE" ]; then
        info "Serving $FILE in Docker ..."
        docker run --rm -it -p 8080:8080 \
            -v "$SCRIPT_DIR:/app" \
            nc:latest serve "/app/$FILE"
    else
        info "Starting NC container (interactive) ..."
        docker run --rm -it -p 8080:8080 \
            -v "$SCRIPT_DIR:/app" \
            nc:latest version
    fi
}

cmd_clean() {
    header "Cleaning Build Artifacts"
    cd "$ENGINE_DIR"
    make clean 2>/dev/null || true
    success "Build artifacts removed."
}

cmd_showcase() {
    header "NC Showcase Projects"
    echo ""
    if [ ! -d "$SHOWCASE_DIR" ]; then
        error "Showcase directory not found."
        exit 1
    fi

    local PROJECTS=()
    local IDX=1
    for d in "$SHOWCASE_DIR"/*/; do
        [ -d "$d" ] || continue
        NAME=$(basename "$d")
        PROJECTS+=("$NAME")
        NC_FILE=$(find "$d" -maxdepth 1 -name "*.nc" | head -1)
        NC_NAME=$(basename "$NC_FILE" 2>/dev/null || echo "???")
        DESC=""
        if [ -f "$d/README.md" ]; then
            DESC=$(head -3 "$d/README.md" | tail -1 | sed 's/^#* *//')
        fi
        echo -e "  ${BOLD}$IDX)${RESET} ${CYAN}$NAME${RESET}"
        if [ -n "$DESC" ]; then
            echo -e "     $DESC"
        fi
        echo -e "     ${DIM}nc serve showcase/$NAME/$NC_NAME${RESET}"
        echo ""
        IDX=$((IDX + 1))
    done

    echo -e "${BOLD}Enter a number to run, or press Enter to go back:${RESET} "
    read -r CHOICE
    if [ -n "$CHOICE" ] && [ "$CHOICE" -ge 1 ] 2>/dev/null && [ "$CHOICE" -le "${#PROJECTS[@]}" ] 2>/dev/null; then
        local SELECTED="${PROJECTS[$((CHOICE - 1))]}"
        local NC_FILE
        NC_FILE=$(find "$SHOWCASE_DIR/$SELECTED" -maxdepth 1 -name "*.nc" | head -1)
        if [ -n "$NC_FILE" ]; then
            check_nc_binary
            info "Starting $SELECTED ..."
            "$NC_BIN" serve "$NC_FILE"
        else
            error "No .nc file found in showcase/$SELECTED"
        fi
    fi
}

cmd_help() {
    echo ""
    echo -e "${BOLD}${CYAN}NC (Notation-as-Code) — Project Starter${RESET}"
    echo ""
    echo -e "  ${BOLD}Usage:${RESET} ./start.sh [command] [args]"
    echo ""
    echo -e "  ${BOLD}Commands:${RESET}"
    echo -e "    ${GREEN}build${RESET}          Build NC engine from source"
    echo -e "    ${GREEN}test${RESET}           Run all tests"
    echo -e "    ${GREEN}install${RESET}        Build + install to /usr/local/bin"
    echo -e "    ${GREEN}serve${RESET} <file>   Run an NC service"
    echo -e "    ${GREEN}docker${RESET}         Build Docker image"
    echo -e "    ${GREEN}docker-run${RESET}     Run NC in Docker container"
    echo -e "    ${GREEN}clean${RESET}          Clean build artifacts"
    echo -e "    ${GREEN}showcase${RESET}       List and run showcase projects"
    echo -e "    ${GREEN}help${RESET}           Show this help"
    echo ""
    echo -e "  ${BOLD}Examples:${RESET}"
    echo -e "    ./start.sh build"
    echo -e "    ./start.sh serve examples/01_hello_world.nc"
    echo -e "    ./start.sh docker"
    echo -e "    ./start.sh showcase"
    echo ""
}

cmd_menu() {
    echo ""
    echo -e "${BOLD}${CYAN}"
    echo "  ███╗   ██╗ ██████╗"
    echo "  ████╗  ██║██╔════╝"
    echo "  ██╔██╗ ██║██║     "
    echo "  ██║╚██╗██║██║     "
    echo "  ██║ ╚████║╚██████╗"
    echo "  ╚═╝  ╚═══╝ ╚═════╝"
    echo -e "${RESET}"
    echo -e "  ${BOLD}Notation-as-Code — The AI Programming Language${RESET}"
    echo -e "  ${DIM}Write AI backends in plain English${RESET}"
    echo ""
    echo -e "  ${BOLD}Select an action:${RESET}"
    echo ""
    echo -e "    ${GREEN}1)${RESET} Build NC engine from source"
    echo -e "    ${GREEN}2)${RESET} Run all tests"
    echo -e "    ${GREEN}3)${RESET} Build + install to /usr/local/bin"
    echo -e "    ${GREEN}4)${RESET} Serve an NC file"
    echo -e "    ${GREEN}5)${RESET} Build Docker image"
    echo -e "    ${GREEN}6)${RESET} Run NC in Docker"
    echo -e "    ${GREEN}7)${RESET} Clean build artifacts"
    echo -e "    ${GREEN}8)${RESET} Browse showcase projects"
    echo -e "    ${GREEN}9)${RESET} Show help"
    echo -e "    ${GREEN}0)${RESET} Exit"
    echo ""
    echo -ne "  ${BOLD}Choice [0-9]:${RESET} "
    read -r CHOICE
    echo ""

    case "$CHOICE" in
        1) cmd_build ;;
        2) cmd_test ;;
        3) cmd_install ;;
        4)
            echo -ne "  ${BOLD}Enter .nc file path:${RESET} "
            read -r FILE
            cmd_serve "$FILE"
            ;;
        5) cmd_docker ;;
        6)
            echo -ne "  ${BOLD}Enter .nc file path (or Enter for version):${RESET} "
            read -r FILE
            cmd_docker_run "$FILE"
            ;;
        7) cmd_clean ;;
        8) cmd_showcase ;;
        9) cmd_help ;;
        0) echo "  Bye!" ; exit 0 ;;
        *) error "Invalid choice: $CHOICE" ; exit 1 ;;
    esac
}

# ── Main ──────────────────────────────────────────────────────────────
COMMAND="${1:-}"
shift 2>/dev/null || true

case "$COMMAND" in
    build)      cmd_build ;;
    test)       cmd_test ;;
    install)    cmd_install ;;
    serve)      cmd_serve "$@" ;;
    docker)     cmd_docker ;;
    docker-run) cmd_docker_run "$@" ;;
    clean)      cmd_clean ;;
    showcase)   cmd_showcase ;;
    help|--help|-h) cmd_help ;;
    "")         cmd_menu ;;
    *)
        error "Unknown command: $COMMAND"
        cmd_help
        exit 1
        ;;
esac
