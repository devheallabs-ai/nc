# NC Language — npm package

**NC** is the AI Programming Language. Build AI-powered APIs in plain English.

```bash
npm install -g nc-lang
```

Or run without installing:

```bash
npx nc-lang version
npx nc-lang "show 42 + 8"
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

This is a **thin Node.js wrapper** that:

1. Downloads the official NC binary during `npm install` (postinstall)
2. Places it in the package's `bin/` directory
3. The `nc` command forwards all arguments to the native NC binary

No Node.js runtime is needed at execution time — the NC binary is a standalone native executable.

## Supported Platforms

| Platform           | Architecture |
|--------------------|-------------|
| Linux              | x64         |
| Linux              | arm64       |
| macOS              | x64         |
| macOS              | arm64 (M1+) |
| Windows            | x64         |

## Environment Variables

| Variable           | Description                                      |
|--------------------|--------------------------------------------------|
| `NC_BINARY`        | Override the path to the NC binary               |
| `NC_AI_KEY`        | API key for AI features                          |

## Alternative Installation Methods

```bash
# pip
pip install nc-lang

# Homebrew (macOS/Linux)
brew install nc-lang

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
