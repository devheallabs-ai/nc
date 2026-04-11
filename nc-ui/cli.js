#!/usr/bin/env node

/**
 * NC UI CLI
 * Command-line interface for building, watching, and serving NC UI files.
 *
 * Usage:
 *   nc-ui build <file.ncui>         Compile to HTML
 *   nc-ui watch <file.ncui>         Watch and rebuild on change
 *   nc-ui serve <file.ncui> [port]  Serve with live reload
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { compile } = require('./compiler');

// ─── Helpers ─────────────────────────────────────────────────────────────────

function usage() {
  console.log(`
  NC UI Compiler CLI v1.0.0

  Usage:
    nc-ui build <file.ncui>           Compile .ncui to .html
    nc-ui watch <file.ncui>           Watch for changes and rebuild
    nc-ui serve <file.ncui> [port]    Serve with live reload (default port: 3000)

  Examples:
    node cli.js build portfolio.ncui
    node cli.js watch landing.ncui
    node cli.js serve dashboard.ncui 8080
`);
}

function resolveFile(file) {
  const abs = path.resolve(file);
  if (!fs.existsSync(abs)) {
    console.error(`Error: File not found: ${abs}`);
    process.exit(1);
  }
  return abs;
}

function outPath(inputPath) {
  const dir = path.dirname(inputPath);
  const base = path.basename(inputPath, path.extname(inputPath));
  return path.join(dir, base + '.html');
}

function buildFile(inputPath) {
  const source = fs.readFileSync(inputPath, 'utf-8');
  const html = compile(source);
  const output = outPath(inputPath);
  fs.writeFileSync(output, html, 'utf-8');
  const sizeKB = (Buffer.byteLength(html, 'utf-8') / 1024).toFixed(1);
  console.log(`  Built: ${path.basename(output)} (${sizeKB} KB)`);
  return { output, html };
}

function timestamp() {
  return new Date().toLocaleTimeString('en-US', { hour12: false });
}

// ─── Commands ────────────────────────────────────────────────────────────────

function cmdBuild(file) {
  const inputPath = resolveFile(file);
  console.log(`\n  NC UI Build`);
  console.log(`  Input:  ${path.basename(inputPath)}`);
  buildFile(inputPath);
  console.log(`  Done.\n`);
}

function cmdWatch(file) {
  const inputPath = resolveFile(file);
  console.log(`\n  NC UI Watch`);
  console.log(`  Watching: ${path.basename(inputPath)}`);
  console.log(`  Press Ctrl+C to stop.\n`);

  // Initial build
  buildFile(inputPath);

  let debounce = null;
  fs.watch(inputPath, () => {
    if (debounce) clearTimeout(debounce);
    debounce = setTimeout(() => {
      try {
        console.log(`  [${timestamp()}] Rebuilding...`);
        buildFile(inputPath);
      } catch (e) {
        console.error(`  [${timestamp()}] Error: ${e.message}`);
      }
    }, 150);
  });
}

function cmdServe(file, port) {
  port = parseInt(port) || 3000;
  const inputPath = resolveFile(file);
  const http = require('http');

  console.log(`\n  NC UI Dev Server`);
  console.log(`  Serving: ${path.basename(inputPath)}`);
  console.log(`  URL:     http://localhost:${port}`);
  console.log(`  Press Ctrl+C to stop.\n`);

  // Track connected SSE clients for live reload
  const clients = new Set();

  // Build initially
  let { html } = buildFile(inputPath);

  // Watch for changes
  let debounce = null;
  fs.watch(inputPath, () => {
    if (debounce) clearTimeout(debounce);
    debounce = setTimeout(() => {
      try {
        console.log(`  [${timestamp()}] Rebuilding...`);
        const source = fs.readFileSync(inputPath, 'utf-8');
        html = compile(source);
        const output = outPath(inputPath);
        fs.writeFileSync(output, html, 'utf-8');
        console.log(`  [${timestamp()}] Built. Reloading browsers...`);
        // Notify all SSE clients
        clients.forEach(res => {
          res.write('data: reload\n\n');
        });
      } catch (e) {
        console.error(`  [${timestamp()}] Error: ${e.message}`);
      }
    }, 150);
  });

  // Inject live-reload script into HTML
  function injectLiveReload(rawHtml) {
    const script = `
<script>
(function(){
  var es = new EventSource('/__ncui_reload');
  es.onmessage = function(e){ if(e.data==='reload') location.reload(); };
  es.onerror = function(){ setTimeout(function(){ location.reload(); }, 2000); };
})();
</script>`;
    return rawHtml.replace('</body>', script + '\n</body>');
  }

  const server = http.createServer((req, res) => {
    // SSE endpoint for live reload
    if (req.url === '/__ncui_reload') {
      res.writeHead(200, {
        'Content-Type': 'text/event-stream',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
        'Access-Control-Allow-Origin': '*'
      });
      res.write('data: connected\n\n');
      clients.add(res);
      req.on('close', () => clients.delete(res));
      return;
    }

    // Serve the compiled HTML
    if (req.url === '/' || req.url === '/index.html') {
      const served = injectLiveReload(html);
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(served);
      return;
    }

    // Serve static files from the same directory
    const filePath = path.join(path.dirname(inputPath), req.url);
    if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
      const ext = path.extname(filePath).toLowerCase();
      const mimeTypes = {
        '.html': 'text/html', '.css': 'text/css', '.js': 'application/javascript',
        '.json': 'application/json', '.png': 'image/png', '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg', '.gif': 'image/gif', '.svg': 'image/svg+xml',
        '.ico': 'image/x-icon', '.woff': 'font/woff', '.woff2': 'font/woff2',
        '.ttf': 'font/ttf', '.webp': 'image/webp', '.mp4': 'video/mp4',
      };
      res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'application/octet-stream' });
      fs.createReadStream(filePath).pipe(res);
      return;
    }

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('404 Not Found');
  });

  server.listen(port);
}

// ─── Main ────────────────────────────────────────────────────────────────────

const args = process.argv.slice(2);
const command = args[0];
const file = args[1];

if (!command || !file) {
  usage();
  process.exit(command ? 1 : 0);
}

switch (command) {
  case 'build':
    cmdBuild(file);
    break;
  case 'watch':
    cmdWatch(file);
    break;
  case 'serve':
    cmdServe(file, args[2]);
    break;
  default:
    console.error(`Unknown command: ${command}`);
    usage();
    process.exit(1);
}
