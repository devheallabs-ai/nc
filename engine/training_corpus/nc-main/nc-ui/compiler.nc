// NC UI Compiler Service
// Native compilation — no Node.js dependency
// Uses the built-in `nc ui build` engine (nc_ui_html.c)

service "nc-ui-compiler"
version "2.0.0"

configure:
    port is 7600

to compile_file with path:
    if path is equal nil or path is equal "":
        respond with {"error": "Missing path", "html": "", "valid": false}
    // Uses native NC engine: `nc ui build <path> -o <dir>`
    set result to exec("nc", "ui", "build", path, "-o", ".")
    respond with {"status": "ok", "path": path, "output": result}

to health:
    respond with {"status": "ok", "service": "nc-ui-compiler", "version": "2.0.0", "target": "native-c-engine"}

api:
    POST /compile/file runs compile_file
    GET /health        runs health
