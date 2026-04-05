/*
 * nc_platform.h — Cross-platform abstraction layer for NC.
 *
 * Wraps POSIX-specific APIs with portable alternatives so the
 * NC runtime compiles on Linux, macOS, and Windows (MSVC/MinGW).
 *
 * Include this header instead of platform-specific headers
 * (sys/socket.h, unistd.h, pthread.h, etc.).
 */

#ifndef NC_PLATFORM_H
#define NC_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════
 *  Platform detection
 * ═══════════════════════════════════════════════════════════ */

#ifdef _WIN32
#  define NC_WINDOWS 1
#else
#  define NC_POSIX 1
#endif

/* MSVC sys/stat.h may not define S_ISREG / S_ISDIR */
#ifdef NC_WINDOWS
#  ifndef S_ISREG
#    define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#  endif
#  ifndef S_ISDIR
#    define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#  endif
#endif

/* Path separators — needed early for nc_mkdir_p and friends */
#ifdef NC_WINDOWS
#  define NC_PATH_SEP '\\'
#  define NC_PATH_SEP_STR "\\"
#  define NC_PATH_LIST_SEP ';'
#else
#  define NC_PATH_SEP '/'
#  define NC_PATH_SEP_STR "/"
#  define NC_PATH_LIST_SEP ':'
#endif

/* ═══════════════════════════════════════════════════════════
 *  Headers
 * ═══════════════════════════════════════════════════════════ */

#include <signal.h>

#ifdef NC_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <io.h>
#  include <direct.h>
#  include <process.h>
#  include <fcntl.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "ws2_32.lib")
#  endif
#else
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <sys/wait.h>
#  include <sys/select.h>
#  include <signal.h>
#  include <fcntl.h>
#  include <dirent.h>
#  include <pthread.h>
#endif

/* ssize_t is not defined by MSVC */
#if defined(NC_WINDOWS) && defined(_MSC_VER) && !defined(ssize_t)
typedef intptr_t ssize_t;
#endif

/* ═══════════════════════════════════════════════════════════
 *  Socket abstraction
 * ═══════════════════════════════════════════════════════════ */

#ifdef NC_WINDOWS
typedef SOCKET nc_socket_t;
#  define NC_INVALID_SOCKET INVALID_SOCKET
#  define nc_closesocket(s)  closesocket(s)
#else
typedef int nc_socket_t;
#  define NC_INVALID_SOCKET (-1)
#  define nc_closesocket(s)  close(s)
#endif

static inline int nc_socket_init(void) {
#ifdef NC_WINDOWS
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
#else
    return 0;
#endif
}

static inline void nc_socket_cleanup(void) {
#ifdef NC_WINDOWS
    WSACleanup();
#endif
}

/* setsockopt compatibility: Windows uses (const char*) for optval */
#ifdef NC_WINDOWS
#  define nc_setsockopt(s, level, name, val, len) \
       setsockopt(s, level, name, (const char *)(val), len)
#else
#  define nc_setsockopt(s, level, name, val, len) \
       setsockopt(s, level, name, val, len)
#endif

/* Cross-platform send/recv — Windows needs (const char*) / (char*) casts */
#ifdef NC_WINDOWS
#  define nc_send(s, buf, len, flags) send(s, (const char *)(buf), len, flags)
#  define nc_recv(s, buf, len, flags) recv(s, (char *)(buf), len, flags)
#else
#  define nc_send(s, buf, len, flags) send(s, buf, len, flags)
#  define nc_recv(s, buf, len, flags) recv(s, buf, len, flags)
#endif

/* SO_RCVTIMEO/SO_SNDTIMEO: Windows uses DWORD ms, POSIX uses struct timeval */
static inline int nc_set_socket_timeout(nc_socket_t s, int timeout_sec) {
#ifdef NC_WINDOWS
    DWORD tv = (DWORD)(timeout_sec * 1000);
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Threading abstraction
 * ═══════════════════════════════════════════════════════════ */

#ifdef NC_WINDOWS

/* ETIMEDOUT may not be defined by all MSVC versions */
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

/*
 * Use SRWLOCK instead of CRITICAL_SECTION.
 * SRWLOCK can be statically initialized with SRWLOCK_INIT (= {0}),
 * while CRITICAL_SECTION requires InitializeCriticalSection() before
 * first use — causing segfaults when used with static {0} initializer.
 */
typedef SRWLOCK             nc_mutex_t;
typedef CONDITION_VARIABLE  nc_cond_t;
typedef HANDLE              nc_thread_t;

#define NC_MUTEX_INITIALIZER SRWLOCK_INIT

static inline void nc_mutex_init(nc_mutex_t *m) { InitializeSRWLock(m); }
static inline void nc_mutex_destroy(nc_mutex_t *m) { (void)m; }
static inline void nc_mutex_lock(nc_mutex_t *m) { AcquireSRWLockExclusive(m); }
static inline void nc_mutex_unlock(nc_mutex_t *m) { ReleaseSRWLockExclusive(m); }

static inline void nc_cond_init(nc_cond_t *c) { InitializeConditionVariable(c); }
static inline void nc_cond_destroy(nc_cond_t *c) { (void)c; }
static inline void nc_cond_signal(nc_cond_t *c) { WakeConditionVariable(c); }
static inline void nc_cond_broadcast(nc_cond_t *c) { WakeAllConditionVariable(c); }
static inline void nc_cond_wait(nc_cond_t *c, nc_mutex_t *m) {
    SleepConditionVariableSRW(c, m, INFINITE, 0);
}
static inline int nc_cond_timedwait(nc_cond_t *c, nc_mutex_t *m, int timeout_ms) {
    return SleepConditionVariableSRW(c, m, timeout_ms, 0) ? 0 : ETIMEDOUT;
}

typedef unsigned (__stdcall *nc_thread_func_t)(void *);

static inline int nc_thread_create(nc_thread_t *t, nc_thread_func_t func, void *arg) {
    *t = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    return *t ? 0 : -1;
}
static inline int nc_thread_join(nc_thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return 0;
}
static inline int nc_thread_detach(nc_thread_t t) {
    CloseHandle(t);
    return 0;
}

/* Thread-local storage */
#if defined(_MSC_VER)
#define nc_thread_local __declspec(thread)
#else
#define nc_thread_local __thread
#endif

#else /* POSIX */

typedef pthread_mutex_t     nc_mutex_t;
typedef pthread_cond_t      nc_cond_t;
typedef pthread_t           nc_thread_t;

#define NC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void nc_mutex_init(nc_mutex_t *m) { pthread_mutex_init(m, NULL); }
static inline void nc_mutex_destroy(nc_mutex_t *m) { pthread_mutex_destroy(m); }
static inline void nc_mutex_lock(nc_mutex_t *m) { pthread_mutex_lock(m); }
static inline void nc_mutex_unlock(nc_mutex_t *m) { pthread_mutex_unlock(m); }

static inline void nc_cond_init(nc_cond_t *c) { pthread_cond_init(c, NULL); }
static inline void nc_cond_destroy(nc_cond_t *c) { pthread_cond_destroy(c); }
static inline void nc_cond_signal(nc_cond_t *c) { pthread_cond_signal(c); }
static inline void nc_cond_broadcast(nc_cond_t *c) { pthread_cond_broadcast(c); }
static inline void nc_cond_wait(nc_cond_t *c, nc_mutex_t *m) {
    pthread_cond_wait(c, m);
}
static inline int nc_cond_timedwait(nc_cond_t *c, nc_mutex_t *m, int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (long)timeout_ms * 1000000L;
    while (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    return pthread_cond_timedwait(c, m, &ts);
}

typedef void *(*nc_thread_func_t)(void *);

static inline int nc_thread_create(nc_thread_t *t, nc_thread_func_t func, void *arg) {
    return pthread_create(t, NULL, func, arg);
}
static inline int nc_thread_join(nc_thread_t t) {
    return pthread_join(t, NULL);
}
static inline int nc_thread_detach(nc_thread_t t) {
    return pthread_detach(t);
}

#define nc_thread_local __thread

#endif

/* ═══════════════════════════════════════════════════════════
 *  Atomic counter for thread-safe worker counting
 * ═══════════════════════════════════════════════════════════ */

#ifdef NC_WINDOWS
typedef volatile LONG nc_atomic_int;
#define nc_atomic_inc(p)  InterlockedIncrement(p)
#define nc_atomic_dec(p)  InterlockedDecrement(p)
#define nc_atomic_load(p) (*(p))
#define nc_atomic_store(p, v) InterlockedExchange(p, v)
#else
#include <stdatomic.h>
typedef atomic_int nc_atomic_int;
#define nc_atomic_inc(p)  (atomic_fetch_add(p, 1) + 1)
#define nc_atomic_dec(p)  (atomic_fetch_sub(p, 1) - 1)
#define nc_atomic_load(p) atomic_load(p)
#define nc_atomic_store(p, v) atomic_store(p, v)
#endif

/* ═══════════════════════════════════════════════════════════
 *  Time functions
 * ═══════════════════════════════════════════════════════════ */

static inline void nc_sleep_ms(int ms) {
#ifdef NC_WINDOWS
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static inline double nc_clock_ms(void) {
#ifdef NC_WINDOWS
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart * 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

static inline double nc_realtime_ms(void) {
#ifdef NC_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (double)(t / 10000ULL) - 11644473600000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  String comparison (case-insensitive)
 * ═══════════════════════════════════════════════════════════ */

#ifdef NC_WINDOWS
#  ifndef strcasecmp
#    define strcasecmp   _stricmp
#  endif
#  ifndef strncasecmp
#    define strncasecmp  _strnicmp
#  endif
#endif

/* strtok_r: MSVC uses strtok_s with the same signature */
#if defined(NC_WINDOWS) && defined(_MSC_VER)
#  define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#endif

/* strdup: POSIX but not C11. MSVC provides _strdup. MinGW has strdup. */
#if defined(_MSC_VER) && !defined(strdup)
#  define strdup _strdup
#endif

/* ═══════════════════════════════════════════════════════════
 *  Filesystem abstraction
 * ═══════════════════════════════════════════════════════════ */

static inline int nc_mkdir(const char *path) {
#ifdef NC_WINDOWS
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

/*
 * nc_mkdir_p — recursively create nested directories (like `mkdir -p`).
 *
 * Handles both '/' and '\\' as path separators so it works on Windows
 * and POSIX.  Normalizes the path to use the platform-native separator
 * before iterating through components.
 *
 * Returns 0 on success, -1 on failure (errno is set).
 */
static inline int nc_mkdir_p(const char *path) {
    if (!path || !path[0]) return -1;

    size_t len = strlen(path);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return -1;
    memcpy(buf, path, len + 1);

    /* Normalize separators to platform-native */
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '/' || buf[i] == '\\')
            buf[i] = NC_PATH_SEP;
    }

    /* Remove trailing separator (unless it's the root "/") */
    while (len > 1 && buf[len - 1] == NC_PATH_SEP) {
        buf[--len] = '\0';
    }

    /* Walk forward, creating each level */
    for (size_t i = 1; i <= len; i++) {
        if (buf[i] == NC_PATH_SEP || buf[i] == '\0') {
            char saved = buf[i];
            buf[i] = '\0';

            struct stat st;
            if (stat(buf, &st) != 0) {
                /* Directory does not exist — create it */
                if (nc_mkdir(buf) != 0 && errno != EEXIST) {
                    free(buf);
                    return -1;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                /* Path component exists but is not a directory */
                free(buf);
                errno = ENOTDIR;
                return -1;
            }

            buf[i] = saved;
        }
    }

    free(buf);
    return 0;
}

static inline const char *nc_tempdir(void) {
#ifdef NC_WINDOWS
    static char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    return buf;
#else
    const char *tmp = getenv("TMPDIR");
    if (tmp && tmp[0]) return tmp;
    return "/tmp";
#endif
}

static inline char *nc_realpath(const char *path, char *resolved) {
#ifdef NC_WINDOWS
    return _fullpath(resolved, path, _MAX_PATH);
#else
    return realpath(path, resolved);
#endif
}

/* Directory iteration */
typedef struct {
#ifdef NC_WINDOWS
    HANDLE          handle;
    WIN32_FIND_DATAA data;
    bool            first;
    bool            done;
    char            pattern[MAX_PATH];
#else
    DIR            *dir;
#endif
} nc_dir_t;

typedef struct {
    const char *name;
    bool        is_dir;
} nc_dirent_t;

static inline nc_dir_t *nc_opendir(const char *path) {
    nc_dir_t *d = calloc(1, sizeof(nc_dir_t));
    if (!d) return NULL;
#ifdef NC_WINDOWS
    snprintf(d->pattern, MAX_PATH, "%s\\*", path);
    d->handle = FindFirstFileA(d->pattern, &d->data);
    if (d->handle == INVALID_HANDLE_VALUE) { free(d); return NULL; }
    d->first = true;
    d->done = false;
#else
    d->dir = opendir(path);
    if (!d->dir) { free(d); return NULL; }
#endif
    return d;
}

static inline bool nc_readdir(nc_dir_t *d, nc_dirent_t *entry) {
    if (!d) return false;
#ifdef NC_WINDOWS
    if (d->done) return false;
    if (d->first) {
        d->first = false;
    } else {
        if (!FindNextFileA(d->handle, &d->data)) {
            d->done = true;
            return false;
        }
    }
    entry->name = d->data.cFileName;
    entry->is_dir = (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
#else
    struct dirent *e = readdir(d->dir);
    if (!e) return false;
    entry->name = e->d_name;
    entry->is_dir = (e->d_type == DT_DIR);
    return true;
#endif
}

static inline void nc_closedir(nc_dir_t *d) {
    if (!d) return;
#ifdef NC_WINDOWS
    if (d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
#else
    if (d->dir) closedir(d->dir);
#endif
    free(d);
}

/* ═══════════════════════════════════════════════════════════
 *  Process / environment abstraction
 * ═══════════════════════════════════════════════════════════ */

static inline int nc_setenv(const char *name, const char *value, int overwrite) {
#ifdef NC_WINDOWS
    if (!overwrite) {
        char buf[1];
        if (GetEnvironmentVariableA(name, buf, sizeof(buf)) > 0)
            return 0;
    }
    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

static inline int nc_unsetenv(const char *name) {
#ifdef NC_WINDOWS
    return _putenv_s(name, "");
#else
    return unsetenv(name);
#endif
}

static inline int nc_getpid(void) {
#ifdef NC_WINDOWS
    return (int)_getpid();
#else
    return (int)getpid();
#endif
}

static inline bool nc_isatty(int fd) {
#ifdef NC_WINDOWS
    return _isatty(fd) != 0;
#else
    return isatty(fd) != 0;
#endif
}

static inline int nc_fileno(FILE *f) {
#ifdef NC_WINDOWS
    return _fileno(f);
#else
    return fileno(f);
#endif
}

/* popen / pclose */
static inline FILE *nc_popen(const char *cmd, const char *mode) {
#ifdef NC_WINDOWS
    return _popen(cmd, mode);
#else
    return popen(cmd, mode);
#endif
}

static inline int nc_pclose(FILE *f) {
#ifdef NC_WINDOWS
    return _pclose(f);
#else
    int status = pclose(f);
#ifdef WIFEXITED
    if (WIFEXITED(status)) return WEXITSTATUS(status);
#endif
    return status;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Signal handling
 * ═══════════════════════════════════════════════════════════ */

#ifdef NC_WINDOWS
#  ifndef SIGPIPE
#    define SIGPIPE 13
#  endif
#  ifndef SIGCHLD
#    define SIGCHLD 17
#  endif
#  ifndef SIGALRM
#    define SIGALRM 14
#  endif
static inline unsigned nc_alarm(unsigned seconds) { (void)seconds; return 0; }
#else
#  define nc_alarm(s) alarm(s)
#endif

/* ═══════════════════════════════════════════════════════════
 *  Memory mapping (for JIT)
 * ═══════════════════════════════════════════════════════════ */

#ifndef NC_WINDOWS
#include <sys/mman.h>
#endif

static inline void *nc_mmap_exec(size_t size) {
#ifdef NC_WINDOWS
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                        PAGE_EXECUTE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#endif
}

static inline void nc_munmap_exec(void *ptr, size_t size) {
#ifdef NC_WINDOWS
    VirtualFree(ptr, 0, MEM_RELEASE);
    (void)size;
#else
    munmap(ptr, size);
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  CPU count detection
 * ═══════════════════════════════════════════════════════════ */

static inline int nc_cpu_count(void) {
#ifdef NC_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    int n = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? n : 4;
#else
    return 4;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Cross-platform path separator lookup
 *
 *  On Windows paths can use both '/' and '\\', so we check
 *  for whichever appears last. On POSIX only '/' is valid.
 * ═══════════════════════════════════════════════════════════ */

static inline char *nc_last_path_sep(const char *path) {
    char *fwd = strrchr(path, '/');
#ifdef NC_WINDOWS
    char *bck = strrchr(path, '\\');
    if (!fwd) return bck;
    if (!bck) return fwd;
    return (bck > fwd) ? bck : fwd;
#else
    return fwd;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Terminal color support
 *
 *  On Windows, ANSI escape codes only work if Virtual Terminal
 *  Processing is enabled (Windows 10 1607+). On older Windows
 *  or when NO_COLOR env var is set, colors are disabled.
 *
 *  Call nc_colors_init() once at startup. Then use NC_COL()
 *  which returns either the ANSI code or "" depending on
 *  terminal support.
 * ═══════════════════════════════════════════════════════════ */

static int nc_colors_active = -1;  /* -1 = uninitialized */
static int nc_utf8_active   = -1;  /* -1 = uninitialized */

static inline void nc_colors_init(void) {
    /* Respect NO_COLOR convention (https://no-color.org) */
    if (getenv("NO_COLOR")) { nc_colors_active = 0; nc_utf8_active = 1; return; }

    /* If not a TTY (piped/redirected), disable colors but keep UTF-8 */
    if (!nc_isatty(nc_fileno(stdout))) { nc_colors_active = 0; nc_utf8_active = 1; return; }

#ifdef NC_WINDOWS
    /* Enable ANSI/VT processing on Windows 10 1607+ */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) { nc_colors_active = 0; nc_utf8_active = 0; return; }
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) { nc_colors_active = 0; nc_utf8_active = 0; return; }
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
    if (SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        nc_colors_active = 1;
    } else {
        nc_colors_active = 0;
    }
    /* Try to enable UTF-8 output */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    nc_utf8_active = (GetConsoleOutputCP() == 65001) ? 1 : 0;
#else
    nc_colors_active = 1;
    nc_utf8_active = 1;
#endif
}

/*
 * nc_strip_ansi — remove ANSI escape sequences from a string.
 * Returns a malloc'd string. Caller must free.
 */
static inline char *nc_strip_ansi(const char *s) {
    int len = (int)strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] == '\033' && i + 1 < len && s[i+1] == '[') {
            i += 2;
            while (i < len && s[i] != 'm') i++;
            continue;
        }
        out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

/*
 * nc_utf8_to_ascii — replace UTF-8 box-drawing and special characters
 * with ASCII equivalents so output looks clean on Windows consoles
 * that don't support UTF-8 (codepage != 65001).
 *
 * Replaces: ═ ║ ╔ ╗ ╚ ╝ ╠ ╣ ╤ ╧ ─ │ ┌ ┐ └ ┘ ├ ┤ → ✓ ✗ • ●
 * Returns a malloc'd string. Caller must free.
 */
static inline char *nc_utf8_to_ascii(const char *s) {
    int len = (int)strlen(s);
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; i < len; ) {
        unsigned char c = (unsigned char)s[i];
        /* 3-byte UTF-8 sequences (U+2500-U+27BF) */
        if (c == 0xE2 && i + 2 < len) {
            unsigned char b2 = (unsigned char)s[i+1];
            unsigned char b3 = (unsigned char)s[i+2];
            if (b2 == 0x94) {
                /* U+2500 block: box drawing */
                if (b3 == 0x80) { out[j++] = '-'; i += 3; continue; }       /* ─ */
                if (b3 == 0x82) { out[j++] = '|'; i += 3; continue; }       /* │ */
                if (b3 == 0x8C) { out[j++] = '+'; i += 3; continue; }       /* ┌ */
                if (b3 == 0x90) { out[j++] = '+'; i += 3; continue; }       /* ┐ */
                if (b3 == 0x94) { out[j++] = '+'; i += 3; continue; }       /* └ */
                if (b3 == 0x98) { out[j++] = '+'; i += 3; continue; }       /* ┘ */
                if (b3 == 0x9C) { out[j++] = '+'; i += 3; continue; }       /* ├ */
                if (b3 == 0xA4) { out[j++] = '+'; i += 3; continue; }       /* ┤ */
                if (b3 == 0xAC) { out[j++] = '+'; i += 3; continue; }       /* ┬ */
                if (b3 == 0xB4) { out[j++] = '+'; i += 3; continue; }       /* ┴ */
                if (b3 == 0xBC) { out[j++] = '+'; i += 3; continue; }       /* ┼ */
                /* skip unknown box drawing */
                out[j++] = '+'; i += 3; continue;
            }
            if (b2 == 0x95) {
                /* U+2550 block: double box drawing */
                if (b3 == 0x90) { out[j++] = '='; i += 3; continue; }       /* ═ */
                if (b3 == 0x91) { out[j++] = '|'; i += 3; continue; }       /* ║ */
                if (b3 == 0x94) { out[j++] = '+'; i += 3; continue; }       /* ╔ */
                if (b3 == 0x97) { out[j++] = '+'; i += 3; continue; }       /* ╗ */
                if (b3 == 0x9A) { out[j++] = '+'; i += 3; continue; }       /* ╚ */
                if (b3 == 0x9D) { out[j++] = '+'; i += 3; continue; }       /* ╝ */
                if (b3 == 0xA0) { out[j++] = '+'; i += 3; continue; }       /* ╠ */
                if (b3 == 0xA3) { out[j++] = '+'; i += 3; continue; }       /* ╣ */
                if (b3 == 0xA6) { out[j++] = '='; i += 3; continue; }       /* ╦ */
                if (b3 == 0xA9) { out[j++] = '='; i += 3; continue; }       /* ╩ */
                out[j++] = '+'; i += 3; continue;
            }
            if (b2 == 0x96) {
                if (b3 == 0xB6) { out[j++] = '*'; i += 3; continue; }       /* ▶ */
                out[j++] = ' '; i += 3; continue;
            }
            if (b2 == 0x86) {
                if (b3 == 0x92) { out[j++] = '-'; out[j++] = '>'; i += 3; continue; } /* → */
                out[j++] = ' '; i += 3; continue;
            }
            if (b2 == 0x9C) {
                if (b3 == 0x93) { out[j++] = '*'; i += 3; continue; }       /* ✓ */
                if (b3 == 0x97) { out[j++] = 'X'; i += 3; continue; }       /* ✗ */
                out[j++] = ' '; i += 3; continue;
            }
            if (b2 == 0x80) {
                if (b3 == 0xA2) { out[j++] = '*'; i += 3; continue; }       /* • */
                out[j++] = ' '; i += 3; continue;
            }
            /* Other 3-byte UTF-8 — pass through or replace */
            out[j++] = s[i]; out[j++] = s[i+1]; out[j++] = s[i+2];
            i += 3; continue;
        }
        /* 2-byte UTF-8 (U+0080-U+07FF) — mostly pass through */
        if (c >= 0xC0 && c < 0xE0 && i + 1 < len) {
            unsigned char b2 = (unsigned char)s[i+1];
            /* U+00B7 middle dot → * */
            if (c == 0xC2 && b2 == 0xB7) { out[j++] = '*'; i += 2; continue; }
            out[j++] = s[i]; out[j++] = s[i+1];
            i += 2; continue;
        }
        /* 4-byte UTF-8 — pass through */
        if (c >= 0xF0 && i + 3 < len) {
            out[j++] = s[i]; out[j++] = s[i+1]; out[j++] = s[i+2]; out[j++] = s[i+3];
            i += 4; continue;
        }
        /* ASCII byte — copy as-is */
        out[j++] = s[i];
        i++;
    }
    out[j] = '\0';
    return out;
}

/*
 * nc_printf — color-aware, encoding-aware printf.
 *
 * Handles three scenarios:
 *   1. Full support (POSIX / modern Windows Terminal): pass through
 *   2. Colors work but UTF-8 doesn't (Windows CMD): strip box-drawing chars
 *   3. Nothing works (old Windows / piped): strip both ANSI and UTF-8
 */
static inline void nc_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (nc_colors_active < 0) nc_colors_init();

    if (nc_colors_active && nc_utf8_active) {
        vprintf(fmt, args);
    } else {
        char buf[8192];
        vsnprintf(buf, sizeof(buf), fmt, args);
        char *step1 = buf;
        char *alloc1 = NULL;
        if (!nc_colors_active) {
            alloc1 = nc_strip_ansi(buf);
            if (alloc1) step1 = alloc1;
        }
        if (!nc_utf8_active) {
            char *alloc2 = nc_utf8_to_ascii(step1);
            if (alloc2) { fputs(alloc2, stdout); free(alloc2); }
            else fputs(step1, stdout);
        } else {
            fputs(step1, stdout);
        }
        free(alloc1);
    }
    va_end(args);
}

#endif /* NC_PLATFORM_H */
