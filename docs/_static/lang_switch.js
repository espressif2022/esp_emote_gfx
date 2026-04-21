/**
 * ZH / EN switcher: same path under /en/ or /zh_CN/ (GitHub Pages subpath safe).
 */
(function () {
  function swapLang(pathname, targetLang) {
    var trimmed = pathname.replace(/^\/+|\/+$/g, '');
    if (!trimmed) {
      return null;
    }
    var segments = trimmed.split('/');
    var i = segments.indexOf('en');
    if (i === -1) {
      i = segments.indexOf('zh_CN');
    }
    if (i === -1) {
      return null;
    }
    segments[i] = targetLang;
    return '/' + segments.join('/');
  }

  document.addEventListener('DOMContentLoaded', function () {
    var enBtn = document.getElementById('gfx-lang-en');
    var zhBtn = document.getElementById('gfx-lang-zh');
    if (!enBtn || !zhBtn) {
      return;
    }

    var path = window.location.pathname;
    var enHref = swapLang(path, 'en');
    var zhHref = swapLang(path, 'zh_CN');

    if (!enHref || !zhHref) {
      enBtn.classList.add('gfx-lang-btn--muted');
      zhBtn.classList.add('gfx-lang-btn--muted');
      enBtn.setAttribute('aria-disabled', 'true');
      zhBtn.setAttribute('aria-disabled', 'true');
      return;
    }

    enBtn.href = enHref;
    zhBtn.href = zhHref;

    if (path.indexOf('/zh_CN/') !== -1) {
      zhBtn.classList.add('is-active');
      zhBtn.setAttribute('aria-current', 'true');
    } else {
      enBtn.classList.add('is-active');
      enBtn.setAttribute('aria-current', 'true');
    }
  });
})();
