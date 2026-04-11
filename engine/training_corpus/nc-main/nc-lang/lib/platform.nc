// NC Standard Library — platform
// System information

service "nc.platform"
version "1.0.0"
// Status: Implemented

to system:
    purpose: "Get the operating system name"
    respond with platform_system()

to architecture:
    purpose: "Get the CPU architecture"
    respond with platform_architecture()

to nc_version:
    purpose: "Get the NC version"
    respond with nc_version()

to hostname:
    purpose: "Get the system hostname"
    respond with platform_hostname()

to user:
    purpose: "Get the current user name"
    respond with platform_user()

to home_dir:
    purpose: "Get the user's home directory"
    respond with platform_home_dir()

to temp_dir:
    purpose: "Get the system temp directory"
    respond with platform_temp_dir()

to info:
    purpose: "Get all platform info"
    respond with platform_info()
