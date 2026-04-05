/**
 * NC UI Compiler
 * Compiles plain-English NC UI markup into stunning HTML + CSS + JS.
 * Part of the NC language ecosystem by DevHeal Labs AI.
 */

'use strict';

// ─── Icon SVGs ───────────────────────────────────────────────────────────────

const ICONS = {
  code: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/></svg>`,
  design: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 19l7-7 3 3-7 7-3-3z"/><path d="M18 13l-1.5-7.5L2 2l3.5 14.5L13 18l5-5z"/><path d="M2 2l7.586 7.586"/><circle cx="11" cy="11" r="2"/></svg>`,
  rocket: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4.5 16.5c-1.5 1.26-2 5-2 5s3.74-.5 5-2c.71-.84.7-2.13-.09-2.91a2.18 2.18 0 0 0-2.91-.09z"/><path d="M12 15l-3-3a22 22 0 0 1 2-3.95A12.88 12.88 0 0 1 22 2c0 2.72-.78 7.5-6 11a22.35 22.35 0 0 1-4 2z"/><path d="M9 12H4s.55-3.03 2-4c1.62-1.08 3 0 3 0"/><path d="M12 15v5s3.03-.55 4-2c1.08-1.62 0-3 0-3"/></svg>`,
  brain: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9.5 2A2.5 2.5 0 0 1 12 4.5v15a2.5 2.5 0 0 1-4.96.44A2.5 2.5 0 0 1 4.5 17.5a2.5 2.5 0 0 1-.44-4.96A2.5 2.5 0 0 1 6.5 10a2.5 2.5 0 0 1 .44-4.96A2.5 2.5 0 0 1 9.5 2z"/><path d="M14.5 2A2.5 2.5 0 0 0 12 4.5v15a2.5 2.5 0 0 0 4.96.44A2.5 2.5 0 0 0 19.5 17.5a2.5 2.5 0 0 0 .44-4.96A2.5 2.5 0 0 0 17.5 10a2.5 2.5 0 0 0-.44-4.96A2.5 2.5 0 0 0 14.5 2z"/></svg>`,
  shield: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>`,
  globe: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10A15.3 15.3 0 0 1 12 2z"/></svg>`,
  star: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/></svg>`,
  heart: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z"/></svg>`,
  check: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>`,
  arrow: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>`,
  menu: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="18" x2="21" y2="18"/></svg>`,
  chart: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>`,
  users: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>`,
  mail: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 4h16c1.1 0 2 .9 2 2v12c0 1.1-.9 2-2 2H4c-1.1 0-2-.9-2-2V6c0-1.1.9-2 2-2z"/><polyline points="22,6 12,13 2,6"/></svg>`,
  clock: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>`,
};

// ─── Parser ──────────────────────────────────────────────────────────────────

class NCUIParser {
  constructor(source) {
    this.source = source;
    this.lines = source.split('\n');
    this.pos = 0;
    this.ast = { type: 'document', children: [], meta: {} };
  }

  parse() {
    while (this.pos < this.lines.length) {
      const line = this.lines[this.pos];
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('//') || trimmed.startsWith('#')) {
        this.pos++;
        continue;
      }
      const indent = this._indent(line);
      if (indent === 0) {
        this._parseTopLevel(trimmed);
      } else {
        this.pos++;
      }
    }
    return this.ast;
  }

  _indent(line) {
    const match = line.match(/^(\s*)/);
    return match ? match[1].replace(/\t/g, '    ').length : 0;
  }

  _str(s) {
    const m = s.match(/"([^"]*)"/);
    return m ? m[1] : null;
  }

  _allStrings(s) {
    const out = [];
    const re = /"([^"]*)"/g;
    let m;
    while ((m = re.exec(s))) out.push(m[1]);
    return out;
  }

  _parseTopLevel(trimmed) {
    if (trimmed.startsWith('page ')) {
      this.ast.meta.title = this._str(trimmed);
      this.pos++;
    } else if (trimmed.startsWith('theme ')) {
      this.ast.meta.theme = this._str(trimmed);
      this.pos++;
    } else if (trimmed.startsWith('accent ')) {
      this.ast.meta.accent = this._str(trimmed);
      this.pos++;
    } else if (trimmed.startsWith('font ')) {
      this.ast.meta.font = this._str(trimmed);
      this.pos++;
    } else if (trimmed.startsWith('nav') && trimmed.endsWith(':')) {
      this.pos++;
      const node = { type: 'nav', children: this._parseBlock(1) };
      this.ast.children.push(node);
    } else if (trimmed.startsWith('section ')) {
      this._parseSection(trimmed);
    } else if (trimmed.startsWith('footer')) {
      this.pos++;
      const node = { type: 'footer', children: this._parseBlock(1) };
      this.ast.children.push(node);
    } else {
      this.pos++;
    }
  }

  _parseSection(trimmed) {
    const node = { type: 'section', modifiers: [], children: [], props: {} };
    // Extract section name/id
    let rest = trimmed.replace(/^section\s+/, '').replace(/:$/, '');
    // Check for "with image"
    const imgMatch = rest.match(/with\s+image\s+"([^"]+)"/);
    if (imgMatch) {
      node.props.backgroundImage = imgMatch[1];
      rest = rest.replace(imgMatch[0], '').trim();
    }
    // Parse modifiers
    const parts = rest.split(/\s+/);
    node.id = parts[0];
    for (let i = 1; i < parts.length; i++) {
      node.modifiers.push(parts[i]);
    }
    this.pos++;
    node.children = this._parseBlock(1);
    this.ast.children.push(node);
  }

  _parseBlock(expectedIndent) {
    const children = [];
    while (this.pos < this.lines.length) {
      const line = this.lines[this.pos];
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('//') || trimmed.startsWith('#')) {
        this.pos++;
        continue;
      }
      const indent = this._indent(line);
      if (indent < expectedIndent) break;
      if (indent > expectedIndent) {
        // belongs to previous child
        if (children.length > 0) {
          const parent = children[children.length - 1];
          if (!parent.children) parent.children = [];
          parent.children.push(...this._parseBlock(indent));
        } else {
          this.pos++;
        }
        continue;
      }
      children.push(this._parseNode(trimmed, expectedIndent));
    }
    return children;
  }

  _parseNode(trimmed, currentIndent) {
    this.pos++;

    // Animate
    if (trimmed.startsWith('animate ')) {
      return { type: 'animate', value: this._str(trimmed) };
    }
    // Heading
    if (trimmed.startsWith('heading ')) {
      const node = { type: 'heading', text: this._str(trimmed) };
      if (trimmed.includes('style ')) {
        const strs = this._allStrings(trimmed);
        if (strs.length > 1) node.style = strs[strs.length - 1];
      }
      return node;
    }
    // Subheading
    if (trimmed.startsWith('subheading ')) {
      return { type: 'subheading', text: this._str(trimmed) };
    }
    // Text
    if (trimmed.startsWith('text ')) {
      return { type: 'text', text: this._str(trimmed) };
    }
    // Image
    if (trimmed.startsWith('image ')) {
      return { type: 'image', src: this._str(trimmed) };
    }
    // Button
    if (trimmed.startsWith('button ')) {
      const node = { type: 'button', text: this._str(trimmed) };
      const linkMatch = trimmed.match(/links?\s+to\s+"([^"]+)"/);
      if (linkMatch) node.href = linkMatch[1];
      const styleMatch = trimmed.match(/style\s+"([^"]+)"/);
      if (styleMatch) node.style = styleMatch[1];
      return node;
    }
    // Link
    if (trimmed.startsWith('link ')) {
      const strs = this._allStrings(trimmed);
      const node = { type: 'link', text: strs[0] };
      const toMatch = trimmed.match(/to\s+"([^"]+)"/);
      if (toMatch) node.href = toMatch[1];
      return node;
    }
    // Row
    if (trimmed.startsWith('row') && (trimmed === 'row:' || trimmed === 'row')) {
      return { type: 'row', children: [] };
    }
    // Grid
    if (trimmed.startsWith('grid ')) {
      const colMatch = trimmed.match(/(\d+)\s+columns?/);
      return { type: 'grid', columns: colMatch ? parseInt(colMatch[1]) : 3, children: [] };
    }
    // Column
    if (trimmed.startsWith('column') && (trimmed === 'column:' || trimmed === 'column')) {
      return { type: 'column', children: [] };
    }
    // Card
    if (trimmed.startsWith('card')) {
      const iconMatch = trimmed.match(/icon\s+"([^"]+)"/);
      return { type: 'card', icon: iconMatch ? iconMatch[1] : null, children: [] };
    }
    // Form
    if (trimmed.startsWith('form')) {
      const actionMatch = trimmed.match(/action\s+"([^"]+)"/);
      return { type: 'form', action: actionMatch ? actionMatch[1] : '#', children: [] };
    }
    // Input
    if (trimmed.startsWith('input ')) {
      const node = { type: 'input', label: this._str(trimmed), required: trimmed.includes('required') };
      const typeMatch = trimmed.match(/type\s+"([^"]+)"/);
      node.inputType = typeMatch ? typeMatch[1] : 'text';
      return node;
    }
    // Textarea
    if (trimmed.startsWith('textarea ')) {
      return { type: 'textarea', label: this._str(trimmed), required: trimmed.includes('required') };
    }
    // List
    if (trimmed === 'list:' || trimmed === 'list') {
      return { type: 'list', children: [] };
    }
    // Item
    if (trimmed.startsWith('item ')) {
      return { type: 'item', text: this._str(trimmed) };
    }
    // Nav brand
    if (trimmed.startsWith('brand ')) {
      return { type: 'brand', text: this._str(trimmed) };
    }
    // Nav links
    if (trimmed.startsWith('links:') || trimmed === 'links') {
      return { type: 'links', children: [] };
    }
    // Stat
    if (trimmed.startsWith('stat ')) {
      const strs = this._allStrings(trimmed);
      return { type: 'stat', value: strs[0], label: strs[1] || '' };
    }
    // Divider
    if (trimmed === 'divider') {
      return { type: 'divider' };
    }
    // Spacer
    if (trimmed.startsWith('spacer')) {
      return { type: 'spacer' };
    }
    // Badge
    if (trimmed.startsWith('badge ')) {
      return { type: 'badge', text: this._str(trimmed) };
    }
    // Progress
    if (trimmed.startsWith('progress ')) {
      const strs = this._allStrings(trimmed);
      const valMatch = trimmed.match(/(\d+)%/);
      return { type: 'progress', label: strs[0], value: valMatch ? parseInt(valMatch[1]) : 50 };
    }

    // Fallback
    return { type: 'unknown', raw: trimmed };
  }
}

// ─── Code Generator ──────────────────────────────────────────────────────────

class NCUIGenerator {
  constructor(ast) {
    this.ast = ast;
    this.theme = ast.meta.theme || 'dark';
    this.accent = ast.meta.accent || '#00d4ff';
    this.title = ast.meta.title || 'NC UI Page';
    this.font = ast.meta.font || 'Inter';
    this.animIndex = 0;
  }

  generate() {
    const body = this.ast.children.map(c => this._node(c)).join('\n');
    return this._wrapHTML(body);
  }

  _node(n, depth) {
    if (!n) return '';
    switch (n.type) {
      case 'nav': return this._nav(n);
      case 'section': return this._section(n);
      case 'footer': return this._footer(n);
      case 'heading': return this._heading(n);
      case 'subheading': return `<p class="ncui-sub">${this._esc(n.text)}</p>`;
      case 'text': return `<p class="ncui-text">${this._esc(n.text)}</p>`;
      case 'button': return this._button(n);
      case 'link': return `<a href="${this._esc(n.href || '#')}" class="ncui-link">${this._esc(n.text)}</a>`;
      case 'row': return `<div class="ncui-row">${(n.children || []).map(c => this._node(c)).join('')}</div>`;
      case 'grid': return this._grid(n);
      case 'column': return `<div class="ncui-col">${(n.children || []).map(c => this._node(c)).join('')}</div>`;
      case 'card': return this._card(n);
      case 'image': return `<img src="${this._esc(n.src)}" alt="" class="ncui-img" loading="lazy">`;
      case 'form': return this._form(n);
      case 'input': return this._input(n);
      case 'textarea': return this._textarea(n);
      case 'list': return `<ul class="ncui-list">${(n.children || []).map(c => this._node(c)).join('')}</ul>`;
      case 'item': return `<li class="ncui-list-item"><span class="ncui-check">${ICONS.check}</span>${this._esc(n.text)}</li>`;
      case 'animate': return '';  // handled at section level
      case 'brand': return `<a href="#" class="ncui-brand">${this._esc(n.text)}</a>`;
      case 'links': return `<div class="ncui-nav-links">${(n.children || []).map(c => this._node(c)).join('')}</div>`;
      case 'stat': return `<div class="ncui-stat"><span class="ncui-stat-val">${this._esc(n.value)}</span><span class="ncui-stat-label">${this._esc(n.label)}</span></div>`;
      case 'divider': return `<hr class="ncui-divider">`;
      case 'spacer': return `<div class="ncui-spacer"></div>`;
      case 'badge': return `<span class="ncui-badge">${this._esc(n.text)}</span>`;
      case 'progress': return `<div class="ncui-progress"><span class="ncui-progress-label">${this._esc(n.label)}</span><div class="ncui-progress-bar"><div class="ncui-progress-fill" style="width:${n.value}%"></div></div></div>`;
      default: return '';
    }
  }

  _esc(s) { return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }

  _heading(n) {
    const cls = n.style === 'gradient' ? 'ncui-heading ncui-gradient-text' : 'ncui-heading';
    return `<h2 class="${cls}">${this._esc(n.text)}</h2>`;
  }

  _button(n) {
    const cls = `ncui-btn ncui-btn-${n.style || 'primary'}`;
    if (n.href) return `<a href="${this._esc(n.href)}" class="${cls}">${this._esc(n.text)}</a>`;
    return `<button class="${cls}">${this._esc(n.text)}</button>`;
  }

  _card(n) {
    let iconHtml = '';
    if (n.icon && ICONS[n.icon]) {
      iconHtml = `<div class="ncui-card-icon">${ICONS[n.icon]}</div>`;
    }
    const inner = (n.children || []).map(c => this._node(c)).join('');
    return `<div class="ncui-card">${iconHtml}${inner}</div>`;
  }

  _grid(n) {
    return `<div class="ncui-grid ncui-grid-${n.columns}">${(n.children || []).map(c => this._node(c)).join('')}</div>`;
  }

  _form(n) {
    const inner = (n.children || []).map(c => this._node(c)).join('');
    return `<form class="ncui-form" action="${this._esc(n.action)}" method="POST">${inner}</form>`;
  }

  _input(n) {
    const id = 'f-' + (n.label || '').toLowerCase().replace(/\s+/g, '-');
    const req = n.required ? ' required' : '';
    return `<div class="ncui-field"><label for="${id}">${this._esc(n.label)}</label><input id="${id}" type="${n.inputType || 'text'}" placeholder="${this._esc(n.label)}"${req}></div>`;
  }

  _textarea(n) {
    const id = 'f-' + (n.label || '').toLowerCase().replace(/\s+/g, '-');
    const req = n.required ? ' required' : '';
    return `<div class="ncui-field"><label for="${id}">${this._esc(n.label)}</label><textarea id="${id}" placeholder="${this._esc(n.label)}" rows="4"${req}></textarea></div>`;
  }

  _section(n) {
    const classes = ['ncui-section'];
    if (n.modifiers.includes('centered')) classes.push('ncui-centered');
    if (n.id === 'hero') classes.push('ncui-hero');

    // Find animation in children
    let anim = null;
    const contentChildren = [];
    for (const c of n.children) {
      if (c.type === 'animate') anim = c.value;
      else contentChildren.push(c);
    }
    if (anim) classes.push(`ncui-anim-${anim}`);

    let bgStyle = '';
    if (n.props.backgroundImage) {
      classes.push('ncui-section-img');
      bgStyle = ` style="--bg-img:url('${this._esc(n.props.backgroundImage)}')"`;
    }

    const inner = contentChildren.map(c => this._node(c)).join('\n');
    return `<section id="${this._esc(n.id)}" class="${classes.join(' ')}"${bgStyle}>\n<div class="ncui-container">\n${inner}\n</div>\n</section>`;
  }

  _nav(n) {
    const inner = (n.children || []).map(c => this._node(c)).join('');
    return `<nav class="ncui-nav"><div class="ncui-container ncui-nav-inner">${inner}<button class="ncui-nav-toggle" onclick="this.parentElement.classList.toggle('open')">${ICONS.menu}</button></div></nav>`;
  }

  _footer(n) {
    const inner = (n.children || []).map(c => this._node(c)).join('\n');
    return `<footer class="ncui-footer"><div class="ncui-container">\n${inner}\n</div></footer>`;
  }

  // ─── Full HTML Output ────────────────────────────────────────────────────

  _wrapHTML(body) {
    return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>${this._esc(this.title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=${encodeURIComponent(this.font)}:wght@300;400;500;600;700;800;900&display=swap" rel="stylesheet">
<style>${this._css()}</style>
</head>
<body>
${body}
<script>${this._js()}</script>
</body>
</html>`;
  }

  // ─── CSS ─────────────────────────────────────────────────────────────────

  _css() {
    const isDark = this.theme === 'dark';
    const bg = isDark ? '#0a0a0f' : '#fafafa';
    const bgCard = isDark ? 'rgba(255,255,255,0.04)' : 'rgba(0,0,0,0.03)';
    const bgCardHover = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.06)';
    const border = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.08)';
    const textMain = isDark ? '#f0f0f5' : '#1a1a2e';
    const textSub = isDark ? 'rgba(255,255,255,0.65)' : 'rgba(0,0,0,0.6)';
    const textMuted = isDark ? 'rgba(255,255,255,0.4)' : 'rgba(0,0,0,0.4)';
    const inputBg = isDark ? 'rgba(255,255,255,0.06)' : 'rgba(0,0,0,0.04)';
    const accent = this.accent;

    return `
*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}
html{scroll-behavior:smooth;-webkit-font-smoothing:antialiased}
body{font-family:'${this.font}',system-ui,sans-serif;background:${bg};color:${textMain};line-height:1.7;overflow-x:hidden}

/* Container */
.ncui-container{max-width:1200px;margin:0 auto;padding:0 24px}

/* ─ Navigation ─ */
.ncui-nav{position:fixed;top:0;left:0;right:0;z-index:1000;padding:16px 0;transition:all .3s;backdrop-filter:blur(20px);background:${isDark ? 'rgba(10,10,15,0.8)' : 'rgba(250,250,250,0.85)'};border-bottom:1px solid ${border}}
.ncui-nav-inner{display:flex;align-items:center;justify-content:space-between}
.ncui-brand{font-weight:700;font-size:1.25rem;color:${textMain};text-decoration:none;letter-spacing:-0.02em}
.ncui-nav-links{display:flex;gap:32px;align-items:center}
.ncui-nav-links .ncui-link{font-size:.9rem;font-weight:500}
.ncui-nav-toggle{display:none;background:none;border:none;color:${textMain};cursor:pointer;width:28px;height:28px}
.ncui-nav-toggle svg{width:24px;height:24px}
@media(max-width:768px){
  .ncui-nav-links{display:none;position:absolute;top:100%;left:0;right:0;flex-direction:column;padding:24px;gap:16px;background:${isDark ? 'rgba(10,10,15,0.95)' : 'rgba(250,250,250,0.95)'};border-bottom:1px solid ${border};backdrop-filter:blur(20px)}
  .ncui-nav-inner.open .ncui-nav-links{display:flex}
  .ncui-nav-toggle{display:block}
}

/* ─ Sections ─ */
.ncui-section{padding:100px 0;position:relative}
.ncui-hero{min-height:100vh;display:flex;align-items:center;padding-top:80px}
.ncui-hero .ncui-container{width:100%}
.ncui-centered{text-align:center}
.ncui-centered .ncui-row{justify-content:center}
.ncui-section-img{position:relative}
.ncui-section-img::before{content:'';position:absolute;inset:0;background:var(--bg-img) center/cover no-repeat;opacity:0.15;z-index:0}
.ncui-section-img .ncui-container{position:relative;z-index:1}

/* ─ Typography ─ */
.ncui-heading{font-size:clamp(2rem,5vw,3.5rem);font-weight:800;letter-spacing:-0.03em;line-height:1.15;margin-bottom:16px}
.ncui-gradient-text{background:linear-gradient(135deg,${accent},${this._shiftHue(accent, 60)});-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.ncui-sub{font-size:clamp(1.1rem,2.5vw,1.5rem);color:${textSub};font-weight:400;margin-bottom:12px}
.ncui-text{color:${textSub};font-size:1.05rem;max-width:640px;margin-bottom:16px;line-height:1.8}
.ncui-centered .ncui-text{margin-left:auto;margin-right:auto}

/* ─ Buttons ─ */
.ncui-btn{display:inline-flex;align-items:center;gap:8px;padding:14px 32px;border-radius:12px;font-size:.95rem;font-weight:600;text-decoration:none;border:none;cursor:pointer;transition:all .3s cubic-bezier(.4,0,.2,1);font-family:inherit}
.ncui-btn-primary{background:linear-gradient(135deg,${accent},${this._shiftHue(accent,40)});color:#fff;box-shadow:0 4px 24px ${accent}44}
.ncui-btn-primary:hover{transform:translateY(-2px);box-shadow:0 8px 32px ${accent}66}
.ncui-btn-outline{background:transparent;color:${accent};border:1.5px solid ${accent};backdrop-filter:blur(8px)}
.ncui-btn-outline:hover{background:${accent}18;transform:translateY(-2px)}
.ncui-btn-glass{background:${isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)'};color:${textMain};border:1px solid ${border};backdrop-filter:blur(12px)}
.ncui-btn-glass:hover{background:${isDark ? 'rgba(255,255,255,0.14)' : 'rgba(0,0,0,0.08)'};transform:translateY(-2px)}
.ncui-btn-gradient{background:linear-gradient(135deg,${accent},${this._shiftHue(accent,120)});color:#fff;box-shadow:0 4px 24px ${accent}44}
.ncui-btn-gradient:hover{transform:translateY(-2px);box-shadow:0 8px 32px ${accent}66}

/* ─ Row ─ */
.ncui-row{display:flex;flex-wrap:wrap;gap:16px;margin:24px 0;align-items:center}

/* ─ Grid ─ */
.ncui-grid{display:grid;gap:24px;margin:40px 0}
.ncui-grid-2{grid-template-columns:repeat(2,1fr)}
.ncui-grid-3{grid-template-columns:repeat(3,1fr)}
.ncui-grid-4{grid-template-columns:repeat(4,1fr)}
@media(max-width:900px){.ncui-grid-3,.ncui-grid-4{grid-template-columns:repeat(2,1fr)}}
@media(max-width:600px){.ncui-grid{grid-template-columns:1fr !important}}

/* ─ Cards ─ */
.ncui-card{background:${bgCard};border:1px solid ${border};border-radius:20px;padding:36px 28px;transition:all .4s cubic-bezier(.4,0,.2,1);position:relative;overflow:hidden}
.ncui-card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;background:linear-gradient(90deg,transparent,${accent},transparent);opacity:0;transition:opacity .4s}
.ncui-card:hover{transform:translateY(-6px);background:${bgCardHover};border-color:${accent}33;box-shadow:0 20px 60px ${isDark ? 'rgba(0,0,0,0.4)' : 'rgba(0,0,0,0.08)'}}
.ncui-card:hover::before{opacity:1}
.ncui-card-icon{width:52px;height:52px;border-radius:14px;background:linear-gradient(135deg,${accent}22,${accent}08);display:flex;align-items:center;justify-content:center;margin-bottom:20px;color:${accent}}
.ncui-card-icon svg{width:26px;height:26px}
.ncui-card .ncui-heading{font-size:1.25rem;margin-bottom:8px}
.ncui-card .ncui-text{font-size:.95rem;margin-bottom:0}

/* ─ Links ─ */
.ncui-link{color:${textSub};text-decoration:none;transition:color .2s;font-weight:500}
.ncui-link:hover{color:${accent}}

/* ─ Lists ─ */
.ncui-list{list-style:none;margin:20px 0}
.ncui-list-item{display:flex;align-items:center;gap:12px;padding:10px 0;color:${textSub};font-size:1.05rem}
.ncui-check{width:22px;height:22px;border-radius:50%;background:${accent}18;color:${accent};display:flex;align-items:center;justify-content:center;flex-shrink:0}
.ncui-check svg{width:14px;height:14px}

/* ─ Images ─ */
.ncui-img{width:100%;border-radius:16px;margin:20px 0}

/* ─ Forms ─ */
.ncui-form{max-width:520px;margin:32px 0;display:flex;flex-direction:column;gap:20px}
.ncui-centered .ncui-form{margin-left:auto;margin-right:auto}
.ncui-field{display:flex;flex-direction:column;gap:6px}
.ncui-field label{font-size:.85rem;font-weight:600;color:${textSub};text-transform:uppercase;letter-spacing:0.05em}
.ncui-field input,.ncui-field textarea{background:${inputBg};border:1.5px solid ${border};border-radius:12px;padding:14px 18px;color:${textMain};font-size:1rem;font-family:inherit;transition:all .3s;outline:none}
.ncui-field input:focus,.ncui-field textarea:focus{border-color:${accent};box-shadow:0 0 0 3px ${accent}22}
.ncui-field input::placeholder,.ncui-field textarea::placeholder{color:${textMuted}}
.ncui-form .ncui-btn{align-self:flex-start;margin-top:8px}

/* ─ Stats ─ */
.ncui-stat{text-align:center;padding:20px}
.ncui-stat-val{display:block;font-size:2.5rem;font-weight:800;letter-spacing:-0.03em;background:linear-gradient(135deg,${accent},${this._shiftHue(accent,60)});-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.ncui-stat-label{display:block;font-size:.9rem;color:${textMuted};margin-top:4px;font-weight:500}

/* ─ Badge ─ */
.ncui-badge{display:inline-block;padding:6px 16px;border-radius:100px;font-size:.8rem;font-weight:600;background:${accent}18;color:${accent};letter-spacing:0.03em}

/* ─ Progress ─ */
.ncui-progress{margin:12px 0}
.ncui-progress-label{font-size:.85rem;font-weight:600;color:${textSub};margin-bottom:6px;display:block}
.ncui-progress-bar{height:8px;border-radius:4px;background:${inputBg};overflow:hidden}
.ncui-progress-fill{height:100%;border-radius:4px;background:linear-gradient(90deg,${accent},${this._shiftHue(accent,60)});transition:width 1s cubic-bezier(.4,0,.2,1)}

/* ─ Divider ─ */
.ncui-divider{border:none;height:1px;background:${border};margin:40px 0}

/* ─ Spacer ─ */
.ncui-spacer{height:60px}

/* ─ Footer ─ */
.ncui-footer{padding:60px 0;border-top:1px solid ${border};text-align:center}
.ncui-footer .ncui-text{color:${textMuted};font-size:.9rem}
.ncui-footer .ncui-row{justify-content:center}
.ncui-footer .ncui-link{font-size:.9rem}

/* ─── Animations ─── */
.ncui-anim-fade-up .ncui-container>*{opacity:0;transform:translateY(30px);transition:all .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-fade-up .ncui-container>.ncui-visible{opacity:1;transform:translateY(0)}

.ncui-anim-fade-in .ncui-container>*{opacity:0;transition:opacity .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-fade-in .ncui-container>.ncui-visible{opacity:1}

.ncui-anim-slide-left .ncui-container>*{opacity:0;transform:translateX(-40px);transition:all .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-slide-left .ncui-container>.ncui-visible{opacity:1;transform:translateX(0)}

.ncui-anim-slide-right .ncui-container>*{opacity:0;transform:translateX(40px);transition:all .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-slide-right .ncui-container>.ncui-visible{opacity:1;transform:translateX(0)}

.ncui-anim-zoom .ncui-container>*{opacity:0;transform:scale(0.9);transition:all .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-zoom .ncui-container>.ncui-visible{opacity:1;transform:scale(1)}

.ncui-anim-stagger .ncui-container>*{opacity:0;transform:translateY(30px);transition:all .7s cubic-bezier(.4,0,.2,1)}
.ncui-anim-stagger .ncui-container>.ncui-visible{opacity:1;transform:translateY(0)}

/* Stagger children inside grids */
.ncui-anim-stagger .ncui-grid>.ncui-card{opacity:0;transform:translateY(30px);transition:all .6s cubic-bezier(.4,0,.2,1)}
.ncui-anim-stagger .ncui-grid>.ncui-card.ncui-visible{opacity:1;transform:translateY(0)}

/* ─── Background Glow ─── */
body::before{content:'';position:fixed;top:-50%;left:-50%;width:200%;height:200%;background:radial-gradient(circle at 30% 20%,${accent}08 0%,transparent 50%),radial-gradient(circle at 70% 80%,${this._shiftHue(accent, 120)}06 0%,transparent 50%);pointer-events:none;z-index:-1}

/* ─── Scrollbar ─── */
::-webkit-scrollbar{width:8px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:${border};border-radius:4px}
::-webkit-scrollbar-thumb:hover{background:${accent}44}

/* ─── Selection ─── */
::selection{background:${accent}33;color:${textMain}}
`;
  }

  // ─── JS ──────────────────────────────────────────────────────────────────

  _js() {
    return `
(function(){
  // Scroll-driven animations via IntersectionObserver
  const obs = new IntersectionObserver(function(entries){
    entries.forEach(function(entry){
      if(entry.isIntersecting){
        // Stagger children
        const parent = entry.target.closest('[class*=ncui-anim-stagger]');
        if(parent){
          const siblings = Array.from(entry.target.parentElement.children);
          const idx = siblings.indexOf(entry.target);
          entry.target.style.transitionDelay = (idx * 0.1) + 's';
        }
        entry.target.classList.add('ncui-visible');
      }
    });
  },{threshold:0.1,rootMargin:'0px 0px -40px 0px'});

  // Observe all section direct children and grid children
  document.querySelectorAll('[class*=ncui-anim-] .ncui-container > *, [class*=ncui-anim-] .ncui-grid > .ncui-card').forEach(function(el){
    obs.observe(el);
  });

  // Smooth nav background
  var nav = document.querySelector('.ncui-nav');
  if(nav){
    window.addEventListener('scroll',function(){
      if(window.scrollY > 50) nav.style.padding = '10px 0';
      else nav.style.padding = '16px 0';
    },{passive:true});
  }

  // Form submission handler
  document.querySelectorAll('.ncui-form').forEach(function(form){
    form.addEventListener('submit',function(e){
      e.preventDefault();
      var btn = form.querySelector('.ncui-btn');
      if(btn){
        var orig = btn.textContent;
        btn.textContent = 'Sent!';
        btn.style.pointerEvents = 'none';
        setTimeout(function(){btn.textContent = orig; btn.style.pointerEvents='auto';},2000);
      }
    });
  });
})();
`;
  }

  // ─── Helpers ─────────────────────────────────────────────────────────────

  _shiftHue(hex, degrees) {
    let r = parseInt(hex.slice(1, 3), 16) / 255;
    let g = parseInt(hex.slice(3, 5), 16) / 255;
    let b = parseInt(hex.slice(5, 7), 16) / 255;
    const max = Math.max(r, g, b), min = Math.min(r, g, b);
    let h, s, l = (max + min) / 2;
    if (max === min) { h = s = 0; }
    else {
      const d = max - min;
      s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
      switch (max) {
        case r: h = ((g - b) / d + (g < b ? 6 : 0)) / 6; break;
        case g: h = ((b - r) / d + 2) / 6; break;
        case b: h = ((r - g) / d + 4) / 6; break;
      }
    }
    h = ((h * 360 + degrees) % 360) / 360;
    function hue2rgb(p, q, t) {
      if (t < 0) t += 1; if (t > 1) t -= 1;
      if (t < 1/6) return p + (q - p) * 6 * t;
      if (t < 1/2) return q;
      if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
      return p;
    }
    if (s === 0) { r = g = b = l; }
    else {
      const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
      const p = 2 * l - q;
      r = hue2rgb(p, q, h + 1/3);
      g = hue2rgb(p, q, h);
      b = hue2rgb(p, q, h - 1/3);
    }
    const toHex = x => Math.round(x * 255).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  }
}

// ─── Public API ──────────────────────────────────────────────────────────────

function compile(source) {
  const parser = new NCUIParser(source);
  const ast = parser.parse();
  const gen = new NCUIGenerator(ast);
  return gen.generate();
}

function parse(source) {
  const parser = new NCUIParser(source);
  return parser.parse();
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { compile, parse, NCUIParser, NCUIGenerator };
}
if (typeof window !== 'undefined') {
  window.NCUICompiler = { compile, parse, NCUIParser, NCUIGenerator };
}
