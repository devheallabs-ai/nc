<|begin|>
// Description: a URL shortener service with analytics and custom slugs
// Type: full app
service "url-shortener"
version "1.0.0"

to shorten with url and custom_slug:
    set links to load("links.json")
    if custom_slug is not empty:
        set slug to custom_slug
    else:
        set slug to random_string(6)
    set existing to find_by(links, "slug", slug)
    if existing is not empty:
        respond with error "Slug taken" status 409
    set link to {"id": generate_id(), "url": url, "slug": slug, "clicks": 0, "created_at": now()}
    add link to links
    save links to "links.json"
    respond with link

to redirect with slug:
    set links to load("links.json")
    set link to find_by(links, "slug", slug)
    if link is empty:
        respond with error "Link not found" status 404
    set index to find_index(links, link.id)
    set links[index].clicks to links[index].clicks + 1
    save links to "links.json"
    redirect to link.url

to get stats with slug:
    set links to load("links.json")
    set link to find_by(links, "slug", slug)
    respond with link

to list links:
    set links to load("links.json")
    respond with links

api:
    POST /shorten runs shorten
    GET /:slug runs redirect
    GET /stats/:slug runs get_stats
    GET /links runs list_links
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "url-shortener"}

// === NC_FILE_SEPARATOR ===

page "Url Shortener"
title "Url Shortener | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "shorten":
    heading "URL Shortener"

    form action "/shorten" method POST:
        input "url" type url placeholder "https://your-long-url.com/..."
        input "custom_slug" placeholder "custom-slug (optional)"
        button "Shorten" type submit style primary

section "links":
    list from "/links" as links:
        card:
            text links.slug style mono
            link links.url to links.url
            text links.clicks style badge
            button "Copy" action "copy" style secondary

// === NC_AGENT_SEPARATOR ===

// Url Shortener AI Agent
service "url-shortener-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to suggest slug with url:
    ask AI to "Suggest a short memorable slug for this URL: {{url}}. Reply with just the slug, no explanation." save as slug
    respond with {"slug": slug}

to handle with prompt:
    purpose: "Handle user request for url-shortener"
    ask AI to "You are a helpful url-shortener assistant. {{prompt}}" save as response
    respond with {"reply": response}

to classify with input:
    ask AI to "Classify as: create, read, update, delete, help. Input: {{input}}" save as intent
    respond with {"intent": intent}

api:
    POST /agent          runs handle
    POST /agent/classify  runs classify
    GET  /agent/health    runs health_check

to health check:
    respond with {"status": "ok", "ai": "local"}
<|end|>
