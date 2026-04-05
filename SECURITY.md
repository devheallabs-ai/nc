# Security Policy (v1)

## Supported versions

Security fixes are prioritized for:

- `main` branch
- The latest release line

Older versions may receive limited or no security patches.

## Reporting a vulnerability

Please report security issues privately:

- Email: `support@devheallabs.in`

Do **not** open public GitHub issues for vulnerabilities before a fix is available.

## What to include in a report

- A clear description of the issue
- Affected component(s) and file path(s), if known
- Reproduction steps or a proof of concept
- Expected impact and any known mitigations
- Your contact details for follow-up

## Disclosure process

For valid reports, maintainers aim to:

1. Acknowledge receipt within 72 hours
2. Triage severity and impact
3. Prepare and test a fix
4. Coordinate responsible disclosure once patch details are ready

## NC-specific security notes

- NC can execute file and process primitives (for example `read_file`, `write_file`, `exec`, and `shell` in the standard library). Treat `.nc` programs as trusted code.
- NC network features (`gather` and AI provider calls) make outbound HTTP requests. Validate URLs and protect API credentials.
- Server deployments should explicitly configure authentication, CORS, and rate limiting middleware based on your environment.
- Secrets should be passed via environment variables (such as `NC_AI_KEY`) and never committed to source control.
- Security-focused language tests are maintained in `tests/lang/test_security.nc`.

## Safe testing guidelines

If you are testing a potential vulnerability:

- Use your own environment and data
- Avoid privacy-impacting or destructive actions
- Do not attempt denial-of-service against shared infrastructure

Good-faith security research is appreciated.

## Security Certificate

For a complete inventory of all hardening measures, mitigations, and test coverage, see [SECURITY_CERTIFICATE.md](SECURITY_CERTIFICATE.md).
