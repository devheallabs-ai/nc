// ══════════════════════════════════════════════════════════════════
//  HiveANT — Architect Agent (Autonomous Software Development)
//
//  Analyzes and designs system architecture. Identifies architectural
//  drift, suggests improvements, and maintains design documentation.
// ══════════════════════════════════════════════════════════════════

to analyze_architecture with codebase_path, description:
    purpose: "Analyze system architecture and identify improvements"

    set twin_data to "none"
    if file_exists("memory/twins/system_model.json"):
        set twin_data to read_file("memory/twins/system_model.json")

    ask AI to "You are an Architect Agent. Analyze this system architecture. CODEBASE: {{codebase_path}}. DESCRIPTION: {{description}}. DIGITAL TWIN: {{twin_data}}. Analyze: 1) Component coupling and cohesion. 2) Dependency complexity. 3) Scalability bottlenecks. 4) Single points of failure. 5) Architecture drift from intended design. Return ONLY valid JSON: {\"architecture_score\": 0.0, \"components\": [{\"name\": \"string\", \"type\": \"string\", \"coupling\": \"low|medium|high\", \"cohesion\": \"low|medium|high\"}], \"issues\": [{\"issue\": \"string\", \"severity\": \"critical|high|medium|low\", \"recommendation\": \"string\"}], \"improvements\": [{\"area\": \"string\", \"suggestion\": \"string\", \"impact\": \"string\", \"effort\": \"low|medium|high\"}], \"spof\": [\"string\"]}" save as arch_result

    respond with arch_result

to design_component with requirements, constraints:
    purpose: "Design a new system component"

    ask AI to "You are an Architect Agent. Design a system component. REQUIREMENTS: {{requirements}}. CONSTRAINTS: {{constraints}}. Return ONLY valid JSON: {\"component\": {\"name\": \"string\", \"purpose\": \"string\", \"interfaces\": [{\"method\": \"string\", \"input\": \"string\", \"output\": \"string\"}], \"dependencies\": [\"string\"], \"data_model\": [{\"entity\": \"string\", \"fields\": [\"string\"]}], \"scalability\": \"string\", \"deployment\": \"string\"}, \"diagram_description\": \"string\"}" save as design

    respond with design
