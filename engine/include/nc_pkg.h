/*
 * nc_pkg.h — Package manager declarations for NC.
 *
 * Provides registry client, dependency resolver, local package management,
 * manifest/lock file handling, and publishing support.
 */

#ifndef NC_PKG_H
#define NC_PKG_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  Registry and paths
 * ═══════════════════════════════════════════════════════════ */

#define NC_REGISTRY_URL       "https://registry.nc-lang.dev/api/v1"
#define NC_MODULES_DIR        "nc_modules"
#define NC_MANIFEST_FILE      "nc_package.json"
#define NC_LOCK_FILE          "nc_package.lock"

/* ═══════════════════════════════════════════════════════════
 *  NcPackage — registry package metadata
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char name[128];
    char version[32];
    char description[256];
    char author[128];
    char url[512];              /* download URL */
    char checksum[65];          /* SHA-256 hex */
    int  dependencies_count;
    char dependencies[32][128]; /* dep_name@version */
} NcPackageInfo;

/* ═══════════════════════════════════════════════════════════
 *  NcManifest — nc_package.json representation
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char name[128];
    char version[32];
    char description[256];
    char entry[256];            /* main .nc file */
    int  dep_count;
    char dep_names[64][128];
    char dep_versions[64][32];
} NcManifest;

/* ═══════════════════════════════════════════════════════════
 *  Version constraint types
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_VER_EXACT,       /* =1.2.3    exact match */
    NC_VER_CARET,       /* ^1.2.3    compatible (same major) */
    NC_VER_TILDE,       /* ~1.2.3    patch-only (same major.minor) */
    NC_VER_GTE,         /* >=1.2.3   greater or equal */
    NC_VER_LT,          /* <2.0.0    less than */
    NC_VER_RANGE,       /* >=1.0 <2.0 */
    NC_VER_ANY          /* *         any version */
} NcVersionConstraintType;

typedef struct {
    int major;
    int minor;
    int patch;
} NcSemVer;

typedef struct {
    NcVersionConstraintType type;
    NcSemVer lower;     /* for range: lower bound */
    NcSemVer upper;     /* for range: upper bound (exclusive) */
} NcVersionConstraint;

/* ═══════════════════════════════════════════════════════════
 *  Registry client
 * ═══════════════════════════════════════════════════════════ */

NcPackageInfo *nc_pkg_search(const char *query, int *count);
NcPackageInfo *nc_pkg_info(const char *name);
int            nc_pkg_download(const char *name, const char *version,
                               const char *dest);

/* ═══════════════════════════════════════════════════════════
 *  Dependency resolver
 * ═══════════════════════════════════════════════════════════ */

int  nc_pkg_resolve(NcPackageInfo *pkg, NcPackageInfo **resolved, int *count);

/* ═══════════════════════════════════════════════════════════
 *  Local package management
 * ═══════════════════════════════════════════════════════════ */

int  nc_pkg_install_v2(const char *name, const char *version);
int  nc_pkg_uninstall(const char *name);
int  nc_pkg_update_v2(const char *name);
int  nc_pkg_list_v2(void);

/* ═══════════════════════════════════════════════════════════
 *  Package manifest (nc_package.json)
 * ═══════════════════════════════════════════════════════════ */

NcManifest *nc_pkg_read_manifest(const char *dir);
int         nc_pkg_write_manifest(const char *dir, NcManifest *m);
int         nc_pkg_init_manifest(const char *dir);

/* ═══════════════════════════════════════════════════════════
 *  Lock file (nc_package.lock)
 * ═══════════════════════════════════════════════════════════ */

int  nc_pkg_lock_write(const char *dir, NcPackageInfo *pkgs, int count);
int  nc_pkg_lock_read(const char *dir, NcPackageInfo **pkgs, int *count);

/* ═══════════════════════════════════════════════════════════
 *  Publishing
 * ═══════════════════════════════════════════════════════════ */

int  nc_pkg_pack(const char *dir, const char *output);
int  nc_pkg_publish(const char *dir);

/* ═══════════════════════════════════════════════════════════
 *  Version utilities
 * ═══════════════════════════════════════════════════════════ */

int  nc_semver_parse(const char *str, NcSemVer *out);
int  nc_semver_compare(NcSemVer a, NcSemVer b);
int  nc_version_constraint_parse(const char *str, NcVersionConstraint *out);
bool nc_version_satisfies(NcSemVer ver, NcVersionConstraint constraint);

/* ═══════════════════════════════════════════════════════════
 *  CLI command router (existing)
 * ═══════════════════════════════════════════════════════════ */

int  nc_pkg_command(int argc, char *argv[]);

#endif /* NC_PKG_H */
