// NC Standard Library — http.client
// HTTP client for making requests

service "nc.http.client"
version "1.0.0"

to get with url:
    purpose: "Make a GET request to a URL"
    gather response from url
    respond with response

to post with url and data:
    purpose: "Make a POST request with data"
    gather response from url:
        method: "POST"
        body: data
    respond with response

to fetch with url and options:
    purpose: "Make any HTTP request with options"
    gather response from url
    respond with response
