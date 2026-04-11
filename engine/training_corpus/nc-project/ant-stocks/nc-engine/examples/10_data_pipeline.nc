service "data-pipeline"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o-mini"

to process_csv with file_path:
    purpose: "Read a CSV file, analyze it with AI, generate insights"

    set raw_data to read_file(file_path)

    ask AI to "Analyze this CSV data. Return JSON with: row_count (number), columns (list of names), summary (what this data is about), insights (list of 3 key findings), quality_issues (list of any data quality problems). Data: {{raw_data}}" save as analysis

    log "Processed {{file_path}}: {{analysis.row_count}} rows, {{analysis.summary}}"

    respond with analysis

to generate_report with data, report_type:
    purpose: "Generate a report from structured data"

    ask AI to "Generate a {{report_type}} report from this data. Include sections: Executive Summary, Key Metrics, Recommendations. Format as markdown. Data: {{data}}" save as report

    set timestamp to time_now()
    write_file("report_output.md", report)
    log "Report generated at {{timestamp}}"

    respond with {"report": report, "generated_at": timestamp}

to clean_data with raw_text:
    purpose: "Clean and normalize messy data using AI"

    ask AI to "Clean and normalize this data. Fix typos, standardize formats (dates, phone numbers, addresses), remove duplicates. Return clean JSON. Data: {{raw_text}}" save as cleaned

    respond with cleaned

api:
    POST /process runs process_csv
    POST /report runs generate_report
    POST /clean runs clean_data
