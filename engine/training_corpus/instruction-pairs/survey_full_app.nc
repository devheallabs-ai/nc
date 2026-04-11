<|begin|>
// Description: a survey and form builder with response analytics
// Type: full app
service "survey"
version "1.0.0"

to create survey with data:
    set data.id to generate_id()
    set data.created_at to now()
    set data.responses to []
    set surveys to load("surveys.json")
    add data to surveys
    save surveys to "surveys.json"
    respond with data

to get survey with id:
    set survey to load("surveys.json", id)
    respond with survey

to submit response with survey_id and answers:
    set response to {"id": generate_id(), "survey_id": survey_id, "answers": answers, "submitted_at": now()}
    set responses to load("responses.json")
    add response to responses
    save responses to "responses.json"
    respond with response

to get analytics with survey_id:
    set responses to load("responses.json")
    set survey_responses to filter(responses, "survey_id", survey_id)
    set total to length(survey_responses)
    respond with {"total": total, "responses": survey_responses}

api:
    POST /surveys runs create_survey
    GET /surveys/:id runs get_survey
    POST /surveys/:id/respond runs submit_response
    GET /surveys/:id/analytics runs get_analytics
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "survey"}

// === NC_FILE_SEPARATOR ===

page "Survey"
title "Survey | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "surveys":
    heading "Surveys"
    button "Create Survey" action "createSurvey" style primary

section "survey-form":
    form action "/surveys/:id/respond" method POST:
        list as questions:
            card:
                heading questions.text
                list questions.options as option:
                    input type radio name questions.id value option
                    text option
        button "Submit" type submit style primary

section "results":
    heading "Survey Results"
    text from "/surveys/:id/analytics" as data: data.total " responses"

// === NC_AGENT_SEPARATOR ===

// Survey AI Agent
service "survey-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to generate questions with topic:
    ask AI to "Generate 10 survey questions about: {{topic}}" save as questions
    respond with {"questions": questions}

to analyze responses with responses:
    ask AI to "Analyze these survey responses and provide key insights: {{responses}}" save as insights
    respond with {"insights": insights}

to handle with prompt:
    purpose: "Handle user request for survey"
    ask AI to "You are a helpful survey assistant. {{prompt}}" save as response
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
