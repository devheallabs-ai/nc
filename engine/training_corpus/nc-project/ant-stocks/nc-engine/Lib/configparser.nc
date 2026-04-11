// NC Standard Library — configparser
// Read and write configuration files

service "nc.configparser"
version "1.0.0"

to read with path:
    purpose: "Read a configuration file"
    gather config from path
    respond with config

to get with config and section and key:
    purpose: "Get a value from configuration"
    respond with config.section.key

to set with config and section and key and value:
    purpose: "Set a value in configuration"
    respond with config

to write with config and path:
    purpose: "Write configuration to a file"
    store config into path
    respond with true
