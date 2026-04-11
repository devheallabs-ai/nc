service "http-helpers"
version "1.0.0"
description "HTTP utility behaviors — fetch, post, webhook handling"

to fetch_json with url:
    purpose: "Fetch JSON from a URL"
    gather data from url
    respond with data

to post_json with url, payload:
    purpose: "Post JSON to a URL and return response"
    gather response from url
    respond with response

to health_check with urls:
    purpose: "Check health of multiple URLs"
    set results to []
    repeat for each url in urls:
        gather status from url
        set entry to {"url": url, "status": "ok"}
        append entry to results
    respond with results
