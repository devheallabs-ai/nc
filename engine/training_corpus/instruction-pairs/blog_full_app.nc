<|begin|>
// Description: a blog with posts, comments, and AI content generation
// Type: full app
service "blog"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to list posts:
    set posts to load("posts.json")
    respond with posts

to get post with id:
    set post to load("posts.json", id)
    if post is empty:
        respond with error "Post not found" status 404
    respond with post

to create post with data:
    set data.id to generate_id()
    set data.created_at to now()
    set data.published to false
    set posts to load("posts.json")
    add data to posts
    save posts to "posts.json"
    respond with data

to publish post with id:
    set posts to load("posts.json")
    set index to find_index(posts, id)
    set posts[index].published to true
    set posts[index].published_at to now()
    save posts to "posts.json"
    respond with posts[index]

to add comment with post_id and data:
    set data.id to generate_id()
    set data.post_id to post_id
    set data.created_at to now()
    set comments to load("comments.json")
    add data to comments
    save comments to "comments.json"
    respond with data

api:
    GET /posts runs list_posts
    GET /posts/:id runs get_post
    POST /posts runs create_post
    PUT /posts/:id/publish runs publish_post
    POST /posts/:id/comments runs add_comment
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "blog"}

// === NC_FILE_SEPARATOR ===

page "Blog"
title "Blog | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

nav:
    brand "My Blog"
    link "Posts" to "/posts"
    link "Write" to "/editor"

section "hero":
    heading "Welcome to My Blog"
    text "Thoughts, stories, and ideas"

section "posts":
    grid from "/posts" as posts columns 3:
        card:
            image posts.cover
            heading posts.title
            text posts.excerpt
            badge posts.published "Published" "Draft"
            link "Read More" to "/posts/{{posts.id}}"

section "editor":
    form action "/posts" method POST:
        input "title" placeholder "Post title"
        input "excerpt" placeholder "Short description"
        input "content" type textarea placeholder "Write your post..."
        button "Save Draft" type submit style secondary
        button "AI Generate" action "aiGenerate" style primary

// === NC_AGENT_SEPARATOR ===

// Blog AI Agent
service "blog-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to generate content with topic:
    ask AI to "Write a blog post about: {{topic}}. Include intro, body, conclusion." save as content
    respond with {"content": content}

to improve writing with text:
    ask AI to "Improve this blog post for clarity and engagement: {{text}}" save as improved
    respond with {"improved": improved}

to generate seo with post:
    ask AI to "Generate SEO title and meta description for: {{post.title}} — {{post.excerpt}}" save as seo
    respond with {"seo": seo}

to handle with prompt:
    purpose: "Handle user request for blog"
    ask AI to "You are a helpful blog assistant. {{prompt}}" save as response
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
