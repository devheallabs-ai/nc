// NC UI CLI
// Minimal NC-native wrapper around the deterministic compiler services

service "nc-ui-cli"
version "1.1.0"

to compile_source with source:
    fetch "http://localhost:7600/compile" method "POST" body {"source": source} save as result
    respond with result

to compile_file_path with file_path:
    gather source from file_path
    if source is equal nil:
        respond with {"error": "File not found"}
    run compile_source with source
    respond with result

to cmd_build with file_path, output_path:
    run compile_file_path with file_path
    if result.error:
        respond with {"success": false, "error": result.error}

    set out_path to output_path
    if out_path is equal nil or out_path is equal "":
        set out_path to replace(file_path, ".ncui", ".html")

    store result.html into out_path
    respond with {"success": true, "output": out_path, "size": len(result.html)}

to cmd_check with file_path:
    gather source from file_path
    if source is equal nil:
        respond with {"valid": false, "errors": ["File not found"]}
    fetch "http://localhost:7600/validate" method "POST" body {"source": source} save as result
    respond with result

to cmd_tokens with file_path:
    gather source from file_path
    if source is equal nil:
        respond with {"valid": false, "error": "File not found"}
    fetch "http://localhost:7600/tokens" method "POST" body {"source": source} save as result
    respond with result

to cmd_ast with file_path:
    gather source from file_path
    if source is equal nil:
        respond with {"valid": false, "error": "File not found"}
    fetch "http://localhost:7600/ast" method "POST" body {"source": source} save as result
    respond with result

to cmd_build_web with file_path, out_dir:
    fetch "http://localhost:7605/github" method "POST" body {"ncui_file_path": file_path, "out_dir": out_dir} save as result
    respond with result

to cmd_serve with file_path, port:
    set serve_port to port
    if serve_port is equal nil or serve_port is equal "":
        set serve_port to 3000

    run cmd_build with file_path, "./.ncui_serve_temp.html"
    if result.error:
        respond with {"success": false, "error": result.error}

    set serve_service to "service \"ncui-dev\"\nconfigure:\n    port is " + string(serve_port) + "\n\nto serve:\n    gather content from \"./.ncui_serve_temp.html\"\n    respond with content\n\napi:\n    GET / runs serve\n    GET /* runs serve"
    store serve_service into "./.ncui_dev_server.nc"
    set shell_result to exec("nc", "serve", "./.ncui_dev_server.nc")
    respond with {"success": true, "port": serve_port}

to health:
    respond with {"status": "ok", "service": "nc-ui-cli", "version": "3.2.0"}
