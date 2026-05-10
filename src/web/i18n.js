// ===== TreeVisual i18n Module =====

(function(global) {
  'use strict';

  // ===== Language Dictionaries =====
  var dicts = {
    zh: {
      title: 'TreeVisual',
      version: '版本 v{v}',
      pathPlaceholder: '输入目录路径...',
      scan: '扫描',
      up: '上级',
      home: '主页',
      refresh: '刷新',
      settings: '设置',
      showHidden: '显示隐藏文件',
      loading: '加载中...',
      loadingInit: '输入路径并点击"扫描"以可视化目录树。',
      errorRequest: '请求失败: {msg}',
      errorPath: '无效路径: {msg}',
      statusLoading: '加载中...',
      statusError: '错误',
      statusReady: '就绪',
      webglToggle: '启用 WebGL 渲染',
      webglWarning: '[Beta] WebGL 渲染为实验性功能，可能存在性能问题或兼容性问题。点击关闭此提示。',
      langSwitch: '语言',
      pressEnterToContinue: '按 Enter 继续'
    },
    en: {
      title: 'TreeVisual',
      version: 'Version v{v}',
      pathPlaceholder: 'Enter directory path...',
      scan: 'Scan',
      up: 'Up',
      home: 'Home',
      refresh: 'Refresh',
      settings: 'Settings',
      showHidden: 'Show hidden files',
      loading: 'Loading...',
      loadingInit: 'Enter a path and click Scan to visualize a directory tree.',
      errorRequest: 'Request failed: {msg}',
      errorPath: 'Invalid path: {msg}',
      statusLoading: 'Loading...',
      statusError: 'Error',
      statusReady: 'Ready',
      webglToggle: 'Enable WebGL Rendering',
      webglWarning: '[Beta] WebGL rendering is experimental. It may have performance or compatibility issues. Click to dismiss.',
      langSwitch: 'Language',
      pressEnterToContinue: 'Press Enter to continue'
    }
  };

  var currentLang = detectLanguage();

  // ===== Public API =====
  global.t = t;
  global.setLang = setLang;
  global.getLang = function() { return currentLang; };
  global.applyI18n = applyI18n;

  // ===== Translation Function =====
  /**
   * Translate a key with optional parameter interpolation.
   * @param {string} key - The translation key
   * @param {Object} params - Optional key-value pairs for interpolation
   * @returns {string} Translated string
   */
  function t(key, params) {
    var dict = dicts[currentLang] || dicts.en;
    var str = dict[key] || dicts.en[key] || key;

    if (params) {
      for (var k in params) {
        if (Object.prototype.hasOwnProperty.call(params, k)) {
          str = str.replace('{' + k + '}', params[k]);
        }
      }
    }

    return str;
  }

  // ===== Language Switching =====
  function setLang(lang) {
    if (!dicts[lang]) return;
    currentLang = lang;
    try {
      localStorage.setItem('treevisual_lang', lang);
    } catch (e) {}
    
    document.documentElement.lang = lang;
    applyI18n();
    
    // Update select
    var sel = document.getElementById('langSelect');
    if (sel) sel.value = lang;
  }

  // ===== Auto-detect Language =====
  function detectLanguage() {
    // Priority: URL param > localStorage > browser language > default (en)
    
    // 1. URL parameter
    var urlParams = new URLSearchParams(window.location.search);
    var urlLang = urlParams.get('lang');
    if (urlLang && dicts[urlLang]) return urlLang;

    // 2. localStorage
    try {
      var saved = localStorage.getItem('treevisual_lang');
      if (saved && dicts[saved]) return saved;
    } catch (e) {}

    // 3. Browser language
    var browserLang = navigator.language || navigator.userLanguage || '';
    if (browserLang.indexOf('zh') === 0) return 'zh';

    // 4. Default
    return 'en';
  }

  // ===== Apply translations to DOM =====
  function applyI18n() {
    // Update elements with data-i18n attribute
    var elements = document.querySelectorAll('[data-i18n]');
    for (var i = 0; i < elements.length; i++) {
      var el = elements[i];
      var key = el.getAttribute('data-i18n');
      if (key) {
        el.textContent = t(key);
      }
    }

    // Update placeholder attributes
    var placeholderEls = document.querySelectorAll('[data-i18n-placeholder]');
    for (var j = 0; j < placeholderEls.length; j++) {
      var pel = placeholderEls[j];
      var pKey = pel.getAttribute('data-i18n-placeholder');
      if (pKey) {
        pel.placeholder = t(pKey);
      }
    }

    // Set html lang attribute
    document.documentElement.lang = currentLang;

    // Sync language selector
    var sel = document.getElementById('langSelect');
    if (sel) sel.value = currentLang;
  }

})(window);
