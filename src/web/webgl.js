// ===== TreeVisual HW-Accelerated Character Tree Renderer =====
(function(global) {
  'use strict';

  var _enabled = false;
  var _canvas = null;
  var _ctx = null;
  var _container = null;
  var _tooltipEl = null;
  var _resizeObs = null;
  var _panX = 0, _panY = 0;
  var _zoom = 1;
  var _dragging = false;
  var _lastMX = 0, _lastMY = 0;
  var _navigateCallback = null;

  var _rootData = null;
  var _lines = [];
  var _lineMeta = [];
  var _lineH = 20;

  var _expanded = {};
  var _childrenCache = {};

  // Animation state
  var _animState = null;

  // Catppuccin themes
  var _themes = {
    mocha:     { bg: '#1a1b2e', text: '#cdd6f4', dir: '#89b4fa', root: '#cba6f7' },
    macchiato: { bg: '#142b1f', text: '#c8e6c9', dir: '#4caf50', root: '#2e7d32' },
    frappe:    { bg: '#2a1810', text: '#f5d6b8', dir: '#e67e22', root: '#c0392b' },
    latte:     { bg: '#1c1917', text: '#e8e8e8', dir: '#60a5fa', root: '#a78bfa' }
  };

  var _themesLight = {
    mocha:     { bg: '#e8e8f2', text: '#2c2c3e', dir: '#4a6cf7', root: '#8b5cf7' },
    macchiato: { bg: '#e8f5e9', text: '#1b3a1b', dir: '#2e7d32', root: '#1b5e20' },
    frappe:    { bg: '#fef3e8', text: '#3d2418', dir: '#d97706', root: '#b91c1c' },
    latte:     { bg: '#f8f5f0', text: '#2c2c2c', dir: '#2563eb', root: '#7c3aed' }
  };

  var _cssThemes = {
    mocha: {
      '--bg-dark': '#1a1b2e', '--bg-panel': '#151624', '--bg-hover': '#2a2b45',
      '--border': '#3d3e5c',
      '--text-primary': '#cdd6f4', '--text-muted': '#a0a8c8',
      '--accent-blue': '#89b4fa', '--accent-purple': '#cba6f7',
      '--accent-green': '#a6e3a1', '--accent-red': '#f38ba8', '--accent-yellow': '#f9e2af'
    },
    macchiato: {
      '--bg-dark': '#142b1f', '--bg-panel': '#0f2217', '--bg-hover': '#1e3d2c',
      '--border': '#2d4f3a',
      '--text-primary': '#c8e6c9', '--text-muted': '#8fad8f',
      '--accent-blue': '#4caf50', '--accent-purple': '#2e7d32',
      '--accent-green': '#66bb6a', '--accent-red': '#e57373', '--accent-yellow': '#ffd54f'
    },
    frappe: {
      '--bg-dark': '#2a1810', '--bg-panel': '#1f110a', '--bg-hover': '#3d2418',
      '--border': '#52301f',
      '--text-primary': '#f5d6b8', '--text-muted': '#c49a7a',
      '--accent-blue': '#e67e22', '--accent-purple': '#c0392b',
      '--accent-green': '#a8b84a', '--accent-red': '#e74c3c', '--accent-yellow': '#f1c40f'
    },
    latte: {
      '--bg-dark': '#1c1917', '--bg-panel': '#292524', '--bg-hover': '#3d3834',
      '--border': '#524e49',
      '--text-primary': '#e8e8e8', '--text-muted': '#a8a29e',
      '--accent-blue': '#60a5fa', '--accent-purple': '#a78bfa',
      '--accent-green': '#34d399', '--accent-red': '#fb7185', '--accent-yellow': '#fbbf24'
    }
  };

  var _cssThemesLight = {
    mocha: {
      '--bg-dark': '#e8e8f2', '--bg-panel': '#dddde8', '--bg-hover': '#d0d0e0',
      '--border': '#c0c0d0',
      '--text-primary': '#2c2c3e', '--text-muted': '#6c6c8a',
      '--accent-blue': '#4a6cf7', '--accent-purple': '#8b5cf7',
      '--accent-green': '#22c55e', '--accent-red': '#ef4444', '--accent-yellow': '#eab308'
    },
    macchiato: {
      '--bg-dark': '#e8f5e9', '--bg-panel': '#dcefdc', '--bg-hover': '#c8e6c9',
      '--border': '#a5d6a7',
      '--text-primary': '#1b3a1b', '--text-muted': '#558b55',
      '--accent-blue': '#2e7d32', '--accent-purple': '#1b5e20',
      '--accent-green': '#43a047', '--accent-red': '#e57373', '--accent-yellow': '#fdd835'
    },
    frappe: {
      '--bg-dark': '#fef3e8', '--bg-panel': '#f5e6d4', '--bg-hover': '#edd9bf',
      '--border': '#dcc4a8',
      '--text-primary': '#3d2418', '--text-muted': '#8c6d56',
      '--accent-blue': '#d97706', '--accent-purple': '#b91c1c',
      '--accent-green': '#84cc16', '--accent-red': '#ef4444', '--accent-yellow': '#f59e0b'
    },
    latte: {
      '--bg-dark': '#f8f5f0', '--bg-panel': '#ece7e0', '--bg-hover': '#ddd8d0',
      '--border': '#c5c0b8',
      '--text-primary': '#2c2c2c', '--text-muted': '#707070',
      '--accent-blue': '#2563eb', '--accent-purple': '#7c3aed',
      '--accent-green': '#16a34a', '--accent-red': '#dc2626', '--accent-yellow': '#ca8a04'
    }
  };

  // Settings (defaults)
  var _fontSize = 14;
  var _fontFamily = '"Cascadia Code","Fira Code","JetBrains Mono",monospace';
  var _animDuration = 200;
  var _animStagger = 30;
  var _animEnabled = true;
  var _colors = Object.assign({}, _themes.mocha);

  // ===== Lazy-load children from API =====
  function apiTreeUrl(path) {
    return '/api/tree?path=' + encodeURIComponent(path);
  }

  function ensureChildren(path, callback) {
    var node = findNode(_rootData, path);
    if (node && node.children && node.children.length > 0) {
      callback(node.children); return;
    }
    if (_childrenCache[path] !== undefined) {
      callback(_childrenCache[path]); return;
    }
    fetch(apiTreeUrl(path))
      .then(function(r) { return r.json(); })
      .then(function(data) {
        var kids = (data && data.children) ? data.children : [];
        _childrenCache[path] = kids;
        callback(kids);
      })
      .catch(function() {
        _childrenCache[path] = [];
        callback([]);
      });
  }

  function findNode(node, targetPath) {
    if (!node) return null;
    var p = node.path || node.name;
    if (p === targetPath) return node;
    var kids = getNodeChildren(p, node);
    if (kids) {
      for (var i = 0; i < kids.length; i++) {
        var found = findNode(kids[i], targetPath);
        if (found) return found;
      }
    }
    return null;
  }

  function getNodeChildren(path, node) {
    if (node && node.children && node.children.length > 0) return node.children;
    if (_childrenCache[path] !== undefined) return _childrenCache[path];
    return null;
  }

  // ===== Build visible lines =====
  function buildLines(json) {
    var out = [], meta = [];

    function walk(node, prefix, isLast, isRoot) {
      var path = node.path || node.name;
      var isDir = node.type === 'directory' || !!node.hasChildren;
      var cachedKids = _childrenCache[path];
      var hasKids = !!(node.children && node.children.length > 0) || !!node.hasChildren || !!(cachedKids && cachedKids.length > 0);
      var expanded = !!_expanded[path];

      if (isRoot) {
        var nm = node.name === '/' ? 'Root' : node.name;
        out.push(nm + (nm[nm.length - 1] !== '/' ? '/' : ''));
        meta.push({ path: path, isDir: isDir, hasChildren: hasKids, arrowEnd: 0, expanded: false });
      } else {
        var conn = isLast ? '\u2514\u2500 ' : '\u251c\u2500 ';
        var arr = hasKids ? (expanded ? '\u25bc ' : '\u25b6 ') : '';
        out.push(prefix + conn + arr + node.name + (isDir && node.name[node.name.length - 1] !== '/' ? '/' : ''));
        var arrowEnd = prefix.length + conn.length + (hasKids ? 2 : 0);
        meta.push({ path: path, isDir: isDir, hasChildren: hasKids, arrowEnd: arrowEnd, expanded: expanded });
      }

      if (expanded) {
        var kids = getNodeChildren(path, node);
        if (kids) {
          for (var i = 0; i < kids.length && i < 300; i++) {
            var cp = prefix;
            if (!isRoot) {
              cp = prefix + (isLast ? '    ' : '\u2502   ');
            }
            walk(kids[i], cp, i === kids.length - 1, false);
          }
        }
      }
    }
    walk(json, '', true, true);
    return { lines: out, meta: meta };
  }

  var _animExpandPending = null;

  function rebuild() {
    if (_rootData) {
      var oldLen = _lines.length;
      var pending = _animExpandPending;
      _animExpandPending = null;

      var tree = buildLines(_rootData);
      _lines = tree.lines;
      _lineMeta = tree.meta;
      clampPanY();

      if (_animEnabled && pending && _lines.length > oldLen) {
        _animState = {
          startTime: performance.now(),
          parentIdx: pending.parentIdx,
          count: _lines.length - oldLen,
          duration: _animDuration,
          stagger: _animStagger
        };
      }
      requestRender();
    }
  }

  // ===== Rendering =====
  function renderFrame() {
    if (!_enabled || !_ctx) return;
    console.log('[renderFrame] bg=' + _colors.bg + ' text=' + _colors.text + ' lines=' + _lines.length);

    var c = _canvas;
    var cw = c.clientWidth || c.parentElement.clientWidth || 1;
    var ch = c.clientHeight || c.parentElement.clientHeight || 1;
    var dpr = window.devicePixelRatio || 1;

    if (c.width !== Math.round(cw * dpr) || c.height !== Math.round(ch * dpr)) {
      c.width = Math.round(cw * dpr); c.height = Math.round(ch * dpr);
    }

    _ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    _ctx.fillStyle = _colors.bg;
    _ctx.fillRect(0, 0, cw, ch);

    _ctx.setTransform(dpr, 0, 0, dpr, _panX * dpr, _panY * dpr);
    _ctx.scale(_zoom, _zoom);

    _ctx.font = _fontSize + 'px ' + _fontFamily;
    _ctx.textBaseline = 'top';

    var yOff = 6;
    var as = _animState;
    var now = as ? performance.now() : 0;

    for (var i = 0; i < _lines.length; i++) {
      var text = _lines[i];
      var m = _lineMeta[i];
      var y = yOff + i * _lineH;
      var alpha = 1;

      if (as && i > as.parentIdx && i <= as.parentIdx + as.count) {
        var childIdx = i - as.parentIdx - 1;
        var delay = childIdx * as.stagger;
        var p = Math.min(1, Math.max(0, (now - as.startTime - delay) / as.duration));
        p = 1 - (1 - p) * (1 - p); // ease-out quad
        alpha = p;
        var startY = yOff + as.parentIdx * _lineH;
        y = startY + (y - startY) * p;
      }

      if (alpha < 1) _ctx.globalAlpha = alpha;

      if (i === 0) {
        _ctx.fillStyle = _colors.root;
      } else {
        _ctx.fillStyle = m && m.isDir ? _colors.dir : _colors.text;
      }
      _ctx.fillText(text, 4, y);

      if (alpha < 1) _ctx.globalAlpha = 1;
    }

    // Continue animation
    if (as) {
      var totalTime = as.duration + (as.count - 1) * as.stagger;
      if (now - as.startTime < totalTime) {
        requestAnimationFrame(function() { requestRender(); });
      } else {
        _animState = null;
      }
    }
  }

  function requestRender() {
    requestAnimationFrame(function() {
      renderFrame();
    });
  }

  function clampPanY() {
    var c = _canvas;
    var ch = c.clientHeight || c.parentElement.clientHeight || 1;
    var treeCssH = _zoom * (6 + _lines.length * _lineH + 6);
    var maxPanY = 6;
    var minPanY = Math.min(ch - treeCssH - 6, 6);
    _panY = Math.min(maxPanY, Math.max(minPanY, _panY));
  }

  // ===== Mouse =====
  function hitTest(screenX, screenY) {
    var ty = (screenY - _panY) / _zoom;
    var idx = Math.floor((ty - 6) / _lineH);
    if (idx >= 0 && idx < _lineMeta.length) {
      return { idx: idx, meta: _lineMeta[idx] };
    }
    return null;
  }

  function handleMouseMove(e) {
    var rect = _canvas.getBoundingClientRect();
    var mx = e.clientX - rect.left;
    var my = e.clientY - rect.top;

    if (_dragging) {
      _panX += (e.clientX - _lastMX) / _zoom;
      _panY += (e.clientY - _lastMY) / _zoom;
      _lastMX = e.clientX; _lastMY = e.clientY;
      requestRender();
      return;
    }

    var hit = hitTest(mx, my);
    if (hit && hit.meta && hit.meta.isDir && _tooltipEl) {
      _tooltipEl.textContent = hit.meta.path;
      _tooltipEl.style.display = 'block';
      _tooltipEl.style.left = (mx + 12) + 'px';
      _tooltipEl.style.top = (my + 12) + 'px';
    } else if (_tooltipEl) {
      _tooltipEl.style.display = 'none';
    }
  }

  function handleMouseDown(e) {
    _ctx.font = _fontSize + 'px ' + _fontFamily;
    var rect = _canvas.getBoundingClientRect();
    var mx = e.clientX - rect.left;
    var my = e.clientY - rect.top;
    var hit = hitTest(mx, my);

    if (hit && hit.meta && hit.meta.isDir) {
      var tx = (mx - _panX) / _zoom;
      var isArrow = false;
      if (hit.meta.hasChildren && hit.meta.arrowEnd > 0) {
        var lineText = _lines[hit.idx];
        var prefixWidth = _ctx.measureText(lineText.substring(0, hit.meta.arrowEnd)).width;
        isArrow = (tx < 4 + prefixWidth);
      }

      if (isArrow) {
        var path = hit.meta.path;
        if (_expanded[path]) {
          delete _expanded[path];
          rebuild();
        } else {
          _expanded[path] = true;
          _animExpandPending = { parentIdx: hit.idx };
          ensureChildren(path, function() { rebuild(); });
        }
        return;
      }

      if (_navigateCallback) {
        _navigateCallback(hit.meta.path);
        return;
      }
    }

    _dragging = true;
    _lastMX = e.clientX; _lastMY = e.clientY;
  }

  function handleMouseUp() { _dragging = false; }

  function handleWheel(e) {
    e.preventDefault();
    _panY -= e.deltaY;
    clampPanY();
    requestRender();
  }

  // ===== Public API =====
  global.HWRenderer = {
    _initialized: false,
    _cssThemes: _cssThemes,
    _cssThemesLight: _cssThemesLight,

    init: function() {
      if (this._initialized) return true;
      try {
        _canvas = document.getElementById('hwCanvas');
        if (!_canvas) return false;
        _ctx = _canvas.getContext('2d');
        if (!_ctx) return false;
        _container = document.getElementById('hwContainer');
        _tooltipEl = document.getElementById('hwTooltip');
        _canvas.addEventListener('mousemove', handleMouseMove);
        _canvas.addEventListener('mousedown', handleMouseDown);
        _canvas.addEventListener('mouseup', handleMouseUp);
        _canvas.addEventListener('mouseleave', function() { _dragging = false; if(_tooltipEl)_tooltipEl.style.display='none'; });
        _canvas.addEventListener('wheel', handleWheel, { passive: false });
        _resizeObs = new ResizeObserver(function() { requestRender(); });
        _resizeObs.observe(_canvas);
        this._initialized = true;
        return true;
      } catch(e) { console.error('[HWRenderer] Init failed:', e); return false; }
    },

    loadTreeData: function(json) {
      _rootData = json;
      _childrenCache = {};
      if (!_enabled) return;
      _panX = 0; _panY = 0; _zoom = 1;
      _expanded = {};
      _expanded[json.path || json.name] = true;
      rebuild();
    },

    enable: function(navigateFn) {
      _navigateCallback = navigateFn || null;
      _enabled = true;
      global._hwEnabled = true;
      if (_rootData) this.loadTreeData(_rootData);
      else if (global._lastTreeData) this.loadTreeData(global._lastTreeData);
      requestRender();
    },

    disable: function() {
      _enabled = false;
      global._hwEnabled = false;
      if (_resizeObs) { _resizeObs.disconnect(); _resizeObs = null; }
      _lines = []; _lineMeta = []; _rootData = null; _expanded = {}; _childrenCache = {};
      _animState = null; _animExpandPending = null;
    },

    isEnabled: function() { return _enabled; },

    // Apply settings from user preferences
    applySettings: function(s) {
      if (s.fontSize !== undefined) _fontSize = s.fontSize;
      if (s.animDuration !== undefined) _animDuration = s.animDuration;
      if (s.animEnabled !== undefined) _animEnabled = s.animEnabled;
      if (s.fontFamily !== undefined) _fontFamily = s.fontFamily;
      // Resolve theme
      if (s.theme !== undefined && s.theme !== 'custom') {
        var pool = (s.mode === 'light') ? _themesLight : _themes;
        var tc = pool[s.theme];
        if (tc) Object.assign(_colors, tc);
      } else if (s.colors) {
        if (s.colors.bg !== undefined) _colors.bg = s.colors.bg;
        if (s.colors.text !== undefined) _colors.text = s.colors.text;
        if (s.colors.dir !== undefined) _colors.dir = s.colors.dir;
        if (s.colors.root !== undefined) _colors.root = s.colors.root;
      }
      _lineH = Math.round(_fontSize * 1.4);
      if (_enabled) { clampPanY(); requestRender(); }
    },

    requestRender: function() { if (_enabled) requestRender(); },

    getState: function() {
      return {
        fontSize: _fontSize, fontFamily: _fontFamily,
        animDuration: _animDuration, animStagger: _animStagger, animEnabled: _animEnabled,
        colors: { bg: _colors.bg, text: _colors.text, dir: _colors.dir, root: _colors.root }
      };
    }
  };
})(window);
