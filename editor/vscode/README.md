# NC Language for Visual Studio Code

Full language support for **NC (Notation-as-Code)** -- write AI workflows, API services, and automation in plain English.

<!-- ![NC Extension Screenshot](screenshots/screenshot.png) -->

## What is NC?

NC lets you build AI-powered services in plain English:

```nc
service "email-classifier"
version "1.0.0"

configure:
    ai_model is "nova"

to classify with email_text:
    ask AI to "Classify this email: {{email_text}}" save as result
    respond with result

api:
    POST /classify runs classify
```

**No imports. No boilerplate. Just plain English.**

## Features

- **Syntax Highlighting** -- Full grammar support for all NC keywords, built-in functions, string templates `{{var}}`, comments, numbers, HTTP methods, and more.
- **35+ Snippets** -- Common patterns: services, API routes, AI calls, loops, error handling, CRUD templates, chatbot patterns, and RAG workflows.
- **Hover Documentation** -- Hover over any NC keyword or built-in function to see its description, syntax, and usage example.
- **Run / Serve / Validate** -- Execute NC files directly from VS Code via commands or keyboard shortcuts.
- **REPL** -- Open an interactive NC REPL session in the terminal.
- **Status Bar** -- Displays the detected NC version in the status bar when editing `.nc` files.
- **LSP Support** -- Optional Language Server integration for diagnostics and completions (when available in your NC build).
- **Auto-Validate on Save** -- Optionally validate `.nc` files every time you save.
- **Right-Click Context Menu** -- Run, serve, or validate from the editor context menu.

## Installation

### From VS Code Marketplace

1. Open VS Code.
2. Go to the Extensions view (`Ctrl+Shift+X` / `Cmd+Shift+X`).
3. Search for **NC Language**.
4. Click **Install**.

### From VSIX File

```bash
# Build the VSIX (see "Building" below)
code --install-extension nc-lang-1.0.0.vsix
```

### From Source

```bash
cd nc-lang/editor/vscode
npm install -g @vscode/vsce
vsce package
code --install-extension nc-lang-1.0.0.vsix
```

## Keyboard Shortcuts

| Shortcut | Mac | Command |
|---|---|---|
| `Ctrl+Shift+R` | `Cmd+Shift+R` | Run current `.nc` file |
| `Ctrl+Shift+S` | `Cmd+Shift+S` | Serve current `.nc` file |
| `Ctrl+Shift+V` | `Cmd+Shift+V` | Validate current `.nc` file |

All shortcuts are active only when an `.nc` file is focused.

## Snippets Reference

| Prefix | Description |
|---|---|
| `service` | Full service boilerplate with configure block |
| `to` | Behavior (function) definition |
| `ask` | AI call: `ask AI to "..." save as result` |
| `gather` | Fetch data from URL, DB, or MCP tool |
| `set` | Variable assignment |
| `if` | Conditional block |
| `if otherwise` | If/else block |
| `match` | Pattern matching (switch/case) |
| `repeat` | Loop over a list |
| `while` | While loop |
| `try` | Error handling block |
| `api` | API route definitions |
| `configure` | Configuration block |
| `crud` | Full CRUD service template |
| `middleware` | Middleware configuration |
| `chatbot` | Multi-turn chatbot pattern |
| `rag` | RAG (document Q&A) pattern |
| `classify` | AI classification pattern |
| `memory` | Conversation memory setup |
| `import` | Import a module |
| `define` | Define a data type |
| `store` | Store data to database |
| `respond` | Return a value |
| `log` | Log a message |
| `show` | Display a value |
| `every` | Scheduled task |
| `on event` | Event handler |

## Configuration Options

| Setting | Default | Description |
|---|---|---|
| `nc.path` | `"nc"` | Path to the NC binary |
| `nc.enableLSP` | `true` | Enable Language Server for diagnostics and completions |
| `nc.showStatusBar` | `true` | Show NC version in the status bar |
| `nc.autoValidateOnSave` | `false` | Automatically validate `.nc` files on save |

## Install NC

```bash
# macOS / Linux
curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | bash

# From source
git clone https://github.com/devheallabs-ai/nc-lang.git
cd nc/nc && make && sudo cp build/nc /usr/local/bin/
```

## Run NC

```bash
nc run service.nc -b behavior_name    # Run a behavior
nc serve service.nc                    # Start HTTP server
nc validate service.nc                 # Validate syntax
nc repl                                # Interactive mode
nc test                                # Run tests
```

## Building and Publishing

### Prerequisites

```bash
npm install -g @vscode/vsce
```

### Package as VSIX

```bash
cd nc-lang/editor/vscode
vsce package
```

This produces `nc-lang-1.0.0.vsix`.

### Install Locally for Testing

```bash
code --install-extension nc-lang-1.0.0.vsix
```

### Publish to VS Code Marketplace

1. Create a publisher at https://marketplace.visualstudio.com/manage
2. Generate a Personal Access Token (PAT) from Azure DevOps.
3. Login and publish:

```bash
vsce login devheal-labs-ai
vsce publish
```

Or use the convenience script:

```bash
chmod +x publish.sh
./publish.sh
```

## Learn More

- [NC Language Guide](https://github.com/devheallabs-ai/nc-lang/blob/main/docs/NC_LANGUAGE_GUIDE.md)
- [Examples](https://github.com/devheallabs-ai/nc-lang/tree/main/examples)
- [GitHub Repository](https://github.com/devheallabs-ai/nc-lang)

## Requirements

- VS Code 1.80.0 or later.
- NC binary installed and available in PATH (or configured via `nc.path`).

## License

Apache-2.0

