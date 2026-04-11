// ═══════════════════════════════════════════════════════════
//  AI Content Generator
//
//  Replaces: 100+ lines of Python with template management + LLM calls
//
//  Generates blog posts, social media, emails, product descriptions.
//
//  curl -X POST http://localhost:8000/blog \
//    -d '{"topic": "AI in healthcare", "tone": "professional", "length": "medium"}'
// ═══════════════════════════════════════════════════════════

service "content-generator"
version "1.0.0"

configure:
    ai_model is "default"

to generate_blog with topic, tone, length:
    purpose: "Generate a blog post"

    set word_count to 500
    match length:
        when "short":
            set word_count to 300
        when "medium":
            set word_count to 600
        when "long":
            set word_count to 1200

    ask AI to "Write a {{tone}} blog post about {{topic}} in approximately {{word_count}} words. Include a compelling title, introduction, 3-4 main sections with headers, and a conclusion. Format in markdown." save as blog

    respond with {"content": blog, "topic": topic, "tone": tone, "target_words": word_count}

to generate_social with topic, platform:
    purpose: "Generate social media posts"

    match platform:
        when "twitter":
            ask AI to "Write 3 engaging tweets about {{topic}}. Each under 280 characters. Include relevant hashtags. Return JSON with: posts (list of strings)." save as posts
        when "linkedin":
            ask AI to "Write a professional LinkedIn post about {{topic}}. 150-200 words. Include a hook, insight, and call to action. Return as a string." save as posts
        when "instagram":
            ask AI to "Write an Instagram caption about {{topic}}. Include emojis and 5-10 relevant hashtags. Return as a string." save as posts
        otherwise:
            ask AI to "Write a social media post about {{topic}} suitable for {{platform}}." save as posts

    respond with {"content": posts, "platform": platform, "topic": topic}

to generate_email with purpose_text, recipient_type, tone:
    purpose: "Generate professional emails"

    ask AI to "Write a {{tone}} email for {{recipient_type}}. Purpose: {{purpose_text}}. Include subject line. Return JSON with: subject (string), body (string)." save as email

    respond with email

to rewrite with text, style:
    purpose: "Rewrite text in a different style"

    ask AI to "Rewrite this text in a {{style}} style. Keep the core message but change the tone and language:\n\n{{text}}" save as rewritten

    respond with {"original": text, "rewritten": rewritten, "style": style}

to generate_product_description with product_name, features, target_audience:
    purpose: "Generate product descriptions for e-commerce"

    ask AI to "Write a compelling product description for {{product_name}}. Target audience: {{target_audience}}. Features: {{features}}. Return JSON with: headline (string), body (string), bullet_points (list of strings), seo_keywords (list of strings)." save as product_desc

    respond with product_desc

api:
    POST /blog runs generate_blog
    POST /social runs generate_social
    POST /email runs generate_email
    POST /rewrite runs rewrite
    POST /product runs generate_product_description
