# NC Language — pip package

**NC** is the AI Programming Language. Build AI-powered APIs in plain English.

```bash
pip install nc-lang
```

## Quick Start

```bash
# Check version
nc version

# Run an NC expression
nc "show 42 + 8"

# Start an API server
nc serve myservice.nc
```

## What This Package Does

This is a **thin Python wrapper** that:

1. Downloads the official NC binary for your platform on first run
2. Caches it in `~/.nc/bin/`
3. Forwards all commands to the native NC binary

No Python runtime is needed at execution time — the NC binary is a standalone native executable.

## Supported Platforms

| Platform           | Architecture |
|--------------------|-------------|
| Linux              | x86_64      |
| macOS              | x86_64      |
| macOS              | arm64 (M1+) |
| Windows            | x86_64      |

## Environment Variables

| Variable           | Description                                      |
|--------------------|--------------------------------------------------|
| `NC_ACCEPT_LICENSE` | Set to `yes` to accept the license non-interactively |
| `NC_BINARY`        | Override the path to the NC binary               |
| `NC_AI_KEY`        | API key for AI features                          |

## Alternative Installation Methods

```bash
# Homebrew (macOS/Linux)
brew install nc-lang

# npm
npm install -g nc-lang

# Direct install script
curl -fsSL https://nc.devheallabs.in/install.sh | bash
```

## Links

- **Website**: https://nc.devheallabs.in
- **Documentation**: https://nc.devheallabs.in/docs
- **GitHub**: https://github.com/devheallabs-ai/nc
- **Issues**: https://github.com/devheallabs-ai/nc/issues

## License

Apache 2.0 — Copyright (c) DevHeal Labs AI
