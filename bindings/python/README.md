# nc-lang — Python bindings for NC

**NC** — The AI Language. Write AI in plain English.

## Install

```bash
pip install nc-lang
```

**Prerequisite:** The `nc` binary must be installed and on your PATH.

```bash
# macOS / Linux
curl -fsSL https://nc.devheallabs.in/install.sh | sh

# Or via Homebrew
brew install nc-lang/tap/nc
```

## Quick Start

```python
from nc_lang import workflow

# Configure your AI provider
workflow.configure(NC_AI_KEY="<YOUR_API_KEY>")

# Run an AI workflow
result = workflow.run('''
    ask AI to "summarize this document" using doc:
        save as: summary
    respond with summary
''', doc="Your long document text here...")

print(result.output)   # The AI-generated summary
print(result.ok)       # True if successful
```

## Advanced Usage

```python
from nc_lang import NC

# Create a runtime with custom config
nc = NC(binary_path="/usr/local/bin/nc")
nc.set_provider("nova", key="<YOUR_API_KEY>", model="nova")

# Multi-step workflow
result = nc.run('''
    ask AI to "classify this ticket" using ticket:
        save as: category

    match category:
        when "urgent":
            ask AI to "draft urgent response" using ticket:
                save as: response
        otherwise:
            set response to "We'll get back to you soon."

    respond with {category: category, response: response}
''', ticket="My billing is wrong and I'm being overcharged!")

print(result.output)
```

## API Reference

### `workflow.run(source, **variables) -> NCResult`

Run NC code with context variables injected.

### `workflow.configure(**env_vars)`

Set environment variables for the default runtime.

### `NC(binary_path=None, providers_path=None)`

Create a custom NC runtime instance.

### `NCResult`

| Field | Type | Description |
|-------|------|-------------|
| `ok` | `bool` | Whether execution succeeded |
| `output` | `str` | Captured stdout |
| `error` | `str` | Captured stderr |
| `exit_code` | `int` | Process exit code |

## License

Apache 2.0 — see [LICENSE](https://github.com/devheallabs-ai/nc/blob/main/LICENSE)
