# How to Publish NC VS Code Extension

## One-time setup

```bash
npm install -g @vscode/vsce

# Get a Personal Access Token from https://dev.azure.com
# Organization → User Settings → Personal Access Tokens
# Scope: Marketplace (manage)
vsce login devheal-labs-ai
```

## Publish

```bash
cd editor/vscode
vsce package          # Creates nc-lang-1.0.0.vsix
vsce publish          # Publishes to VS Code Marketplace
```

## Test locally first

```bash
code --install-extension nc-lang-1.0.0.vsix
```

That's it. Users can then install from VS Code: Extensions → search "nc-lang".
