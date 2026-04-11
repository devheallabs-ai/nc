/*
 * nc_plugin.c — Plugin system (FFI) for NC.
 *
 * Dynamically loads shared libraries (.so/.dll/.dylib) and maps
 * their exported functions into the NC runtime. This allows users
 * to extend NC without recompiling the binary.
 *
 * Plugin interface:
 *   Every plugin exports nc_plugin_init(NcPluginAPI *api).
 *   Inside init, the plugin calls api->register_func("name", func_ptr)
 *   to make functions available to NC scripts.
 */

#include "../include/nc.h"
#include "../include/nc_plugin.h"
#include "../include/nc_platform.h"

#ifdef NC_WINDOWS
#  define NC_DLOPEN(path)       LoadLibraryA(path)
#  define NC_DLSYM(handle, sym) GetProcAddress(handle, sym)
#  define NC_DLCLOSE(handle)    FreeLibrary(handle)
#  define NC_DLERROR()          "LoadLibrary failed"
typedef HMODULE nc_dlhandle_t;
#else
#  include <dlfcn.h>
#  define NC_DLOPEN(path)       dlopen(path, RTLD_NOW)
#  define NC_DLSYM(handle, sym) dlsym(handle, sym)
#  define NC_DLCLOSE(handle)    dlclose(handle)
#  define NC_DLERROR()          dlerror()
typedef void *nc_dlhandle_t;
#endif

#define NC_MAX_PLUGINS     32
#define NC_MAX_PLUGIN_FUNCS 256

typedef struct {
    char           name[128];
    NcPluginFunc   func;
} PluginFuncEntry;

typedef struct {
    nc_dlhandle_t  handle;
    char           path[512];
} LoadedPlugin;

static LoadedPlugin     plugins[NC_MAX_PLUGINS];
static int              plugin_count = 0;
static PluginFuncEntry  plugin_funcs[NC_MAX_PLUGIN_FUNCS];
static int              pfunc_count = 0;

static void plugin_register_func(const char *name, NcPluginFunc func) {
    if (pfunc_count >= NC_MAX_PLUGIN_FUNCS) return;
    strncpy(plugin_funcs[pfunc_count].name, name, 127);
    plugin_funcs[pfunc_count].name[127] = '\0';
    plugin_funcs[pfunc_count].func = func;
    pfunc_count++;
}

static const char *plugin_get_env(const char *name) {
    return getenv(name);
}

static char *plugin_json_serialize(NcValue v) {
    return nc_json_serialize(v, false);
}

static NcPluginAPI plugin_api = {
    .register_func  = plugin_register_func,
    .get_env        = plugin_get_env,
    .json_parse     = nc_json_parse,
    .json_serialize = plugin_json_serialize,
    .string_new     = nc_string_new,
    .string_from_cstr = nc_string_from_cstr,
    .list_new       = nc_list_new,
    .list_push      = nc_list_push,
    .map_new        = nc_map_new,
    .map_set        = nc_map_set,
    .map_get        = nc_map_get,
};

int nc_plugin_load(const char *path) {
    if (plugin_count >= NC_MAX_PLUGINS) {
        fprintf(stderr, "[NC Plugin] Too many plugins loaded (max %d)\n", NC_MAX_PLUGINS);
        return -1;
    }

    /* Try multiple paths: as-is, with .so/.dll suffix, in plugin directories */
    nc_dlhandle_t handle = NC_DLOPEN(path);

    if (!handle) {
        char with_ext[600];
#ifdef NC_WINDOWS
        snprintf(with_ext, sizeof(with_ext), "%s.dll", path);
#elif defined(__APPLE__)
        snprintf(with_ext, sizeof(with_ext), "%s.dylib", path);
#else
        snprintf(with_ext, sizeof(with_ext), "%s.so", path);
#endif
        handle = NC_DLOPEN(with_ext);
    }

    if (!handle) {
        /* Try .nc_packages/plugins/ directory */
        char pkg_path[600];
#ifdef NC_WINDOWS
        snprintf(pkg_path, sizeof(pkg_path), ".nc_packages" NC_PATH_SEP_STR "plugins" NC_PATH_SEP_STR "%s.dll", path);
#elif defined(__APPLE__)
        snprintf(pkg_path, sizeof(pkg_path), ".nc_packages" NC_PATH_SEP_STR "plugins" NC_PATH_SEP_STR "%s.dylib", path);
#else
        snprintf(pkg_path, sizeof(pkg_path), ".nc_packages" NC_PATH_SEP_STR "plugins" NC_PATH_SEP_STR "%s.so", path);
#endif
        handle = NC_DLOPEN(pkg_path);
    }

    if (!handle) {
        fprintf(stderr, "[NC Plugin] Cannot load '%s': %s\n", path, NC_DLERROR());
        return -1;
    }

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    NcPluginInitFunc init_fn = (NcPluginInitFunc)NC_DLSYM(handle, "nc_plugin_init");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (!init_fn) {
        fprintf(stderr, "[NC Plugin] '%s' has no nc_plugin_init() function\n", path);
        NC_DLCLOSE(handle);
        return -1;
    }

    /* Initialize the plugin — it registers its functions via the API */
    int prev_count = pfunc_count;
    init_fn(&plugin_api);
    int registered = pfunc_count - prev_count;

    snprintf(plugins[plugin_count].path, sizeof(plugins[plugin_count].path), "%s", path);
    plugins[plugin_count].handle = handle;
    plugin_count++;

    NC_INFO("Plugin loaded: %s (%d functions registered)", path, registered);
    return 0;
}

NcValue nc_plugin_call(const char *func_name, int argc, NcValue *args) {
    for (int i = 0; i < pfunc_count; i++) {
        if (strcmp(plugin_funcs[i].name, func_name) == 0) {
            return plugin_funcs[i].func(argc, args);
        }
    }
    return NC_NONE();
}

bool nc_plugin_has(const char *func_name) {
    for (int i = 0; i < pfunc_count; i++) {
        if (strcmp(plugin_funcs[i].name, func_name) == 0)
            return true;
    }
    return false;
}

void nc_plugin_unload_all(void) {
    for (int i = 0; i < plugin_count; i++) {
        if (plugins[i].handle) NC_DLCLOSE(plugins[i].handle);
    }
    plugin_count = 0;
    pfunc_count = 0;
}
