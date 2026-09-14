(function () {
  'use strict';

  // ── Theme: no stored preference means "follow the system" ──
  var root = document.documentElement;
  var STORE = 'devheal-theme';

  function stored() {
    try { return localStorage.getItem(STORE); } catch (e) { return null; }
  }
  function systemPrefersDark() {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
  }
  function effectiveTheme() {
    return root.getAttribute('data-theme') || (systemPrefersDark() ? 'dark' : 'light');
  }

  var saved = stored();
  if (saved === 'dark' || saved === 'light') root.setAttribute('data-theme', saved);

  var toggle = document.getElementById('themeToggle');
  if (toggle) {
    toggle.addEventListener('click', function () {
      var next = effectiveTheme() === 'dark' ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem(STORE, next); } catch (e) {}
    });
  }

  // ── Nav ──
  var nav = document.getElementById('nav');
  if (nav) {
    var onScroll = function () { nav.classList.toggle('scrolled', window.scrollY > 8); };
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();
  }

  var burger = document.querySelector('.hamburger');
  var links = document.querySelector('.nav-links');
  if (burger && links) {
    burger.addEventListener('click', function () { links.classList.toggle('open'); });
    links.querySelectorAll('a').forEach(function (a) {
      a.addEventListener('click', function () { links.classList.remove('open'); });
    });
  }

  // ── Sub-nav scroll-spy (long pages only) ──
  var subnav = document.getElementById('subnav');
  if (subnav && 'IntersectionObserver' in window) {
    var subLinks = [].slice.call(subnav.querySelectorAll('a[href^="#"]'));
    var targets = subLinks
      .map(function (a) { return document.getElementById(a.getAttribute('href').slice(1)); })
      .filter(Boolean);

    var lastId = null;
    var bar = subnav.querySelector('.subnav-inner');

    var setCurrent = function (id) {
      if (id === lastId) return;
      lastId = id;
      subLinks.forEach(function (a) {
        var on = a.getAttribute('href') === '#' + id;
        a.classList.toggle('current', on);
        // keep the active pill in view when the bar scrolls horizontally
        if (on && bar && bar.scrollWidth > bar.clientWidth + 1) {
          var want = a.offsetLeft - bar.clientWidth / 2 + a.offsetWidth / 2;
          bar.scrollTo({ left: Math.max(0, want), behavior: 'smooth' });
        }
      });
    };

    // Pick the last section whose top has crossed the line just under the
    // sticky chrome. A percentage-based IntersectionObserver band collapses on
    // short/zoomed viewports and highlights the wrong section, so measure directly.
    var CHROME = 170;
    var queued = false;

    var updateSpy = function () {
      queued = false;
      var currentId = null;
      for (var i = 0; i < targets.length; i++) {
        if (targets[i].getBoundingClientRect().top <= CHROME) currentId = targets[i].id;
      }
      // past the end of the page, keep the last section lit
      if (!currentId && window.scrollY + window.innerHeight >= document.documentElement.scrollHeight - 4) {
        currentId = targets[targets.length - 1].id;
      }
      setCurrent(currentId);
    };

    var onSpyScroll = function () {
      if (queued) return;
      queued = true;
      requestAnimationFrame(updateSpy);
    };

    window.addEventListener('scroll', onSpyScroll, { passive: true });
    window.addEventListener('resize', onSpyScroll, { passive: true });
    updateSpy();
  }

  // ── Mark the current page in the nav (skip same-page anchors) ──
  var path = location.pathname.split('/').pop() || 'index.html';
  document.querySelectorAll('.nav-links a[href]').forEach(function (a) {
    var href = a.getAttribute('href');
    if (href.indexOf('#') !== -1 || href.indexOf('mailto:') === 0) return;
    if (href === path && path !== 'index.html') a.classList.add('active');
  });
})();
