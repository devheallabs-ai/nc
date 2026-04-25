/*
 * nc_terminal_ui.c — Terminal animations, progress bars, and branding.
 *
 * Pure C11. Zero dependencies. Works on Windows, macOS, Linux.
 * Uses ANSI escape codes for colors and cursor movement.
 *
 * Copyright 2026 DevHeal Labs AI. Apache-2.0 License.
 */

#include "nc_terminal_ui.h"
#include "../include/nc_version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static bool g_ui_initialized = false;
static bool g_animation_enabled = true;
static bool g_ansi_enabled = false;
static bool g_unicode_enabled = true;

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define nc_isatty(fd) _isatty(fd)
#define nc_fileno(fp) _fileno(fp)
#define nc_sleep_ms(ms) Sleep(ms)
static void nc_enable_ansi(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode) &&
        SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */)) {
        g_ansi_enabled = true;
    }
}
#else
#include <unistd.h>
#define nc_isatty(fd) isatty(fd)
#define nc_fileno(fp) fileno(fp)
#define nc_sleep_ms(ms) usleep((ms) * 1000)
static void nc_enable_ansi(void) { g_ansi_enabled = true; }
#endif

/* ═══════════════════════════════════════════════════════════
 *  ANSI Color Codes
 * ═══════════════════════════════════════════════════════════ */
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define ITALIC      "\033[3m"
#define UNDERLINE   "\033[4m"

#define FG_BLACK    "\033[30m"
#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

#define FG_BRIGHT_CYAN    "\033[96m"
#define FG_BRIGHT_GREEN   "\033[92m"
#define FG_BRIGHT_YELLOW  "\033[93m"
#define FG_BRIGHT_MAGENTA "\033[95m"
#define FG_BRIGHT_BLUE    "\033[94m"
#define FG_BRIGHT_WHITE   "\033[97m"
#define FG_BRIGHT_RED     "\033[91m"

#define BG_BLUE     "\033[44m"
#define BG_CYAN     "\033[46m"

#define CLEAR_LINE  "\033[2K"
#define CURSOR_UP   "\033[1A"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

/* ═══════════════════════════════════════════════════════════
 *  Verbosity Control
 * ═══════════════════════════════════════════════════════════ */
static NcTerminalLogLevel g_log_level = NC_LOG_NORMAL;

static int nc_str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool nc_env_true(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return false;
    return nc_str_ieq(v, "1") || nc_str_ieq(v, "true") ||
           nc_str_ieq(v, "yes") || nc_str_ieq(v, "on");
}

static void nc_ui_init(void) {
    if (g_ui_initialized) return;
    g_ui_initialized = true;

    g_ansi_enabled = false;
    g_unicode_enabled = true;
    g_animation_enabled = true;

    if (!nc_isatty(nc_fileno(stdout))) {
        g_animation_enabled = false;
        g_ansi_enabled = false;
    }

    if (nc_env_true("NC_NO_ANIM")) {
        g_animation_enabled = false;
    }

    if (nc_env_true("NO_COLOR")) {
        g_ansi_enabled = false;
    }

    {
        const char *term = getenv("TERM");
        if (term && (nc_str_ieq(term, "dumb") || nc_str_ieq(term, "unknown"))) {
            g_animation_enabled = false;
            g_ansi_enabled = false;
        }
    }

    if (!nc_env_true("NO_COLOR") && nc_isatty(nc_fileno(stdout))) {
        nc_enable_ansi();
    }

#ifdef _WIN32
    if (GetConsoleOutputCP() != 65001) {
        g_unicode_enabled = false;
    }
#endif

    if (!g_ansi_enabled) {
        g_animation_enabled = false;
    }
}

static const char *nc_ok_symbol(void)   { return g_unicode_enabled ? "✓" : "OK"; }
static const char *nc_err_symbol(void)  { return g_unicode_enabled ? "✗" : "X"; }
static const char *nc_warn_symbol(void) { return g_unicode_enabled ? "⚠" : "!"; }
static const char *nc_info_symbol(void) { return g_unicode_enabled ? "→" : ">"; }
static const char *nc_arrow_symbol(void){ return g_unicode_enabled ? "→" : ">"; }

void nc_set_log_level(NcTerminalLogLevel level) { g_log_level = level; }
NcTerminalLogLevel nc_get_log_level(void) { return g_log_level; }

/* ═══════════════════════════════════════════════════════════
 *  NC Startup Animation — "N" and "C" merge from sides
 *
 *  Frame sequence (50ms each):
 *    N                              C
 *       N                       C
 *          N                 C
 *             N           C
 *                N     C
 *                  N C
 *                  NC
 *                 _NC_
 *         ╔═══════════════╗
 *         ║  NC  vX.Y.Z   ║
 *         ╚═══════════════╝
 * ═══════════════════════════════════════════════════════════ */

void nc_animate_startup(void) {
    nc_ui_init();
    if (!g_animation_enabled) {
        printf("\n  NC v%s\n  Notation-as-Code\n\n", NC_VERSION);
        return;
    }
    printf(HIDE_CURSOR);

    /* Phase 1: N and C converge from sides */
    const int width = 40;
    const int center = width / 2;
    const int frames = 12;

    for (int i = 0; i < frames; i++) {
        int n_pos = (center * i) / frames;
        int c_pos = width - 1 - (center * i) / frames;

        printf("\r" CLEAR_LINE);
        for (int j = 0; j < width; j++) {
            if (j == n_pos)
                printf(BOLD FG_BRIGHT_CYAN "N" RESET);
            else if (j == c_pos)
                printf(BOLD FG_BRIGHT_MAGENTA "C" RESET);
            else
                printf(" ");
        }
        fflush(stdout);
        nc_sleep_ms(45);
    }

    /* Phase 2: Merge and glow */
    const char *merge_frames[] = {
        BOLD FG_BRIGHT_CYAN "N" FG_BRIGHT_MAGENTA "C" RESET,
        BOLD FG_BRIGHT_WHITE "NC" RESET,
        BOLD FG_BRIGHT_CYAN "N" FG_BRIGHT_MAGENTA "C" RESET,
        BOLD FG_BRIGHT_WHITE "NC" RESET,
    };

    for (int i = 0; i < 4; i++) {
        printf("\r" CLEAR_LINE);
        /* Center the NC text */
        for (int j = 0; j < center - 1; j++) printf(" ");
        printf("%s", merge_frames[i]);
        fflush(stdout);
        nc_sleep_ms(100);
    }

    /* Phase 3: Expand to full brand */
    printf("\r" CLEAR_LINE "\n");
    nc_sleep_ms(80);

    /* ASCII art logo with color */
    printf("  " FG_BRIGHT_CYAN BOLD
        "   _  _  ___\n"
        "  | \\| |/ __|\n"
        "  | .` | (__\n"
        "  |_|\\_|\\___|" RESET "\n");
    nc_sleep_ms(80);
    printf("\n");
    printf("  " BOLD FG_BRIGHT_WHITE "Notation-as-Code" RESET
           DIM " v%s" RESET "\n", NC_VERSION);
    printf("  " DIM "The fastest way to build AI APIs" RESET "\n");
    printf("  " DIM "─────────────────────────────────" RESET "\n\n");

    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  NOVA Animation — Neural star burst
 *
 *    ✦ NOVA ✦
 *    Neural Optimized Virtual Architecture
 * ═══════════════════════════════════════════════════════════ */

void nc_animate_nova(void) {
    nc_ui_init();
    if (!g_animation_enabled) {
        printf("\n  NOVA\n  Neural Optimized Virtual Architecture\n\n");
        return;
    }
    printf(HIDE_CURSOR);

    /* Phase 1: Star pulse */
    const char *star_frames[] = {
        "      " DIM "." RESET,
        "     " FG_YELLOW "·" RESET,
        "    " FG_BRIGHT_YELLOW "✦" RESET,
        "   " BOLD FG_BRIGHT_YELLOW "✦ ✦" RESET,
        "  " BOLD FG_BRIGHT_YELLOW "✦   ✦" RESET,
    };
    for (int i = 0; i < 5; i++) {
        printf("\r" CLEAR_LINE "%s", star_frames[i]);
        fflush(stdout);
        nc_sleep_ms(60);
    }
    printf("\n");

    /* Phase 2: Letters appear one by one */
    const char *letters[] = {
        BOLD FG_BRIGHT_YELLOW "N" RESET,
        BOLD FG_BRIGHT_YELLOW "NO" RESET,
        BOLD FG_BRIGHT_YELLOW "NOV" RESET,
        BOLD FG_BRIGHT_YELLOW "NOVA" RESET,
    };
    for (int i = 0; i < 4; i++) {
        printf("\r" CLEAR_LINE "     %s", letters[i]);
        fflush(stdout);
        nc_sleep_ms(70);
    }
    nc_sleep_ms(100);

    /* Phase 3: Full branded display */
    printf("\r" CLEAR_LINE "\n");
    printf("  " BOLD FG_BRIGHT_YELLOW "  ✦  N O V A  ✦" RESET "\n");
    nc_sleep_ms(50);
    printf("  " DIM FG_YELLOW "  Neural Optimized Virtual Architecture" RESET "\n");
    printf("  " DIM "  ─────────────────────────────────────" RESET "\n");
    nc_sleep_ms(50);
    printf("  " DIM "  by DevHeal Labs AI" RESET "\n\n");

    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  Nova-X Animation — Enterprise/advanced version
 *
 *    ⚡ NOVA-X ⚡
 *    eXtended Intelligence Platform
 * ═══════════════════════════════════════════════════════════ */

void nc_animate_nova_x(void) {
    nc_ui_init();
    if (!g_animation_enabled) {
        printf("\n  NOVA-X\n  eXtended Intelligence Platform\n\n");
        return;
    }
    printf(HIDE_CURSOR);

    /* Phase 1: Electric pulse effect */
    const char *pulse_frames[] = {
        "        " DIM "─" RESET,
        "      " FG_CYAN "──" RESET,
        "    " FG_BRIGHT_CYAN "────" RESET,
        "  " FG_BRIGHT_CYAN "──────" RESET,
        " " BOLD FG_BRIGHT_CYAN "━━━━━━━━" RESET,
    };
    for (int i = 0; i < 5; i++) {
        printf("\r" CLEAR_LINE "%s", pulse_frames[i]);
        fflush(stdout);
        nc_sleep_ms(50);
    }
    printf("\n");

    /* Phase 2: Name materializes */
    const char *name_frames[] = {
        "     " DIM "N   -" RESET,
        "     " FG_CYAN "NO  -" RESET,
        "     " FG_BRIGHT_CYAN "NOV -" RESET,
        "     " FG_BRIGHT_CYAN "NOVA-" RESET,
        "     " BOLD FG_BRIGHT_CYAN "NOVA-X" RESET,
    };
    for (int i = 0; i < 5; i++) {
        printf("\r" CLEAR_LINE "%s", name_frames[i]);
        fflush(stdout);
        nc_sleep_ms(60);
    }
    nc_sleep_ms(100);

    /* Phase 3: Full display */
    printf("\r" CLEAR_LINE "\n");
    printf("  " BOLD FG_BRIGHT_CYAN "  ⚡ N O V A - X ⚡" RESET "\n");
    nc_sleep_ms(50);
    printf("  " DIM FG_CYAN "  eXtended Intelligence Platform" RESET "\n");
    printf("  " DIM "  ─────────────────────────────────" RESET "\n");
    nc_sleep_ms(50);
    printf("  " DIM "  Enterprise AI by DevHeal Labs AI" RESET "\n\n");

    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  NC AI Animation — Brain pulse
 * ═══════════════════════════════════════════════════════════ */

void nc_animate_nc_ai(void) {
    nc_ui_init();
    if (!g_animation_enabled) {
        printf("\n  NC AI\n  Built-in Intelligence Engine\n\n");
        return;
    }
    printf(HIDE_CURSOR);

    /* Phase 1: Brain pulse */
    const char *brain_frames[] = {
        "       " DIM "◯" RESET,
        "      " FG_MAGENTA "◉" RESET,
        "     " FG_BRIGHT_MAGENTA "⦿" RESET,
        "    " BOLD FG_BRIGHT_MAGENTA "🧠" RESET,
    };
    for (int i = 0; i < 4; i++) {
        printf("\r" CLEAR_LINE "%s", brain_frames[i]);
        fflush(stdout);
        nc_sleep_ms(80);
    }
    printf("\n");

    /* Phase 2: NC AI appears */
    printf("\r" CLEAR_LINE);
    printf("  " BOLD FG_BRIGHT_CYAN "  N" FG_BRIGHT_MAGENTA "C" FG_BRIGHT_WHITE " AI" RESET "\n");
    nc_sleep_ms(80);
    printf("  " DIM "  Built-in Intelligence Engine" RESET "\n");
    printf("  " DIM "  ────────────────────────────" RESET "\n\n");

    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  Progress Bar
 *
 *  Usage:
 *    nc_progress_start("Building", 100);
 *    for (int i = 0; i <= 100; i++) nc_progress_update(i);
 *    nc_progress_finish("Done!");
 *
 *  Output:
 *    Building  ████████████░░░░░░░░  60%
 * ═══════════════════════════════════════════════════════════ */

static char g_progress_label[64] = "";
static int  g_progress_total = 100;
static int  g_progress_last_pct = -1;

void nc_progress_start(const char *label, int total) {
    nc_ui_init();
    if (label) {
        strncpy(g_progress_label, label, sizeof(g_progress_label) - 1);
        g_progress_label[sizeof(g_progress_label) - 1] = '\0';
    }
    g_progress_total = total > 0 ? total : 100;
    g_progress_last_pct = -1;
    if (!g_animation_enabled || !g_ansi_enabled) {
        printf("  %s...\n", g_progress_label[0] ? g_progress_label : "Working");
        return;
    }
    printf(HIDE_CURSOR);
    nc_progress_update(0);
}

void nc_progress_update(int current) {
    if (!g_animation_enabled || !g_ansi_enabled) return;
    if (g_progress_total <= 0) return;
    int pct = (current * 100) / g_progress_total;
    if (pct > 100) pct = 100;
    if (pct == g_progress_last_pct) return;  /* Skip redundant redraws */
    g_progress_last_pct = pct;

    const int bar_width = 24;
    int filled = (pct * bar_width) / 100;

    printf("\r" CLEAR_LINE "  " DIM "%s" RESET "  " FG_BRIGHT_CYAN, g_progress_label);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled)
            printf("█");
        else
            printf(DIM "░" FG_BRIGHT_CYAN);
    }
    printf(RESET "  " BOLD "%d%%" RESET, pct);
    fflush(stdout);
}

void nc_progress_finish(const char *done_msg) {
    nc_ui_init();
    if (!g_animation_enabled || !g_ansi_enabled) {
        printf("  %s %s\n", nc_ok_symbol(), done_msg ? done_msg : "Done!");
        return;
    }
    /* Fill bar to 100% */
    nc_progress_update(g_progress_total);
    printf("\r" CLEAR_LINE "  " FG_BRIGHT_GREEN "%s" RESET " %s\n",
           nc_ok_symbol(),
           done_msg ? done_msg : "Done!");
    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  Spinner
 *
 *  Output:
 *    ⠋ Loading model...
 *    ⠙ Loading model...
 *    ⠸ Loading model...
 * ═══════════════════════════════════════════════════════════ */

static int g_spinner_frame = 0;

static const char *SPINNER_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
#define SPINNER_COUNT 10

void nc_spinner_start(const char *msg) {
    nc_ui_init();
    g_spinner_frame = 0;
    if (!g_animation_enabled || !g_ansi_enabled) {
        printf("  %s\n", msg ? msg : "Working...");
        return;
    }
    printf(HIDE_CURSOR);
    printf("\r" CLEAR_LINE "  " FG_BRIGHT_CYAN "%s" RESET " %s",
           SPINNER_FRAMES[0], msg ? msg : "");
    fflush(stdout);
}

void nc_spinner_update(const char *msg) {
    if (!g_animation_enabled || !g_ansi_enabled) return;
    g_spinner_frame = (g_spinner_frame + 1) % SPINNER_COUNT;
    printf("\r" CLEAR_LINE "  " FG_BRIGHT_CYAN "%s" RESET " %s",
           SPINNER_FRAMES[g_spinner_frame], msg ? msg : "");
    fflush(stdout);
}

void nc_spinner_stop(const char *done_msg) {
    nc_ui_init();
    if (!g_animation_enabled || !g_ansi_enabled) {
        printf("  %s %s\n", nc_ok_symbol(), done_msg ? done_msg : "Done!");
        return;
    }
    printf("\r" CLEAR_LINE "  " FG_BRIGHT_GREEN "%s" RESET " %s\n",
           nc_ok_symbol(),
           done_msg ? done_msg : "Done!");
    printf(SHOW_CURSOR);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════
 *  Styled Output Helpers
 * ═══════════════════════════════════════════════════════════ */

void nc_print_success(const char *msg) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        printf("  %s %s\n", nc_ok_symbol(), msg);
        return;
    }
    printf("  " FG_BRIGHT_GREEN "%s" RESET " %s\n", nc_ok_symbol(), msg);
}

void nc_print_error(const char *msg) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        fprintf(stderr, "  %s %s\n", nc_err_symbol(), msg);
        return;
    }
    fprintf(stderr, "  " FG_BRIGHT_RED "%s" RESET " %s\n", nc_err_symbol(), msg);
}

void nc_print_warn(const char *msg) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        fprintf(stderr, "  %s %s\n", nc_warn_symbol(), msg);
        return;
    }
    fprintf(stderr, "  " FG_BRIGHT_YELLOW "%s" RESET " %s\n", nc_warn_symbol(), msg);
}

void nc_print_info(const char *msg) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        printf("  %s %s\n", nc_info_symbol(), msg);
        return;
    }
    printf("  " FG_BRIGHT_CYAN "%s" RESET " %s\n", nc_info_symbol(), msg);
}

void nc_print_step(const char *msg) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        printf("  %s %s\n", nc_arrow_symbol(), msg);
        return;
    }
    printf("  " FG_BRIGHT_CYAN "%s" RESET " " BOLD "%s" RESET "\n", nc_arrow_symbol(), msg);
}

void nc_print_banner(const char *title) {
    nc_ui_init();
    if (!g_ansi_enabled) {
        printf("\n  %s\n  ---------------------------------\n\n", title);
        return;
    }
    printf("\n  " BOLD FG_BRIGHT_WHITE "%s" RESET, title);
    printf("\n  " DIM "─────────────────────────────────" RESET "\n\n");
}
