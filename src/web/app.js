// ===== TreeVisual Web App - Core Logic =====

var currentPath = "";
var container = document.getElementById("treeContainer");
var pathInput = document.getElementById("pathInput");
var statusEl = document.getElementById("status");

pathInput.addEventListener("keydown", function(e) {
  if (e.key === "Enter") scanDirectory();
});

// ===== Utility Functions =====
function esc(s) {
  var d = document.createElement("div");
  d.textContent = s;
  return d.innerHTML;
}

function navigateTo(path) {
  pathInput.value = path;
  scanDirectory();
}

function goUp() {
  var p = currentPath.replace(/[/\\\\][^/\\\\]*$/, "") || "/";
  pathInput.value = p;
  scanDirectory();
}

function goHome() {
  pathInput.value = "/home/yyh";
  scanDirectory();
}

function refresh() {
  scanDirectory();
}

function toggleSettings() {
  var panel = document.getElementById("settingsPanel");
  panel.classList.toggle("open");
}

function buildApiUrl(path) {
  var url = "/api/tree?path=" + encodeURIComponent(path);
  if (document.getElementById("showHidden").checked) {
    url += "&show_hidden=true";
  }
  return url;
}

// Close settings on outside click
document.addEventListener("click", function(e) {
  var panel = document.getElementById("settingsPanel");
  var btn = e.target.closest(".settings-btn");
  if (!btn && panel.classList.contains("open")) {
    panel.classList.remove("open");
  }
});

// ===== Directory Scanning =====
function scanDirectory() {
  var path = pathInput.value.trim();
  if (!path) return;

  currentPath = path;
  container.innerHTML = '<div class="loading">' + t('loading') + '</div>';
  statusEl.textContent = t('statusLoading');

  fetch(buildApiUrl(path))
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (data.error) {
        container.innerHTML = '<div class="error">' + esc(t('errorPath', { msg: data.error })) + '</div>';
        statusEl.textContent = t('statusError');
        return;
      }
      renderTree(data);

      // Notify WebGL module if active
      if (window.WebGLRenderer && window._webglEnabled) {
        window.WebGLRenderer.loadTreeData(data);
      }

      statusEl.textContent = path;
    })
    .catch(function(e) {
      container.innerHTML = '<div class="error">' + esc(t('errorRequest', { msg: e.message })) + '</div>';
      statusEl.textContent = t('statusError');
    });
}

// ===== Tree Rendering =====
function renderTree(data) {
  container.innerHTML = "";
  var rootLine = createNodeEl(data, "", true, true);
  container.appendChild(rootLine);

  if (data.children) {
    for (var i = 0; i < data.children.length; i++) {
      var isLast = (i == data.children.length - 1);
      var prefix = isLast ? "    " : "\u2502   ";
      var childLine = createNodeEl(data.children[i], prefix, isLast, false);
      container.appendChild(childLine);
    }
  }
}

function createNodeEl(node, prefix, isLast, isRoot) {
  var line = document.createElement("div");
  line.className = "tree-line";
  line.dataset.path = node.path || node.name || "";
  line.dataset.type = node.type || "file";
  line.dataset.hasChildren = node.hasChildren ? "true" : "false";
  line.style.whiteSpace = "pre";

  if (isRoot) {
    line.textContent = (isLast ? "\u2514\u2500 " : "\u251c\u2500 ") + node.name +
      (node.name[node.name.length - 1] !== "/" ? "/" : "");
    if (node.type === "directory" && node.hasChildren === true) {
      line.style.color = "#7aa2f7";
      line.style.fontWeight = "bold";
    }
    if (node.type === "directory") {
      line.style.cursor = "pointer";
      line.onclick = function() { navigateTo(node.path || node.name); };
    }
  } else {
    var connector = isLast ? "\u2514\u2500 " : "\u251c\u2500 ";
    if (node.type === "directory") {
      var arrow = document.createElement("span");
      arrow.className = "tree-arrow";
      if (node.hasChildren === true) {
        arrow.textContent = "\u25b6";
        arrow.style.cursor = "pointer";
      } else {
        arrow.textContent = "  ";
      }
      arrow.style.marginRight = "2px";
      arrow._expanded = false;
      arrow._nodePath = node.path || "";
      arrow.onclick = node.hasChildren ? function() { toggleDir(this); } : null;

      line.appendChild(document.createTextNode(prefix + connector));
      line.appendChild(arrow);

      var nameSpan = document.createElement("span");
      nameSpan.className = "dir-link";
      nameSpan.textContent = node.name + (node.name[node.name.length - 1] !== "/" ? "/" : "");
      nameSpan.onclick = function() { navigateTo(node.path || node.name); };
      line.appendChild(nameSpan);
    } else {
      line.textContent = prefix + connector + node.name;
    }
  }
  return line;
}

// ===== Lazy Loading: Toggle Directory =====
function toggleDir(arrowEl) {
  var lineEl = arrowEl.parentNode;

  if (arrowEl._expanded) {
    // Collapse
    var next = lineEl.nextSibling;
    if (next && next.className === "children-container") {
      next.remove();
    }
    arrowEl.textContent = "\u25b6";
    arrowEl._expanded = false;
  } else {
    // Expand
    arrowEl.textContent = "\u25bc";
    arrowEl._expanded = true;
    var dirPath = arrowEl._nodePath;
    if (!dirPath) return;

    fetch(buildApiUrl(dirPath))
      .then(function(res) { return res.json(); })
      .then(function(data) {
        if (!data.children || data.children.length === 0) {
          arrowEl.textContent = "  ";
          arrowEl._expanded = false;
          return;
        }

        var container2 = document.createElement("div");
        container2.className = "children-container";

        // Calculate prefix for children
        var basePrefix = getPrefixOfLine(lineEl);
        for (var i = 0; i < data.children.length; i++) {
          var isLast = (i == data.children.length - 1);
          var childPrefix = basePrefix + (isLast ? "    " : "\u2502   ");
          var childLine = createNodeEl(data.children[i], childPrefix, isLast, false);
          container2.appendChild(childLine);
        }

        lineEl.parentNode.insertBefore(container2, lineEl.nextSibling);
      })
      .catch(function(e) {
        console.error("Expand failed:", e);
        arrowEl.textContent = "\u25b6";
        arrowEl._expanded = false;
      });
  }
}

function getPrefixOfLine(lineEl) {
  var text = lineEl.textContent || "";
  var prefix = "";
  for (var i = 0; i < text.length; i++) {
    var c = text[i];
    if (c === " " || c === "\u2502" || c === "\u251c" || c === "\u2514" || c === "\u2500") {
      prefix += c;
    } else {
      break;
    }
  }
  return prefix;
}

// ===== Initialize i18n on page load =====
if (typeof applyI18n === 'function') {
  applyI18n();

  // Language switch handler
  document.getElementById('langSelect').addEventListener('change', function() {
    setLang(this.value);
  });
}

// ===== WebGL Mode Toggle =====
var webglCheckbox = document.getElementById('enableWebGL');
if (webglCheckbox) {
  webglCheckbox.addEventListener('change', function() {
    if (this.checked) {
      if (!window.WebGLRenderer || !window.WebGLRenderer.init()) {
        this.checked = false;
        alert(t ? t('errorRequest', { msg: 'WebGL not supported in your browser' }) : 'WebGL not supported');
        return;
      }
      if (window.WebGLRenderer.enable(navigateTo)) {
        container.style.display = 'none';
      } else {
        this.checked = false;
      }
    } else {
      if (window.WebGLRenderer) window.WebGLRenderer.disable();
      container.style.display = '';
    }
  });
}
