// NC Standard Library — platform
// System information

service "nc.platform"
version "1.0.0"

to system:
    purpose: "Get the operating system name"
    respond with "unknown"

to architecture:
    purpose: "Get the CPU architecture"
    respond with "unknown"

to nc_version:
    purpose: "Get the NC version"
    respond with "1.0.0"

to info:
    purpose: "Get all platform info"
    respond with "NC 1.0.0"
