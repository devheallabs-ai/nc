// NC UI Static Export Service
// Deterministic export wrapper around compiler_bridge.js

service "nc-ui-static-export"
version "3.2.0"

configure:
    port is 7605

to emit_html with ncui_file_path:
    if ncui_file_path is equal nil or ncui_file_path is equal "":
        respond with {"success": false, "error": "Missing file path"}

    set compile_result to exec("node", "./nc-ui/compiler_bridge.js", "compile-file", ncui_file_path)
    if compile_result.error:
        respond with {"success": false, "error": compile_result.error}

    set out_path to replace(ncui_file_path, ".ncui", ".html")
    store compile_result.html into out_path
    respond with {"success": true, "output": out_path, "size": len(compile_result.html), "warnings": compile_result.warnings}

to prepare_github_pages with ncui_file_path, out_dir:
    if ncui_file_path is equal nil or ncui_file_path is equal "":
        respond with {"success": false, "error": "Missing file path"}

    set dir to out_dir
    if dir is equal nil or dir is equal "":
        set dir to "./dist"

    set compile_result to exec("node", "./nc-ui/compiler_bridge.js", "compile-file", ncui_file_path)
    if compile_result.error:
        respond with {"success": false, "error": compile_result.error}

    store compile_result.html into dir + "/index.html"
    respond with {"success": true, "dir": dir, "warnings": compile_result.warnings}

to health:
    respond with {"status": "ok", "service": "nc-ui-static-export", "version": "3.2.0", "target": "static-html"}

api:
    POST /emit   runs emit_html
    POST /github runs prepare_github_pages
    GET /health  runs health
