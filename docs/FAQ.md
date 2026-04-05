
  


# NC — Frequently Asked Questions

## General

### What is NC?
NC is a programming language where you write code in plain English. It's designed for AI platforms — AI operations like "ask AI to analyze" are built into the language itself.

### Why another programming language?
Most languages treat AI as a library. NC treats AI as a **language primitive**. Instead of `response = await litellm.acompletion(model="nova", messages=[...])`, you write `ask AI to "analyze this" using data`.

### What is NC written in?
The NC runtime is a single 570KB binary with zero runtime dependencies.

### Does NC replace Python?
No. NC is designed for a specific use case: defining services, AI workflows, and infrastructure behaviors in plain English. Use Python for data science notebooks, NC for service definitions.

### Is NC production-ready?
Yes. NC v1.0.0 is a stable release with a full runtime (lexer, parser, bytecode compiler, VM, JIT), 85 language test files (947 behaviors), 485 unit assertions, 112 Python integration tests, cross-platform support (Linux, macOS, Windows — MinGW + MSVC), 21 security fixes, 15 bug fixes, and Docker images. It is ready for production AI workflows and HTTP services.

## Language

### What does NC code look like?
```
service "my-app"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name

api:
    GET /greet runs greet
```

### What types does NC have?
| NC Type | Description | Example |
|---------|-------------|---------|
| text | string | "hello" |
| number | integer or float | 42, 3.14 |
| yesno | boolean | yes, no, true, false |
| list | ordered collection | [1, 2, 3] |
| record | key-value map | (from gather/ask AI) |
| nothing | null | nothing, none |

### How do I do if/else?
```
if score is above 90:
    respond with "excellent"
otherwise:
    respond with "ok"
```

### How do I do loops?
```
repeat for each item in items:
    log item
```

### How do I call AI?
```
ask AI to "classify this ticket" using ticket_data:
    confidence: 0.8
    save as: classification
```

### How do I handle errors?
```
try:
    gather data from risky_source
on error:
    log "something went wrong"
    respond with "error"
```

## Building & Running

### How do I build NC?
```bash
cd nc
make
```

### How do I run a .nc file?
```bash
nc run myfile.nc
nc run myfile.nc -b my_behavior
```

### How do I test NC?
```bash
make test-unit     # 485 unit assertions
./build/nc test    # 85 language tests (947 behaviors)
make test          # integration tests
nc conformance    # language conformance suite
```

### Can NC make real AI calls?
Yes, if you build with libcurl support:
```bash
make with-curl
```
Then set your AI provider credentials as environment variables:
```bash
export NC_AI_URL="<your-provider-endpoint>"
export NC_AI_KEY="<your-api-key>"
export NC_AI_MODEL="<your-model-name>"
```
Without libcurl, AI calls return simulated responses.

### How do I connect NC to a AI proxy proxy or AI-provider-compatible gateway?

Set `NC_AI_URL` to your proxy endpoint. NC uses the standard `/chat/completions` request format by default, so it works with any AI-provider-compatible proxy:

```bash
export NC_AI_URL="<your-proxy-url>/chat/completions"
export NC_AI_KEY="<your-proxy-api-key>"
export NC_AI_MODEL="nova"
```

Your NC code does not change — `ask AI` works the same regardless of whether the endpoint is NC AI locally, an AI proxy, local AI server, or any gateway, or any other gateway.

You can also configure this in the NC file itself:
```
configure:
    ai_url is env:PROXY_URL
    ai_key is env:PROXY_API_KEY
    ai_model is "nova"
```

The `env:` prefix reads the value from environment variables at runtime, so credentials never appear in source code.

See [AI proxy Proxy documentation](https://docs.litellm.ai/docs/proxy/user_keys) for setting up a proxy.

### Where do I put my API key?

Never put API keys in source code. Use one of these approaches:

1. **Environment variables** — `export NC_AI_KEY="<YOUR_API_KEY>"` in your shell
2. **Configure block with env: prefix** — `ai_key is env:YOUR_KEY_VAR` in your .nc file
3. **CI/CD secrets** — set NC_AI_KEY in your pipeline configuration
4. **Container environment** — pass via docker run `-e NC_AI_KEY=...`

NC automatically redacts API keys from log output and error messages.

### Can NC convert Python/JS code?
Yes:
```bash
nc digest app.py       # Python → NC
nc digest server.js    # JavaScript → NC
nc digest config.yaml  # YAML → NC
```

**Note:** `nc digest` converts most constructs automatically, but `for` loops in Python/JS are emitted as `// TODO: convert loop` comments that need manual conversion to NC's `repeat for each` syntax. Use `nc migrate` (AI-powered) for smarter loop conversion.

## Technical

### How does NC execute code?
```
Source (.nc) → Lexer → Parser → AST → Bytecode Compiler → VM
```
The VM is a stack-based machine with 48 opcodes, purpose-built for NC.

### Does NC have a garbage collector?
Yes — mark & sweep, same algorithm as Go and Lua.

### Does NC support concurrency?
Yes — coroutines, event loop, and worker threads (via pthreads).

### Can NC compile to native code?
NC can generate LLVM IR (`nc compile file.nc`). With LLVM installed, you can compile to native machine code.

### Does NC work in my IDE?
A VS Code extension is included in `editor/vscode/`. It provides syntax highlighting for `.nc` files. The LSP server (`nc lsp`) provides autocomplete and diagnostics.

### Does NC have auto-correct?
Yes. NC uses Damerau-Levenshtein distance to suggest corrections for typos. If you write `pritn` instead of `print`, NC says: `Is this what you meant → 'print'?`. This works for keywords, variables, and function names — integrated into both the parser and interpreter.

### Can I use Python/JavaScript syntax in NC?
Yes. NC has a synonym engine with 39 cross-language keyword mappings. Write `def` and NC reads it as `to`. Write `return` and NC reads it as `respond with`. Write `for item in items:` and NC reads it as `repeat for each item in items:`. Mappings cover Python, JavaScript, Go, Rust, Ruby, Java, C, and Swift. Enable `NC_SYNONYM_NOTICES=1` to see what NC translates.

### Does NC show error tracebacks?
Yes. When an error occurs, NC shows the full call chain with behavior names, file, and line numbers — similar to Python tracebacks.

### How does the AI engine work internally?
NC's C runtime has zero knowledge of any AI company or API format. All format knowledge lives in `nc_ai_providers.json` — a template engine fills `{{placeholders}}` in request JSON, and a path extractor navigates response JSON by dot-path. Switch AI providers by editing JSON, no recompile needed.

### Can NC convert my existing Python/JavaScript code?
Yes. `nc digest app.py` converts Python, JavaScript, TypeScript, YAML, and JSON to NC using pattern matching (offline, no AI needed). `nc migrate app.py` uses AI for smarter conversion. Use `--dry-run` to preview changes, `--hybrid` to wrap code instead of converting it.

### Does NC have a debugger?
Yes. `nc debug app.nc` starts a step-through debugger. Commands: `s` (step), `n` (next), `c` (continue), `b <line>` (breakpoint), `p <var>` (print variable), `vars` (show all variables), `bt` (backtrace). `nc debug --dap` starts a DAP server for IDE integration.

### Does NC have a profiler?
Yes. `nc profile app.nc` profiles execution and identifies bottlenecks. The bytecode optimizer performs constant folding, dead store elimination, and jump threading automatically.

### Can I call one behavior from another?
Use `run helper` to execute another behavior. NC services are designed as **single-file APIs** — keep all your logic in behaviors within the same file. Each behavior handles one API route.

### Does `import` work for multi-file projects?
`import "filename"` works for loading standard library modules (`json`, `csv`, `math`, etc.). NC is designed for **single-file services** — one `.nc` file = one complete API. This keeps deployments simple (one file to copy, one file to review).

### Does `define` enforce types?
`define User as: age is number` documents the data shape for readability. For runtime validation, use `if` checks: `if data.age is below 0: respond with {"error": "invalid age"}`. This gives you full control over error messages.

### Does NC support inheritance?
NC uses composition over inheritance. Define each type independently and compose them in your behaviors. NC is not an OOP language — it's a plain-English API language.

### How do I troubleshoot my setup?
Run `nc doctor` — it checks your .env file, AI provider config, service file syntax, CORS, JWT, and port settings. It tells you exactly what's wrong and how to fix it.

### Are my API keys safe?
Never hardcode API keys in `.nc` files. Use `.env` files instead:
- Put keys in `.env` (auto-loaded by NC)
- `.gitignore` excludes `.env` by default (created by `nc init`)
- Use `env:NC_AI_KEY` in configure blocks
- Server logs do NOT include request bodies or auth headers
- `nc doctor` warns if your key is still a placeholder

### Does NC have build logs?
`nc validate` shows syntax check results. `nc serve` logs each request (method, path, response time, trace ID). Enable JSON logs with `NC_LOG_FORMAT=json` for structured logging. Enable OpenTelemetry tracing with `NC_OTEL_ENDPOINT`.
