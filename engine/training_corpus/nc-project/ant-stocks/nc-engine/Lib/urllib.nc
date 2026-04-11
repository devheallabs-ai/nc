// NC Standard Library — urllib
// URL handling

service "nc.urllib"
version "1.0.0"

to fetch with url:
    purpose: "Fetch content from a URL"
    gather response from url
    respond with response

to encode with params:
    purpose: "Encode parameters for a URL query string"
    respond with str(params)

to parse with url:
    purpose: "Parse a URL into its components"
    respond with url

to join with base and path:
    purpose: "Join a base URL with a path"
    respond with base + path
