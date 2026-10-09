/* EE# — الصدفة (Shell) للمتصفح وWebView: ee-shell.js
 * تربط أوامر $صدفة_* في EE# بصفحة HTML وجافاسكربت.
 * يعمل مع الكود المُترجَم (ee-runtime.js) ومع محرك C++ المبني بـ WebAssembly.
 *
 *   EEShell.install(EE)                       تركيب الصدفة على بيئة التشغيل
 *   EEShell.create({ dispatch, extra })       إنشاء صدفة مستقلة: { call(cmd, args) }
 *   extra: { "أمر.مخصص": function (args) { ... return "نص"; } }  أوامر إضافية من JS
 *
 * بروتوكول الأوامر (كلها نصوص): dom.text dom.value dom.html dom.attr dom.style dom.class
 *   dom.append dom.on timer.after timer.every timer.clear ui.alert ui.toast ui.share
 *   console.log store.set store.get js.eval fs.read fs.write
 * وفي أندرويد يوفّر التطبيق الكائن window.EEAndroid فتعمل ui.toast وui.share وfs.* محلياً.
 */
(function (root) {
  'use strict';

  function create(opts) {
    opts = opts || {};
    var dispatch = opts.dispatch || function () {};
    var extra = opts.extra || {};
    var timers = {};
    var timerSeq = 0;
    var android = typeof root.EEAndroid !== 'undefined' ? root.EEAndroid : null;

    function el(sel) {
      var e = root.document.querySelector(sel);
      if (!e) throw new Error('العنصر غير موجود: ' + sel);
      return e;
    }
    function storeKey(k) { return 'ee:' + k; }
    function toast(text) {
      if (android && android.toast) { android.toast(text); return; }
      var t = root.document.createElement('div');
      t.textContent = text;
      t.setAttribute('role', 'status');
      t.style.cssText = 'position:fixed;inset-inline:16px;bottom:24px;margin:auto;max-width:420px;padding:12px 16px;' +
        'background:#222;color:#fff;border-radius:10px;font:16px/1.5 system-ui,sans-serif;text-align:center;z-index:99999';
      root.document.body.appendChild(t);
      setTimeout(function () { t.remove(); }, 2500);
    }

    var cmds = {
      'dom.text': function (a) { var e = el(a[0]); if (a.length > 1) e.textContent = a[1]; return e.textContent; },
      'dom.value': function (a) { var e = el(a[0]); if (a.length > 1) e.value = a[1]; return e.value; },
      'dom.html': function (a) { var e = el(a[0]); if (a.length > 1) e.innerHTML = a[1]; return e.innerHTML; },
      'dom.attr': function (a) {
        var e = el(a[0]);
        if (a.length > 2) { e.setAttribute(a[1], a[2]); return a[2]; }
        var v = e.getAttribute(a[1]);
        return v === null ? '' : v;
      },
      'dom.style': function (a) { el(a[0]).style.setProperty(a[1], a[2]); return a[2]; },
      'dom.class': function (a) { el(a[0]).classList.toggle(a[1], a[2] === 'true'); return ''; },
      'dom.append': function (a) { el(a[0]).insertAdjacentHTML('beforeend', a[1]); return ''; },
      'dom.on': function (a) {
        var sel = a[0], ev = a[1], id = parseInt(a[2], 10);
        var capture = ev === 'focus' || ev === 'blur';  // لا تفقاعة لها
        root.document.addEventListener(ev, function (e) {
          var t = e.target && e.target.closest ? e.target.closest(sel) : null;
          if (!t) return;
          if (ev === 'submit') e.preventDefault();
          var val = t.value !== undefined && t.value !== null ? String(t.value) : (t.textContent || '');
          dispatch(id, [val, e.key || '']);
        }, capture);
        return '';
      },
      'timer.after': function (a) {
        var id = parseInt(a[1], 10), tid = String(++timerSeq);
        timers[tid] = setTimeout(function () { delete timers[tid]; dispatch(id, []); }, Number(a[0]));
        return tid;
      },
      'timer.every': function (a) {
        var id = parseInt(a[1], 10), tid = String(++timerSeq);
        timers[tid] = setInterval(function () { dispatch(id, []); }, Number(a[0]));
        return tid;
      },
      'timer.clear': function (a) { clearTimeout(timers[a[0]]); clearInterval(timers[a[0]]); delete timers[a[0]]; return ''; },
      'ui.alert': function (a) { root.alert(a[0]); return ''; },
      'ui.toast': function (a) { toast(a[0]); return ''; },
      'ui.share': function (a) {
        if (android && android.share) android.share(a[0]);
        else if (root.navigator && root.navigator.share) root.navigator.share({ text: a[0] });
        else throw new Error('المشاركة غير مدعومة هنا');
        return '';
      },
      'console.log': function (a) { root.console.log(a[0]); return ''; },
      'store.set': function (a) { root.localStorage.setItem(storeKey(a[0]), a[1]); return ''; },
      'store.get': function (a) { var v = root.localStorage.getItem(storeKey(a[0])); return v === null ? '' : v; },
      'js.eval': function (a) { var r = (0, eval)(a[0]); return r === undefined || r === null ? '' : String(r); },
      'fs.read': function (a) {
        if (android && android.readFile) return android.readFile(a[0]);
        throw new Error('قراءة الملفات غير متاحة في الويب (متاحة في أندرويد وسطح المكتب)');
      },
      'fs.write': function (a) {
        if (android && android.writeFile) { android.writeFile(a[0], a[1]); return ''; }
        throw new Error('كتابة الملفات غير متاحة في الويب (متاحة في أندرويد وسطح المكتب)');
      }
    };

    return {
      call: function (cmd, args) {
        if (Object.prototype.hasOwnProperty.call(extra, cmd)) return extra[cmd](args);
        if (Object.prototype.hasOwnProperty.call(cmds, cmd)) return cmds[cmd](args);
        throw new Error('أمر صدفة غير معروف: ' + cmd);
      }
    };
  }

  function install(EE, opts) {
    opts = opts || {};
    var o = { dispatch: function (id, args) { EE.dispatch(id, args); }, extra: opts.extra };
    EE.use(create(o));
    return EE;
  }

  root.EEShell = { create: create, install: install };
  if (typeof module !== 'undefined' && module.exports) module.exports = root.EEShell;
})(typeof globalThis !== 'undefined' ? globalThis : (typeof window !== 'undefined' ? window : this));
