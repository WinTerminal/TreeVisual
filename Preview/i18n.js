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
      pickDir: '选择目录',
      up: '上级',
      home: '主页',
      refresh: '刷新',
      settings: '设置',
    showHidden: '显示隐藏文件',
      serviceMode: '服务模式',
      hwAccel: '硬件加速',
      fontSize: '字体大小',
      fontFamily: '字体',
      theme: '主题',
      mode: '深浅模式',
      dark: '深色',
      light: '浅色',
      custom: '自定义',
      anim: '动画',
      animEnabled: '动画',
      animSpeed: '动画速度',
      fast: '快',
      normal: '中',
      smooth: '流畅',
      slow: '慢',
      langSwitch: '语言',
      pressEnterToContinue: '按 Enter 继续',
      jsMode: 'JS模式',
      jsModeWarning: '实验性功能 - 可能存在问题。使用浏览器文件系统 API。',
      restrictedTitle: '受限路径',
      restrictedMsg: '路径 "{path}" 无法在 JS 模式下访问',
      restrictedDesc: '由于浏览器安全限制，JS 模式无法访问系统目录。',
      downloadFull: '下载完整版',
      anyways: '仍然尝试',
      browserNotSupported: '此浏览器不支持。请使用 Chrome/Edge 86+ 版本。'
    },
    en: {
      title: 'TreeVisual',
      version: 'Version v{v}',
      pathPlaceholder: 'Enter directory path...',
      scan: 'Scan',
      pickDir: 'Pick Dir',
      up: 'Up',
      home: 'Home',
      refresh: 'Refresh',
    settings: 'Settings',
      showHidden: 'Show hidden files',
      serviceMode: 'Service Mode',
      hwAccel: 'Hardware Acceleration',
      fontSize: 'Font Size',
      fontFamily: 'Font',
      theme: 'Theme',
      mode: 'Mode',
      dark: 'Dark',
      light: 'Light',
      custom: 'Custom',
      anim: 'Animation',
      animEnabled: 'Animation',
      animSpeed: 'Anim Speed',
      fast: 'Fast',
      normal: 'Normal',
      smooth: 'Smooth',
      slow: 'Slow',
      langSwitch: 'Language',
      pressEnterToContinue: 'Press Enter to continue',
      jsMode: 'JS Mode',
      jsModeWarning: 'Experimental feature - may have issues. Uses browser File System API.',
      restrictedTitle: 'Restricted Path',
      restrictedMsg: 'Path "{path}" cannot be accessed in JS Mode',
      restrictedDesc: 'System directories cannot be accessed in JS Mode due to browser security restrictions.',
      downloadFull: 'Download Full Version',
      anyways: 'Anyways',
      browserNotSupported: 'Not supported in this browser. Please use Chrome/Edge 86+.'
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
