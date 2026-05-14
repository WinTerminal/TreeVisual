// ===== TreeVisual WebGL Module (Beta) =====
// Force-directed graph visualization for directory trees

(function(global) {
  'use strict';

  // ===== State =====
  var _enabled = false;
  var _gl = null;
  var _canvas = null;
  var _container = null;
  var _tooltipEl = null;
  var _animId = null;
  var _nodes = [];
  var _edges = [];
  var _rootData = null;
  var _panX = 0, _panY = 0;
  var _zoom = 1;
  var _dragging = false;
  var _lastMouseX = 0, _lastMouseY = 0;
  var _hoveredNode = null;
  var _navigateCallback = null;

  console.warn('[WebGL] Experimental feature enabled. Performance may vary.');

  // ===== Vec2 Utility =====
  function Vec2(x, y) { this.x = x || 0; this.y = y || 0; }
  Vec2.prototype.add = function(v) { return new Vec2(this.x + v.x, this.y + v.y); };
  Vec2.prototype.sub = function(v) { return new Vec2(this.x - v.x, this.y - v.y); };
  Vec2.prototype.mul = function(s) { return new Vec2(this.x * s, this.y * s); };
  Vec2.prototype.length = function() { return Math.sqrt(this.x * this.x + this.y * this.y); };
  Vec2.prototype.normalize = function() { var l = this.length(); return l > 0 ? this.mul(1 / l) : new Vec2(0, 0); };

  // ===== Shaders =====
  var VERT_SHADER_SRC =
    'attribute vec2 a_pos;' +
    'attribute vec3 a_color;' +
    'attribute float a_size;' +
    'uniform vec2 u_resolution;' +
    'uniform vec2 u_pan;' +
    'uniform float u_zoom;' +
    'varying vec3 v_color;' +
    'void main() {' +
      'vec2 pos = (a_pos + u_pan) * u_zoom;' +
      'vec2 clipPos = (pos / u_resolution) * 2.0 - 1.0;' +
      'gl_Position = vec4(clipPos * vec2(1, -1), 0, 1);' +
      'gl_PointSize = a_size * u_zoom;' +
      'v_color = a_color;' +
    '}';

  var FRAG_SHADER_SRC =
    'precision mediump float;' +
    'varying vec3 v_color;' +
    'void main() {' +
      'vec2 coord = gl_PointCoord - vec2(0.5);' +
      'float dist = length(coord);' +
      'if (dist > 0.5) discard;' +
      'float alpha = smoothstep(0.5, 0.35, dist);' +
      'gl_FragColor = vec4(v_color, alpha);' +
    '}';

  var LINE_VERT_SHADER_SRC =
    'attribute vec2 a_pos;' +
    'attribute vec3 a_color;' +
    'uniform vec2 u_resolution;' +
    'uniform vec2 u_pan;' +
    'uniform float u_zoom;' +
    'varying vec3 v_color;' +
    'void main() {' +
      'vec2 pos = (a_pos + u_pan) * u_zoom;' +
      'vec2 clipPos = (pos / u_resolution) * 2.0 - 1.0;' +
      'gl_Position = vec4(clipPos * vec2(1, -1), 0, 1);' +
      'v_color = a_color;' +
    '}';

  var LINE_FRAG_SHADER_SRC =
    'precision mediump float;' +
    'varying vec3 v_color;' +
    'void main() {' +
      'gl_FragColor = vec4(v_color * 0.6, 0.8);' +
    '}';

  // ===== Shader Compilation =====
  function createShader(gl, type, src) {
    var sh = gl.createShader(type);
    gl.shaderSource(sh, src);
    gl.compileShader(sh);
    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
      console.error('[WebGL] Shader compile error:', gl.getShaderInfoLog(sh));
      gl.deleteShader(sh);
      return null;
    }
    return sh;
  }

  function createProgram(gl, vsSrc, fsSrc) {
    var vs = createShader(gl, gl.VERTEX_SHADER, vsSrc);
    var fs = createShader(gl, gl.FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return null;
    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      console.error('[WebGL] Program link error:', gl.getProgramInfoLog(prog));
      return null;
    }
    return prog;
  }

  // ===== WebGL Renderer Class =====
  function WebGLRenderer(canvas) {
    this.canvas = canvas;
    this.gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
    if (!this.gl) throw new Error('WebGL not supported');

    var gl = this.gl;

    // Programs
    this.nodeProg = createProgram(gl, VERT_SHADER_SRC, FRAG_SHADER_SRC);
    this.lineProg = createProgram(gl, LINE_VERT_SHADER_SRC, LINE_FRAG_SHADER_SRC);

    // Node program locations
    this.nodeLocs = {
      pos: gl.getAttribLocation(this.nodeProg, 'a_pos'),
      color: gl.getAttribLocation(this.nodeProg, 'a_color'),
      size: gl.getAttribLocation(this.nodeProg, 'a_size'),
      resolution: gl.getUniformLocation(this.nodeProg, 'u_resolution'),
      pan: gl.getUniformLocation(this.nodeProg, 'u_pan'),
      zoom: gl.getUniformLocation(this.nodeProg, 'u_zoom')
    };

    // Line program locations
    this.lineLocs = {
      pos: gl.getAttribLocation(this.lineProg, 'a_pos'),
      color: gl.getAttribLocation(this.lineProg, 'a_color'),
      resolution: gl.getUniformLocation(this.lineProg, 'u_resolution'),
      pan: gl.getUniformLocation(this.lineProg, 'u_pan'),
      zoom: gl.getUniformLocation(this.lineProg, 'u_zoom')
    };

    // Buffers
    this.nodeBuffer = gl.createBuffer();
    this.nodeColorBuffer = gl.createBuffer();
    this.nodeSizeBuffer = gl.createBuffer();
    this.lineBuffer = gl.createBuffer();
    this.lineColorBuffer = gl.createBuffer();

    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    return this;
  }

  WebGLRenderer.prototype.resize = function() {
    var c = this.canvas;
    c.width = c.clientWidth * window.devicePixelRatio;
    c.height = c.clientHeight * window.devicePixelRatio;
    this.viewportW = c.width;
    this.viewportH = c.height;
    this.centerX = c.width / 2;
    this.centerY = c.height / 2;
  };

  WebGLRenderer.prototype.clear = function(color) {
    var gl = this.gl;
    gl.viewport(0, 0, this.viewportW, this.viewportH);
    gl.clearColor(color[0], color[1], color[2], 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
  };

  WebGLRenderer.prototype.drawNodes = function(nodes) {
    if (!nodes.length) return;
    var gl = this.gl;

    var positions = [];
    var colors = [];
    var sizes = [];

    for (var i = 0; i < nodes.length; i++) {
      var n = nodes[i];
      positions.push(n.x, n.y);
      colors.push(n.r, n.g, n.b);
      sizes.push(n.size || 8);
    }

    gl.useProgram(this.nodeProg);
    gl.uniform2f(this.nodeLocs.resolution, this.viewportW, this.viewportH);
    gl.uniform2f(this.nodeLocs.pan, _panX, _panY);
    gl.uniform1f(this.nodeLocs.zoom, _zoom);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.nodeBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.nodeLocs.pos);
    gl.vertexAttribPointer(this.nodeLocs.pos, 2, gl.FLOAT, false, 0, 0);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.nodeColorBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.nodeLocs.color);
    gl.vertexAttribPointer(this.nodeLocs.color, 3, gl.FLOAT, false, 0, 0);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.nodeSizeBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(sizes), gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.nodeLocs.size);
    gl.vertexAttribPointer(this.nodeLocs.size, 1, gl.FLOAT, false, 0, 0);

    gl.drawArrays(gl.POINTS, 0, nodes.length);
  };

  WebGLRenderer.prototype.drawLines = function(edges) {
    if (!edges.length) return;
    var gl = this.gl;

    var positions = [];
    var colors = [];

    for (var i = 0; i < edges.length; i++) {
      var e = edges[i];
      positions.push(e.x1, e.y1, e.x2, e.y2);
      colors.push(e.r, e.g, e.b, e.r, e.g, e.b);
    }

    gl.useProgram(this.lineProg);
    gl.uniform2f(this.lineLocs.resolution, this.viewportW, this.viewportH);
    gl.uniform2f(this.lineLocs.pan, _panX, _panY);
    gl.uniform1f(this.lineLocs.zoom, _zoom);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.lineBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.lineLocs.pos);
    gl.vertexAttribPointer(this.lineLocs.pos, 2, gl.FLOAT, false, 0, 0);

    gl.bindBuffer(gl.ARRAY_BUFFER, this.lineColorBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.DYNAMIC_DRAW);
    gl.enableVertexAttribArray(this.lineLocs.color);
    gl.vertexAttribPointer(this.lineLocs.color, 3, gl.FLOAT, false, 0, 0);

    gl.drawArrays(gl.LINES, 0, edges.length * 2);
  };

  // ===== Graph Data Structure =====
  function buildGraphFromJSON(json, parentX, parentY, depth) {
    var nodes = [];
    var edges = [];

    if (!json) return { nodes: nodes, edges: edges };

    var isRoot = depth === undefined;
    depth = depth || 0;
    parentX = parentX !== undefined ? parentX : 0;
    parentY = parentY !== undefined ? parentY : 0;

    // Root node
    var rootX = isRoot ? 0 : parentX + (Math.random() - 0.5) * 150;
    var rootY = isRoot ? 0 : parentY + (Math.random() - 0.5) * 150 + 60;

    var isDir = json.type === 'directory';
    nodes.push({
      id: json.path || json.name,
      label: json.name,
      type: json.type,
      path: json.path,
      x: rootX,
      y: rootY,
      vx: 0, vy: 0,
      r: isDir ? 0.48 : 0.62,
      g: isDir ? 0.64 : 0.81,
      b: 0.97,
      size: isDir ? 12 : 7,
      hasChildren: !!json.hasChildren,
      children: json.children || []
    });

    // Children
    if (json.children) {
      var childCount = json.children.length;
      for (var i = 0; i < childCount && i < 200; i++) {
        var angle = (i / childCount) * Math.PI * 2;
        var dist = 80 + Math.random() * 40;
        var cx = rootX + Math.cos(angle) * dist;
        var cy = rootY + Math.sin(angle) * dist;
        var child = json.children[i];
        var cIsDir = child.type === 'directory';

        nodes.push({
          id: child.path || child.name,
          label: child.name,
          type: child.type,
          path: child.path,
          x: cx,
          y: cy,
          vx: 0, vy: 0,
          r: cIsDir ? 0.48 : 0.62,
          g: cIsDir ? 0.64 : 0.81,
          b: 0.97,
          size: cIsDir ? 9 : 5,
          hasChildren: !!child.hasChildren
        });

      }
    }

    return { nodes: nodes, edges: edges };
  }

  // ===== Physics Simulation =====
  function simulateStep() {
    if (!_nodes.length) return;

    var kRepel = 800;   // Repulsion constant
    var kAttract = 0.03; // Attraction constant
    var damping = 0.85;  // Velocity damping

    // Repulsion between all pairs
    for (var i = 0; i < _nodes.length; i++) {
      for (var j = i + 1; j < _nodes.length; j++) {
        var ni = _nodes[i], nj = _nodes[j];
        var dx = nj.x - ni.x;
        var dy = nj.y - ni.y;
        var d2 = dx * dx + dy * dy;
        if (d2 < 1) d2 = 1;
        var f = kRepel / d2;
        var fx = dx * f, fy = dy * f;
        ni.vx -= fx; ni.vy -= fy;
        nj.vx += fx; nj.vy += fy;
      }
    }

    // Edge attraction
    for (var e = 0; e < _edges.length; e++) {
      var edge = _edges[e];
      var n1 = findNode(edge.srcId);
      var n2 = findNode(edge.tgtId);
      if (!n1 || !n2) continue;

      var edx = n2.x - n1.x;
      var edy = n2.y - n1.y;
      n1.vx += edx * kAttract;
      n1.vy += edy * kAttract;
      n2.vx -= edx * kAttract;
      n2.vy -= edy * kAttract;
    }

    // Center gravity
    for (var m = 0; m < _nodes.length; m++) {
      var nm = _nodes[m];
      nm.vx -= nm.x * 0.005;
      nm.vy -= nm.y * 0.005;
    }

    // Update positions
    for (var p = 0; p < _nodes.length; p++) {
      var np = _nodes[p];
      np.vx *= damping;
      np.vy *= damping;
      np.x += np.vx;
      np.y += np.vy;
    }
  }

  function findNode(id) {
    for (var i = 0; i < _nodes.length; i++) {
      if (_nodes[i].id === id) return _nodes[i];
    }
    return null;
  }

  // ===== Render Loop =====
  function renderLoop() {
    if (!_enabled || !_renderer) return;

    simulateStep();
    simulateStep();

    _renderer.resize();

    // Build draw arrays
    var drawNodes = [];
    for (var i = 0; i < _nodes.length; i++) {
      var n = _nodes[i];
      drawNodes.push({
        x: n.x + _renderer.centerX,
        y: n.y + _renderer.centerY,
        r: n.r, g: n.g, b: n.b,
        size: n.size
      });
    }

    var drawEdges = [];
    for (var j = 0; j < _edges.length; j++) {
      var ed = _edges[j];
      var sN = findNode(ed.srcId);
      var tN = findNode(ed.tgtId);
      if (sN && tN) {
        drawEdges.push({
          x1: sN.x + _renderer.centerX, y1: sN.y + _renderer.centerY,
          x2: tN.x + _renderer.centerX, y2: tN.y + _renderer.centerY,
          r: ed.r, g: ed.g, b: ed.b
        });
      }
    }

    _renderer.clear([0.106, 0.107, 0.149]); // --bg-dark
    _renderer.drawLines(drawEdges);
    _renderer.drawNodes(drawNodes);

    _animId = requestAnimationFrame(renderLoop);
  }

  // ===== Mouse Interaction =====
  function handleMouseMove(e) {
    var rect = _canvas.getBoundingClientRect();
    var mx = e.clientX - rect.left;
    var my = e.clientY - rect.top;

    if (_dragging) {
      _panX += (e.clientX - _lastMouseX) / _zoom;
      _panY += (e.clientY - _lastMouseY) / _zoom;
      _lastMouseX = e.clientX;
      _lastMouseY = e.clientY;
      return;
    }

    // Hit test
    var found = null;
    var cx = (_canvas.width / window.devicePixelRatio) / 2;
    var cy = (_canvas.height / window.devicePixelRatio) / 2;
    for (var i = 0; i < _nodes.length; i++) {
      var n = _nodes[i];
      var nx = (n.x + cx + _panX) * _zoom;
      var ny = (n.y + cy + _panY) * _zoom;
      var dist = Math.sqrt((mx - nx) * (mx - nx) + (my - ny) * (my - ny));
      if (dist < (n.size || 8) * _zoom + 4) {
        found = n;
        break;
      }
    }

    if (found !== _hoveredNode) {
      _hoveredNode = found;
      if (found && _tooltipEl) {
        _tooltipEl.textContent = found.path || found.label;
        _tooltipEl.style.display = 'block';
        _tooltipEl.style.left = (mx + 10) + 'px';
        _tooltipEl.style.top = (my + 10) + 'px';
      } else if (_tooltipEl) {
        _tooltipEl.style.display = 'none';
      }
    } else if (found && _tooltipEl) {
      _tooltipEl.style.left = (mx + 10) + 'px';
      _tooltipEl.style.top = (my + 10) + 'px';
    }
  }

  function handleMouseDown(e) {
    if (_hoveredNode && _hoveredNode.hasChildren) {
      // Click on directory node -> navigate
      if (_navigateCallback) {
        _navigateCallback(_hoveredNode.path || '');
      }
      return;
    }
    _dragging = true;
    _lastMouseX = e.clientX;
    _lastMouseY = e.clientY;
  }

  function handleMouseUp() {
    _dragging = false;
  }

  function handleWheel(e) {
    e.preventDefault();
    var delta = e.deltaY > 0 ? 0.9 : 1.1;
    _zoom *= delta;
    _zoom = Math.max(0.1, Math.min(10, _zoom));
  }

  // ===== Public API =====
  global.WebGLRenderer = {
    init: function() {
      try {
        _canvas = document.getElementById('webglCanvas');
        if (!_canvas) return false;
        
        _container = document.getElementById('webglContainer');
        _tooltipEl = document.getElementById('webglTooltip');
        _warningEl = document.getElementById('webglWarning');
        
        _renderer = new WebGLRenderer(_canvas);
        
        // Event listeners
        _canvas.addEventListener('mousemove', handleMouseMove);
        _canvas.addEventListener('mousedown', handleMouseDown);
        _canvas.addEventListener('mouseup', handleMouseUp);
        _canvas.addEventListener('mouseleave', function() { _dragging = false; if(_tooltipEl)_tooltipEl.style.display='none'; _hoveredNode=null; });
        _canvas.addEventListener('wheel', handleWheel, { passive: false });

        if (_warningEl) {
          _warningEl.addEventListener('click', function() {
            _warningEl.classList.remove('show');
          });
        }

        return true;
      } catch (err) {
        console.error('[WebGL] Init failed:', err.message);
        return false;
      }
    },

    enable: function(navigateFn) {
      _navigateCallback = navigateFn || null;
      
      if (!_renderer) {
        if (!this.init()) {
          alert(window.t ? t('errorRequest', { msg: 'WebGL initialization failed' }) : 'WebGL init failed');
          return false;
        }
      }

      _enabled = true;
      global._webglEnabled = true;

      if (_container) _container.classList.add('active');
      if (_warningEl) _warningEl.classList.add('show');

      if (_rootData) this.loadTreeData(_rootData);

      renderLoop();
      return true;
    },

    disable: function() {
      _enabled = false;
      global._webglEnabled = false;

      if (_animId) cancelAnimationFrame(_animId);
      if (_container) _container.classList.remove('active');
      if (_warningEl) _warningEl.classList.remove('show');
      if (_tooltipEl) _tooltipEl.style.display = 'none';

      _nodes = [];
      _edges = [];
      _hoveredNode = null;
      _rootData = null;
    },

    loadTreeData: function(json) {
      _rootData = json;
      if (!_enabled) return;

      // Reset view
      _panX = 0;
      _panY = 0;
      _zoom = 1;

      var graph = buildGraphFromJSON(json);
      _nodes = graph.nodes;
      _edges = [];

      // Store source/target IDs for edges
      if (json.children) {
        var rootId = json.path || json.name;
        for (var i = 0; i < json.children.length && i < 200; i++) {
          var child = json.children[i];
          _edges.push({
            srcId: rootId,
            tgtId: child.path || child.name,
            r: 0.35, g: 0.40, b: 0.52
          });
        }
      }
    },

    isEnabled: function() {
      return _enabled;
    }
  };

})(window);
