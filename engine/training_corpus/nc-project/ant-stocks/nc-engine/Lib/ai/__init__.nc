// NC Standard Library — ai
// AI and machine learning operations
// This is what makes NC unique — AI is a first-class citizen

service "nc.ai"
version "1.0.0"
description "Built-in AI capabilities"

to classify with text and categories:
    purpose: "Classify text into one of the given categories"
    ask AI to "classify this text into one of these categories and respond with JSON containing category and confidence" using text, categories:
        confidence: 0.7
        save as: classification
    respond with classification

to summarize with text:
    purpose: "Create a concise summary of the text"
    ask AI to "summarize this text concisely, preserving key information" using text:
        save as: summary
    respond with summary

to analyze with data:
    purpose: "Find patterns and insights in data"
    ask AI to "analyze this data, find patterns, anomalies, and insights" using data:
        save as: analysis
    respond with analysis

to generate with prompt:
    purpose: "Generate text from a prompt"
    ask AI to prompt:
        save as: generated
    respond with generated

to extract with text and fields:
    purpose: "Extract structured data from unstructured text"
    ask AI to "extract these fields from the text and respond as JSON" using text, fields:
        save as: extracted
    respond with extracted

to translate with text and target_language:
    purpose: "Translate text to another language"
    ask AI to "translate this text to the target language" using text, target_language:
        save as: translated
    respond with translated

to sentiment with text:
    purpose: "Determine the sentiment of text"
    ask AI to "analyze the sentiment as positive, negative, or neutral with confidence score" using text:
        save as: result
    respond with result

to compare with text_a and text_b:
    purpose: "Compare two texts for similarity and differences"
    ask AI to "compare these two texts, identify similarities and differences" using text_a, text_b:
        save as: comparison
    respond with comparison

to explain with topic:
    purpose: "Get an explanation of a topic"
    ask AI to "explain this topic clearly and concisely" using topic:
        save as: explanation
    respond with explanation

to decide with question and context:
    purpose: "Make a decision based on context"
    ask AI to "analyze the context and make a recommendation for this question" using question, context:
        confidence: 0.8
        save as: decision
    respond with decision
