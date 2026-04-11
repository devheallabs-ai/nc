const vscode = require('vscode');
const { spawn, execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

let lspProcess = null;
let outputChannel = null;
let statusBarItem = null;
let ncBinaryPath = 'nc';

/**
 * Locate the NC binary on this system.
 * Priority: user setting > well-known paths > workspace build dir > PATH lookup.
 */
function findNcBinary() {
    const configured = vscode.workspace.getConfiguration('nc').get('path', '');
    if (configured && configured !== 'nc') return configured;

    const isWindows = process.platform === 'win32';
    const binName = isWindows ? 'nc.exe' : 'nc';

    const searchPaths = [];

    if (isWindows) {
        const localApp = process.env.LOCALAPPDATA || '';
        if (localApp) searchPaths.push(path.join(localApp, 'nc', 'bin', binName));
        searchPaths.push(path.join(os.homedir(), '.nc', 'bin', binName));
    } else {
        searchPaths.push('/usr/local/bin/nc');
        searchPaths.push('/usr/bin/nc');
        searchPaths.push(path.join(os.homedir(), '.local', 'bin', 'nc'));
        searchPaths.push(path.join(os.homedir(), '.nc', 'bin', 'nc'));
    }

    const workspaceFolder = vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath;
    if (workspaceFolder) {
        searchPaths.push(path.join(workspaceFolder, 'nc', 'build', binName));
        searchPaths.push(path.join(workspaceFolder, 'build', binName));
    }

    for (const p of searchPaths) {
        if (fs.existsSync(p)) return p;
    }

    try {
        const whichCmd = isWindows ? 'where nc' : 'which nc';
        const result = execSync(whichCmd, { encoding: 'utf8', timeout: 3000 }).trim();
        if (result) return result.split('\n')[0].trim();
    } catch (e) { /* not in PATH */ }

    return 'nc';
}

/**
 * Try to get the NC version string from the binary.
 */
function getNcVersion(ncPath) {
    try {
        const result = execSync(`"${ncPath}" --version`, { encoding: 'utf8', timeout: 5000 }).trim();
        // Expect something like "nc 1.0.0" or just "1.0.0"
        return result;
    } catch (e) {
        return null;
    }
}

/**
 * Get the active .nc file path, or show an error and return null.
 */
function getActiveNcFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('No active editor. Open a .nc file first.');
        return null;
    }
    const file = editor.document.fileName;
    if (!file.endsWith('.nc')) {
        vscode.window.showErrorMessage('Current file is not a .nc file.');
        return null;
    }
    return file;
}

/**
 * Execute an NC CLI command, sending output to the NC output channel.
 */
function runNcCommand(ncPath, subcommand, file, terminalName) {
    const terminal = vscode.window.createTerminal(terminalName);
    terminal.show();
    terminal.sendText(`"${ncPath}" ${subcommand} "${file}"`);
}

/**
 * Execute an NC CLI command and capture output in the output channel.
 */
function runNcCommandInOutput(ncPath, args, label) {
    outputChannel.show(true);
    outputChannel.appendLine(`--- ${label} ---`);

    const proc = spawn(ncPath, args, {
        cwd: vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath || process.cwd(),
        shell: true
    });

    proc.stdout?.on('data', (data) => {
        outputChannel.append(data.toString());
    });

    proc.stderr?.on('data', (data) => {
        outputChannel.append(data.toString());
    });

    proc.on('error', (err) => {
        outputChannel.appendLine(`Error: ${err.message}`);
        vscode.window.showErrorMessage(`NC ${label} failed: ${err.message}`);
    });

    proc.on('close', (code) => {
        if (code === 0) {
            outputChannel.appendLine(`${label} completed successfully.`);
        } else {
            outputChannel.appendLine(`${label} exited with code ${code}.`);
        }
    });
}

/**
 * Create and update the status bar item.
 */
function setupStatusBar(context, ncPath) {
    const showStatus = vscode.workspace.getConfiguration('nc').get('showStatusBar', true);
    if (!showStatus) return;

    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.command = 'nc.run';
    statusBarItem.tooltip = 'NC Language — Click to run current file';

    const version = getNcVersion(ncPath);
    if (version) {
        statusBarItem.text = `$(zap) NC ${version}`;
    } else {
        statusBarItem.text = '$(zap) NC';
    }

    context.subscriptions.push(statusBarItem);

    // Only show when an NC file is active
    function updateVisibility() {
        const editor = vscode.window.activeTextEditor;
        if (editor && editor.document.languageId === 'nc') {
            statusBarItem.show();
        } else {
            statusBarItem.hide();
        }
    }

    updateVisibility();
    context.subscriptions.push(
        vscode.window.onDidChangeActiveTextEditor(() => updateVisibility())
    );
}

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('NC');

    ncBinaryPath = findNcBinary();
    const enableLSP = vscode.workspace.getConfiguration('nc').get('enableLSP', true);

    outputChannel.appendLine(`NC binary: ${ncBinaryPath}`);
    outputChannel.appendLine(`Platform: ${process.platform} / ${process.arch}`);

    // Warn if NC binary not found
    if (ncBinaryPath === 'nc') {
        try {
            execSync('which nc 2>/dev/null || where nc 2>nul', { encoding: 'utf8', timeout: 3000 });
        } catch (e) {
            const isWindows = process.platform === 'win32';
            const installMsg = isWindows
                ? 'NC binary not found. Install: powershell -File install.ps1 (from repo root)'
                : 'NC binary not found. Install: curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | bash';
            vscode.window.showWarningMessage(installMsg, 'Open Install Guide').then(choice => {
                if (choice === 'Open Install Guide') {
                    vscode.env.openExternal(vscode.Uri.parse('https://github.com/DevHealLabs/nc-lang#install'));
                }
            });
            outputChannel.appendLine('WARNING: NC binary not found in PATH or common locations');
        }
    }

    // Status bar
    setupStatusBar(context, ncBinaryPath);

    // LSP
    if (enableLSP) {
        startLSP(ncBinaryPath, context);
    }

    // --- Commands ---

    context.subscriptions.push(
        vscode.commands.registerCommand('nc.run', () => {
            const file = getActiveNcFile();
            if (!file) return;
            runNcCommand(ncBinaryPath, 'run', file, 'NC Run');
        }),

        vscode.commands.registerCommand('nc.serve', () => {
            const file = getActiveNcFile();
            if (!file) return;
            runNcCommand(ncBinaryPath, 'serve', file, 'NC Server');
        }),

        vscode.commands.registerCommand('nc.validate', () => {
            const file = getActiveNcFile();
            if (!file) return;
            runNcCommandInOutput(ncBinaryPath, ['validate', file], `Validate ${path.basename(file)}`);
        }),

        vscode.commands.registerCommand('nc.repl', () => {
            const terminal = vscode.window.createTerminal('NC REPL');
            terminal.show();
            terminal.sendText(`"${ncBinaryPath}" repl`);
        })
    );

    // --- Auto-validate on save ---

    const autoValidate = vscode.workspace.getConfiguration('nc').get('autoValidateOnSave', false);
    if (autoValidate) {
        context.subscriptions.push(
            vscode.workspace.onDidSaveTextDocument((doc) => {
                if (doc.languageId === 'nc') {
                    runNcCommandInOutput(ncBinaryPath, ['validate', doc.fileName], `Auto-validate ${path.basename(doc.fileName)}`);
                }
            })
        );
    }

    // --- Hover provider for built-in functions ---

    context.subscriptions.push(
        vscode.languages.registerHoverProvider('nc', {
            provideHover(document, position) {
                const range = document.getWordRangeAtPosition(position);
                if (!range) return null;
                const word = document.getText(range);
                const help = BUILTIN_HELP[word];
                if (help) {
                    const md = new vscode.MarkdownString();
                    md.appendMarkdown(`**${help.name}**\n\n`);
                    md.appendMarkdown(`${help.description}\n\n`);
                    if (help.syntax) md.appendCodeblock(help.syntax, 'nc');
                    if (help.example) {
                        md.appendMarkdown(`\n**Example:**\n`);
                        md.appendCodeblock(help.example, 'nc');
                    }
                    return new vscode.Hover(md);
                }
                return null;
            }
        })
    );

    // --- NC AI Commands ---

    // NC AI: Open Chat (sidebar or quick input)
    context.subscriptions.push(
        vscode.commands.registerCommand('nc.ai.chat', async () => {
            const question = await vscode.window.showInputBox({
                prompt: 'Ask NC AI anything',
                placeHolder: 'e.g., "Write an email about a meeting" or "Why does my server crash?"'
            });
            if (!question) return;
            runNcAiCommand(ncBinaryPath, 'reason', question);
        })
    );

    // NC AI: Review Current File
    context.subscriptions.push(
        vscode.commands.registerCommand('nc.ai.review', () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                vscode.window.showErrorMessage('No active editor');
                return;
            }
            const file = editor.document.fileName;
            runNcCommandInOutput(ncBinaryPath, ['ai', 'review', file], `AI Review: ${path.basename(file)}`);
        })
    );

    // NC AI: Explain Selection
    context.subscriptions.push(
        vscode.commands.registerCommand('nc.ai.explain', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) return;
            const selection = editor.document.getText(editor.selection);
            if (!selection) {
                vscode.window.showInformationMessage('Select some code first');
                return;
            }
            const question = `Explain this code:\n${selection}`;
            runNcAiCommand(ncBinaryPath, 'reason', question);
        })
    );

    // NC AI: Ask a Question
    context.subscriptions.push(
        vscode.commands.registerCommand('nc.ai.ask', async () => {
            const question = await vscode.window.showInputBox({
                prompt: 'Ask NC AI',
                placeHolder: 'e.g., "Compare Docker vs Kubernetes"'
            });
            if (!question) return;
            runNcAiCommand(ncBinaryPath, 'reason', question);
        })
    );

    // NC AI: Generate Code
    context.subscriptions.push(
        vscode.commands.registerCommand('nc.ai.generate', async () => {
            const prompt = await vscode.window.showInputBox({
                prompt: 'What should NC AI generate?',
                placeHolder: 'e.g., "Build a task management API with users"'
            });
            if (!prompt) return;
            runNcAiCommand(ncBinaryPath, 'reason', prompt);
        })
    );

    // NC AI Sidebar Webview
    const aiProvider = new NcAiViewProvider(context.extensionUri, ncBinaryPath);
    context.subscriptions.push(
        vscode.window.registerWebviewViewProvider('nc.ai.panel', aiProvider)
    );

    // --- Re-detect binary when settings change ---

    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration((e) => {
            if (e.affectsConfiguration('nc.path')) {
                ncBinaryPath = findNcBinary();
                outputChannel.appendLine(`NC binary path updated: ${ncBinaryPath}`);
                if (statusBarItem) {
                    const version = getNcVersion(ncBinaryPath);
                    statusBarItem.text = version ? `$(zap) NC ${version}` : '$(zap) NC';
                }
            }
        })
    );

    outputChannel.appendLine('NC extension activated');
}

function startLSP(ncPath, context) {
    try {
        lspProcess = spawn(ncPath, ['lsp'], { stdio: ['pipe', 'pipe', 'pipe'] });

        lspProcess.on('error', (err) => {
            outputChannel.appendLine(`LSP failed to start: ${err.message}`);
            outputChannel.appendLine(`Make sure '${ncPath}' is in your PATH or set nc.path in settings`);
        });

        lspProcess.stderr?.on('data', (data) => {
            outputChannel.appendLine(`LSP: ${data.toString()}`);
        });

        // Simple LSP initialization
        const initMessage = JSON.stringify({
            jsonrpc: '2.0',
            id: 1,
            method: 'initialize',
            params: {
                capabilities: {
                    textDocument: {
                        completion: { completionItem: {} },
                        hover: {},
                        definition: {}
                    }
                }
            }
        });

        const header = `Content-Length: ${Buffer.byteLength(initMessage)}\r\n\r\n`;
        lspProcess.stdin?.write(header + initMessage);

        outputChannel.appendLine('LSP server started');

        context.subscriptions.push({
            dispose: () => {
                if (lspProcess) {
                    lspProcess.kill();
                    lspProcess = null;
                }
            }
        });
    } catch (e) {
        outputChannel.appendLine(`Could not start LSP: ${e.message}`);
    }
}

const BUILTIN_HELP = {
    'ask': { name: 'ask AI to', description: 'Send a prompt to your configured AI model.', syntax: 'ask AI to "prompt" save as result', example: 'ask AI to "Classify this email: {{email}}" save as classification' },
    'gather': { name: 'gather', description: 'Fetch data from a URL, database, or MCP tool.', syntax: 'gather data from source', example: 'gather users from "https://api.example.com/users"' },
    'respond': { name: 'respond with', description: 'Return a value from a behavior (like return in other languages).', syntax: 'respond with value', example: 'respond with {"status": "ok", "data": result}' },
    'set': { name: 'set', description: 'Assign a value to a variable.', syntax: 'set name to value', example: 'set count to len(items)' },
    'show': { name: 'show', description: 'Display a value to the output.', syntax: 'show value', example: 'show "Hello, " + name' },
    'display': { name: 'display', description: 'Display a value to the output (synonym for show).', syntax: 'display value', example: 'display "Result: " + result' },
    'print': { name: 'print', description: 'Print a value to the output (synonym for show).', syntax: 'print value', example: 'print "Debug: " + data' },
    'output': { name: 'output', description: 'Output a value (synonym for show).', syntax: 'output value', example: 'output result' },
    'log': { name: 'log', description: 'Log a message (with timestamp in production).', syntax: 'log "message"', example: 'log "Processing {{user.name}}"' },
    'store': { name: 'store into', description: 'Persist data to configured database.', syntax: 'store data into "target"', example: 'store result into "processed_items"' },
    'notify': { name: 'notify', description: 'Send a notification to a channel.', syntax: 'notify "channel" "message"', example: 'notify "ops-team" "Alert: service down"' },
    'emit': { name: 'emit', description: 'Emit an event.', syntax: 'emit "event_name"', example: 'emit "order.completed"' },
    'wait': { name: 'wait', description: 'Pause execution for a duration.', syntax: 'wait N seconds', example: 'wait 5 seconds' },
    'run': { name: 'run', description: 'Call another behavior.', syntax: 'run behavior_name with args', example: 'run validate_input with data' },
    'len': { name: 'len()', description: 'Get the length of a string, list, or map.', syntax: 'len(value)', example: 'set count to len(items)' },
    'upper': { name: 'upper()', description: 'Convert string to uppercase.', syntax: 'upper(string)', example: 'set name to upper("hello")  // "HELLO"' },
    'lower': { name: 'lower()', description: 'Convert string to lowercase.', syntax: 'lower(string)', example: 'set name to lower("HELLO")  // "hello"' },
    'trim': { name: 'trim()', description: 'Remove whitespace from both ends.', syntax: 'trim(string)', example: 'set clean to trim("  hello  ")  // "hello"' },
    'split': { name: 'split()', description: 'Split a string by delimiter.', syntax: 'split(string, delimiter)', example: 'set words to split("a,b,c", ",")  // ["a","b","c"]' },
    'join': { name: 'join()', description: 'Join list items into a string.', syntax: 'join(list, separator)', example: 'set csv to join(items, ",")' },
    'contains': { name: 'contains()', description: 'Check if string contains substring.', syntax: 'contains(string, substring)', example: 'if contains(email, "@"): ...' },
    'replace': { name: 'replace()', description: 'Replace occurrences in a string.', syntax: 'replace(string, old, new)', example: 'set clean to replace(text, "bad", "good")' },
    'starts_with': { name: 'starts_with()', description: 'Check if string starts with prefix.', syntax: 'starts_with(string, prefix)', example: 'if starts_with(url, "https"): ...' },
    'ends_with': { name: 'ends_with()', description: 'Check if string ends with suffix.', syntax: 'ends_with(string, suffix)', example: 'if ends_with(file, ".nc"): ...' },
    'append': { name: 'append', description: 'Add an item to a list.', syntax: 'append value to list', example: 'append "new item" to items' },
    'remove': { name: 'remove from', description: 'Remove an item from a list.', syntax: 'remove value from list', example: 'remove "spam" from emails' },
    'sort': { name: 'sort()', description: 'Sort a list.', syntax: 'sort(list)', example: 'set sorted_items to sort(scores)' },
    'reverse': { name: 'reverse()', description: 'Reverse a list.', syntax: 'reverse(list)', example: 'set reversed to reverse(items)' },
    'range': { name: 'range()', description: 'Generate a list of numbers.', syntax: 'range(end) or range(start, end)', example: 'set nums to range(10)  // [0,1,2,...,9]' },
    'keys': { name: 'keys()', description: 'Get all keys from a map/record.', syntax: 'keys(map)', example: 'set field_names to keys(user)' },
    'values': { name: 'values()', description: 'Get all values from a map/record.', syntax: 'values(map)', example: 'set all_values to values(config)' },
    'type': { name: 'type()', description: 'Get the type of a value: text, number, yesno, list, record, none.', syntax: 'type(value)', example: 'if type(x) is equal "text": ...' },
    'str': { name: 'str()', description: 'Convert value to string.', syntax: 'str(value)', example: 'set label to "Count: " + str(42)' },
    'int': { name: 'int()', description: 'Convert value to integer.', syntax: 'int(value)', example: 'set n to int("42")' },
    'abs': { name: 'abs()', description: 'Absolute value.', syntax: 'abs(number)', example: 'set positive to abs(-5)  // 5' },
    'sqrt': { name: 'sqrt()', description: 'Square root.', syntax: 'sqrt(number)', example: 'set root to sqrt(16)  // 4' },
    'random': { name: 'random()', description: 'Random number between 0 and 1.', syntax: 'random()', example: 'set r to random()' },
    'time_now': { name: 'time_now()', description: 'Current timestamp as string.', syntax: 'time_now()', example: 'set ts to time_now()' },
    'read_file': { name: 'read_file()', description: 'Read a file and return its contents as text.', syntax: 'read_file(path)', example: 'set doc to read_file("data.txt")' },
    'write_file': { name: 'write_file()', description: 'Write text content to a file.', syntax: 'write_file(path, content)', example: 'write_file("output.txt", result)' },
    'file_exists': { name: 'file_exists()', description: 'Check if a file exists.', syntax: 'file_exists(path)', example: 'if file_exists("config.nc"): ...' },
    'json_encode': { name: 'json_encode()', description: 'Convert value to JSON string.', syntax: 'json_encode(value)', example: 'set json_str to json_encode(data)' },
    'json_decode': { name: 'json_decode()', description: 'Parse JSON string to NC value.', syntax: 'json_decode(string)', example: 'set data to json_decode(raw_json)' },
    'env': { name: 'env()', description: 'Get environment variable value.', syntax: 'env(name)', example: 'set key to env("API_KEY")' },
    'chunk': { name: 'chunk()', description: 'Split text into chunks for RAG.', syntax: 'chunk(text, chunk_size)', example: 'set parts to chunk(document, 500)' },
    'top_k': { name: 'top_k()', description: 'Get top K items from a list.', syntax: 'top_k(list, k)', example: 'set best to top_k(chunks, 5)' },
    'validate': { name: 'validate()', description: 'Check if a record has required fields.', syntax: 'validate(record, ["field1", "field2"])', example: 'set ok to validate(result, ["name", "age"])' },
    'load_model': { name: 'load_model()', description: 'Load a Python ML model (.pkl, .pt, .h5, .onnx).', syntax: 'load_model(path)', example: 'set model to load_model("classifier.pkl")' },
    'predict': { name: 'predict()', description: 'Run prediction on a loaded model.', syntax: 'predict(model, features)', example: 'set result to predict(model, [age, salary])' },
    'memory_new': { name: 'memory_new()', description: 'Create conversation memory for chatbots.', syntax: 'memory_new(max_turns)', example: 'set mem to memory_new(30)' },
    'memory_add': { name: 'memory_add()', description: 'Add a message to conversation memory.', syntax: 'memory_add(memory, role, content)', example: 'memory_add(mem, "user", message)' },
    'memory_get': { name: 'memory_get()', description: 'Get all messages from memory.', syntax: 'memory_get(memory)', example: 'set messages to memory_get(mem)' },
    'memory_summary': { name: 'memory_summary()', description: 'Get conversation as a single string.', syntax: 'memory_summary(memory)', example: 'set history to memory_summary(mem)' },
    'memory_clear': { name: 'memory_clear()', description: 'Clear all messages from memory.', syntax: 'memory_clear(memory)', example: 'memory_clear(mem)' },
    'ai_with_fallback': { name: 'ai_with_fallback()', description: 'Try multiple AI models, use first that succeeds.', syntax: 'ai_with_fallback(prompt, context, models_list)', example: 'set answer to ai_with_fallback("question", {}, ["openai/gpt-4o", "openai/gpt-4o-mini"])' },
    'csv_parse': { name: 'csv_parse()', description: 'Parse CSV text into a list of records.', syntax: 'csv_parse(text)', example: 'set rows to csv_parse(raw_csv)' },
    'yaml_parse': { name: 'yaml_parse()', description: 'Parse YAML text into NC values.', syntax: 'yaml_parse(text)', example: 'set config to yaml_parse(raw_yaml)' },
    'xml_parse': { name: 'xml_parse()', description: 'Parse XML text into NC values.', syntax: 'xml_parse(text)', example: 'set doc to xml_parse(raw_xml)' },
    'configure': { name: 'configure:', description: 'Configuration block -- set AI model, API keys, database URLs, and service settings.', syntax: 'configure:\n    ai_model is "openai/gpt-4o"\n    ai_key is "env:API_KEY"' },
    'service': { name: 'service', description: 'Declare the service name.', syntax: 'service "my-service"' },
    'version': { name: 'version', description: 'Declare the service version.', syntax: 'version "1.0.0"' },
    'define': { name: 'define', description: 'Define a data type with fields.', syntax: 'define TypeName as:\n    field is text\n    count is number' },
    'create': { name: 'create', description: 'Define a data type (synonym for define).', syntax: 'create TypeName as:\n    field is text' },
    'import': { name: 'import', description: 'Import another .nc file or installed package.', syntax: 'import "helpers"', example: 'import "data-parsers"' },
    'middleware': { name: 'middleware:', description: 'Add middleware to the server (auth, rate_limit, cors).', syntax: 'middleware:\n    auth\n    rate_limit' },
    'check': { name: 'check', description: 'Verify a condition (synonym for validate/if).', syntax: 'check condition', example: 'check len(name) above 0' },
    'verify': { name: 'verify', description: 'Verify a condition (synonym for check).', syntax: 'verify condition', example: 'verify user.authenticated is yes' },
    'obtain': { name: 'obtain', description: 'Fetch data (synonym for gather).', syntax: 'obtain data from source', example: 'obtain users from "https://api.example.com/users"' },
    'fetch': { name: 'fetch', description: 'Fetch data (synonym for gather).', syntax: 'fetch data from source', example: 'fetch orders from "https://api.example.com/orders"' },
    'retrieve': { name: 'retrieve', description: 'Retrieve data (synonym for gather).', syntax: 'retrieve data from source', example: 'retrieve reports from db' }
};

/**
 * Run NC AI command and show result in output channel.
 */
function runNcAiCommand(ncPath, subcmd, question) {
    outputChannel.show(true);
    outputChannel.appendLine(`\n--- NC AI: ${subcmd} ---`);
    outputChannel.appendLine(`> ${question}\n`);

    const proc = spawn(ncPath, ['ai', subcmd, question], {
        cwd: vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath || process.cwd(),
        shell: true
    });

    proc.stdout?.on('data', (data) => {
        const text = data.toString().replace(/\[LOG\]\s*/g, '');
        outputChannel.append(text);
    });

    proc.stderr?.on('data', (data) => {
        outputChannel.append(data.toString());
    });

    proc.on('close', () => {
        outputChannel.appendLine('\n--- End ---');
    });
}

/**
 * NC AI Sidebar Webview Provider — chat panel in the sidebar.
 */
class NcAiViewProvider {
    constructor(extensionUri, ncPath) {
        this._extensionUri = extensionUri;
        this._ncPath = ncPath;
    }

    resolveWebviewView(webviewView) {
        this._view = webviewView;
        webviewView.webview.options = { enableScripts: true };
        webviewView.webview.html = this._getHtml();

        webviewView.webview.onDidReceiveMessage(async (msg) => {
            if (msg.type === 'ask') {
                const answer = await this._askNcAi(msg.question);
                webviewView.webview.postMessage({ type: 'answer', text: answer });
            }
        });
    }

    _askNcAi(question) {
        return new Promise((resolve) => {
            let output = '';
            const proc = spawn(this._ncPath, ['ai', 'reason', question], {
                cwd: vscode.workspace.workspaceFolders?.[0]?.uri?.fsPath || process.cwd(),
                shell: true
            });

            proc.stdout?.on('data', (data) => {
                output += data.toString();
            });

            proc.stderr?.on('data', (data) => {
                output += data.toString();
            });

            proc.on('close', () => {
                // Clean up [LOG] prefixes
                const clean = output.replace(/\[LOG\]\s*/g, '').trim();
                resolve(clean);
            });

            proc.on('error', () => {
                resolve('Error: Could not run NC AI. Make sure NC is installed.');
            });
        });
    }

    _getHtml() {
        return `<!DOCTYPE html>
<html><head>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:var(--vscode-font-family);color:var(--vscode-foreground);background:var(--vscode-sideBar-background);padding:8px;display:flex;flex-direction:column;height:100vh}
.chat{flex:1;overflow-y:auto;padding:4px 0}
.msg{margin:6px 0;padding:8px 10px;border-radius:8px;font-size:12px;line-height:1.5;white-space:pre-wrap;word-break:break-word}
.msg.user{background:var(--vscode-input-background);border:1px solid var(--vscode-input-border)}
.msg.ai{background:var(--vscode-editor-background);border:1px solid var(--vscode-panel-border)}
.msg .label{font-weight:600;font-size:11px;margin-bottom:4px;opacity:.7}
.input-area{display:flex;gap:4px;padding:8px 0 4px;border-top:1px solid var(--vscode-panel-border)}
input{flex:1;padding:6px 10px;border:1px solid var(--vscode-input-border);background:var(--vscode-input-background);color:var(--vscode-input-foreground);border-radius:6px;font-size:12px;outline:none}
input:focus{border-color:var(--vscode-focusBorder)}
button{padding:6px 12px;border:none;background:var(--vscode-button-background);color:var(--vscode-button-foreground);border-radius:6px;cursor:pointer;font-size:12px}
button:hover{background:var(--vscode-button-hoverBackground)}
.typing{opacity:.5;font-style:italic}
.welcome{text-align:center;padding:20px 10px;opacity:.6;font-size:12px}
</style>
</head><body>
<div class="chat" id="chat">
    <div class="welcome">
        <b>NC AI</b><br><br>
        Local AI assistant — no cloud, no API keys.<br><br>
        Try: "Write an email" or "Compare Python vs Go"
    </div>
</div>
<div class="input-area">
    <input id="input" placeholder="Ask NC AI..." />
    <button id="send">Ask</button>
</div>
<script>
const vscode = acquireVsCodeApi();
const chat = document.getElementById('chat');
const input = document.getElementById('input');
let firstMsg = true;

function addMsg(text, type) {
    if (firstMsg) { chat.innerHTML = ''; firstMsg = false; }
    const div = document.createElement('div');
    div.className = 'msg ' + type;
    const label = type === 'user' ? 'You' : 'NC AI';
    div.innerHTML = '<div class="label">' + label + '</div>' + text.replace(/</g,'&lt;').replace(/>/g,'&gt;');
    chat.appendChild(div);
    chat.scrollTop = chat.scrollHeight;
    return div;
}

function send() {
    const q = input.value.trim();
    if (!q) return;
    addMsg(q, 'user');
    input.value = '';
    const loading = addMsg('Thinking...', 'ai');
    loading.classList.add('typing');
    vscode.postMessage({ type: 'ask', question: q });
}

input.addEventListener('keydown', e => { if (e.key === 'Enter') send(); });
document.getElementById('send').addEventListener('click', send);

window.addEventListener('message', e => {
    const msg = e.data;
    if (msg.type === 'answer') {
        const typing = chat.querySelector('.typing');
        if (typing) typing.remove();
        addMsg(msg.text, 'ai');
    }
});
</script>
</body></html>`;
    }
}

function deactivate() {
    if (lspProcess) {
        lspProcess.kill();
        lspProcess = null;
    }
    if (statusBarItem) {
        statusBarItem.dispose();
        statusBarItem = null;
    }
}

module.exports = { activate, deactivate };

