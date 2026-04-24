// assets/theme.js - tema global y inicialización de Mermaid (SonarLint-clean)
(function () {
  'use strict';

  const INDEX_KEY = 'mifutbolc-index-theme';
  const DOC_KEY = 'mifutbolc-doc-theme';
  const THEME_KEY = 'mifutbolc-theme';

  function safeGetItem(key) {
    try {
      return localStorage.getItem(key);
    } catch (error_) {
      console.warn('No se pudo leer localStorage', error_);
      return null;
    }
  }

  function safeSetItem(key, value) {
    try {
      localStorage.setItem(key, value);
    } catch (error_) {
      console.warn('No se pudo guardar localStorage', error_);
    }
  }

  function prefersDark() {
    return globalThis.matchMedia?.('(prefers-color-scheme: dark)')?.matches ? 'dark' : 'light';
  }

  function updateBtn(button, theme) {
    if (!button) return;

    let orig = button.dataset.origText;
    if (!orig) {
      orig = button.textContent || '';
      button.dataset.origText = orig;
    }

    const compact = orig.includes(':');
    const dark = theme === 'dark';

    if (compact) {
      button.textContent = dark ? 'Modo oscuro: activo' : 'Modo claro: activo';
    } else {
      button.textContent = dark ? 'Modo oscuro' : 'Modo claro';
    }

    button.setAttribute('aria-pressed', dark ? 'true' : 'false');
    button.setAttribute('aria-label', dark ? 'Cambiar a modo claro' : 'Cambiar a modo oscuro');
  }

  function saveAllThemes(theme) {
    safeSetItem(INDEX_KEY, theme);
    safeSetItem(DOC_KEY, theme);
    safeSetItem(THEME_KEY, theme);
  }

  function init() {
    const root = document.documentElement;
    const button = document.getElementById('theme-toggle');

    const stored = safeGetItem(INDEX_KEY) || safeGetItem(DOC_KEY) || safeGetItem(THEME_KEY);
    const theme = (stored === 'dark' || stored === 'light') ? stored : prefersDark();

    root.dataset.theme = theme;
    updateBtn(button, theme);

    if (button) {
      button.addEventListener('click', function () {
        const current = root.dataset.theme === 'dark' ? 'dark' : 'light';
        const next = current === 'dark' ? 'light' : 'dark';
        root.dataset.theme = next;
        saveAllThemes(next);
        updateBtn(button, next);
      });
    }

    const yearEl = document.getElementById('foot-year');
    if (yearEl) {
      yearEl.textContent = String(new Date().getFullYear());
    }

    if (globalThis.mermaid && typeof globalThis.mermaid.initialize === 'function') {
      try {
        globalThis.mermaid.initialize({
          startOnLoad: true,
          theme: theme === 'dark' ? 'dark' : 'default'
        });
      } catch (error_) {
        console.warn('mermaid init failed', error_);
      }
    }
  }

  document.addEventListener('DOMContentLoaded', init);
})();
