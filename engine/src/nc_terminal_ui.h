/*
 * nc_terminal_ui.h — Terminal animations, progress bars, and branding.
 *
 * Provides polished terminal output for the NC ecosystem:
 *   - NC startup animation (N ← → C merge)
 *   - NOVA / Nova-X branded intros
 *   - Progress bars for operations
 *   - Spinner for loading states
 *   - Verbosity control to suppress raw debug output
 */

#ifndef NC_TERMINAL_UI_H
#define NC_TERMINAL_UI_H

#include <stdbool.h>

/* ── Verbosity control ─────────────────────────────────── */
typedef enum {
    NC_LOG_QUIET   = 0,   /* No debug output */
    NC_LOG_NORMAL  = 1,   /* User-facing only */
    NC_LOG_VERBOSE = 2    /* Full debug (for developers) */
} NcTerminalLogLevel;

void nc_set_log_level(NcTerminalLogLevel level);
NcTerminalLogLevel nc_get_log_level(void);

/* ── Branding animations ───────────────────────────────── */
void nc_animate_startup(void);           /* N ← → C merge animation */
void nc_animate_nova(void);              /* NOVA branded intro */
void nc_animate_nova_x(void);            /* Nova-X branded intro */
void nc_animate_nc_ai(void);             /* NC AI branded intro */

/* ── Progress bar ──────────────────────────────────────── */
void nc_progress_start(const char *label, int total);
void nc_progress_update(int current);
void nc_progress_finish(const char *done_msg);

/* ── Spinner ───────────────────────────────────────────── */
void nc_spinner_start(const char *msg);
void nc_spinner_update(const char *msg);
void nc_spinner_stop(const char *done_msg);

/* ── Styled output ─────────────────────────────────────── */
void nc_print_success(const char *msg);
void nc_print_error(const char *msg);
void nc_print_warn(const char *msg);
void nc_print_info(const char *msg);
void nc_print_step(const char *msg);
void nc_print_banner(const char *title);

#endif /* NC_TERMINAL_UI_H */
