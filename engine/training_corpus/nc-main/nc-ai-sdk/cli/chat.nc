// ═══════════════════════════════════════════════════════════
//  NC AI Chat — Unified AI Interface
//
//  The single command that lets users talk to NC AI locally.
//  Routes every question to the right module and generates
//  actual text responses — not just metadata.
//
//  Capabilities:
//    - Write emails, letters, messages
//    - Explain concepts, debug errors
//    - Generate code, plan architectures
//    - Creative writing (stories, poems)
//    - Math/logic reasoning
//    - Tech comparisons and decisions
//    - Translation (6 languages)
//    - Summarize, review, classify
//    - General conversation
//
//  Usage:
//    nc ai chat                       (interactive)
//    nc ai reason "your question"     (one-shot)
//    nc run nc-ai/chat.nc -b demo     (demo conversation)
//    nc serve nc-ai/chat.nc           (HTTP API)
//
//  Copyright 2026 DevHeal Labs AI. All rights reserved.
// ═══════════════════════════════════════════════════════════

service "nc-ai-chat"
version "1.0.0"

configure:
    port is 8800

// ═══════════════════════════════════════════════════════════
//  INTENT ROUTER — Classify what the user wants
// ═══════════════════════════════════════════════════════════

to route_question with question:
    purpose: "Route a question to the best AI module"
    set q to lower(question)
    set qlen to len(q)
    set intent to "general"
    set sub_type to "conversation"
    // ── Compose intent (email, letter, message) ──
    if len(replace(q, "write an email", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "write a email", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "write email", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "draft an email", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "draft email", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "email about", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "email to", "")) is below qlen:
        set intent to "compose"
        set sub_type to "email"
    if len(replace(q, "write a letter", "")) is below qlen:
        set intent to "compose"
        set sub_type to "letter"
    if len(replace(q, "write a message", "")) is below qlen:
        set intent to "compose"
        set sub_type to "message"
    if len(replace(q, "send a message", "")) is below qlen:
        set intent to "compose"
        set sub_type to "message"
    if len(replace(q, "compose", "")) is below qlen:
        if intent is equal "general":
            set intent to "compose"
            set sub_type to "email"
    // ── Creative intent (story, poem) ──
    if len(replace(q, "write a story", "")) is below qlen:
        set intent to "creative"
        set sub_type to "story"
    if len(replace(q, "tell me a story", "")) is below qlen:
        set intent to "creative"
        set sub_type to "story"
    if len(replace(q, "write a poem", "")) is below qlen:
        set intent to "creative"
        set sub_type to "poem"
    if len(replace(q, "poem about", "")) is below qlen:
        set intent to "creative"
        set sub_type to "poem"
    if len(replace(q, "story about", "")) is below qlen:
        set intent to "creative"
        set sub_type to "story"
    // ── Summarize intent ──
    if len(replace(q, "summarize", "")) is below qlen:
        set intent to "summarize"
        set sub_type to "text"
    if len(replace(q, "summary of", "")) is below qlen:
        set intent to "summarize"
        set sub_type to "text"
    if len(replace(q, "tldr", "")) is below qlen:
        set intent to "summarize"
        set sub_type to "text"
    if len(replace(q, "sum up", "")) is below qlen:
        set intent to "summarize"
        set sub_type to "text"
    // ── Translate intent ──
    if len(replace(q, "translate", "")) is below qlen:
        set intent to "translate"
        set sub_type to "text"
    if len(replace(q, "say in spanish", "")) is below qlen:
        set intent to "translate"
        set sub_type to "spanish"
    if len(replace(q, "say in french", "")) is below qlen:
        set intent to "translate"
        set sub_type to "french"
    if len(replace(q, "say in hindi", "")) is below qlen:
        set intent to "translate"
        set sub_type to "hindi"
    if len(replace(q, "say in telugu", "")) is below qlen:
        set intent to "translate"
        set sub_type to "telugu"
    if len(replace(q, "say in japanese", "")) is below qlen:
        set intent to "translate"
        set sub_type to "japanese"
    if len(replace(q, "say in german", "")) is below qlen:
        set intent to "translate"
        set sub_type to "german"
    if len(replace(q, " in spanish", "")) is below qlen:
        if len(replace(q, "translate", "")) is below qlen:
            set intent to "translate"
            set sub_type to "spanish"
    if len(replace(q, " in french", "")) is below qlen:
        if len(replace(q, "translate", "")) is below qlen:
            set intent to "translate"
            set sub_type to "french"
    if len(replace(q, " in hindi", "")) is below qlen:
        if len(replace(q, "translate", "")) is below qlen:
            set intent to "translate"
            set sub_type to "hindi"
    if len(replace(q, " in telugu", "")) is below qlen:
        if len(replace(q, "translate", "")) is below qlen:
            set intent to "translate"
            set sub_type to "telugu"
    // ── Review intent (code review, codebase review) ──
    if len(replace(q, "review my code", "")) is below qlen:
        set intent to "review"
        set sub_type to "codebase"
    if len(replace(q, "review code", "")) is below qlen:
        set intent to "review"
        set sub_type to "codebase"
    if len(replace(q, "code review", "")) is below qlen:
        set intent to "review"
        set sub_type to "codebase"
    if len(replace(q, "review my project", "")) is below qlen:
        set intent to "review"
        set sub_type to "project"
    if len(replace(q, "check my code", "")) is below qlen:
        set intent to "review"
        set sub_type to "codebase"
    if len(replace(q, "find bugs", "")) is below qlen:
        set intent to "review"
        set sub_type to "bugs"
    if len(replace(q, "find issues", "")) is below qlen:
        set intent to "review"
        set sub_type to "bugs"
    if len(replace(q, "security review", "")) is below qlen:
        set intent to "review"
        set sub_type to "security"
    if len(replace(q, "security audit", "")) is below qlen:
        set intent to "review"
        set sub_type to "security"
    if len(replace(q, "vulnerability", "")) is below qlen:
        set intent to "review"
        set sub_type to "security"
    if len(replace(q, "code quality", "")) is below qlen:
        set intent to "review"
        set sub_type to "quality"
    if len(replace(q, "code smell", "")) is below qlen:
        set intent to "review"
        set sub_type to "quality"
    if len(replace(q, "anti-pattern", "")) is below qlen:
        set intent to "review"
        set sub_type to "quality"
    if len(replace(q, "best practice", "")) is below qlen:
        set intent to "review"
        set sub_type to "quality"
    // ── Codegen intent ──
    if len(replace(q, "generate code", "")) is below qlen:
        set intent to "codegen"
        set sub_type to "generate"
    if len(replace(q, "write code", "")) is below qlen:
        set intent to "codegen"
        set sub_type to "generate"
    if len(replace(q, "build a", "")) is below qlen:
        if len(replace(q, "api", "")) is below qlen:
            set intent to "codegen"
            set sub_type to "generate"
    if len(replace(q, "create a", "")) is below qlen:
        if len(replace(q, "service", "")) is below qlen:
            set intent to "codegen"
            set sub_type to "generate"
    if len(replace(q, "build an api", "")) is below qlen:
        set intent to "codegen"
        set sub_type to "generate"
    if len(replace(q, "build a service", "")) is below qlen:
        set intent to "codegen"
        set sub_type to "generate"
    // ── Decision intent ──
    if len(replace(q, "should i", "")) is below qlen:
        set intent to "decide"
        set sub_type to "decision"
    if len(replace(q, "which is better", "")) is below qlen:
        set intent to "decide"
        set sub_type to "decision"
    if len(replace(q, "recommend", "")) is below qlen:
        set intent to "decide"
        set sub_type to "recommendation"
    // ── Reason subtypes (only if not already classified) ──
    if intent is equal "general":
        // Mathematical
        if len(replace(q, "calculate", "")) is below qlen:
            set intent to "reason"
            set sub_type to "mathematical"
        if len(replace(q, "how many", "")) is below qlen:
            set intent to "reason"
            set sub_type to "mathematical"
        if len(replace(q, " km", "")) is below qlen:
            set intent to "reason"
            set sub_type to "mathematical"
        if len(replace(q, "sum of", "")) is below qlen:
            set intent to "reason"
            set sub_type to "mathematical"
        if len(replace(q, "average", "")) is below qlen:
            set intent to "reason"
            set sub_type to "mathematical"
        // Debugging
        if len(replace(q, "crash", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "error", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "fix", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "bug", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "debug", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "fail", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "timeout", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        if len(replace(q, "not working", "")) is below qlen:
            set intent to "reason"
            set sub_type to "debugging"
        // Planning
        if len(replace(q, "deploy", "")) is below qlen:
            set intent to "reason"
            set sub_type to "planning"
        if len(replace(q, "plan", "")) is below qlen:
            set intent to "reason"
            set sub_type to "planning"
        if len(replace(q, "steps to", "")) is below qlen:
            set intent to "reason"
            set sub_type to "planning"
        if len(replace(q, "how to", "")) is below qlen:
            set intent to "reason"
            set sub_type to "planning"
        if len(replace(q, "migrate", "")) is below qlen:
            set intent to "reason"
            set sub_type to "planning"
        // Comparison
        if len(replace(q, "difference between", "")) is below qlen:
            set intent to "reason"
            set sub_type to "comparison"
        if len(replace(q, "compare", "")) is below qlen:
            set intent to "reason"
            set sub_type to "comparison"
        if len(replace(q, " vs ", "")) is below qlen:
            set intent to "reason"
            set sub_type to "comparison"
        // Causal
        if len(replace(q, "why does", "")) is below qlen:
            set intent to "reason"
            set sub_type to "causal"
        if len(replace(q, "what causes", "")) is below qlen:
            set intent to "reason"
            set sub_type to "causal"
        if len(replace(q, "root cause", "")) is below qlen:
            set intent to "reason"
            set sub_type to "causal"
        // Explain
        if len(replace(q, "explain", "")) is below qlen:
            set intent to "reason"
            set sub_type to "explain"
        if len(replace(q, "what is", "")) is below qlen:
            set intent to "reason"
            set sub_type to "explain"
        if len(replace(q, "how does", "")) is below qlen:
            set intent to "reason"
            set sub_type to "explain"
        if len(replace(q, "what are", "")) is below qlen:
            set intent to "reason"
            set sub_type to "explain"
        // Greeting
        if len(replace(q, "hello", "")) is below qlen:
            set intent to "general"
            set sub_type to "greeting"
        if len(replace(q, "hi ", "")) is below qlen:
            set intent to "general"
            set sub_type to "greeting"
        if len(replace(q, "hey", "")) is below qlen:
            set intent to "general"
            set sub_type to "greeting"
        // Help
        if len(replace(q, "help", "")) is below qlen:
            set intent to "general"
            set sub_type to "help"
        if len(replace(q, "what can you do", "")) is below qlen:
            set intent to "general"
            set sub_type to "help"
    respond with {"intent": intent, "type": sub_type}

// ═══════════════════════════════════════════════════════════
//  COMPOSE — Email, letter, message generation
// ═══════════════════════════════════════════════════════════

to answer_compose with question and sub_type:
    purpose: "Generate emails, letters, and messages"
    set q to lower(question)
    set qlen to len(q)
    // Detect tone
    set tone to "professional"
    if len(replace(q, "casual", "")) is below qlen:
        set tone to "casual"
    if len(replace(q, "formal", "")) is below qlen:
        set tone to "formal"
    if len(replace(q, "friendly", "")) is below qlen:
        set tone to "friendly"
    if len(replace(q, "urgent", "")) is below qlen:
        set tone to "urgent"
    if len(replace(q, "apologize", "")) is below qlen:
        set tone to "apologetic"
    if len(replace(q, "apology", "")) is below qlen:
        set tone to "apologetic"
    if len(replace(q, "thank", "")) is below qlen:
        set tone to "grateful"
    // Detect topic
    set topic to "general"
    if len(replace(q, "meeting", "")) is below qlen:
        set topic to "meeting"
    if len(replace(q, "interview", "")) is below qlen:
        set topic to "interview"
    if len(replace(q, "leave", "")) is below qlen:
        set topic to "leave"
    if len(replace(q, "leaving", "")) is below qlen:
        set topic to "resignation"
    if len(replace(q, "resign", "")) is below qlen:
        set topic to "resignation"
    if len(replace(q, "resignation", "")) is below qlen:
        set topic to "resignation"
    if len(replace(q, "quitting", "")) is below qlen:
        set topic to "resignation"
    if len(replace(q, "quit my job", "")) is below qlen:
        set topic to "resignation"
    if len(replace(q, "follow up", "")) is below qlen:
        set topic to "followup"
    if len(replace(q, "following up", "")) is below qlen:
        set topic to "followup"
    if len(replace(q, "followup", "")) is below qlen:
        set topic to "followup"
    if len(replace(q, "introduce", "")) is below qlen:
        set topic to "introduction"
    if len(replace(q, "introducing", "")) is below qlen:
        set topic to "introduction"
    if len(replace(q, "introduction", "")) is below qlen:
        set topic to "introduction"
    if len(replace(q, "outage", "")) is below qlen:
        set topic to "incident"
    if len(replace(q, "incident", "")) is below qlen:
        set topic to "incident"
    if len(replace(q, "update", "")) is below qlen:
        set topic to "update"
    if len(replace(q, "project", "")) is below qlen:
        set topic to "project_update"
    if len(replace(q, "feedback", "")) is below qlen:
        set topic to "feedback"
    if len(replace(q, "complaint", "")) is below qlen:
        set topic to "complaint"
    if len(replace(q, "offer", "")) is below qlen:
        set topic to "offer"
    if len(replace(q, "proposal", "")) is below qlen:
        set topic to "proposal"
    // Generate email based on topic
    set subject to ""
    set body to ""
    if topic is equal "meeting":
        set subject to "Meeting Request"
        set body to "Hi,\n\nI would like to schedule a meeting to discuss our current progress and align on next steps.\n\nCould you please share your availability for this week? I expect the discussion will take about 30 minutes.\n\nPlease let me know what time works best for you.\n\nBest regards"
    if topic is equal "interview":
        set subject to "Thank You for the Interview"
        set body to "Dear Hiring Manager,\n\nThank you for taking the time to meet with me today. I truly enjoyed learning more about the role and the team.\n\nOur conversation reinforced my enthusiasm for the position. I am confident that my skills and experience align well with the requirements.\n\nI look forward to hearing about the next steps. Please do not hesitate to reach out if you need any additional information.\n\nBest regards"
    if topic is equal "leave":
        set subject to "Leave Request"
        set body to "Dear Manager,\n\nI am writing to request leave from [start date] to [end date] for personal reasons.\n\nI have ensured that my current tasks are on track and will arrange for coverage during my absence. All pending deliverables will be completed before I leave.\n\nPlease let me know if you need any further details.\n\nThank you for your consideration.\n\nBest regards"
    if topic is equal "resignation":
        set subject to "Resignation Notice"
        set body to "Dear Manager,\n\nI am writing to formally notify you of my resignation from my position, effective [last working day, typically 2 weeks from now].\n\nI have genuinely valued my time here and the opportunities for growth. I am committed to ensuring a smooth transition and will do everything I can to hand over my responsibilities properly.\n\nThank you for the support and mentorship over the years.\n\nSincerely"
    if topic is equal "followup":
        set subject to "Following Up"
        set body to "Hi,\n\nI wanted to follow up on our previous conversation regarding [topic]. I understand you have a busy schedule, but I wanted to check if there are any updates or if you need any additional information from my side.\n\nI am happy to jump on a quick call if that would be easier.\n\nLooking forward to hearing from you.\n\nBest regards"
    if topic is equal "introduction":
        set subject to "Introduction"
        set body to "Hi,\n\nI hope this message finds you well. My name is [Your Name] and I am [your role/title].\n\nI am reaching out because [reason for introduction]. I believe there could be a great opportunity for us to collaborate on [topic].\n\nI would love to schedule a brief call to discuss this further. Would you be available sometime this week?\n\nBest regards"
    if topic is equal "incident":
        set subject to "Incident Report - Service Disruption"
        set body to "Dear Team,\n\nI am writing to report a service incident that occurred today.\n\nImpact: [describe affected services and user impact]\nDuration: [start time] to [end time]\nRoot Cause: [brief description of what went wrong]\n\nImmediate Actions Taken:\n- [action 1]\n- [action 2]\n\nPrevention Plan:\n- [improvement 1]\n- [improvement 2]\n\nA detailed postmortem will follow within 48 hours.\n\nBest regards"
    if topic is equal "project_update":
        set subject to "Project Status Update"
        set body to "Hi Team,\n\nHere is this week's project update:\n\nCompleted:\n- [completed item 1]\n- [completed item 2]\n\nIn Progress:\n- [current work 1]\n- [current work 2]\n\nBlocked:\n- [blocker if any, or 'None']\n\nNext Week:\n- [planned work 1]\n- [planned work 2]\n\nOverall Status: On Track\n\nPlease reach out if you have any questions.\n\nBest regards"
    if topic is equal "feedback":
        set subject to "Feedback"
        set body to "Hi,\n\nThank you for [what they did]. I wanted to share some thoughts:\n\nWhat went well:\n- [positive point 1]\n- [positive point 2]\n\nSuggestions for improvement:\n- [suggestion 1]\n- [suggestion 2]\n\nOverall, great work. Keep it up!\n\nBest regards"
    if topic is equal "complaint":
        set subject to "Concern Regarding [Issue]"
        set body to "Dear Support,\n\nI am writing to bring to your attention an issue I have experienced with [product/service].\n\nDescription: [what happened]\nDate: [when it happened]\nImpact: [how it affected you]\n\nI would appreciate it if this could be looked into at your earliest convenience. I am available to provide any additional details if needed.\n\nThank you for your time.\n\nBest regards"
    if topic is equal "offer":
        set subject to "Offer Letter"
        set body to "Dear [Candidate],\n\nWe are pleased to extend an offer for the position of [role] at [company].\n\nDetails:\n- Position: [role]\n- Start Date: [date]\n- Compensation: [details]\n- Benefits: [brief overview]\n\nPlease review the attached offer letter and let us know your decision by [deadline].\n\nWe are excited about the possibility of you joining our team!\n\nBest regards"
    if topic is equal "proposal":
        set subject to "Proposal: [Project Name]"
        set body to "Dear [Recipient],\n\nI am writing to propose [brief description of the project/idea].\n\nObjective: [what you want to achieve]\n\nApproach:\n1. [phase 1]\n2. [phase 2]\n3. [phase 3]\n\nTimeline: [estimated duration]\nResources Needed: [brief list]\nExpected Outcome: [what success looks like]\n\nI would welcome the opportunity to discuss this in detail.\n\nBest regards"
    // Default general email
    if len(subject) is equal 0:
        set subject to "Regarding Your Request"
        set body to "Hi,\n\nThank you for reaching out. I am writing regarding [your topic].\n\n[Your main message here]\n\nPlease let me know if you have any questions or need further information.\n\nBest regards"
    // Apply tone adjustments
    if tone is equal "urgent":
        set subject to "URGENT: " + subject
    if tone is equal "apologetic":
        set subject to "Our Apologies: " + subject
    log ""
    log "  NC AI — Compose"
    log "  ───────────────"
    log ""
    log "  Subject: " + subject
    log "  Tone: " + tone
    log ""
    log "  ────────────────────────────────────────"
    log "  " + body
    log "  ────────────────────────────────────────"
    log ""
    log "  Tip: Replace [bracketed] placeholders with your details."
    respond with {"subject": subject, "body": body, "tone": tone, "topic": topic, "module": "compose"}

// ═══════════════════════════════════════════════════════════
//  CREATIVE — Story, poem generation
// ═══════════════════════════════════════════════════════════

to answer_creative with question and sub_type:
    purpose: "Generate creative writing"
    set q to lower(question)
    set qlen to len(q)
    // Detect theme
    set theme to "technology"
    if len(replace(q, "nature", "")) is below qlen:
        set theme to "nature"
    if len(replace(q, "love", "")) is below qlen:
        set theme to "love"
    if len(replace(q, "adventure", "")) is below qlen:
        set theme to "adventure"
    if len(replace(q, "space", "")) is below qlen:
        set theme to "space"
    if len(replace(q, "ai", "")) is below qlen:
        set theme to "technology"
    if len(replace(q, "farmer", "")) is below qlen:
        set theme to "rural"
    if len(replace(q, "ocean", "")) is below qlen:
        set theme to "ocean"
    if len(replace(q, "friendship", "")) is below qlen:
        set theme to "friendship"
    set text to ""
    if sub_type is equal "story":
        if theme is equal "technology":
            set text to "The Last Compiler\n\nIn a world where every language had been written, one programmer believed there was still something missing.\n\n'Languages talk to machines,' she told her mentor. 'But none of them talk WITH you.'\n\nShe spent three years building something different. Not faster. Not more elegant. But alive. A language that understood what you meant, not just what you typed.\n\nThe first time it worked, she typed: 'Why does my server keep crashing?'\n\nAnd instead of an error, instead of documentation, the screen printed six clear steps. Each one correct. Each one leading to the fix.\n\nHer mentor read the output and sat down slowly.\n\n'You did not build a language,' he said quietly. 'You built a partner.'\n\nShe smiled. 'I built NC.'"
        if theme is equal "nature":
            set text to "The Garden That Learned\n\nOld Maya had tended her garden for forty years. Every plant had a name. Every season, a rhythm.\n\nBut this year the rain came wrong — too much in March, nothing in April. Her tomatoes yellowed. Her herbs wilted.\n\nHer granddaughter brought a small sensor. 'Let it listen,' she said.\n\nThe device measured soil, moisture, sunlight. By morning it had a plan: move the herbs to the east bed, water the tomatoes at dusk, add lime to the soil.\n\nMaya followed skeptically. Within weeks, the garden was greener than it had been in a decade.\n\n'It does not replace your hands,' her granddaughter said. 'It just gives them better instructions.'\n\nMaya touched the sensor gently. 'Then it learned from the best teacher — the soil itself.'"
        if theme is equal "space":
            set text to "Signal\n\nThe probe had been silent for eleven years.\n\nMission Control had moved on. The screens that once glowed with telemetry now displayed quarterly budgets.\n\nThen, at 3:47 AM on a Tuesday, a single packet arrived. Not noise. Not echo. A structured message: coordinates, atmospheric composition, and one word the probe was never programmed to send.\n\n'Home.'\n\nThe night engineer stared at her screen. Checked the checksum. Verified the source.\n\nShe picked up the phone with trembling hands.\n\n'Wake everyone up,' she said. 'It found something.'"
        if theme is equal "friendship":
            set text to "Two Keyboards\n\nThey met in a hackathon — she wrote frontend, he wrote backend. Her code was beautiful, his was bulletproof. Together they built something neither could alone.\n\nYears passed. Different companies, different cities. But every Saturday morning, they would open a shared repository and push code to each other. No words needed. Just commits.\n\nOne Saturday, his commits stopped.\n\nShe waited a week. Then two. Then she flew across the country.\n\nShe found him overwhelmed, burned out, staring at a blank editor.\n\n'I forgot why I started,' he said.\n\nShe sat beside him and opened their repo. Scrolled to the first commit. The message read: 'Because building things together is the whole point.'\n\nHe smiled for the first time in months.\n\nThey pushed a new commit that afternoon."
        if len(text) is equal 0:
            set text to "The Builder\n\nShe did not set out to change the world. She set out to fix one thing — a server that kept crashing at 3 AM.\n\nBut fixing one thing led to understanding another. And understanding led to building. And building led to something she never expected.\n\nA language that thought.\n\nNot with neurons or magic or billion-dollar clusters. With patterns. With graphs. With the quiet logic of someone who had debugged enough systems to know what questions to ask.\n\nThe first user typed: 'Why is my app slow?'\n\nAnd the system replied with five precise steps. Each one correct.\n\nThe user stared at the screen. 'How did it know?'\n\n'It did not know,' she said. 'It reasoned.'\n\nAnd that made all the difference."
    if sub_type is equal "poem":
        if theme is equal "technology":
            set text to "Code and Light\n\nLines of logic, running deep,\nThrough silicon valleys while we sleep.\nA question asked, an answer found,\nIn patterns woven, knowledge bound.\n\nNot born of clouds or distant servers,\nBut local minds — persistent learners.\nNo API key, no monthly cost,\nNo data sent, no privacy lost.\n\nFrom graph to score, from swarm to thought,\nThe intelligence that can't be bought.\nIt lives inside the code you write,\nA quiet engine, burning bright."
        if theme is equal "nature":
            set text to "Roots\n\nBeneath the soil, where no one sees,\nThe roots reach out like memories.\nThey find the water, find the way,\nAnd hold the ground for one more day.\n\nThe tree above may bend and sway,\nMay lose its leaves in disarray.\nBut underground, where strength begins,\nThe roots hold fast through thick and thin.\n\nSo when the storms of life descend,\nRemember roots outlast the wind."
        if theme is equal "love":
            set text to "Constants\n\nIn a world of variables,\nYou are my constant — declared once,\nNever reassigned.\n\nEvery function I write returns to you.\nEvery loop I run circles back to your name.\n\nYou are not a bug in my code.\nYou are the reason it compiles.\n\nAnd when the system crashes,\nAs systems do,\nYou are the backup I never have to verify.\n\nAlways there. Always true.\nMy favorite line of code is you."
        if len(text) is equal 0:
            set text to "Questions\n\nWe ask the machines our questions now,\nAnd marvel when they answer well.\nBut the deepest question, the one that counts,\nNo algorithm can quite tell.\n\nWhy do we build? Why do we seek?\nWhy do we code through endless nights?\nNot for the output on the screen,\nBut for the spark. The quiet lights.\n\nThe joy of making something work.\nThe pride of solving something hard.\nThe human need to build and dream —\nThat is the real credit card."
    log ""
    log "  NC AI — Creative Writing"
    log "  ────────────────────────"
    log "  Theme: " + theme
    log ""
    log "  ────────────────────────────────────────"
    log "  " + text
    log "  ────────────────────────────────────────"
    respond with {"text": text, "theme": theme, "type": sub_type, "module": "creative"}

// ═══════════════════════════════════════════════════════════
//  SUMMARIZE — Text summarization approach
// ═══════════════════════════════════════════════════════════

to answer_summarize with question:
    purpose: "Help summarize text"
    log ""
    log "  NC AI — Summarize"
    log "  ─────────────────"
    log ""
    log "  To summarize text with NC AI, you have two options:"
    log ""
    log "  1. Paste your text directly:"
    log "     nc ai reason \"Summarize: [your text here]\""
    log ""
    log "  2. Summarize a file:"
    log "     nc run nc-ai/cortex/inference.nc -b generate"
    log "     with mode: hybrid, prompt: summarize + your text"
    log ""
    log "  NOVA uses hybrid mode (neural + graph + template)"
    log "  to extract key points and generate concise summaries."
    log ""
    set steps to ["Identify the main topic and scope", "Extract key facts and claims", "Identify relationships between ideas", "Compress into concise key points", "Generate summary preserving essential meaning"]
    respond with {"steps": steps, "module": "summarize", "mode": "hybrid"}

// ═══════════════════════════════════════════════════════════
//  TRANSLATE — Multi-language translation
// ═══════════════════════════════════════════════════════════

to answer_translate with question and target_lang:
    purpose: "Translate text between languages"
    set q to lower(question)
    set qlen to len(q)
    // Detect target language
    set lang to target_lang
    if lang is equal "text":
        set lang to "spanish"
        if len(replace(q, "french", "")) is below qlen:
            set lang to "french"
        if len(replace(q, "hindi", "")) is below qlen:
            set lang to "hindi"
        if len(replace(q, "telugu", "")) is below qlen:
            set lang to "telugu"
        if len(replace(q, "japanese", "")) is below qlen:
            set lang to "japanese"
        if len(replace(q, "german", "")) is below qlen:
            set lang to "german"
    // Common phrases
    set greetings to []
    append {"en": "hello", "es": "hola", "fr": "bonjour", "hi": "namaste", "te": "namaskaram", "ja": "konnichiwa", "de": "hallo"} to greetings
    append {"en": "good morning", "es": "buenos dias", "fr": "bonjour", "hi": "suprabhat", "te": "subhodayam", "ja": "ohayou gozaimasu", "de": "guten morgen"} to greetings
    append {"en": "how are you", "es": "como estas", "fr": "comment allez-vous", "hi": "aap kaise hain", "te": "meeru ela unnaru", "ja": "ogenki desu ka", "de": "wie geht es ihnen"} to greetings
    append {"en": "thank you", "es": "gracias", "fr": "merci", "hi": "dhanyavaad", "te": "dhanyavaadaalu", "ja": "arigatou", "de": "danke"} to greetings
    append {"en": "goodbye", "es": "adios", "fr": "au revoir", "hi": "alvida", "te": "selavu", "ja": "sayonara", "de": "auf wiedersehen"} to greetings
    append {"en": "i love you", "es": "te quiero", "fr": "je t'aime", "hi": "main tumse pyaar karta hoon", "te": "nenu ninnu premisthunnanu", "ja": "aishiteru", "de": "ich liebe dich"} to greetings
    append {"en": "please", "es": "por favor", "fr": "s'il vous plait", "hi": "kripya", "te": "dayachesi", "ja": "onegaishimasu", "de": "bitte"} to greetings
    append {"en": "yes", "es": "si", "fr": "oui", "hi": "haan", "te": "avunu", "ja": "hai", "de": "ja"} to greetings
    append {"en": "no", "es": "no", "fr": "non", "hi": "nahi", "te": "kaadu", "ja": "iie", "de": "nein"} to greetings
    // Pick the language key
    set lk to "es"
    if lang is equal "french":
        set lk to "fr"
    if lang is equal "hindi":
        set lk to "hi"
    if lang is equal "telugu":
        set lk to "te"
    if lang is equal "japanese":
        set lk to "ja"
    if lang is equal "german":
        set lk to "de"
    log ""
    log "  NC AI — Translate"
    log "  ─────────────────"
    log "  Target: " + lang
    log ""
    log "  Common Phrases:"
    set gi to 0
    repeat while gi is below len(greetings):
        set phrase to greetings[gi]
        if lk is equal "es":
            log "    " + phrase.en + " -> " + phrase.es
        if lk is equal "fr":
            log "    " + phrase.en + " -> " + phrase.fr
        if lk is equal "hi":
            log "    " + phrase.en + " -> " + phrase.hi
        if lk is equal "te":
            log "    " + phrase.en + " -> " + phrase.te
        if lk is equal "ja":
            log "    " + phrase.en + " -> " + phrase.ja
        if lk is equal "de":
            log "    " + phrase.en + " -> " + phrase.de
        set gi to gi + 1
    log ""
    log "  For full translation, use:"
    log "    nc run nc-ai/cortex/inference.nc -b generate"
    log "    with mode: translate"
    respond with {"language": lang, "module": "translate"}

// ═══════════════════════════════════════════════════════════
//  REASON — Step-by-step reasoning
// ═══════════════════════════════════════════════════════════

to answer_reason with question and sub_type:
    purpose: "Generate a reasoning-based answer with actual guidance"
    set steps to []
    set answer to ""
    set confidence to 0.7
    if sub_type is equal "mathematical":
        append "Identify the quantities and unknowns" to steps
        append "Set up the mathematical relationship" to steps
        append "Apply proportional reasoning or formula" to steps
        append "Calculate the result" to steps
        append "Verify: does the answer make sense?" to steps
        set answer to "Break the problem into known values and unknowns. Identify the formula (distance=speed*time, area=length*width, etc). Substitute values and solve step by step. Always check your answer with a sanity test."
        set confidence to 0.85
    if sub_type is equal "debugging":
        append "Identify the symptom (what exactly fails?)" to steps
        append "Check recent changes (what changed?)" to steps
        append "Reproduce the issue (can you trigger it reliably?)" to steps
        append "Check logs, metrics, stack trace" to steps
        append "Hypothesize root cause" to steps
        append "Apply fix and verify" to steps
        set answer to "Start with the error message — it usually tells you where to look. Check git log for recent changes. Reproduce it locally. Read logs for the actual error (not just the symptom). Fix one thing at a time and verify each fix."
        set confidence to 0.80
    if sub_type is equal "planning":
        append "Define the goal and constraints" to steps
        append "Break into phases" to steps
        append "Identify dependencies and risks" to steps
        append "Create timeline and milestones" to steps
        append "Define success criteria" to steps
        set answer to "Start with the end state you want. Work backwards to identify the phases. For each phase, list what must be true before it can start (dependencies) and what could go wrong (risks). Set concrete milestones so you know if you are on track."
        set confidence to 0.75
    if sub_type is equal "comparison":
        append "Identify items to compare" to steps
        append "Define comparison criteria" to steps
        append "Evaluate each item on each criterion" to steps
        append "Identify trade-offs" to steps
        append "Summarize key differentiators" to steps
        set answer to "Every comparison depends on context. Define what matters for YOUR use case first (performance? cost? team expertise? ecosystem?). Evaluate each option against those criteria. There is rarely a universally better option — only better for your situation."
        set confidence to 0.75
    if sub_type is equal "causal":
        append "Observe the effect clearly" to steps
        append "Trace back through the system" to steps
        append "Identify candidate causes" to steps
        append "Test each hypothesis" to steps
        append "Confirm root cause" to steps
        set answer to "The symptom is not the cause. Trace backwards from what you observe to what could produce it. List every candidate cause. For each one, ask: if this were the cause, what else would be true? Check for those signals. The real root cause will explain all the symptoms."
        set confidence to 0.80
    if sub_type is equal "explain":
        append "Define the concept clearly" to steps
        append "Break into components" to steps
        append "Explain how components interact" to steps
        append "Give a concrete example" to steps
        set answer to "Start with a one-sentence definition. Then break it into the key parts. Explain how those parts work together. Finally, show a real example that makes it concrete. The best explanations go from abstract to specific."
        set confidence to 0.75
    if len(steps) is equal 0:
        append "Understand the question" to steps
        append "Gather relevant information" to steps
        append "Reason through the problem" to steps
        append "Formulate answer" to steps
        set answer to "Let me think through this step by step. Every good answer starts with understanding what is really being asked."
        set confidence to 0.65
    log ""
    log "  NC AI — Reason"
    log "  ──────────────"
    log "  Type: " + sub_type
    log "  Confidence: " + str(confidence)
    log ""
    log "  Approach:"
    set si to 0
    repeat while si is below len(steps):
        log "    " + str(si + 1) + ". " + steps[si]
        set si to si + 1
    log ""
    log "  Answer:"
    log "    " + answer
    log ""
    respond with {"question": question, "type": sub_type, "steps": steps, "answer": answer, "confidence": confidence, "module": "reason"}

// ═══════════════════════════════════════════════════════════
//  CODEGEN — Code generation
// ═══════════════════════════════════════════════════════════

to answer_codegen with question:
    purpose: "Generate code description and scaffold from prompt"
    set q to lower(question)
    set qlen to len(q)
    set entities to []
    if len(replace(q, "user", "")) is below qlen:
        append "user" to entities
    if len(replace(q, "task", "")) is below qlen:
        append "task" to entities
    if len(replace(q, "product", "")) is below qlen:
        append "product" to entities
    if len(replace(q, "order", "")) is below qlen:
        append "order" to entities
    if len(replace(q, "post", "")) is below qlen:
        append "post" to entities
    if len(replace(q, "comment", "")) is below qlen:
        append "comment" to entities
    if len(replace(q, "item", "")) is below qlen:
        append "item" to entities
    if len(entities) is equal 0:
        append "item" to entities
    set app_type to "api"
    if len(replace(q, "cli", "")) is below qlen:
        set app_type to "cli"
    if len(replace(q, "pipeline", "")) is below qlen:
        set app_type to "pipeline"
    set has_auth to "no"
    if len(replace(q, "auth", "")) is below qlen:
        set has_auth to "yes"
    if len(replace(q, "login", "")) is below qlen:
        set has_auth to "yes"
    log ""
    log "  NC AI — Codegen"
    log "  ───────────────"
    log "  App type: " + app_type
    log "  Entities: " + str(len(entities))
    set ei to 0
    repeat while ei is below len(entities):
        log "    - " + entities[ei]
        set ei to ei + 1
    log "  Auth: " + has_auth
    log ""
    log "  Generated structure:"
    log "    service \"my-app\""
    log "    version \"1.0.0\""
    log "    configure:"
    log "        port is 9000"
    log ""
    set ci to 0
    repeat while ci is below len(entities):
        set e to entities[ci]
        log "    // " + e + " CRUD"
        log "    to create_" + e + " with data:"
        log "    to list_" + e + "s:"
        log "    to get_" + e + " with id:"
        log "    to update_" + e + " with id and data:"
        log "    to delete_" + e + " with id:"
        log ""
        set ci to ci + 1
    log "  For full code generation:"
    log "    nc run nc-ai/cortex/codegen.nc -b demo"
    log ""
    respond with {"type": app_type, "entities": entities, "auth": has_auth, "module": "codegen"}

// ═══════════════════════════════════════════════════════════
//  DECIDE — Decision engine
// ═══════════════════════════════════════════════════════════

to answer_decide with question:
    purpose: "Decision-based answer with reasoning"
    set q to lower(question)
    set qlen to len(q)
    set state to "normal"
    if len(replace(q, "crash", "")) is below qlen:
        set state to "critical"
    if len(replace(q, "slow", "")) is below qlen:
        set state to "degraded"
    if len(replace(q, "deploy", "")) is below qlen:
        set state to "deployment"
    if len(replace(q, "security", "")) is below qlen:
        set state to "security"
    if len(replace(q, "hire", "")) is below qlen:
        set state to "hiring"
    if len(replace(q, "buy", "")) is below qlen:
        set state to "purchase"
    if len(replace(q, "framework", "")) is below qlen:
        set state to "technology"
    if len(replace(q, "database", "")) is below qlen:
        set state to "technology"
    if len(replace(q, "language", "")) is below qlen:
        set state to "technology"
    set recommendation to "Gather more data before deciding. List pros and cons for each option. Consider reversibility — prefer decisions that are easy to undo."
    set reasoning to "The best decision framework: (1) What are the options? (2) What are the criteria? (3) What are the risks? (4) What is reversible?"
    if state is equal "critical":
        set recommendation to "Rollback to last known good state immediately. Investigate root cause after stability is restored. Do not deploy fixes on top of a broken state."
        set reasoning to "In critical situations: stabilize first, investigate second. The cost of downtime exceeds the cost of a rollback."
    if state is equal "degraded":
        set recommendation to "Scale up resources as immediate relief. Profile the hot paths to find the bottleneck. Optimize the top 1-2 offenders for 80% improvement."
        set reasoning to "Degraded performance is usually caused by 1-2 bottlenecks. Find them with profiling, fix them specifically."
    if state is equal "deployment":
        set recommendation to "Use canary deployment with 5% traffic initially. Monitor error rate and latency for 15 minutes. If clean, ramp to 25%, then 50%, then 100%."
        set reasoning to "Canary deployments limit blast radius. If something goes wrong, only 5% of users are affected and rollback is instant."
    if state is equal "security":
        set recommendation to "Isolate affected systems immediately. Rotate all credentials. Audit access logs for the last 72 hours. Patch the vulnerability before reconnecting."
        set reasoning to "Security incidents require immediate containment. Assume the worst, contain first, assess second."
    if state is equal "hiring":
        set recommendation to "Define must-have vs nice-to-have skills clearly. Use structured interviews with the same questions. Prioritize problem-solving ability over specific technology experience."
        set reasoning to "Good engineers learn technologies quickly. Hire for thinking ability and culture fit, train for specific tools."
    if state is equal "technology":
        set recommendation to "Evaluate based on: team expertise, community size, long-term maintenance, and your specific requirements. Avoid choosing based on hype alone."
        set reasoning to "The best technology is the one your team can ship and maintain. Proven and boring often beats shiny and new."
    log ""
    log "  NC AI — Decision Engine"
    log "  ───────────────────────"
    log "  State: " + state
    log ""
    log "  Recommendation:"
    log "    " + recommendation
    log ""
    log "  Reasoning:"
    log "    " + reasoning
    log ""
    respond with {"state": state, "recommendation": recommendation, "reasoning": reasoning, "module": "decision"}

// ═══════════════════════════════════════════════════════════
//  GENERAL — Greetings, help, conversation
// ═══════════════════════════════════════════════════════════

to answer_general with question and sub_type:
    purpose: "Handle general conversation, greetings, and help"
    if sub_type is equal "greeting":
        log ""
        log "  NC AI"
        log "  ─────"
        log "  Hello! I am NC AI, your local AI assistant."
        log "  I run entirely on your machine — no cloud, no API keys, no data leaves your computer."
        log ""
        log "  Try asking me:"
        log "    - \"Write an email about a project update\""
        log "    - \"Why does my server crash under load?\""
        log "    - \"Write a story about space\""
        log "    - \"Translate hello to Japanese\""
        log "    - \"Build a task management API\""
        log "    - \"Compare PostgreSQL vs MongoDB\""
        log "    - \"Calculate 300km at 60km per hour\""
        log ""
        respond with {"response": "greeting", "module": "general"}
    if sub_type is equal "help":
        log ""
        log "  NC AI — What I Can Do"
        log "  ─────────────────────"
        log ""
        log "  Write & Compose:"
        log "    - Emails (meeting, leave, resignation, incident, followup, proposal)"
        log "    - Letters and messages"
        log "    - Creative writing (stories, poems)"
        log ""
        log "  Reason & Analyze:"
        log "    - Debug errors and crashes"
        log "    - Explain concepts"
        log "    - Math and logic problems"
        log "    - Compare technologies"
        log "    - Root cause analysis"
        log ""
        log "  Build & Plan:"
        log "    - Generate code and APIs"
        log "    - Plan deployments and migrations"
        log "    - Architecture decisions"
        log ""
        log "  Translate:"
        log "    - Spanish, French, Hindi, Telugu, Japanese, German"
        log ""
        log "  Decide:"
        log "    - Technology choices"
        log "    - Deployment strategies"
        log "    - Incident response"
        log ""
        log "  Everything runs locally. No internet required."
        log ""
        respond with {"response": "help", "module": "general"}
    // General conversation fallback
    log ""
    log "  NC AI"
    log "  ─────"
    log "  I understand your question: \"" + question + "\""
    log ""
    log "  I can help you best with:"
    log "    - Writing (emails, stories, poems) — try \"write an email about...\""
    log "    - Debugging — try \"why does my app crash...\""
    log "    - Explaining — try \"explain...\""
    log "    - Building — try \"build a... API\""
    log "    - Reviewing — try \"review my code\" or \"security review\""
    log "    - Comparing — try \"compare X vs Y\""
    log "    - Translating — try \"translate... to Spanish\""
    log "    - Deciding — try \"should I...\""
    log ""
    log "  Type \"help\" to see all my capabilities."
    log ""
    respond with {"question": question, "module": "general"}

// ═══════════════════════════════════════════════════════════
//  REVIEW — Code review guidance and recommendations
// ═══════════════════════════════════════════════════════════

to answer_review with question and sub_type:
    purpose: "Provide code review guidance and security recommendations"
    set q to lower(question)
    set qlen to len(q)
    log ""
    log "  NC AI — Code Review"
    log "  ────────────────────"
    // ── Security review ──
    if sub_type is equal "security":
        log ""
        log "  Security Review Checklist:"
        log ""
        log "  [Critical] Check for these vulnerabilities:"
        log "    - SQL Injection: Never concatenate user input into SQL queries"
        log "      Fix: Use parameterized queries / prepared statements"
        log "    - XSS (Cross-Site Scripting): Never use innerHTML with untrusted data"
        log "      Fix: Use textContent or DOM sanitization libraries"
        log "    - Command Injection: Never pass user input to system() / eval() / exec()"
        log "      Fix: Use safe APIs like execFile with argument arrays"
        log "    - Buffer Overflow (C/C++): Never use gets(), strcpy(), sprintf()"
        log "      Fix: Use fgets(), strncpy(), snprintf()"
        log "    - Hardcoded Secrets: Search for passwords, API keys, tokens in source"
        log "      Fix: Use environment variables or secret managers"
        log "    - Deserialization: Python pickle.load / Java ObjectInputStream"
        log "      Fix: Only deserialize trusted data, use safe alternatives"
        log ""
        log "  [Important] Also check:"
        log "    - Path traversal: Validate file paths, reject ../"
        log "    - CORS: Restrict Access-Control-Allow-Origin"
        log "    - Authentication: Verify all endpoints require auth"
        log "    - Logging: Never log passwords or tokens"
        log ""
        log "  Run full scan: nc ai review <your-directory>"
        log ""
        respond with {"type": "security_review", "module": "review"}
    // ── Quality review ──
    if sub_type is equal "quality":
        log ""
        log "  Code Quality Review Checklist:"
        log ""
        log "  [Structure]"
        log "    - Files > 500 lines? Split into smaller modules"
        log "    - Functions > 50 lines? Refactor into smaller functions"
        log "    - Nesting > 3 levels? Flatten with early returns or extract methods"
        log "    - Duplicate code? Extract into shared functions"
        log ""
        log "  [Naming & Clarity]"
        log "    - Magic numbers? Replace with named constants"
        log "    - Single-letter variables? Use descriptive names"
        log "    - Unclear function names? Name by what it does, not how"
        log ""
        log "  [Anti-Patterns to Fix]"
        log "    - Python: bare except, wildcard import *, mutable defaults"
        log "    - Java: System.out.println, catch(Exception), raw Thread"
        log "    - JavaScript: var (use let/const), == (use ===), console.log"
        log "    - Go: ignored errors (_ = err), panic in libraries, fmt.Println"
        log "    - C: gets(), strcpy(), sprintf(), global mutable state"
        log "    - NC: function (use to), return (use respond with)"
        log ""
        log "  Run full scan: nc ai review <your-directory>"
        log ""
        respond with {"type": "quality_review", "module": "review"}
    // ── Bug finding ──
    if sub_type is equal "bugs":
        log ""
        log "  Bug Detection Checklist:"
        log ""
        log "  [Common Bug Patterns]"
        log "    - Off-by-one errors in loops and array indexing"
        log "    - Null/None dereference without checking"
        log "    - Race conditions in concurrent code"
        log "    - Resource leaks: unclosed files, connections, sockets"
        log "    - Integer overflow in arithmetic operations"
        log "    - Type coercion bugs (JavaScript == vs ===)"
        log "    - Unhandled error returns (Go, C)"
        log ""
        log "  [Detection Tools]"
        log "    - Python: pylint, mypy, bandit"
        log "    - Java: SpotBugs, PMD, SonarQube"
        log "    - JavaScript: ESLint, TypeScript strict mode"
        log "    - Go: go vet, staticcheck, golangci-lint"
        log "    - C: Valgrind, AddressSanitizer, cppcheck"
        log "    - NC: nc ai review <file>"
        log ""
        log "  Run full scan: nc ai review <your-directory>"
        log ""
        respond with {"type": "bug_review", "module": "review"}
    // ── General codebase review ──
    log ""
    log "  I can review your code for:"
    log ""
    log "  1. Security vulnerabilities"
    log "     SQL injection, XSS, command injection, hardcoded secrets"
    log "     Ask: \"security review\" or \"find vulnerabilities\""
    log ""
    log "  2. Code quality"
    log "     Anti-patterns, code smells, naming, structure"
    log "     Ask: \"code quality\" or \"find anti-patterns\""
    log ""
    log "  3. Bug detection"
    log "     Common bug patterns, race conditions, resource leaks"
    log "     Ask: \"find bugs\" or \"find issues\""
    log ""
    log "  4. Full automated scan"
    log "     Run: nc ai review <file-or-directory>"
    log "     Supports: Python, Java, JavaScript, TypeScript, Go, C, NC, Rust"
    log ""
    log "  Languages supported: NC, Python, Java, JavaScript, TypeScript, Go, C, C++, Rust, Ruby, PHP"
    log ""
    respond with {"type": "general_review", "module": "review"}

// ═══════════════════════════════════════════════════════════
//  MAIN CHAT HANDLER — One function routes everything
// ═══════════════════════════════════════════════════════════

to chat_once with question:
    purpose: "Answer a single question using the best AI module"
    set route to route_question(question)
    if route.intent is equal "compose":
        set result to answer_compose(question, route.type)
        respond with result
    if route.intent is equal "creative":
        set result to answer_creative(question, route.type)
        respond with result
    if route.intent is equal "summarize":
        set result to answer_summarize(question)
        respond with result
    if route.intent is equal "translate":
        set result to answer_translate(question, route.type)
        respond with result
    if route.intent is equal "review":
        set result to answer_review(question, route.type)
        respond with result
    if route.intent is equal "codegen":
        set result to answer_codegen(question)
        respond with result
    if route.intent is equal "decide":
        set result to answer_decide(question)
        respond with result
    if route.intent is equal "reason":
        set result to answer_reason(question, route.type)
        respond with result
    // General / greeting / help / fallback
    set result to answer_general(question, route.type)
    respond with result

// ═══════════════════════════════════════════════════════════
//  DEMO — Show all capabilities
// ═══════════════════════════════════════════════════════════

to demo:
    purpose: "Demo NC AI chat with various question types"
    log "========================================================="
    log "  NC AI Chat v1.0 — Demo Conversation"
    log "  DevHeal Labs AI"
    log "========================================================="
    set questions to ["Hello!", "Write an email about a project update", "Why does my API timeout under heavy load?", "Build a task management API with users", "Calculate how long to travel 300km at 60km per hour", "Write a poem about technology", "Compare PostgreSQL vs MongoDB", "Should I use canary or blue-green deployment?", "Translate hello to Japanese", "What can you do?"]
    set qi to 0
    repeat while qi is below len(questions):
        set q to questions[qi]
        log ""
        log "  --- Question " + str(qi + 1) + " ---"
        log "  User: " + q
        chat_once(q)
        set qi to qi + 1
    log ""
    log "========================================================="
    log "  Demo Complete — " + str(len(questions)) + " questions answered"
    log "  All processing done locally on your machine."
    log "========================================================="
    respond with {"questions": len(questions), "status": "complete"}

// ═══════════════════════════════════════════════════════════
//  HEALTH
// ═══════════════════════════════════════════════════════════

to health:
    respond with {"service": "nc-ai-chat", "status": "running", "version": "1.0.0", "capabilities": ["compose", "creative", "reason", "codegen", "decide", "translate", "summarize", "review", "general"]}

api:
    POST /chat runs chat_once
    GET /demo runs demo
    GET /health runs health
