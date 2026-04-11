// NC Standard Library — migrate
// AI-powered code migration and hybrid orchestration
// Converts code from any language to NC, or wraps it for hybrid execution

service "nc.migrate"
version "1.0.0"
description "Code migration toolkit — convert or wrap any language for NC"

// ═══════════════════════════════════════════════════════════
//  Equivalence Tables — Language → NC pattern mappings
// ═══════════════════════════════════════════════════════════

to get_python_equivalences:
    set table to {
        "print()": "show",
        "def function():": "to behavior_name:",
        "return value": "respond with value",
        "if condition:": "if condition:",
        "elif condition:": "otherwise if condition:",
        "else:": "otherwise:",
        "for item in list:": "repeat for each item in list:",
        "for i in range(n):": "repeat n times:",
        "while condition:": "while condition:",
        "x = value": "set x to value",
        "list.append(x)": "append x to list",
        "list.remove(x)": "remove x from list",
        "len(x)": "len(x)",
        "str.upper()": "upper(str)",
        "str.lower()": "lower(str)",
        "str.strip()": "trim(str)",
        "str.split()": "split(str)",
        "','.join(list)": "join(list, \",\")",
        "str.replace(a, b)": "replace(str, a, b)",
        "try: / except:": "try: / on error:",
        "import module": "import \"module\"",
        "class Name:": "define Name as:",
        "dict / {}": "record / {}",
        "list / []": "list / []",
        "True / False": "yes / no",
        "None": "nothing",
        "f\"Hello {name}\"": "\"Hello {{name}}\"",
        "isinstance(x, int)": "is_number(x)",
        "isinstance(x, str)": "is_text(x)",
        "isinstance(x, list)": "is_list(x)",
        "isinstance(x, dict)": "is_record(x)",
        "json.dumps(x)": "json_encode(x)",
        "json.loads(x)": "json_decode(x)",
        "os.environ.get()": "env(\"KEY\")",
        "subprocess.run()": "exec(\"command\")",
        "requests.get(url)": "gather from url",
        "open(file).read()": "read_file(file)",
        "open(file).write()": "write_file(file, content)"
    }
    respond with table

to get_java_equivalences:
    set table to {
        "System.out.println()": "show",
        "public void method()": "to behavior_name:",
        "public Type method()": "to behavior_name:",
        "return value;": "respond with value",
        "if (cond) {": "if cond:",
        "} else if (cond) {": "otherwise if cond:",
        "} else {": "otherwise:",
        "for (Type x : list)": "repeat for each x in list:",
        "for (int i=0; i<n; i++)": "repeat n times:",
        "while (cond) {": "while cond:",
        "Type x = value;": "set x to value",
        "list.add(x)": "append x to list",
        "list.remove(x)": "remove x from list",
        "list.size()": "len(list)",
        "str.toUpperCase()": "upper(str)",
        "str.toLowerCase()": "lower(str)",
        "str.trim()": "trim(str)",
        "str.split()": "split(str)",
        "String.join()": "join(list, sep)",
        "str.replace(a, b)": "replace(str, a, b)",
        "try { } catch (E e) { }": "try: / on error e:",
        "import package;": "import \"module\"",
        "class Name {": "define Name as:",
        "HashMap / Map": "record / {}",
        "ArrayList / List": "list / []",
        "true / false": "yes / no",
        "null": "nothing",
        "new ObjectMapper().writeValueAsString()": "json_encode(x)",
        "new ObjectMapper().readValue()": "json_decode(x)",
        "System.getenv()": "env(\"KEY\")",
        "Runtime.exec()": "exec(\"command\")"
    }
    respond with table

to get_javascript_equivalences:
    set table to {
        "console.log()": "show",
        "function name() {": "to behavior_name:",
        "const name = () => {": "to behavior_name:",
        "return value;": "respond with value",
        "if (cond) {": "if cond:",
        "} else if (cond) {": "otherwise if cond:",
        "} else {": "otherwise:",
        "for (const x of list)": "repeat for each x in list:",
        "for (let i=0; i<n; i++)": "repeat n times:",
        "while (cond) {": "while cond:",
        "const/let/var x = value": "set x to value",
        "array.push(x)": "append x to list",
        "array.splice()": "remove x from list",
        "array.length": "len(list)",
        "str.toUpperCase()": "upper(str)",
        "str.toLowerCase()": "lower(str)",
        "str.trim()": "trim(str)",
        "str.split()": "split(str)",
        "array.join()": "join(list, sep)",
        "str.replace()": "replace(str, a, b)",
        "try { } catch (e) { }": "try: / on error e:",
        "require() / import": "import \"module\"",
        "class Name {": "define Name as:",
        "Object / {}": "record / {}",
        "Array / []": "list / []",
        "true / false": "yes / no",
        "null / undefined": "nothing",
        "`Hello ${name}`": "\"Hello {{name}}\"",
        "JSON.stringify()": "json_encode(x)",
        "JSON.parse()": "json_decode(x)",
        "process.env.KEY": "env(\"KEY\")",
        "fetch(url)": "gather from url",
        "fs.readFileSync()": "read_file(file)",
        "fs.writeFileSync()": "write_file(file, content)"
    }
    respond with table

to get_go_equivalences:
    set table to {
        "fmt.Println()": "show",
        "func name() {": "to behavior_name:",
        "return value": "respond with value",
        "if cond {": "if cond:",
        "} else if cond {": "otherwise if cond:",
        "} else {": "otherwise:",
        "for _, x := range list {": "repeat for each x in list:",
        "for i := 0; i < n; i++ {": "repeat n times:",
        "for cond {": "while cond:",
        "x := value": "set x to value",
        "append(slice, x)": "append x to list",
        "len(x)": "len(x)",
        "strings.ToUpper()": "upper(str)",
        "strings.ToLower()": "lower(str)",
        "strings.TrimSpace()": "trim(str)",
        "strings.Split()": "split(str)",
        "strings.Join()": "join(list, sep)",
        "strings.Replace()": "replace(str, a, b)",
        "if err != nil {": "on error:",
        "import \"package\"": "import \"module\"",
        "type Name struct {": "define Name as:",
        "map[string]interface{}": "record / {}",
        "[]Type": "list / []",
        "true / false": "yes / no",
        "nil": "nothing",
        "json.Marshal()": "json_encode(x)",
        "json.Unmarshal()": "json_decode(x)",
        "os.Getenv()": "env(\"KEY\")",
        "exec.Command()": "exec(\"command\")"
    }
    respond with table

// ═══════════════════════════════════════════════════════════
//  Helper: show migration reference for a language
// ═══════════════════════════════════════════════════════════

to show_equivalences with language:
    match language:
        when "python":
            set table to get_python_equivalences()
        when "java":
            set table to get_java_equivalences()
        when "javascript":
            set table to get_javascript_equivalences()
        when "go":
            set table to get_go_equivalences()
        otherwise:
            show "Supported languages: python, java, javascript, go"
            respond with nothing

    show "NC Migration Reference — {{language}}"
    show "======================================"
    repeat for each key in table:
        set nc_equiv to table.{{key}}
        show "  {{key}}  →  {{nc_equiv}}"

// ═══════════════════════════════════════════════════════════
//  AI-powered migration behavior (callable from NC code)
// ═══════════════════════════════════════════════════════════

to migrate_code with source_code and language:
    purpose: "Convert code from another language to NC using AI"
    set prompt to "Convert this {{language}} code to valid NC (Notation-as-Code). Output ONLY the NC code, no markdown or explanations.\n\n{{source_code}}"
    ask AI to prompt:
        save as: result
    respond with result

to migrate_file with filepath:
    purpose: "Read a file and convert its code to NC"
    set source to read_file(filepath)
    if source is empty:
        respond with "Error: could not read {{filepath}}"

    // Detect language from extension
    if filepath ends_with ".py":
        set lang to "Python"
    otherwise if filepath ends_with ".java":
        set lang to "Java"
    otherwise if filepath ends_with ".js":
        set lang to "JavaScript"
    otherwise if filepath ends_with ".ts":
        set lang to "TypeScript"
    otherwise if filepath ends_with ".go":
        set lang to "Go"
    otherwise if filepath ends_with ".rb":
        set lang to "Ruby"
    otherwise if filepath ends_with ".rs":
        set lang to "Rust"
    otherwise if filepath ends_with ".php":
        set lang to "PHP"
    otherwise if filepath ends_with ".cs":
        set lang to "C#"
    otherwise:
        set lang to "Unknown"

    set result to migrate_code(source, lang)
    respond with result

// ═══════════════════════════════════════════════════════════
//  Hybrid wrapper generator (callable from NC code)
// ═══════════════════════════════════════════════════════════

to wrap_for_hybrid with filepath and runtime:
    purpose: "Generate NC hybrid wrapper that shells out to the original runtime"
    set source to read_file(filepath)
    if source is empty:
        respond with "Error: could not read {{filepath}}"

    set prompt to "Generate an NC hybrid wrapper for this code. NC should orchestrate calls to the original {{runtime}} runtime using exec() or shell(). Keep the heavy computation in {{runtime}}, NC handles the orchestration. Output ONLY valid NC code.\n\nSource file: {{filepath}}\n\n{{source}}"
    ask AI to prompt:
        save as: wrapper
    respond with wrapper
