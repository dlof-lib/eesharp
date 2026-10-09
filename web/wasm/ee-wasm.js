/* EE# عبر WebAssembly: يشغّل محرك C++ نفسه داخل الصفحة (بلا ترجمة مسبقة).  [تجريبي: لم يُختبر هنا]
 *   <script src="ee.js"></script><script src="ee-shell.js"></script><script src="ee-wasm.js"></script>
 *   <script>EEWasm.runPage();</script>      ← يشغّل كل <script type="text/ee"> في الصفحة
 * أو برمجياً:  const e = await EEWasm.create(); e.eval('$اعرض "مرحبا"');
 */
(function (root) {
  'use strict';
  async function create(opts) {
    opts = opts || {};
    var M = await root.EEModule();
    var s = M._ee_session_new();
    var outEl = function () { return root.document && root.document.querySelector(opts.output || '#ee-output'); };
    var api = {};

    function flush() {
      var t = M.ccall('ee_take_output', 'string', ['number'], [s]);
      if (!t) return '';
      var el = outEl();
      if (el) el.textContent += t; else root.console.log(t.replace(/\n$/, ''));
      return t;
    }
    function packArgs(args) {  // معاملات مفصولة بـ \0 في ذاكرة WASM
      var bytes = new TextEncoder().encode((args || []).map(function (a) { return String(a) + '\0'; }).join(''));
      var p = M._malloc(bytes.length + 1);
      M.HEAPU8.set(bytes, p);
      M.HEAPU8[p + bytes.length] = 0;
      return p;
    }
    var lastResult = 0;
    function dispatch(id, args) {
      var p = packArgs(args);
      var rc = M._ee_dispatch(s, id, p, (args || []).length);
      M._free(p);
      flush();
      return rc;
    }

    var shell = root.EEShell.create({ dispatch: dispatch, extra: opts.extra });
    var cb = M.addFunction(function (cmdP, argsP, nargs) {
      var cmd = M.UTF8ToString(cmdP), p = argsP, a = [];
      for (var i = 0; i < nargs; i++) {
        var x = M.UTF8ToString(p);
        a.push(x);
        p += M.lengthBytesUTF8(x) + 1;
      }
      var res;
      try { res = String(shell.call(cmd, a) || ''); } catch (e) { res = '\x01' + (e && e.message ? e.message : e); }
      if (lastResult) M._free(lastResult);
      lastResult = M.stringToNewUTF8(res);
      return lastResult;
    }, 'iiiii');
    M._ee_set_shell(s, cb, 0);
    M.ccall('ee_set_platform', null, ['number', 'string'], [s, opts.platform || 'ويب']);

    api.eval = function (src) {
      var rc = M.ccall('ee_eval', 'number', ['number', 'string'], [s, src]);
      return { code: rc, output: flush() };
    };
    api.call = function (name, args) {
      var p = packArgs(args);
      var rc = M.ccall('ee_call', 'number', ['number', 'string', 'number', 'number'], [s, name, p, (args || []).length]);
      M._free(p);
      flush();
      return rc;
    };
    api.dispatch = dispatch;
    api.setInput = function (t) { M.ccall('ee_set_input', null, ['number', 'string'], [s, t]); };
    api.isComplete = function (src) { return M.ccall('ee_is_complete', 'number', ['string'], [src]) === 1; };
    api.version = function () { return M.ccall('ee_version', 'string', [], []); };
    return api;
  }

  async function runPage(opts) {
    var e = await create(opts);
    var scripts = root.document.querySelectorAll('script[type="text/ee"]');
    for (var i = 0; i < scripts.length; i++) {
      var sc = scripts[i], src = sc.textContent;
      if (sc.src) src = await (await root.fetch(sc.src)).text();
      e.eval(src);
    }
    return e;
  }
  root.EEWasm = { create: create, runPage: runPage };
})(typeof globalThis !== 'undefined' ? globalThis : window);
