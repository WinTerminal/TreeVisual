// ===== TreeVisual Web App - Core Logic =====

var currentPath = "";
var container = document.getElementById("treeText");
var pathInput = document.getElementById("pathInput");
var statusEl = document.getElementById("status");

// ===== Autocomplete =====
var suggestionsContainer = document.createElement("div");
suggestionsContainer.id = "pathSuggestions";
pathInput.parentNode.appendChild(suggestionsContainer);
var _acTimer = null;
var _sugIdx = -1;

function ensureTrailingSep(p) {
  if (!p) return "/";
  var c = p[p.length - 1];
  return (c === "/" || c === "\\") ? p : p + "/";
}

function getParentAndPartial(input) {
  var sepIdx = -1;
  for (var i = input.length - 1; i >= 0; i--) {
    if (input[i] === "/" || input[i] === "\\") { sepIdx = i; break; }
  }
  if (sepIdx === -1) {
    return { parent: ensureTrailingSep(currentPath || "/"), partial: input };
  }
  var parent = input.substring(0, sepIdx + 1);
  var partial = input.substring(sepIdx + 1);
  if (parent === "") parent = "/";
  return { parent: parent, partial: partial };
}

function hideSuggestions() {
  suggestionsContainer.style.display = "none";
  suggestionsContainer.innerHTML = "";
  _sugIdx = -1;
}

function updateSuggestionHighlight(items) {
  for (var i = 0; i < items.length; i++)
    items[i].classList.toggle("highlighted", i === _sugIdx);
}

function fetchSuggestions(input) {
  var pp = getParentAndPartial(input, currentPath);
  if (!pp.parent || !pp.parent.trim()) { hideSuggestions(); return; }
  fetch("/api/list?path=" + encodeURIComponent(pp.parent))
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.error || !Array.isArray(data)) { hideSuggestions(); return; }
      var partialLower = pp.partial.toLowerCase();
      var filtered = [];
      for (var i = 0; i < data.length; i++) {
        if (data[i].name.toLowerCase().indexOf(partialLower) === 0)
          filtered.push(data[i]);
      }
      if (filtered.length === 0) { hideSuggestions(); return; }
      filtered.sort(function(a, b) {
        if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
        return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
      });
      if (filtered.length > 20) filtered = filtered.slice(0, 20);
      suggestionsContainer.innerHTML = "";
      for (var i = 0; i < filtered.length; i++) {
        var item = document.createElement("div");
        item.className = "suggestion-item";
        var nameSpan = document.createElement("span");
        nameSpan.className = "suggestion-name";
        nameSpan.textContent = filtered[i].name;
        item.appendChild(nameSpan);
        if (filtered[i].isDir) {
          var label = document.createElement("span");
          label.className = "suggestion-dir-label";
          label.textContent = "/";
          item.appendChild(label);
        }
        var fullPath = pp.parent + filtered[i].name;
        item.dataset.fullPath = fullPath;
        item.addEventListener("click", function(e) {
          e.stopPropagation();
          pathInput.value = this.dataset.fullPath;
          hideSuggestions();
          scanDirectory();
        });
        suggestionsContainer.appendChild(item);
      }
      suggestionsContainer.style.display = "block";
    })
    .catch(function() { hideSuggestions(); });
}

pathInput.addEventListener("input", function() {
  clearTimeout(_acTimer);
  var val = this.value.trim();
  if (!val) { hideSuggestions(); return; }
  _acTimer = setTimeout(function() { fetchSuggestions(val); }, 200);
});

pathInput.addEventListener("keydown", function(e) {
  var vis = suggestionsContainer.style.display !== "none";
  var items = suggestionsContainer.querySelectorAll(".suggestion-item");
  if (vis && items.length > 0) {
    if (e.key === "ArrowDown") {
      e.preventDefault();
      _sugIdx = Math.min(_sugIdx + 1, items.length - 1);
      updateSuggestionHighlight(items);
      return;
    }
    if (e.key === "ArrowUp") {
      e.preventDefault();
      _sugIdx = Math.max(_sugIdx - 1, -1);
      updateSuggestionHighlight(items);
      return;
    }
    if (e.key === "Enter" && _sugIdx >= 0) {
      e.preventDefault();
      pathInput.value = items[_sugIdx].dataset.fullPath;
      hideSuggestions();
      scanDirectory();
      return;
    }
    if (e.key === "Escape") {
      hideSuggestions();
      return;
    }
  }
  _sugIdx = -1;
  if (e.key === "Enter") scanDirectory();
});

document.addEventListener("click", function(e) {
  if (!e.target.closest(".autocomplete-wrapper")) hideSuggestions();
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
  fetch("/api/home")
    .then(function(r) { return r.json(); })
    .then(function(data) {
      pathInput.value = data.path || "/";
      scanDirectory();
    })
    .catch(function() {
      pathInput.value = "/";
      scanDirectory();
    });
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
  container.classList.remove('init-msg');
  container.innerHTML = '<div class="loading">' + t('loading') + '</div>';
  statusEl.textContent = t('statusLoading');

  // Check if JS Mode is enabled
  if (window.isJSMode && window.isJSMode()) {
    scanWithJSMode(path);
    return;
  }

  fetch(buildApiUrl(path))
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (data.error) {
        container.innerHTML = '<div class="error">' + esc(t('errorPath', { msg: data.error })) + '</div>';
        container.classList.remove('loading');
        statusEl.textContent = t('statusError');
        return;
      }
      renderTree(data);
      window._lastTreeData = data;

      // Feed data to HW-accelerated renderer if active
      if (window.HWRenderer && window._hwEnabled) {
        window.HWRenderer.loadTreeData(data);
      }

      statusEl.textContent = path;
    })
    .catch(function(e) {
      container.classList.remove('loading');
      var host = window.location.hostname;
      if (host === 'winterminal.github.io' || host.endsWith('.github.io')) {
        document.body.classList.add('preview-mode');
        // Try to find path in demo tree
        var demoNode = findDemoNode(path);
        if (demoNode && demoNode.type === 'directory') {
          renderTree(demoNode);
          window._lastTreeData = demoNode;
          if (window.HWRenderer && window._hwEnabled) {
            window.HWRenderer.loadTreeData(demoNode);
          }
          statusEl.textContent = path + ' (Preview)';
        } else {
          container.innerHTML = '<div class="error">Path not found in preview. <a href="javascript:loadDemoTree()">Reload demo tree</a></div>';
          statusEl.textContent = t('statusError');
        }
      } else {
        container.innerHTML = '<div class="error">' + esc(t('errorRequest', { msg: e.message })) + '</div>';
        statusEl.textContent = t('statusError');
      }
    });
}

// ===== JS Mode Directory Scanning =====
var _restrictedPaths = ['/', '/boot', '/dev', '/proc', '/sys', '/etc', '/usr', '/bin', '/sbin', '/lib', '/root', '/run', '/var', '/tmp'];

function isRestrictedPath(path) {
  var normalizedPath = path.replace(/\/+$/, '');
  if (normalizedPath === '') normalizedPath = '/';
  
  for (var i = 0; i < _restrictedPaths.length; i++) {
    if (normalizedPath === _restrictedPaths[i] || normalizedPath === _restrictedPaths[i]) {
      return true;
    }
    if (_restrictedPaths[i] !== '/' && normalizedPath.startsWith(_restrictedPaths[i] + '/')) {
      return true;
    }
  }
  
  if (/^\/$/.test(normalizedPath) || /^[a-zA-Z]:\\?$/.test(normalizedPath)) {
    return true;
  }
  
  return false;
}

function showRestrictedPathWarning(path) {
  var downloadUrl = 'https://github.com/WinTerminal/TreeVisual/releases/latest';
  
  container.innerHTML = '<div class="js-mode-restricted">' +
    '<div class="restricted-icon">🔒</div>' +
    '<h3 data-i18n="restrictedTitle">Restricted Path</h3>' +
    '<p class="restricted-msg">' + esc(t('restrictedMsg', { path: path })) + '</p>' +
    '<p class="restricted-desc" data-i18n="restrictedDesc">System directories cannot be accessed in JS Mode due to browser security restrictions.</p>' +
    '<div class="restricted-actions">' +
      '<a href="' + downloadUrl + '" target="_blank" rel="noopener noreferrer" class="btn-download">' +
        '⬇️ <span data-i18n="downloadFull">Download Full Version</span>' +
      '</a>' +
      '<button onclick="forceScanJSMode()" class="btn-anyways">' +
        '⚡ <span data-i18n="anyways">Anyways</span>' +
      '</button>' +
    '</div>' +
  '</div>';
  container.classList.remove('loading');
}

function forceScanJSMode() {
  container.innerHTML = '<div class="loading">' + t('loading') + '</div>';
  statusEl.textContent = t('statusLoading');
  
  window._forceJSMode = true;
  scanWithJSMode(currentPath, true);
}

function scanWithJSMode(path, force) {
  if (!window.scanDirectoryJS) {
    container.innerHTML = '<div class="error">JS Mode not available</div>';
    statusEl.textContent = 'Error';
    return;
  }

  if (!force && isRestrictedPath(path)) {
    showRestrictedPathWarning(path);
    statusEl.textContent = path + ' (Restricted)';
    return;
  }

  window.scanDirectoryJS(path)
    .then(function(data) {
      if (!data) {
        container.innerHTML = '<div class="error">Failed to access directory</div>';
        container.classList.remove('loading');
        statusEl.textContent = 'Error';
        return;
      }

      renderTree(data);
      window._lastTreeData = data;

      // Feed data to HW-accelerated renderer if active
      if (window.HWRenderer && window._hwEnabled) {
        window.HWRenderer.loadTreeData(data);
      }

      statusEl.textContent = data.path + ' (JS Mode)';
    })
    .catch(function(e) {
      container.classList.remove('loading');
      
      if (e.name === 'NotFoundError' || e.message.includes('Not found') || e.message.includes('access')) {
        showRestrictedPathWarning(path);
        statusEl.textContent = path + ' (Access Denied)';
      } else {
        container.innerHTML = '<div class="error">' + esc(t('errorRequest', { msg: e.message })) + '</div>';
        statusEl.textContent = 'Error';
      }
    });
}

// ===== Demo Tree for GitHub Pages Preview =====
var _demoTree = null;

function findDemoNode(path) {
  if (!_demoTree) return null;

  var parts = path.replace(/^\/+|\/+$/g, '').split('/');
  var node = _demoTree;

  if (path === '/' || path === '/DemoProject') return _demoTree;

  // Skip root name check
  for (var i = 0; i < parts.length; i++) {
    if (!node || !node.children) return null;
    if (parts[i] === node.name) continue;
    var found = false;
    for (var j = 0; j < node.children.length; j++) {
      if (node.children[j].name === parts[i]) {
        node = node.children[j];
        found = true;
        break;
      }
    }
    if (!found) return null;
  }
  return node;
}

function loadDemoTree() {
  console.log('[loadDemoTree] Starting...');
  _demoTree = {
    name: 'DemoProject',
    path: '/DemoProject',
    type: 'directory',
    hasChildren: true,
    children: [
      {
        name: 'src',
        path: '/DemoProject/src',
        type: 'directory',
        hasChildren: true,
        children: [
          { name: 'main.cpp', path: '/DemoProject/src/main.cpp', type: 'file', hasChildren: false },
          { name: 'utils.cpp', path: '/DemoProject/src/utils.cpp', type: 'file', hasChildren: false },
          { name: 'utils.h', path: '/DemoProject/src/utils.h', type: 'file', hasChildren: false },
          {
            name: 'components',
            path: '/DemoProject/src/components',
            type: 'directory',
            hasChildren: true,
            children: [
              { name: 'App.tsx', path: '/DemoProject/src/components/App.tsx', type: 'file', hasChildren: false },
              { name: 'Header.tsx', path: '/DemoProject/src/components/Header.tsx', type: 'file', hasChildren: false }
            ]
          }
        ]
      },
      {
        name: 'include',
        path: '/DemoProject/include',
        type: 'directory',
        hasChildren: true,
        children: [
          { name: 'api.h', path: '/DemoProject/include/api.h', type: 'file', hasChildren: false },
          { name: 'config.h', path: '/DemoProject/include/config.h', type: 'file', hasChildren: false }
        ]
      },
      {
        name: 'tests',
        path: '/DemoProject/tests',
        type: 'directory',
        hasChildren: true,
        children: [
          { name: 'test_main.cpp', path: '/DemoProject/tests/test_main.cpp', type: 'file', hasChildren: false },
          { name: 'test_utils.cpp', path: '/DemoProject/tests/test_utils.cpp', type: 'file', hasChildren: false }
        ]
      },
      {
        name: 'docs',
        path: '/DemoProject/docs',
        type: 'directory',
        hasChildren: true,
        children: [
          { name: 'README.md', path: '/DemoProject/docs/README.md', type: 'file', hasChildren: false },
          { name: 'architecture.md', path: '/DemoProject/docs/architecture.md', type: 'file', hasChildren: false }
        ]
      },
      {
        name: 'scripts',
        path: '/DemoProject/scripts',
        type: 'directory',
        hasChildren: true,
        children: [
          { name: 'build.sh', path: '/DemoProject/scripts/build.sh', type: 'file', hasChildren: false },
          { name: 'deploy.sh', path: '/DemoProject/scripts/deploy.sh', type: 'file', hasChildren: false }
        ]
      },
      { name: 'Makefile', path: '/DemoProject/Makefile', type: 'file', hasChildren: false },
      { name: '.gitignore', path: '/DemoProject/.gitignore', type: 'file', hasChildren: false },
      { name: 'README.md', path: '/DemoProject/README.md', type: 'file', hasChildren: false }
    ]
  };

  pathInput.value = '/DemoProject';
  currentPath = '/DemoProject';
  renderTree(_demoTree);
  window._lastTreeData = _demoTree;
  if (window.HWRenderer && window._hwEnabled) {
    window.HWRenderer.loadTreeData(_demoTree);
  }
  statusEl.textContent = '/DemoProject (Preview)';
  console.log('[loadDemoTree] Complete');
}

// ===== Tree Rendering =====
function renderTree(data) {
  console.log('[renderTree] Rendering tree:', data ? data.name : 'null');
  console.log('[renderTree] Container before:', { html: container.innerHTML.substring(0,50), classes: container.className, display: container.style.display });
  container.innerHTML = "";
  container.classList.remove('loading', 'init-msg');
  var rootLine = createNodeEl(data, "", true, true);
  container.appendChild(rootLine);

  if (data.children) {
    for (var i = 0; i < data.children.length; i++) {
      var isLast = (i == data.children.length - 1);
      var childLine = createNodeEl(data.children[i], "", isLast, false);
      container.appendChild(childLine);
    }
  }
  console.log('[renderTree] Container after:', { html: container.innerHTML.substring(0,100), childCount: container.children.length, classes: container.className });
}

function createNodeEl(node, prefix, isLast, isRoot) {
  var line = document.createElement("div");
  line.className = "tree-line";
  line.dataset.path = node.path || node.name || "";
  line.dataset.type = node.type || "file";
  line.dataset.hasChildren = node.hasChildren ? "true" : "false";
  line.style.whiteSpace = "pre";

    if (isRoot) {
    var rootName = node.name === "/" ? "Root" : node.name;
    line.textContent = rootName +
      (rootName[rootName.length - 1] !== "/" ? "/" : "");
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
        arrow.textContent = "";
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

// ===== Helper: Find node in in-memory tree data =====
function findNodeInTree(path) {
  var treeData = window._lastTreeData || _demoTree;
  if (!treeData || !path) return null;
  
  if (treeData.path === path) return treeData;
  
  function search(node) {
    if (!node || !node.children) return null;
    for (var i = 0; i < node.children.length; i++) {
      if (node.children[i].path === path) return node.children[i];
      var found = search(node.children[i]);
      if (found) return found;
    }
    return null;
  }
  return search(treeData);
}

// ===== Helper: Render children nodes (Smooth Expand) =====
function renderChildren(lineEl, arrowEl, children) {
  var container2 = document.createElement("div");
  
  // 初始状态: 收起 (CSS已定义 max-height:0; opacity:0)
  container2.className = "children-container";

  var basePrefix = getPrefixOfLine(lineEl);
  var plen = basePrefix.length;
  var childPrefix = basePrefix;
  if (plen >= 3 && basePrefix[plen-2] === "\u2500" && basePrefix[plen-1] === " ") {
    if (basePrefix[plen-3] === "\u251c")
      childPrefix = basePrefix.substring(0, plen - 3) + "\u2502   ";
    else if (basePrefix[plen-3] === "\u2514")
      childPrefix = basePrefix.substring(0, plen - 3) + "    ";
  }
  
  // Batch DOM operations
  var fragment = document.createDocumentFragment();
  for (var i = 0; i < children.length; i++) {
    var isLast = (i == children.length - 1);
    var childLine = createNodeEl(children[i], childPrefix, isLast, false);
    fragment.appendChild(childLine);
  }
  container2.appendChild(fragment);

  // 插入DOM (此时容器高度为0，不影响布局)
  lineEl.parentNode.insertBefore(container2, lineEl.nextSibling);

  // 下一帧触发展开动画 (让浏览器先渲染初始状态)
  requestAnimationFrame(function() {
    // 动态添加GPU加速属性 (仅动画期间)
    container2.style.willChange = 'max-height, opacity';
    container2.style.transform = 'translateZ(0)';
    container2.style.backfaceVisibility = 'hidden';
    
    // 为子元素也添加GPU加速
    var childLines = container2.querySelectorAll('.tree-line');
    for (var ci = 0; ci < childLines.length; ci++) {
      childLines[ci].style.willChange = 'opacity, transform';
      childLines[ci].style.transform = 'translateZ(0)';
      childLines[ci].style.backfaceVisibility = 'hidden';
    }
    
    // 先设置max-height为实际内容高度 (scrollHeight)
    container2.style.maxHeight = container2.scrollHeight + 'px';
    
    // 然后添加 expanded 类，触发 opacity 过渡
    container2.classList.add('expanded');
    
    // 动画结束后清理GPU加速属性 (释放内存!)
    setTimeout(function() {
      // 移除 max-height 限制，让内容自然流动
      container2.style.maxHeight = '';
      
      // 清理容器GPU加速
      container2.style.willChange = 'auto';
      container2.style.transform = '';
      container2.style.backfaceVisibility = '';
      
      // 清理子元素GPU加速
      for (var cj = 0; cj < childLines.length; cj++) {
        childLines[cj].style.willChange = 'auto';
        childLines[cj].style.transform = '';
        childLines[cj].style.backfaceVisibility = '';
      }
    }, 350);  // 略大于 CSS transition 时间 (320ms + buffer)
  });
}

// ===== Lazy Loading: Toggle Directory (Smooth Animation) =====
function toggleDir(arrowEl) {
  var lineEl = arrowEl.parentNode;

  if (arrowEl._expanded) {
    // 收起动画: 平滑过渡到 max-height: 0
    var next = lineEl.nextSibling;
    if (next && (next.className === "children-container" || next.classList.contains("children-container"))) {
      // 动态添加GPU加速属性 (仅动画期间)
      next.style.willChange = 'max-height, opacity';
      next.style.transform = 'translateZ(0)';
      next.style.backfaceVisibility = 'hidden';
      
      // 为子元素也添加GPU加速
      var collapseChildLines = next.querySelectorAll('.tree-line');
      for (var cci = 0; cci < collapseChildLines.length; cci++) {
        collapseChildLines[cci].style.willChange = 'opacity, transform';
        collapseChildLines[cci].style.transform = 'translateZ(0)';
        collapseChildLines[cci].style.backfaceVisibility = 'hidden';
      }
      
      // 移除 expanded 类，添加 collapsed 类
      next.classList.remove('expanded');
      next.classList.add('collapsed');
      
      // 动画结束后移除容器 (DOM移除自动释放GPU内存)
      setTimeout(function() {
        if (next.parentNode) {
          next.remove();
        }
      }, 330);  // 匹配 CSS transition 时间 (320ms + buffer)
    }
    arrowEl.textContent = "\u25b6";
    arrowEl._expanded = false;
  } else {
    // Expand
    arrowEl.textContent = "\u25bc";
    arrowEl._expanded = true;
    var dirPath = arrowEl._nodePath;
    if (!dirPath) return;

    // First try to find children from in-memory tree data (for Preview/Demo mode)
    var memChildren = findNodeInTree(dirPath);
    if (memChildren && memChildren.children && memChildren.children.length > 0) {
      renderChildren(lineEl, arrowEl, memChildren.children);
      return;
    }
    
    if (window.isJSMode && window.isJSMode() && window.expandDirectoryJS) {
      // Use File System API for expansion
      window.expandDirectoryJS(dirPath)
        .then(function(data) {
          if (!data || !data.children || data.children.length === 0) {
            arrowEl.textContent = "  ";
            arrowEl._expanded = false;
            return;
          }
          renderChildren(lineEl, arrowEl, data.children);
        })
        .catch(function(e) {
          console.error("Expand failed in JS Mode:", e);
          arrowEl.textContent = "\u25b6";
          arrowEl._expanded = false;
        });
      
      return;
    }

    // Fallback to API call
    fetch(buildApiUrl(dirPath))
      .then(function(res) { return res.json(); })
      .then(function(data) {
        if (!data.children || data.children.length === 0) {
          arrowEl.textContent = "  ";
          arrowEl._expanded = false;
          return;
        }
        renderChildren(lineEl, arrowEl, data.children);
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

// ===== Settings Persistence =====
var _settingsSaveTimer = null;

function saveSettings() {
  var themeEl = document.getElementById("themeSelect");
  var theme = themeEl ? themeEl.value : 'mocha';
  var settings = {
    showHidden: document.getElementById("showHidden").checked,
    enableHW: document.getElementById("enableHW") ? document.getElementById("enableHW").checked : false,
    language: typeof getLang === "function" ? getLang() : "en",
    theme: theme,
    mode: (document.getElementById("modeSelect") || {}).value || 'dark',
    fontSize: parseInt((document.getElementById("fontSize") || {}).value, 10) || 14,
    fontFamily: (document.getElementById("fontFamily") || {}).value || '"Cascadia Code","Fira Code","JetBrains Mono",monospace',
    animDuration: parseInt((document.getElementById("animSpeed") || {}).value, 10) || 280,
    animEnabled: document.getElementById("animEnabled") ? document.getElementById("animEnabled").checked : true,
    enableJSMode: document.getElementById("enableJSMode") ? document.getElementById("enableJSMode").checked : false
  };
  if (theme === 'custom') {
    settings.colors = {
      bg: (document.getElementById("colorBg") || {}).value || '#1e1e2e',
      text: (document.getElementById("colorText") || {}).value || '#cdd6f4',
      dir: (document.getElementById("colorDir") || {}).value || '#89b4fa',
      root: (document.getElementById("colorRoot") || {}).value || '#cba6f7'
    };
  }
  clearTimeout(_settingsSaveTimer);
  _settingsSaveTimer = setTimeout(function() {
    fetch("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(settings)
    }).catch(function(e) { console.warn("[Settings] Save failed:", e); });
  }, 300);
  try { localStorage.setItem('treevisual_settings', JSON.stringify(settings)); } catch(e) {}
  applyHWSettings(settings);
}

function toggleCustomColors(theme) {
  var cc = document.getElementById('customColors');
  if (cc) cc.classList.toggle('show', theme === 'custom');
}

function applyHWSettings(s) {
  console.log('[applyHWSettings]', JSON.stringify(s));
  if (window.HWRenderer && window.HWRenderer.applySettings) {
    window.HWRenderer.applySettings(s);
  }
  applyThemeCSS(s);
}

var _cssThemeVars = {
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

var _cssThemeVarsLight = {
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

function getThemeStyleSheet() {
  var el = document.getElementById('theme-style');
  if (!el) {
    el = document.createElement('style');
    el.id = 'theme-style';
    document.head.appendChild(el);
  }
  return el;
}

function applyThemeCSS(s) {
  var vars = {};
  if (s.theme !== 'custom' && s.theme !== undefined) {
    var pool = (s.mode === 'light') ? _cssThemeVarsLight : _cssThemeVars;
    vars = pool[s.theme] || {};
  } else if (s.colors) {
    vars = {
      '--bg-dark': s.colors.bg, '--bg-panel': s.colors.bg,
      '--bg-hover': s.colors.bg, '--border': s.colors.bg,
      '--text-primary': s.colors.text, '--text-muted': s.colors.text,
      '--accent-blue': s.colors.dir, '--accent-purple': s.colors.root,
      '--accent-green': s.colors.text, '--accent-red': s.colors.text, '--accent-yellow': s.colors.text
    };
  }
  // Apply via <style> tag for maximum cascade priority
  if (s.fontFamily) vars['--font-mono'] = s.fontFamily;
  var css = ':root {\n';
  Object.keys(vars).forEach(function(k) { css += '  ' + k + ': ' + vars[k] + ';\n'; });
  css += '}';
  getThemeStyleSheet().textContent = css;
  var bg = vars['--bg-dark'] || '';
  var text = vars['--text-primary'] || '';
  console.log('[applyThemeCSS] theme=' + s.theme + ' bg=' + bg + ' text=' + text);
}

function loadSettings() {
  // In preview mode, skip API call and use localStorage directly
  if (typeof isPreviewMode === 'function' && isPreviewMode()) {
    console.log('[Settings] Preview mode: using localStorage');
    try {
      var local = localStorage.getItem('treevisual_settings');
      if (local) applyLoadedSettings(JSON.parse(local));
    } catch(e) {}
    return;
  }

  var controller = new AbortController();
  var timeout = setTimeout(function() { controller.abort(); }, 1500);  // Reduced from 3000ms
  
  fetch("/api/settings", { signal: controller.signal })
    .then(function(r) { 
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json(); 
    })
    .then(function(s) {
      clearTimeout(timeout);
      if (!s || !s.showHidden && !s.enableHW && !s.language && !s.fontSize && !s.fontFamily && !s.mode) return;
      applyLoadedSettings(s);
    })
    .catch(function(e) {
      clearTimeout(timeout);
      console.log('[Settings] API failed, using localStorage:', e.message);
      try {
        var local = localStorage.getItem('treevisual_settings');
        if (local) applyLoadedSettings(JSON.parse(local));
      } catch(err) {}
    });
}

function applyLoadedSettings(s) {
  if (!s) return;
  if (s.showHidden !== undefined)
    document.getElementById("showHidden").checked = s.showHidden;
  if (s.enableHW !== undefined) {
    var cb = document.getElementById("enableHW");
    if (cb) cb.checked = s.enableHW;
  }
  if (s.language !== undefined && s.language && typeof setLang === "function")
    setLang(s.language);
  if (s.fontSize !== undefined) {
    var el = document.getElementById("fontSize");
    if (el) el.value = s.fontSize;
  }
  if (s.fontFamily !== undefined) {
    var el = document.getElementById("fontFamily");
    if (el) el.value = s.fontFamily;
  }
  if (s.theme !== undefined) {
    var el = document.getElementById("themeSelect");
    if (el) { el.value = s.theme; toggleCustomColors(s.theme); }
  }
  if (s.mode !== undefined) {
    var el = document.getElementById("modeSelect");
    if (el) el.value = s.mode;
  }
  if (s.colors) {
    var c = s.colors;
    if (c.bg) document.getElementById("colorBg").value = c.bg;
    if (c.text) document.getElementById("colorText").value = c.text;
    if (c.dir) document.getElementById("colorDir").value = c.dir;
    if (c.root) document.getElementById("colorRoot").value = c.root;
  }
  if (s.animDuration !== undefined) {
    var el = document.getElementById("animSpeed");
    if (el) el.value = s.animDuration;
  }
  if (s.animEnabled !== undefined) {
    var el = document.getElementById("animEnabled");
    if (el) el.checked = s.animEnabled;
  }
  if (s.enableJSMode !== undefined) {
    var jsCb = document.getElementById("enableJSMode");
    if (jsCb) jsCb.checked = s.enableJSMode;
  }
  applyHWSettings(s);
  restoreHWState();
  console.log('[applyLoadedSettings] After restoreHWState, container:', { display: container.style.display, visible: container.offsetParent !== null, html: container.innerHTML.substring(0,50) });
  if (typeof toggleJSMode === 'function') toggleJSMode();
  console.log('[applyLoadedSettings] After toggleJSMode, container:', { display: container.style.display, visible: container.offsetParent !== null, html: container.innerHTML.substring(0,50) });
}

function restoreHWState() {
  var cb = document.getElementById("enableHW");
  var hwContainer = document.getElementById("hwContainer");
  if (!cb || !hwContainer) return;
  // Toggle HW-only settings visibility
  var hwOnly = document.querySelectorAll('.hw-only');
  for (var i = 0; i < hwOnly.length; i++) {
    hwOnly[i].classList.toggle('show', cb.checked);
  }
  if (cb.checked) {
    if (window.HWRenderer && window.HWRenderer.init()) {
      hwContainer.classList.add('active');
      container.style.display = 'none';
      window.HWRenderer.enable(navigateTo);
    } else {
      cb.checked = false;
    }
  } else {
    hwContainer.classList.remove('active');
    container.style.display = '';
    if (window.HWRenderer) window.HWRenderer.disable();
  }
}

// ===== Environment Detection =====
function isPreviewMode() {
  var host = window.location.hostname;
  return host === 'winterminal.github.io' || 
         host.endsWith('.github.io') || 
         host === 'localhost' || 
         host.startsWith('127.') ||
         host.includes('vercel.app') ||
         host.includes('netlify.app');
}

// ===== Service Status UI (Optimized) =====
var _serviceRunning = false;
var _serviceCheckFailCount = 0;
var _maxServiceCheckFails = 3;

function checkServiceStatus() {
  if (isPreviewMode()) return;

  var controller = new AbortController();
  var timeout = setTimeout(function() { controller.abort(); }, 2000);
  
  fetch("/api/service/status", { signal: controller.signal })
    .then(function(r) { 
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json(); 
    })
    .then(function(data) {
      clearTimeout(timeout);
      _serviceCheckFailCount = 0;
      _serviceRunning = data.running;
      var label = document.getElementById("serviceStatusLabel");
      var btn = document.getElementById("serviceToggleBtn");
      if (!label || !btn) return;
      if (data.running) {
        label.textContent = "Running";
        label.classList.add("running");
        btn.textContent = "Stop";
        btn.classList.add("running");
      } else {
        label.textContent = "Off";
        label.classList.remove("running");
        btn.textContent = "Start";
        btn.classList.remove("running");
      }
    })
    .catch(function(e) {
      clearTimeout(timeout);
      _serviceRunning = false;
      _serviceCheckFailCount++;
      
      var label = document.getElementById("serviceStatusLabel");
      var btn = document.getElementById("serviceToggleBtn");
      if (label) { label.textContent = "Off"; label.classList.remove("running"); }
      if (btn) { btn.textContent = "Start"; btn.classList.remove("running"); }
      
      if (_serviceCheckFailCount >= _maxServiceCheckFails) {
        console.warn('[ServiceStatus] Stopping after', _maxServiceCheckFails, 'consecutive failures');
        var intervals = setInterval(function(){}, 0); // Get interval ID
        for (var i = 1; i <= intervals; i++) clearInterval(i);
      }
    });
}

function toggleService() {
  var action = _serviceRunning ? "stop" : "start";
  var controller = new AbortController();
  var timeout = setTimeout(function() { controller.abort(); }, 3000);
  
  fetch("/api/service/" + action, { method: "POST", signal: controller.signal })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      clearTimeout(timeout);
      var retries = 0;
      function poll() {
        checkServiceStatus();
        if (!_serviceRunning && action === 'start' && ++retries < 10) {
          setTimeout(poll, 1000);
        }
      }
      setTimeout(poll, 500);
    })
    .catch(function() { 
      clearTimeout(timeout);
      setTimeout(checkServiceStatus, 1000); 
    });
}

// Smart polling: only in non-preview mode
if (typeof isPreviewMode === 'function' && !isPreviewMode()) {
  setInterval(checkServiceStatus, 10000);
} else {
  console.log('[ServiceStatus] Polling disabled in preview mode');
}

// ===== Initialize i18n on page load =====
if (typeof applyI18n === 'function') {
  applyI18n();

  // Load saved settings first, then apply i18n (which may override language)
  loadSettings();

  // === GitHub Pages: auto-load demo tree when no backend ===
  (function checkPreview() {
    var host = window.location.hostname;
    var urlParams = new URLSearchParams(window.location.search);
    console.log('[Init] checkPreview:', { host, isPreview: urlParams.get('preview'), isGithub: host.endsWith('.github.io') });
    if (urlParams.get('preview') === '1' || host === 'winterminal.github.io' || host.endsWith('.github.io')) {
      document.body.classList.add('preview-mode');
      console.log('[Init] Preview mode detected, will load demo tree after settings...');
      
      // Wait for settings to load, then render demo tree
      var attempts = 0;
      var maxAttempts = 20;
      function tryLoadDemo() {
        attempts++;
        var container = document.getElementById("treeText");
        if (container && (container.innerHTML === '' || container.classList.contains('init-msg') || container.innerHTML.indexOf('loadingInit') >= 0)) {
          if (attempts < maxAttempts) {
            setTimeout(tryLoadDemo, 100);
            return;
          }
        }
        console.log('[Init] Loading demo tree (attempt', attempts, ')');
        loadDemoTree();
      }
      setTimeout(tryLoadDemo, 500);
    } else {
      console.log('[Init] Not preview mode, skipping demo tree');
    }
  })();

  // Language switch handler
  document.getElementById('langSelect').addEventListener('change', function() {
    setLang(this.value);
    saveSettings();
  });

  // Bind saveSettings to checkbox changes
  var showHiddenCb = document.getElementById("showHidden");
  if (showHiddenCb) showHiddenCb.addEventListener("change", saveSettings);

  var hwCb = document.getElementById("enableHW");
  if (hwCb) hwCb.addEventListener("change", function() {
    saveSettings();
    restoreHWState();
  });

  // Bind HW-only settings change
  ['fontSize', 'fontFamily', 'animSpeed', 'animEnabled'].forEach(function(id) {
    var el = document.getElementById(id);
    if (el) el.addEventListener('change', saveSettings);
  });

  // Theme selector
  var themeSel = document.getElementById('themeSelect');
  if (themeSel) themeSel.addEventListener('change', function() {
    toggleCustomColors(this.value);
    saveSettings();
  });

  // Mode selector
  var modeSel = document.getElementById('modeSelect');
  if (modeSel) modeSel.addEventListener('change', saveSettings);

  // Custom color pickers
  ['colorBg', 'colorText', 'colorDir', 'colorRoot'].forEach(function(id) {
    var el = document.getElementById(id);
    if (el) el.addEventListener('input', saveSettings);
  });

  // ===== JS Mode (File System Access API) =====
  var _jsModeEnabled = false;
  var _directoryHandle = null;
  var _handleMap = {}; // Store path -> handle mapping for expansion

  function isJSMode() {
    return _jsModeEnabled && 'showDirectoryPicker' in window;
  }

  async function scanDirectoryJS(path) {
    if (!_directoryHandle) {
      console.warn('[scanDirectoryJS] No directory selected. Please click "Pick Dir" button first.');
      return null;
    }

    var rootData = await buildTreeFromHandle(_directoryHandle, '/' + _directoryHandle.name);
    return rootData;
  }

  async function pickDirectoryJS() {
    try {
      _directoryHandle = await window.showDirectoryPicker();
      currentPath = '/' + _directoryHandle.name;
      pathInput.value = currentPath;
      
      var rootData = await buildTreeFromHandle(_directoryHandle, currentPath);
      if (rootData) {
        renderTree(rootData);
        window._lastTreeData = rootData;
        statusEl.textContent = currentPath + ' (JS Mode)';
        
        if (window.HWRenderer && window._hwEnabled) {
          window.HWRenderer.loadTreeData(rootData);
        }
      }
    } catch (e) {
      if (e.name !== 'AbortError') {
        console.error('Directory picker failed:', e);
      }
    }
  }

  async function buildTreeFromHandle(dirHandle, path) {
    var children = [];
    
    try {
      for await (const entry of dirHandle.values()) {
        var childPath = path + '/' + entry.name;
        
        if (entry.kind === 'file') {
          children.push({
            name: entry.name,
            path: childPath,
            type: 'file',
            hasChildren: false
          });
        } else if (entry.kind === 'directory') {
          // Store the subdirectory handle in our map
          _handleMap[childPath] = entry;
          
          children.push({
            name: entry.name,
            path: childPath,
            type: 'directory',
            hasChildren: true
          });
        }
      }
      
      // Sort: directories first, then files, alphabetically
      children.sort(function(a, b) {
        if (a.type !== b.type) return a.type === 'directory' ? -1 : 1;
        return a.name.localeCompare(b.name);
      });

      return {
        name: dirHandle.name,
        path: path,
        type: 'directory',
        hasChildren: children.length > 0,
        children: children
      };
    } catch (e) {
      console.error('Error reading directory:', e);
      return {
        name: dirHandle.name,
        path: path,
        type: 'directory',
        hasChildren: false,
        children: []
      };
    }
  }

  async function expandDirectoryJS(dirPath) {
    var dirHandle = _handleMap[dirPath];
    
    if (!dirHandle) {
      // Try to navigate from root handle
      if (!_directoryHandle) return null;
      
      // Navigate to the requested path
      var parts = dirPath.replace(/^\/+|\/+$/g, '').split('/');
      var currentHandle = _directoryHandle;
      
      try {
        for (var i = 0; i < parts.length; i++) {
          currentHandle = await currentHandle.getDirectoryHandle(parts[i]);
        }
        
        // Cache this handle
        _handleMap[dirPath] = currentHandle;
        dirHandle = currentHandle;
      } catch (e) {
        console.error('Failed to navigate to path:', e);
        return null;
      }
    }

    if (!dirHandle) return null;

    return buildTreeFromHandle(dirHandle, dirPath);
  }

  function toggleJSMode() {
    var cb = document.getElementById('enableJSMode');
    var warning = document.getElementById('jsModeWarning');
    var statusEl = document.getElementById('jsModeStatus');
    var pickBtn = document.getElementById('jsPickDirBtn');
    _jsModeEnabled = cb ? cb.checked : false;
    
    if (warning) {
      warning.style.display = _jsModeEnabled ? 'block' : 'none';
    }
    
    if (statusEl) {
      statusEl.style.display = _jsModeEnabled ? 'inline-flex' : 'none';
      statusEl.textContent = _jsModeEnabled ? (window.getLang && window.getLang() === 'zh' ? ' JS模式' : ' JS Mode') : '';
    }

    // Show/hide Pick Dir button
    if (pickBtn) {
      pickBtn.style.display = _jsModeEnabled ? '' : 'none';
    }

    // Reset directory handle when disabling
    if (!_jsModeEnabled) {
      _directoryHandle = null;
    }

    saveSettings();
  }

  // Bind JS Mode checkbox
  var jsModeCb = document.getElementById('enableJSMode');
  if (jsModeCb) {
    jsModeCb.addEventListener('change', toggleJSMode);
    
    // Check browser support
    if (!('showDirectoryPicker' in window)) {
      jsModeCb.disabled = true;
      jsModeCb.title = t('browserNotSupported');
      var warning = document.getElementById('jsModeWarning');
      if (warning) {
        warning.innerHTML = '<span>❌ <span>' + t('browserNotSupported') + '</span></span>';
        warning.style.display = 'block';
      }
    }
  }

  // Expose JS mode functions globally
  window.isJSMode = isJSMode;
  window.scanDirectoryJS = scanDirectoryJS;
  window.pickDirectoryJS = pickDirectoryJS;
  window.buildTreeFromHandle = buildTreeFromHandle;
  window.expandDirectoryJS = expandDirectoryJS;
}
