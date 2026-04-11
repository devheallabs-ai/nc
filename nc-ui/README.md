# NC UI

**Build websites in plain English.**

NC UI is a frontend language and compiler that takes human-readable markup and outputs stunning, production-ready HTML + CSS + JS. It is part of the [NC language](https://github.com/devheal-labs/nc) ecosystem — the same way NC builds APIs in plain English, NC UI builds websites.

Write 20 lines of NC UI. Get a website that looks like a skilled designer built it.

---

## Quick Start

### 1. Write a `.ncui` file

```nc
page "My Site"
theme "dark"
accent "#00d4ff"

section hero centered:
    heading "Hello World" style "gradient"
    text "Built with NC UI"
    button "Learn More" links to "#about" style "primary"
    animate "fade-up"
```

### 2. Compile it

```bash
node nc-ui/cli.js build mysite.ncui
```

This produces `mysite.html` — a complete, self-contained HTML file with embedded CSS and JS. No build tools, no dependencies, no frameworks.

### 3. Or serve it with live reload

```bash
node nc-ui/cli.js serve mysite.ncui
```

Open `http://localhost:3000` and edit your `.ncui` file. The browser refreshes automatically.

---

## Installation

NC UI has zero external dependencies when used through the NC engine.

```bash
# Clone the NC repository
git clone https://github.com/devheallabs-ai/nc.git
cd nc/nc-lang/nc-ui

# Build an example
nc ui build examples/portfolio.ncui

# Serve with live reload
nc ui serve examples/portfolio.ncui
```

---

## CLI Reference

| Command | Description |
|---------|-------------|
| `nc ui build <file.ncui>` | Compile to a single `.html` file |
| `nc ui watch <file.ncui>` | Watch for changes and rebuild automatically |
| `nc ui serve <file.ncui> [port]` | Serve with live reload (default port: 3000) |

---

## Syntax Reference

### Page Metadata

Set the page title, theme, and accent color at the top of your file:

```nc
page "My Website"
theme "dark"
accent "#00d4ff"
font "Inter"
```

| Property | Values | Default |
|----------|--------|---------|
| `theme` | `"dark"`, `"light"` | `"dark"` |
| `accent` | Any hex color | `"#00d4ff"` |
| `font` | Any Google Font name | `"Inter"` |

### Navigation

```nc
nav:
    brand "MySite"
    links:
        link "Home" to "#"
        link "About" to "#about"
        button "Sign Up" links to "#signup" style "primary"
```

The nav is fixed, has a glass blur effect, and collapses into a hamburger menu on mobile.

### Sections

Sections are the top-level layout blocks:

```nc
section hero centered:
    heading "Welcome" style "gradient"
    animate "fade-up"

section features:
    heading "Features"
    grid 3 columns:
        card icon "code":
            heading "Fast"
            text "Lightning speed"

section about with image "bg.jpg":
    heading "Our Story"
    text "We started in 2020..."
```

| Modifier | Effect |
|----------|--------|
| `centered` | Centers all text and content |
| `with image "url"` | Adds a background image overlay |

The section name becomes its HTML `id`, so `section features:` becomes `<section id="features">`.

### Components

#### heading
```nc
heading "Title"
heading "Fancy Title" style "gradient"
```

#### subheading
```nc
subheading "A secondary line of text"
```

#### text
```nc
text "Body copy goes here"
```

#### button
```nc
button "Click Me" style "primary"
button "Learn More" links to "#about" style "outline"
button "Glass Button" style "glass"
button "Gradient" style "gradient"
```

Styles: `primary`, `outline`, `glass`, `gradient`

#### link
```nc
link "GitHub" to "https://github.com"
```

#### row
```nc
row:
    button "One" style "primary"
    button "Two" style "outline"
```

#### grid
```nc
grid 3 columns:
    card:
        heading "A"
    card:
        heading "B"
    card:
        heading "C"
```

Supports 2, 3, or 4 columns. Automatically becomes 2 columns on tablet and 1 column on mobile.

#### card
```nc
card:
    heading "Title"
    text "Description"

card icon "rocket":
    heading "With Icon"
    text "Cards can have icons"
```

Cards have glass borders, hover lift effects, and a gradient top-border on hover.

#### image
```nc
image "photo.jpg"
```

#### list
```nc
list:
    item "First thing"
    item "Second thing"
    item "Third thing"
```

Items get a styled checkmark icon automatically.

#### form
```nc
form action "/api/submit":
    input "Name" required
    input "Email" type "email" required
    textarea "Message"
    button "Submit" style "primary"
```

Forms have styled inputs with focus effects and are responsive by default.

#### stat
```nc
stat "10K+" "Users"
```

Displays a large gradient number with a label beneath it.

#### badge
```nc
badge "New Feature"
```

A small pill-shaped label, great for announcements.

#### progress
```nc
progress "Completion" 75%
```

An animated progress bar with a gradient fill.

#### divider / spacer
```nc
divider
spacer
```

### Footer

```nc
footer:
    text "Copyright 2026"
    row:
        link "GitHub" to "https://github.com"
        link "Twitter" to "https://twitter.com"
```

### Animations

Add an `animate` directive inside any section to animate its children on scroll:

```nc
section features:
    heading "Features"
    grid 3 columns:
        card:
            heading "One"
    animate "stagger"
```

| Animation | Effect |
|-----------|--------|
| `"fade-up"` | Fades in while sliding up |
| `"fade-in"` | Simple fade |
| `"slide-left"` | Slides in from the left |
| `"slide-right"` | Slides in from the right |
| `"zoom"` | Scales up from 90% |
| `"stagger"` | Like fade-up, but each child animates in sequence |

Animations are powered by IntersectionObserver and trigger when elements scroll into view.

### Built-in Icons

Use icons in cards:

```nc
card icon "code":
card icon "design":
card icon "rocket":
card icon "brain":
card icon "shield":
card icon "globe":
card icon "star":
card icon "heart":
card icon "check":
card icon "arrow":
card icon "chart":
card icon "users":
card icon "mail":
card icon "clock":
card icon "menu":
```

Icons are inline SVGs — no external icon library needed.

---

## Browser Runtime

Include NC UI in any HTML page for client-side compilation:

```html
<script src="compiler.js"></script>
<script src="nc-ui.js"></script>

<script type="text/nc-ui">
page "Dynamic Page"
theme "dark"
section hero centered:
    heading "Compiled in the browser!" style "gradient"
</script>
```

### JavaScript API

```js
// Compile to HTML string
const html = NCUI.compile(source);

// Compile and render into an element
NCUI.render(source, document.getElementById('preview'));

// Compile and render into an iframe
NCUI.render(source, document.querySelector('iframe'));

// Live mode: auto-recompile as user types in a textarea
NCUI.live(textarea, iframe, 300);
```

---

## Comparison: NC UI vs HTML + CSS + JS

### NC UI (18 lines)

```nc
page "Portfolio"
theme "dark"
accent "#00d4ff"

section hero centered:
    heading "Jane Smith" style "gradient"
    subheading "Designer & Developer"
    text "I build beautiful things"
    row:
        button "My Work" links to "#work" style "primary"
        button "Contact" links to "#contact" style "outline"
    animate "fade-up"

footer:
    text "Built with NC UI"
    row:
        link "GitHub" to "https://github.com"
```

### Equivalent HTML + CSS + JS (~200 lines)

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Portfolio</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;800&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: 'Inter', sans-serif; background: #0a0a0f; color: #f0f0f5; }
    .container { max-width: 1200px; margin: 0 auto; padding: 0 24px; }
    .hero { min-height: 100vh; display: flex; align-items: center; text-align: center; }
    h1 { font-size: 3.5rem; font-weight: 800;
         background: linear-gradient(135deg, #00d4ff, #00ff88);
         -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .subtitle { font-size: 1.5rem; color: rgba(255,255,255,0.65); margin: 12px 0; }
    p { color: rgba(255,255,255,0.65); font-size: 1.05rem; max-width: 640px; margin: 0 auto 16px; }
    .buttons { display: flex; gap: 16px; justify-content: center; margin: 24px 0; }
    .btn { padding: 14px 32px; border-radius: 12px; font-weight: 600; text-decoration: none;
           transition: all 0.3s; cursor: pointer; border: none; font-family: inherit; }
    .btn-primary { background: linear-gradient(135deg, #00d4ff, #00ffaa); color: #fff;
                   box-shadow: 0 4px 24px rgba(0,212,255,0.27); }
    .btn-primary:hover { transform: translateY(-2px); box-shadow: 0 8px 32px rgba(0,212,255,0.4); }
    .btn-outline { background: transparent; color: #00d4ff; border: 1.5px solid #00d4ff; }
    .btn-outline:hover { background: rgba(0,212,255,0.09); transform: translateY(-2px); }
    footer { padding: 60px 0; border-top: 1px solid rgba(255,255,255,0.08); text-align: center; }
    footer p { color: rgba(255,255,255,0.4); font-size: 0.9rem; }
    footer .links { display: flex; gap: 16px; justify-content: center; margin-top: 12px; }
    footer a { color: rgba(255,255,255,0.65); text-decoration: none; }
    footer a:hover { color: #00d4ff; }
    /* ... animations, responsive, scrollbar, selection, etc. — 100+ more lines */
  </style>
</head>
<body>
  <section class="hero">
    <div class="container">
      <h1>Jane Smith</h1>
      <p class="subtitle">Designer & Developer</p>
      <p>I build beautiful things</p>
      <div class="buttons">
        <a href="#work" class="btn btn-primary">My Work</a>
        <a href="#contact" class="btn btn-outline">Contact</a>
      </div>
    </div>
  </section>
  <footer>
    <div class="container">
      <p>Built with NC UI</p>
      <div class="links">
        <a href="https://github.com">GitHub</a>
      </div>
    </div>
  </footer>
  <script>
    // IntersectionObserver animation code...
    // Scroll effects...
    // 30+ lines of JS
  </script>
</body>
</html>
```

**NC UI: 18 lines. HTML/CSS/JS: ~200 lines. Same result.**

---

## Design Philosophy

1. **Plain English first.** If you can describe it, you can build it.
2. **Beautiful by default.** Every site looks polished — glassmorphism, smooth animations, responsive layout — without writing a single line of CSS.
3. **Zero dependencies.** The compiler is a single JS file. The output is a single HTML file. No npm install, no webpack, no React.
4. **Part of NC.** NC UI handles the frontend. NC handles the backend. Together, they let you build a full-stack app in plain English.

---

## Examples

See the `examples/` directory:

- `portfolio.ncui` — Personal portfolio site
- `landing.ncui` — SaaS product landing page
- `dashboard.ncui` — Analytics dashboard

Build all of them:

```bash
node cli.js build examples/portfolio.ncui
node cli.js build examples/landing.ncui
node cli.js build examples/dashboard.ncui
```

---

## License

MIT License. Part of the NC project by DevHeal Labs AI.
