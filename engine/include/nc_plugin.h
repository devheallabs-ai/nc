/*
 * nc_plugin.h — Plugin system for NC (Foreign Function Interface).
 *
 * Allows NC to dynamically load .so (Linux/macOS) or .dll (Windows)
 * libraries at runtime, enabling user-created extensions without
 * recompiling the NC binary.
 *
 * Plugin authors implement nc_plugin_init() which registers functions
 * that NC scripts can then call like any built-in function.
 *
 * Usage in NC:
 *   import plugin "my_crypto_lib"
 *   set encrypted to aes_encrypt(data, key)
 *
 * Plugin C API:
 *   #include "nc_plugin.h"
 *   NC_PLUGIN_EXPORT void nc_plugin_init(NcPluginAPI *api) {
 *       api->register_func("aes_encrypt", my_aes_encrypt);
 *   }
 */

#ifndef NC_PLUGIN_H
#define NC_PLUGIN_H

#include "nc_value.h"

typedef NcValue (*NcPluginFunc)(int argc, NcValue *args);

typedef struct {
    void (*register_func)(const char *name, NcPluginFunc func);
    const char *(*get_env)(const char *name);
    NcValue (*json_parse)(const char *json);
    char *(*json_serialize)(NcValue val);
    NcString *(*string_new)(const char *chars, int length);
    NcString *(*string_from_cstr)(const char *cstr);
    NcList *(*list_new)(void);
    void (*list_push)(NcList *l, NcValue v);
    NcMap *(*map_new)(void);
    void (*map_set)(NcMap *m, NcString *key, NcValue val);
    NcValue (*map_get)(NcMap *m, NcString *key);
} NcPluginAPI;

typedef void (*NcPluginInitFunc)(NcPluginAPI *api);

#ifdef _WIN32
#  define NC_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define NC_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

int     nc_plugin_load(const char *path);
NcValue nc_plugin_call(const char *func_name, int argc, NcValue *args);
bool    nc_plugin_has(const char *func_name);
void    nc_plugin_unload_all(void);

#endif /* NC_PLUGIN_H */
