// Test: agent definition and run agent parsing
// Verifies that agent keyword, tool decorator, and run agent are parsed correctly

service "test-agent"

// Define tools
@tool
to search with query:
    purpose: "Search for information"
    set result to {"found": query, "source": "test"}
    respond with result

@tool
to summarize with text:
    purpose: "Summarize text content"
    set summary to "Summary of: " + str(text)
    respond with summary

@tool
to calculate with expression:
    purpose: "Evaluate math expressions"
    respond with {"result": 42, "expression": expression}

// Define agent
agent researcher:
    purpose: "Research topics and summarize findings"
    model: "test-model"
    tools: [search, summarize, calculate]
    max_steps: 5

// Test: tool behaviors work standalone
to test tool standalone:
    set result to search("quantum computing")
    assert result is not nothing, "search tool should return result"

to test summarize tool:
    set result to summarize("Hello world")
    assert result is not nothing, "summarize tool should return result"

to test calculate tool:
    set result to calculate("2 + 2")
    assert result is not nothing, "calculate tool should return result"

// Test: agent definition is parsed (doesn't crash)
to test agent definition parsed:
    // If we got here, parsing succeeded
    assert true, "agent definition parsed without error"

// Test: multiple agents can be defined
agent planner:
    purpose: "Plan and delegate tasks"
    tools: [search]
    max_steps: 3

to test multiple agents:
    assert true, "multiple agent definitions parsed"

// Test: agent with minimal config
agent simple:
    purpose: "A simple agent"
    tools: [search]

to test minimal agent:
    assert true, "minimal agent definition parsed"
