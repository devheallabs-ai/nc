<|begin|>
// Description: a blog with posts, comments, and AI content generation (API only, no frontend)
// Type: service
service "blog-api"
version "1.0.0"

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
    respond with {"status": "ok"}
<|end|>
