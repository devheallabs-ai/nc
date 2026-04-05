/**
 * NC UI — Browser Runtime
 * Enables in-browser compilation of NC UI markup.
 *
 * Usage:
 *   <script src="nc-ui.js"></script>
 *   <script type="text/nc-ui">
 *     page "Hello"
 *     section hero centered:
 *         heading "Hello World" style "gradient"
 *   </script>
 *
 * Or via the API:
 *   NCUI.compile(source)         -> returns HTML string
 *   NCUI.render(source, element) -> compiles and injects into element
 */

(function (root) {
  'use strict';

  // ─── Inline Compiler (self-contained for browser) ────────────────────────
  // We load the full compiler.js logic. In a bundled build this would be
  // imported; for standalone usage we inline a lightweight copy.

  var _compiler = null;

  function _getCompiler() {
    if (_compiler) return _compiler;

    // Try to use the global NCUICompiler set by compiler.js if it was loaded
    if (root.NCUICompiler && root.NCUICompiler.compile) {
      _compiler = root.NCUICompiler;
      return _compiler;
    }

    // If compiler.js was not loaded separately, try to dynamically load it
    // by looking for a sibling script tag
    var scripts = document.querySelectorAll('script[src]');
    var basePath = '';
    for (var i = 0; i < scripts.length; i++) {
      var src = scripts[i].getAttribute('src');
      if (src && src.indexOf('nc-ui.js') !== -1) {
        basePath = src.replace(/nc-ui\.js.*$/, '');
        break;
      }
    }

    // Synchronous load for immediate availability
    if (basePath) {
      try {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', basePath + 'compiler.js', false); // synchronous
        xhr.send();
        if (xhr.status === 200) {
          var fn = new Function(xhr.responseText + '\nreturn {compile:compile,parse:parse};');
          _compiler = fn();
          return _compiler;
        }
      } catch (e) {
        console.warn('[NC UI] Could not load compiler.js:', e);
      }
    }

    // Fallback: check again for global
    if (root.NCUICompiler) {
      _compiler = root.NCUICompiler;
      return _compiler;
    }

    throw new Error('[NC UI] Compiler not found. Include compiler.js before nc-ui.js or load it in the same directory.');
  }

  // ─── Public API ──────────────────────────────────────────────────────────

  var NCUI = {
    /**
     * Compile NC UI source to an HTML string.
     * @param {string} source - NC UI markup
     * @returns {string} Complete HTML document
     */
    compile: function (source) {
      return _getCompiler().compile(source);
    },

    /**
     * Parse NC UI source into an AST.
     * @param {string} source - NC UI markup
     * @returns {object} AST
     */
    parse: function (source) {
      return _getCompiler().parse(source);
    },

    /**
     * Compile NC UI source and render into a target element.
     * If the target is an iframe, the compiled HTML is written to it.
     * Otherwise, the body content is injected along with styles.
     *
     * @param {string} source - NC UI markup
     * @param {HTMLElement} target - DOM element to render into
     * @returns {string} The compiled HTML
     */
    render: function (source, target) {
      var html = this.compile(source);

      if (!target) {
        console.error('[NC UI] render() requires a target element.');
        return html;
      }

      if (target.tagName === 'IFRAME') {
        var doc = target.contentDocument || target.contentWindow.document;
        doc.open();
        doc.write(html);
        doc.close();
      } else {
        // Extract body content and styles from the full HTML
        var bodyMatch = html.match(/<body[^>]*>([\s\S]*?)<\/body>/i);
        var styleMatch = html.match(/<style>([\s\S]*?)<\/style>/i);

        if (styleMatch) {
          var existingStyle = target.querySelector('style[data-ncui]');
          if (existingStyle) {
            existingStyle.textContent = styleMatch[1];
          } else {
            var style = document.createElement('style');
            style.setAttribute('data-ncui', 'true');
            style.textContent = styleMatch[1];
            (target.shadowRoot || document.head).appendChild(style);
          }
        }

        if (bodyMatch) {
          target.innerHTML = bodyMatch[1];
        } else {
          target.innerHTML = html;
        }

        // Execute scripts
        var scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/gi);
        if (scriptMatch) {
          scriptMatch.forEach(function (tag) {
            var code = tag.replace(/<\/?script>/gi, '');
            try { new Function(code)(); } catch (e) { console.warn('[NC UI] Script error:', e); }
          });
        }
      }

      return html;
    },

    /**
     * Live-compile: watches an input element and renders on every change.
     * @param {HTMLElement} input - textarea or input with NC UI source
     * @param {HTMLElement} target - element or iframe to render into
     * @param {number} [debounce=300] - debounce delay in ms
     */
    live: function (input, target, debounce) {
      debounce = debounce || 300;
      var timer = null;
      var self = this;

      function update() {
        var source = input.value || input.textContent || '';
        try {
          self.render(source, target);
        } catch (e) {
          console.warn('[NC UI] Compilation error:', e);
        }
      }

      input.addEventListener('input', function () {
        clearTimeout(timer);
        timer = setTimeout(update, debounce);
      });

      // Initial render
      update();
    },

    /** Version */
    version: '1.0.0'
  };

  // ─── Auto-compile <script type="text/nc-ui"> tags ────────────────────────

  function autoCompile() {
    var scripts = document.querySelectorAll('script[type="text/nc-ui"]');

    if (scripts.length === 0) return;

    scripts.forEach(function (script) {
      var source = script.textContent;
      try {
        var html = NCUI.compile(source);

        // Extract body and styles
        var bodyMatch = html.match(/<body[^>]*>([\s\S]*?)<\/body>/i);
        var styleMatch = html.match(/<style>([\s\S]*?)<\/style>/i);

        // Inject styles into head
        if (styleMatch) {
          var style = document.createElement('style');
          style.setAttribute('data-ncui', 'true');
          style.textContent = styleMatch[1];
          document.head.appendChild(style);
        }

        // Inject fonts
        var fontMatch = html.match(/<link[^>]*fonts[^>]*>/gi);
        if (fontMatch) {
          fontMatch.forEach(function (tag) {
            var href = tag.match(/href="([^"]+)"/);
            if (href) {
              var link = document.createElement('link');
              link.rel = 'stylesheet';
              link.href = href[1];
              document.head.appendChild(link);
            }
          });
        }

        // Replace body or inject after script
        if (bodyMatch) {
          var container = document.createElement('div');
          container.setAttribute('data-ncui-root', 'true');
          container.innerHTML = bodyMatch[1];
          script.parentNode.insertBefore(container, script.nextSibling);
        }

        // Execute inline scripts
        var jsMatch = html.match(/<script>([\s\S]*?)<\/script>/gi);
        if (jsMatch) {
          jsMatch.forEach(function (tag) {
            var code = tag.replace(/<\/?script>/gi, '');
            try { new Function(code)(); } catch (e) { /* ignore */ }
          });
        }

        // Update page title
        var titleMatch = html.match(/<title>([^<]+)<\/title>/i);
        if (titleMatch) document.title = titleMatch[1];

      } catch (e) {
        console.error('[NC UI] Failed to compile inline NC UI:', e);
      }
    });
  }

  // Run auto-compile when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', autoCompile);
  } else {
    autoCompile();
  }

  // ─── Export ──────────────────────────────────────────────────────────────

  root.NCUI = NCUI;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = NCUI;
  }

})(typeof window !== 'undefined' ? window : this);
