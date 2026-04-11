service "data-parsers"
version "1.0.0"
description "Parse and transform data formats — CSV, JSON, text"

to parse_csv_file with file_path:
    purpose: "Read and parse a CSV file"
    set raw to read_file(file_path)
    set parsed to csv_parse(raw)
    respond with parsed

to parse_json_file with file_path:
    purpose: "Read and parse a JSON file"
    set raw to read_file(file_path)
    set parsed to json_decode(raw)
    respond with parsed

to file_to_chunks with file_path, chunk_size:
    purpose: "Read a file and split into chunks for RAG"
    set raw to read_file(file_path)
    set chunks to chunk(raw, chunk_size)
    respond with {"chunks": chunks, "count": len(chunks), "source": file_path}

to merge_files with file_paths:
    purpose: "Read and merge multiple files into one string"
    set merged to ""
    repeat for each path in file_paths:
        set content to read_file(path)
        set merged to merged + "\n---\n" + content
    respond with merged
