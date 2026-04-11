# Contributing to DevHeal Labs AI

Thank you for your interest in contributing! All of our open-source projects follow the same process.

## Projects

| Repository | What it is | Language |
|------------|-----------|----------|
| [nc](https://github.com/devheallabs-ai/nc) | NC Programming Language — C11 engine, compiler, tests | C, NC |
| [nc-ui](https://github.com/devheallabs-ai/nc-ui) | NC UI frontend framework — compiler and runtime | JavaScript, NC |
| [nc-ai](https://github.com/devheallabs-ai/nc-ai) | NC AI SDK — inference, code generation, ML pipelines | NC, Python |
| [nc-apps](https://github.com/devheallabs-ai/nc-apps) | Example applications and reference implementations | NC, HTML |

## How to Contribute

### 1. Fork and Clone

```bash
gh repo fork devheallabs-ai/<repo> --clone
cd <repo>
```

### 2. Create a Branch

```bash
git checkout -b fix/my-improvement
```

### 3. Make Changes

- Follow the existing code style.
- Add tests for new functionality.
- Ensure all existing tests pass.

### 4. Test

```bash
# NC Lang
cd engine && make && NC_ALLOW_EXEC=1 ./build/nc test

# NC UI
npm ci && npm test

# NC AI
npm test
```

### 5. Submit a Pull Request

```bash
git push origin fix/my-improvement
gh pr create --fill
```

## Pull Request Guidelines

- **One PR per feature/fix** — keep changes focused.
- **Write a clear description** — explain what and why, not just what.
- **Add tests** — all new features need coverage.
- **Pass CI** — all checks must be green before merge.
- **Squash merge** — we use squash merges for a clean linear history.

## Reporting Bugs

Use [GitHub Issues](https://github.com/devheallabs-ai/nc/issues/new?template=bug_report.md) with the **Bug Report** template.

## Requesting Features

Use [GitHub Issues](https://github.com/devheallabs-ai/nc/issues/new?template=feature_request.md) with the **Feature Request** template.

## Security Vulnerabilities

**Do not open public issues for security vulnerabilities.** Email [support@devheallabs.in](mailto:support@devheallabs.in) instead. See our [Security Policy](SECURITY.md).

## Code of Conduct

All contributors are expected to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](https://github.com/devheallabs-ai/nc/blob/main/LICENSE).
