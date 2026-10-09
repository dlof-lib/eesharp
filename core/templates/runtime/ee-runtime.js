/* EE# — بيئة التشغيل لـ JavaScript (ee-runtime.js)
 * تنفّذ دلالات لغة EE# نفسها التي ينفّذها المفسّر C++، فيعمل الكود المُترجَم
 * في المتصفح أو WebView أو Node بنفس النتائج ونفس رسائل الأخطاء.
 *
 * واجهة JS ↔ EE#:
 *   EE.configure({ platform, output })   إعداد المنصة وعنصر المخرجات (محدد CSS)
 *   EE.use(shell)                        تركيب الصدفة (كائن فيه call(cmd, args))
 *   EE.invoke("اسم", [معاملات])          استدعاء مفهوم EE# من JS
 *   EE.expose("اسم", function)           جعل دالة JS متاحة في EE# كـ $اسم
 *   EE.getVar("اسم") / EE.setVar(...)    قراءة/كتابة متغير عام
 *   EE.dispatch(id, args)                (تستعمله الصدفة) إطلاق معالج حدث
 */
(function (root) {
  'use strict';

  var VERSION = '0.2.0';
  var isNode = typeof process !== 'undefined' && !!(process.versions && process.versions.node);
  var isBrowser = typeof document !== 'undefined';

  // ───────────── الأخطاء ─────────────
  function EEError(line, msg) {
    this.eeLine = line;
    this.eeMsg = msg;
    this.message = msg;
  }
  EEError.prototype = Object.create(Error.prototype);
  EEError.prototype.constructor = EEError;
  function fail(line, msg) { throw new EEError(line, msg); }

  // ───────────── الحالة العامة ─────────────
  var config = { platform: isNode ? 'طرفية' : 'ويب', output: '#ee-output', maxOutput: 0 };
  var shell = null;
  var handlers = [];
  var stopFlag = false;
  var depth = 0;
  var written = 0;
  var rootEnv = null;

  // ───────────── الدخل والخرج ─────────────
  var lineBuf = '';
  var stdinLines = null;
  var io = {
    write: function (s) {
      if (isNode) {
        try { require('fs').writeSync(1, s); } catch (e) { process.stdout.write(s); }
      } else if (isBrowser && typeof document.querySelector === 'function' && document.querySelector(config.output)) {
        document.querySelector(config.output).textContent += s;
      } else {
        lineBuf += s;
        var i;
        while ((i = lineBuf.indexOf('\n')) >= 0) {
          console.log(lineBuf.slice(0, i));
          lineBuf = lineBuf.slice(i + 1);
        }
      }
    },
    writeErr: function (s) {
      if (isNode) {
        try { require('fs').writeSync(2, s); } catch (e) { process.stderr.write(s); }
      } else if (isBrowser && typeof document.querySelector === 'function' && document.querySelector(config.output)) {
        document.querySelector(config.output).textContent += s;
      } else {
        console.error(s.replace(/^\n+|\n+$/g, ''));
      }
    },
    readLine: function (promptText) {
      if (isNode) {
        if (stdinLines === null) {
          var data = '';
          try { data = require('fs').readFileSync(0, 'utf8'); } catch (e) { data = ''; }
          stdinLines = data.length ? data.split('\n') : [];
          if (stdinLines.length && stdinLines[stdinLines.length - 1] === '') stdinLines.pop();
        }
        if (!stdinLines.length) return null;
        var l = stdinLines.shift();
        return l.replace(/\r$/, '');
      }
      if (typeof prompt === 'function') {
        var r = prompt(promptText || '');
        if (r === null) return null;
        io.write(r + '\n');
        return r;
      }
      return null;
    }
  };

  function emit(line, s) {
    if (config.maxOutput) {
      written += s.length;
      if (written > config.maxOutput) fail(line, 'تجاوز الحد الأقصى للمخرجات');
    }
    io.write(s);
  }

  // ───────────── نصوص ─────────────
  function chars(s) { return Array.from(s); }

  function normalizeKw(s) {
    return s.replace(/[آأإ]/g, 'ا').replace(/[ـً-ْ]/g, '');
  }

  function toAsciiDigits(s) {
    return s.replace(/[٠-٩]/g, function (c) { return String(c.charCodeAt(0) - 0x660); })
            .replace(/[۰-۹]/g, function (c) { return String(c.charCodeAt(0) - 0x6F0); })
            .replace(/٫/g, '.');
  }
  function toArabicDigits(s) {
    return s.replace(/[0-9]/g, function (c) { return String.fromCharCode(0x660 + (c.charCodeAt(0) - 48)); })
            .replace(/\./g, '٫');
  }

  // ───────────── الأنواع ─────────────
  function typeName(v) {
    if (v === null || v === undefined) return 'عدم';
    switch (typeof v) {
      case 'number': return 'رقم';
      case 'boolean': return 'منطقي';
      case 'string': return 'نص';
      case 'function': return 'مفهوم';
      case 'bigint': return 'عدد_كبير';
    }
    return Array.isArray(v) ? 'قائمة' : '؟';
  }

  function truthy(v) {
    if (v === null || v === undefined) return false;
    switch (typeof v) {
      case 'number': return v !== 0;
      case 'boolean': return v;
      case 'string': return v.length > 0;
      case 'bigint': return v !== 0n;
    }
    if (Array.isArray(v)) return v.length > 0;
    return true;
  }

  // تنسيق مطابق لـ %.14g في C++
  function fmtG(d, p) {
    if (d === 0) return '0';
    var e = d.toExponential(p - 1).split('e');
    var m = e[0], x = parseInt(e[1], 10);
    if (x < -4 || x >= p) {
      if (m.indexOf('.') >= 0) m = m.replace(/\.?0+$/, '');
      return m + 'e' + (x < 0 ? '-' : '+') + String(Math.abs(x)).padStart(2, '0');
    }
    var f = d.toFixed(Math.max(0, p - 1 - x));
    if (f.indexOf('.') >= 0) f = f.replace(/\.?0+$/, '');
    return f;
  }
  function fmtNum(d) {
    if (d !== d) return 'غير_رقم';
    if (d === Infinity) return 'ما_لا_نهاية';
    if (d === -Infinity) return '-ما_لا_نهاية';
    if (Number.isInteger(d) && Math.abs(d) < 1e15) return String(Math.trunc(d));
    return fmtG(d, 14);
  }

  function toStr(v, quote) {
    if (v === null || v === undefined) return 'عدم';
    switch (typeof v) {
      case 'number': return fmtNum(v);
      case 'boolean': return v ? 'صح' : 'خطأ';
      case 'string': return quote ? '"' + v + '"' : v;
      case 'bigint': return v.toString();
      case 'function':
        if (v.eeNative) return '<مفهوم مدمج $' + v.eeNative + '>';
        return '<مفهوم ' + (v.eeName ? v.eeName : 'مجهول') + '>';
    }
    if (Array.isArray(v)) return '[' + v.map(function (x) { return toStr(x, true); }).join('، ') + ']';
    return String(v);
  }

  // يحوّل رقماً صحيحاً عادياً إلى BigInt (إن أمكن) مثل numToBig في C++
  function numToBig(v) {
    if (typeof v === 'bigint') return v;
    if (typeof v === 'number' && v === Math.floor(v) && Math.abs(v) < 9e18) return BigInt(v);
    return null;
  }
  var MAX_HEX = 900000;  // ≈ مليون خانة عشرية
  function bigCheck(r, line) {
    if (r.toString(16).length > MAX_HEX) fail(line, 'النتيجة كبيرة جداً (أكثر من مليون خانة)');
    return r;
  }
  function bigOp(o, a, b, line) {
    var okT = function (v) { return typeof v === 'bigint' || typeof v === 'number'; };
    if (!okT(a) || !okT(b)) {
      if (o === '<' || o === '>' || o === '<=' || o === '>=')
        fail(line, 'لا يمكن مقارنة ' + typeName(a) + ' مع ' + typeName(b));
      fail(line, "العملية '" + o + "' تتطلب رقمين لكن وُجد " + typeName(a) + ' و' + typeName(b));
    }
    var x = numToBig(a), y = numToBig(b);
    if (o === '<' || o === '>' || o === '<=' || o === '>=') {
      var c;
      if (x !== null && y !== null) c = x < y ? -1 : (x > y ? 1 : 0);
      else { var dx = Number(a), dy = Number(b); c = dx < dy ? -1 : (dx > dy ? 1 : 0); }
      if (o === '<') return c < 0;
      if (o === '>') return c > 0;
      if (o === '<=') return c <= 0;
      return c >= 0;
    }
    if (x === null || y === null) fail(line, 'لا يمكن خلط عدد كبير مع رقم عشري أو ضخم جداً؛ حوّله أولاً بـ $كبير(...)');
    if (o === '+') return x + y;
    if (o === '-') return x - y;
    if (o === '*') return bigCheck(x * y, line);
    if (o === '/' || o === '%') {
      if (y === 0n) fail(line, 'القسمة على صفر');
      return o === '/' ? x / y : x % y;
    }
    fail(line, 'عملية غير معروفة: ' + o);
  }

  function equals(a, b) {
    if (a === undefined) a = null;
    if (b === undefined) b = null;
    if (typeof a === 'bigint' || typeof b === 'bigint') {
      var x = numToBig(a), y = numToBig(b);
      return x !== null && y !== null && x === y;
    }
    if (typeName(a) !== typeName(b)) return false;
    if (a === null) return true;
    if (Array.isArray(a)) {
      if (a === b) return true;
      if (a.length !== b.length) return false;
      for (var i = 0; i < a.length; i++) if (!equals(a[i], b[i])) return false;
      return true;
    }
    return a === b;
  }

  function op(o, a, b, line) {
    if (o === '==') return equals(a, b);
    if (o === '!=') return !equals(a, b);
    var ta = typeof a, tb = typeof b;
    if ((ta === 'bigint' || tb === 'bigint') && ta !== 'string' && tb !== 'string') return bigOp(o, a, b, line);
    if (o === '+') {
      if (ta === 'number' && tb === 'number') return a + b;
      if (ta === 'string' || tb === 'string') return toStr(a) + toStr(b);
      if (Array.isArray(a) && Array.isArray(b)) return a.concat(b);
      fail(line, 'لا يمكن جمع النوعين: ' + typeName(a) + ' و' + typeName(b));
    }
    if (o === '*' && ((ta === 'string' && tb === 'number') || (ta === 'number' && tb === 'string'))) {
      var sv = ta === 'string' ? a : b;
      var cnt = ta === 'number' ? a : b;
      if (cnt < 0 || cnt !== Math.floor(cnt)) fail(line, 'عدد التكرار يجب أن يكون عدداً صحيحاً غير سالب');
      if (cnt * Buffer_len(sv) > 50e6) fail(line, 'النص الناتج كبير جداً');
      return sv.repeat(cnt);
    }
    if (o === '-' || o === '*' || o === '/' || o === '%') {
      if (ta !== 'number' || tb !== 'number')
        fail(line, "العملية '" + o + "' تتطلب رقمين لكن وُجد " + typeName(a) + ' و' + typeName(b));
      if (o === '-') return a - b;
      if (o === '*') return a * b;
      if (b === 0) fail(line, 'القسمة على صفر');
      if (o === '/') return a / b;
      return a % b;
    }
    if (o === '<' || o === '>' || o === '<=' || o === '>=') {
      var c;
      if (ta === 'number' && tb === 'number') c = a < b ? -1 : (a > b ? 1 : 0);
      else if (ta === 'string' && tb === 'string') c = a < b ? -1 : (a > b ? 1 : 0);
      else fail(line, 'لا يمكن مقارنة ' + typeName(a) + ' مع ' + typeName(b));
      if (o === '<') return c < 0;
      if (o === '>') return c > 0;
      if (o === '<=') return c <= 0;
      return c >= 0;
    }
    fail(line, 'عملية غير معروفة: ' + o);
  }
  function Buffer_len(s) { return unescape(encodeURIComponent(s)).length; }

  function neg(v, line) {
    if (typeof v === 'bigint') return -v;
    if (typeof v !== 'number') fail(line, 'لا يمكن عكس إشارة قيمة من نوع ' + typeName(v));
    return -v;
  }

  function toIndex(v, size, line) {
    if (typeof v !== 'number') fail(line, 'الفهرس يجب أن يكون رقماً');
    if (v !== Math.floor(v)) fail(line, 'الفهرس يجب أن يكون عدداً صحيحاً');
    var i = v;
    if (i < 0) i += size;
    if (i < 0 || i >= size) fail(line, 'الفهرس خارج النطاق (الطول = ' + size + ')');
    return i;
  }

  function index(obj, ix, line) {
    if (Array.isArray(obj)) return obj[toIndex(ix, obj.length, line)];
    if (typeof obj === 'string') {
      var ch = chars(obj);
      return ch[toIndex(ix, ch.length, line)];
    }
    fail(line, 'لا يمكن استخدام الفهرس [] مع النوع ' + typeName(obj));
  }

  // ───────────── النطاقات ─────────────
  function Env(parent) { this.vars = new Map(); this.parent = parent || null; }
  Env.prototype.has = function (n) { return this.vars.has(n); };
  Env.prototype.find = function (n) {
    for (var e = this; e; e = e.parent) if (e.vars.has(n)) return e;
    return null;
  };
  function child(S) { return new Env(S); }

  var nativeAlias = new Map();

  function def(S, name, v, line) {
    if (S.vars.has(name))
      fail(line, (name.charAt(0) === '$' ? 'المفهوم ' + name : 'المتغير <<' + name + '>>') + ' معرّف مسبقاً في هذا النطاق');
    S.vars.set(name, v === undefined ? null : v);
    return v;
  }
  function loopVar(S, name, v) { S.vars.set(name, v); }
  function get(S, name, line) {
    var e = S.find(name);
    if (!e) fail(line, 'المتغير <<' + name + '>> غير معرّف');
    return e.vars.get(name);
  }
  function fnref(S, name, line) {
    var e = S.find('$' + name);
    if (e) return e.vars.get('$' + name);
    var al = nativeAlias.get(normalizeKw(name));
    if (al !== undefined && rootEnv.vars.has(al)) return rootEnv.vars.get(al);
    fail(line, 'المفهوم $' + name + ' غير معرّف');
  }
  function arith(o, cur, rhs, line) { return op(o.charAt(0), cur, rhs, line); }
  function assign(S, name, o, rhs, line) {
    var e = S.find(name);
    if (!e) fail(line, 'المتغير <<' + name + '>> غير معرّف (عرّفه أولاً بـ: متغير <<' + name + '>> = ...)');
    if (o !== '=') rhs = arith(o, e.vars.get(name), rhs, line);
    e.vars.set(name, rhs);
    return rhs;
  }
  function setIndex(rhs, obj, ix, o, line) {
    if (!Array.isArray(obj)) fail(line, 'الإسناد بالفهرس مدعوم للقوائم فقط');
    var i = toIndex(ix, obj.length, line);
    if (o !== '=') rhs = arith(o, obj[i], rhs, line);
    obj[i] = rhs;
    return rhs;
  }
  function iter(it, line) {
    if (Array.isArray(it)) return it.slice();
    if (typeof it === 'string') return chars(it);
    fail(line, 'لا يمكن التكرار على النوع ' + typeName(it));
  }
  function tick(line) { if (stopFlag) fail(line, 'تم إيقاف البرنامج'); }

  // ───────────── المفاهيم ─────────────
  function lambda(S, name, params, body) {
    var f = function (args, line) {
      if (args.length !== params.length)
        fail(line, 'المفهوم ' + (name ? name : 'المجهول') + ' يتوقع ' + params.length + ' معاملات لكن أُعطيت ' + args.length);
      if (depth >= 1000) fail(line, 'تجاوز الحد الأقصى لعمق الاستدعاءات (تكرار لا نهائي؟)');
      var env = new Env(S);
      for (var i = 0; i < params.length; i++) env.vars.set(params[i], args[i]);
      depth++;
      try {
        var r = body(env);
        return r === undefined ? null : r;
      } finally { depth--; }
    };
    f.eeName = name;
    f.eeParams = params;
    return f;
  }
  function call(f, args, line) {
    if (stopFlag) fail(line, 'تم إيقاف البرنامج');
    if (typeof f !== 'function') fail(line, 'هذه القيمة ليست مفهوماً قابلاً للاستدعاء (النوع: ' + typeName(f) + ')');
    var r = f(args, line);
    return r === undefined ? null : r;
  }

  // ───────────── المفاهيم المدمجة ─────────────
  function argc(name, a, lo, hi, ln) {
    if (a.length < lo || a.length > hi) {
      var exp = lo === hi ? String(lo) : lo + ' إلى ' + hi;
      fail(ln, 'المفهوم $' + name + ' يتوقع ' + exp + ' معاملات لكن أُعطيت ' + a.length);
    }
  }
  function needNum(name, v, ln) {
    if (typeof v === 'bigint') return Number(v);
    if (typeof v !== 'number') fail(ln, 'المفهوم $' + name + ' يتطلب رقماً لكن وُجد ' + typeName(v));
    return v;
  }
  function parseNumber(t) {
    t = toAsciiDigits(t);
    if (!/^\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?\s*$/.test(t)) return null;
    return parseFloat(t);
  }

  function reg(name, fn) {
    var f = function (a, ln) { return fn(a, ln); };
    f.eeNative = name;
    rootEnv.vars.set('$' + name, f);
    nativeAlias.set(normalizeKw(name), '$' + name);
  }
  function alias(newName, oldName) {
    rootEnv.vars.set('$' + newName, rootEnv.vars.get('$' + oldName));
    nativeAlias.set(normalizeKw(newName), '$' + newName);
  }

  function shellCall(cmd, a, ln) {
    if (!shell) fail(ln, 'لا توجد صدفة مضيفة في هذه البيئة (المنصة: ' + config.platform + ') — الأمر: ' + cmd);
    var sargs = a.map(function (v) {
      if (typeof v === 'function') fail(ln, 'لا يمكن تمرير مفهوم إلى الصدفة مباشرة؛ استعمل $صدفة_اربط أو $صدفة_انتظر');
      return toStr(v);
    });
    try {
      var r = shell.call(cmd, sargs);
      return r === undefined || r === null ? '' : String(r);
    } catch (e) {
      if (e instanceof EEError) throw e;
      fail(ln, 'الصدفة (' + cmd + '): ' + (e && e.message ? e.message : String(e)));
    }
  }
  function addHandler(f, who, ln) {
    if (typeof f !== 'function') fail(ln, 'المعامل الأخير في $' + who + ' يجب أن يكون مفهوماً (مثل $عند_النقر)');
    handlers.push(f);
    return handlers.length - 1;
  }

  function refreshHostVars() {
    rootEnv.vars.set('منصة', config.platform);
    rootEnv.vars.set('مضيف', !!shell);
  }

  function setupBuiltins() {
    rootEnv.vars.set('باي', Math.PI);
    rootEnv.vars.set('اصدار', VERSION);
    refreshHostVars();

    function printer(nl) {
      return function (a, ln) {
        emit(ln, a.map(function (x) { return toStr(x); }).join(' ') + (nl ? '\n' : ''));
        return null;
      };
    }
    reg('اطبع', printer(true));
    reg('اكتب', printer(false));
    reg('اقرأ', function (a, ln) {
      argc('اقرأ', a, 0, 1, ln);
      var q = a.length ? toStr(a[0]) : '';
      if (isNode && q) emit(ln, q);
      var line = io.readLine(q);
      return line === null ? null : line;
    });
    reg('اقرأ_رقم', function (a, ln) {
      argc('اقرأ_رقم', a, 0, 1, ln);
      var q = a.length ? toStr(a[0]) : '';
      if (isNode && q) emit(ln, q);
      var line = io.readLine(q);
      if (line === null) return null;
      return parseNumber(line);
    });
    reg('طول', function (a, ln) {
      argc('طول', a, 1, 1, ln);
      if (typeof a[0] === 'string') return chars(a[0]).length;
      if (Array.isArray(a[0])) return a[0].length;
      fail(ln, 'المفهوم $طول يتطلب نصاً أو قائمة');
    });
    reg('اضف', function (a, ln) {
      argc('اضف', a, 2, 2, ln);
      if (!Array.isArray(a[0])) fail(ln, 'المفهوم $اضف يتطلب قائمة كمعامل أول');
      a[0].push(a[1]);
      return null;
    });
    reg('ازل', function (a, ln) {
      argc('ازل', a, 1, 1, ln);
      if (!Array.isArray(a[0])) fail(ln, 'المفهوم $ازل يتطلب قائمة');
      if (!a[0].length) fail(ln, 'القائمة فارغة');
      return a[0].pop();
    });
    reg('مدى', function (a, ln) {
      argc('مدى', a, 1, 3, ln);
      var from = 0, to, step = 1;
      if (a.length === 1) to = needNum('مدى', a[0], ln);
      else {
        from = needNum('مدى', a[0], ln);
        to = needNum('مدى', a[1], ln);
        if (a.length === 3) step = needNum('مدى', a[2], ln);
      }
      if (step === 0) fail(ln, "خطوة 'مدى' لا يمكن أن تكون صفراً");
      var l = [];
      for (var x = from; step > 0 ? x < to : x > to; x += step) {
        l.push(x);
        if (l.length > 10000000) fail(ln, 'المدى كبير جداً');
      }
      return l;
    });
    reg('جذر', function (a, ln) {
      argc('جذر', a, 1, 1, ln);
      var x = needNum('جذر', a[0], ln);
      if (x < 0) fail(ln, 'لا يمكن حساب جذر عدد سالب');
      return Math.sqrt(x);
    });
    reg('اس', function (a, ln) {
      argc('اس', a, 2, 2, ln);
      return Math.pow(needNum('اس', a[0], ln), needNum('اس', a[1], ln));
    });
    reg('مطلق', function (a, ln) { argc('مطلق', a, 1, 1, ln); return Math.abs(needNum('مطلق', a[0], ln)); });
    reg('ارضية', function (a, ln) { argc('ارضية', a, 1, 1, ln); return Math.floor(needNum('ارضية', a[0], ln)); });
    reg('سقف', function (a, ln) { argc('سقف', a, 1, 1, ln); return Math.ceil(needNum('سقف', a[0], ln)); });
    reg('تقريب', function (a, ln) {
      argc('تقريب', a, 1, 1, ln);
      var x = needNum('تقريب', a[0], ln);
      return x < 0 ? -Math.round(-x) : Math.round(x);  // مثل std::round: الأبعد عن الصفر عند المنتصف
    });
    function minmax(isMax) {
      var nm = isMax ? 'اكبر' : 'اصغر';
      return function (a, ln) {
        var vals = a.length === 1 && Array.isArray(a[0]) ? a[0] : a;
        if (!vals.length) fail(ln, 'المفهوم $' + nm + ' يحتاج قيمة واحدة على الأقل');
        var r = needNum(nm, vals[0], ln);
        for (var i = 0; i < vals.length; i++) {
          var x = needNum(nm, vals[i], ln);
          r = isMax ? Math.max(r, x) : Math.min(r, x);
        }
        return r;
      };
    }
    reg('اكبر', minmax(true));
    reg('اصغر', minmax(false));
    reg('نص', function (a, ln) { argc('نص', a, 1, 1, ln); return toStr(a[0]); });
    reg('رقم', function (a, ln) {
      argc('رقم', a, 1, 1, ln);
      var v = a[0];
      if (typeof v === 'number') return v;
      if (typeof v === 'bigint') return Number(v);
      if (typeof v === 'boolean') return v ? 1 : 0;
      if (typeof v === 'string') return parseNumber(v);
      return null;
    });
    reg('نوع', function (a, ln) { argc('نوع', a, 1, 1, ln); return typeName(a[0]); });
    reg('قص', function (a, ln) {
      argc('قص', a, 2, 3, ln);
      if (typeof a[0] !== 'string') fail(ln, 'المفهوم $قص يتطلب نصاً');
      var ch = chars(a[0]), n = ch.length;
      var from = Math.trunc(needNum('قص', a[1], ln));
      var to = a.length === 3 ? Math.trunc(needNum('قص', a[2], ln)) : n;
      if (from < 0) from += n;
      if (to < 0) to += n;
      from = Math.max(0, Math.min(from, n));
      to = Math.max(0, Math.min(to, n));
      return ch.slice(from, to).join('');
    });
    reg('فرز', function (a, ln) {
      argc('فرز', a, 1, 1, ln);
      if (!Array.isArray(a[0])) fail(ln, 'المفهوم $فرز يتطلب قائمة');
      var l = a[0].slice();
      var allNum = l.every(function (v) { return typeof v === 'number'; });
      var allStr = l.every(function (v) { return typeof v === 'string'; });
      if (!allNum && !allStr && l.length) fail(ln, 'لا يمكن فرز قائمة بأنواع مختلطة');
      l.sort(function (x, y) { return x < y ? -1 : (x > y ? 1 : 0); });
      return l;
    });
    reg('عكس', function (a, ln) {
      argc('عكس', a, 1, 1, ln);
      if (Array.isArray(a[0])) return a[0].slice().reverse();
      if (typeof a[0] === 'string') return chars(a[0]).reverse().join('');
      fail(ln, 'المفهوم $عكس يتطلب قائمة أو نصاً');
    });
    reg('يحتوي', function (a, ln) {
      argc('يحتوي', a, 2, 2, ln);
      if (Array.isArray(a[0])) return a[0].some(function (v) { return equals(v, a[1]); });
      if (typeof a[0] === 'string' && typeof a[1] === 'string') return a[0].indexOf(a[1]) >= 0;
      fail(ln, 'المفهوم $يحتوي يتطلب (قائمة، قيمة) أو (نص، نص)');
    });
    reg('عشوائي', function (a, ln) {
      if (!a.length) return Math.random();
      argc('عشوائي', a, 2, 2, ln);
      var lo = Math.trunc(needNum('عشوائي', a[0], ln)), hi = Math.trunc(needNum('عشوائي', a[1], ln));
      if (lo > hi) { var t = lo; lo = hi; hi = t; }
      return lo + Math.floor(Math.random() * (hi - lo + 1));
    });
    reg('ارقام_عربية', function (a, ln) { argc('ارقام_عربية', a, 1, 1, ln); return toArabicDigits(toStr(a[0])); });
    alias('اعرض', 'اطبع');
    alias('اسأل', 'اقرأ');
    alias('اسأل_رقم', 'اقرأ_رقم');

    // ── الأعداد الكبيرة ──
    function parseBig(str) {
      var t = toAsciiDigits(str).replace(/^[ \t\n\r]+|[ \t\n\r]+$/g, '');
      if (!/^[+-]?[0-9]+$/.test(t)) return null;
      var r = BigInt(t.replace(/^\+/, ''));
      if (r.toString(16).length > MAX_HEX) fail(0, 'النتيجة كبيرة جداً (أكثر من مليون خانة)');
      return r;
    }
    function toBigArg(name, v, ln) {
      if (typeof v === 'bigint') return v;
      var b = numToBig(v);
      if (typeof v === 'number' && b !== null) return b;
      if (typeof v === 'string') { b = parseBig(v); if (b !== null) return b; }
      fail(ln, 'المفهوم $' + name + ' يتطلب عدداً صحيحاً (أو نصاً من أرقام) لكن وُجد ' + typeName(v) +
        (typeof v === 'number' ? ' غير صحيح' : ''));
    }
    function modpow(base, e, m, ln) {
      if (m < 0n) m = -m;
      if (m === 0n) fail(ln, 'الباقي لا يمكن أن يكون صفراً');
      if (e < 0n) fail(ln, 'الأس يجب أن يكون غير سالب');
      var result = 1n % m;
      base = ((base % m) + m) % m;
      while (e > 0n) {
        tick(ln);
        if (e & 1n) result = (result * base) % m;
        e >>= 1n;
        if (e > 0n) base = (base * base) % m;
      }
      return result;
    }
    function isPrime(n, ln) {
      var small = [2n, 3n, 5n, 7n, 11n, 13n, 17n, 19n, 23n, 29n, 31n, 37n];
      if (n < 2n) return false;
      for (var i = 0; i < small.length; i++) {
        if (n === small[i]) return true;
        if (n % small[i] === 0n) return false;
      }
      var nm1 = n - 1n, dd = nm1, s = 0;
      while ((dd & 1n) === 0n) { dd >>= 1n; s++; }
      for (var j = 0; j < small.length; j++) {
        var x = modpow(small[j], dd, n, ln);
        if (x === 1n || x === nm1) continue;
        var composite = true;
        for (var k = 1; k < s; k++) {
          x = (x * x) % n;
          if (x === nm1) { composite = false; break; }
          if (x === 1n) break;
        }
        if (composite) return false;
      }
      return true;
    }
    reg('كبير', function (a, ln) { argc('كبير', a, 1, 1, ln); return toBigArg('كبير', a[0], ln); });
    reg('اس_كبير', function (a, ln) {
      argc('اس_كبير', a, 2, 2, ln);
      var b = toBigArg('اس_كبير', a[0], ln);
      if (typeof a[1] !== 'number' || a[1] < 0 || a[1] !== Math.floor(a[1]) || a[1] > 1e9)
        fail(ln, 'الأس في $اس_كبير يجب أن يكون عدداً صحيحاً غير سالب');
      var e = BigInt(a[1]), r = 1n;
      while (e > 0n) {
        tick(ln);
        if (e & 1n) r = bigCheck(r * b, ln);
        e >>= 1n;
        if (e > 0n) b = bigCheck(b * b, ln);
      }
      return r;
    });
    reg('اس_بالباقي', function (a, ln) {
      argc('اس_بالباقي', a, 3, 3, ln);
      return modpow(toBigArg('اس_بالباقي', a[0], ln), toBigArg('اس_بالباقي', a[1], ln),
                    toBigArg('اس_بالباقي', a[2], ln), ln);
    });
    reg('اولي_كبير', function (a, ln) {
      argc('اولي_كبير', a, 1, 1, ln);
      return isPrime(toBigArg('اولي_كبير', a[0], ln), ln);
    });

    // ── مفاهيم نصية إضافية ──
    reg('استبدل', function (a, ln) {
      argc('استبدل', a, 3, 3, ln);
      if (typeof a[0] !== 'string' || typeof a[1] !== 'string' || typeof a[2] !== 'string')
        fail(ln, 'المفهوم $استبدل يتطلب ثلاثة نصوص: (النص، القديم، الجديد)');
      if (!a[1].length) fail(ln, 'النص القديم في $استبدل لا يمكن أن يكون فارغاً');
      return a[0].split(a[1]).join(a[2]);
    });
    reg('قسم', function (a, ln) {
      argc('قسم', a, 2, 2, ln);
      if (typeof a[0] !== 'string' || typeof a[1] !== 'string') fail(ln, 'المفهوم $قسم يتطلب نصين: (النص، الفاصل)');
      return a[1].length ? a[0].split(a[1]) : chars(a[0]);
    });
    reg('دمج', function (a, ln) {
      argc('دمج', a, 1, 2, ln);
      if (!Array.isArray(a[0])) fail(ln, 'المفهوم $دمج يتطلب قائمة');
      var sep = a.length === 2 ? toStr(a[1]) : '';
      return a[0].map(function (x) { return toStr(x); }).join(sep);
    });
    reg('قص_فراغ', function (a, ln) {
      argc('قص_فراغ', a, 1, 1, ln);
      if (typeof a[0] !== 'string') fail(ln, 'المفهوم $قص_فراغ يتطلب نصاً');
      return a[0].replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, '');
    });
    function caseFn(upper) {
      return function (a, ln) {
        argc(upper ? 'حروف_كبيرة' : 'حروف_صغيرة', a, 1, 1, ln);
        if (typeof a[0] !== 'string') fail(ln, 'هذا المفهوم يتطلب نصاً');
        return a[0].replace(/[A-Za-z]/g, function (c) { return upper ? c.toUpperCase() : c.toLowerCase(); });
      };
    }
    reg('حروف_كبيرة', caseFn(true));
    reg('حروف_صغيرة', caseFn(false));
    reg('يبدأ_ب', function (a, ln) {
      argc('يبدأ_ب', a, 2, 2, ln);
      if (typeof a[0] !== 'string' || typeof a[1] !== 'string') fail(ln, 'المفهوم $يبدأ_ب يتطلب نصين');
      return a[0].startsWith(a[1]);
    });
    reg('ينتهي_ب', function (a, ln) {
      argc('ينتهي_ب', a, 2, 2, ln);
      if (typeof a[0] !== 'string' || typeof a[1] !== 'string') fail(ln, 'المفهوم $ينتهي_ب يتطلب نصين');
      return a[0].endsWith(a[1]);
    });
    reg('موضع', function (a, ln) {
      argc('موضع', a, 2, 2, ln);
      if (typeof a[0] !== 'string' || typeof a[1] !== 'string') fail(ln, 'المفهوم $موضع يتطلب نصين');
      var f = a[0].indexOf(a[1]);
      return f < 0 ? -1 : chars(a[0].slice(0, f)).length;
    });
    reg('وقت', function (a, ln) { argc('وقت', a, 0, 0, ln); return Date.now(); });

    // ── الصدفة ──
    reg('صدفة', function (a, ln) {
      argc('صدفة', a, 1, 64, ln);
      if (typeof a[0] !== 'string') fail(ln, 'المعامل الأول في $صدفة هو اسم الأمر (نص)، مثل "dom.text"');
      return shellCall(a[0], a.slice(1), ln);
    });
    reg('صدفة_متاح', function (a, ln) { argc('صدفة_متاح', a, 0, 0, ln); return !!shell; });
    reg('منصة', function (a, ln) { argc('منصة', a, 0, 0, ln); return config.platform; });
    reg('على_منصة', function (a, ln) {
      argc('على_منصة', a, 1, 1, ln);
      return normalizeKw(toStr(a[0])) === normalizeKw(config.platform);
    });
    [
      ['صدفة_نص', 'dom.text', 1, 2], ['صدفة_قيمة', 'dom.value', 1, 2], ['صدفة_html', 'dom.html', 1, 2],
      ['صدفة_خاصية', 'dom.attr', 2, 3], ['صدفة_نمط', 'dom.style', 3, 3], ['صدفة_صنف', 'dom.class', 3, 3],
      ['صدفة_اضف', 'dom.append', 2, 2], ['صدفة_تنبيه', 'ui.alert', 1, 1], ['صدفة_رسالة', 'ui.toast', 1, 1],
      ['صدفة_شارك', 'ui.share', 1, 1], ['صدفة_سجل', 'console.log', 1, 1], ['صدفة_خزن', 'store.set', 2, 2],
      ['صدفة_اقرأ_مخزن', 'store.get', 1, 1], ['صدفة_js', 'js.eval', 1, 1],
      ['صدفة_ملف_اقرأ', 'fs.read', 1, 1], ['صدفة_ملف_اكتب', 'fs.write', 2, 2],
      ['صدفة_نظام', 'os.exec', 1, 1], ['صدفة_بيئة', 'os.env', 1, 1], ['صدفة_الغ_مؤقت', 'timer.clear', 1, 1]
    ].forEach(function (d) {
      reg(d[0], function (a, ln) { argc(d[0], a, d[2], d[3], ln); return shellCall(d[1], a, ln); });
    });
    reg('صدفة_اربط', function (a, ln) {
      argc('صدفة_اربط', a, 3, 3, ln);
      var id = addHandler(a[2], 'صدفة_اربط', ln);
      return shellCall('dom.on', [a[0], a[1], id], ln);
    });
    reg('صدفة_انتظر', function (a, ln) {
      argc('صدفة_انتظر', a, 2, 2, ln);
      var id = addHandler(a[1], 'صدفة_انتظر', ln);
      return shellCall('timer.after', [a[0], id], ln);
    });
    reg('صدفة_كرر_كل', function (a, ln) {
      argc('صدفة_كرر_كل', a, 2, 2, ln);
      var id = addHandler(a[1], 'صدفة_كرر_كل', ln);
      return shellCall('timer.every', [a[0], id], ln);
    });
  }

  function ensureRoot() {
    if (!rootEnv) {
      rootEnv = new Env(null);
      setupBuiltins();
    }
    return rootEnv;
  }

  // ───────────── التشغيل والأخطاء ─────────────
  function report(e) {
    if (e instanceof EEError) {
      io.writeErr('\n⚠ خطأ' + (e.eeLine > 0 ? ' (السطر ' + e.eeLine + ')' : '') + ': ' + e.eeMsg + '\n');
    } else if (e instanceof RangeError && /call stack/i.test(e.message)) {
      io.writeErr('\n⚠ خطأ: تجاوز الحد الأقصى لعمق الاستدعاءات (تكرار لا نهائي؟)\n');
    } else {
      io.writeErr('\n⚠ خطأ داخلي: ' + (e && e.message ? e.message : String(e)) + '\n');
    }
    if (isNode) process.exitCode = 1;
    return 1;
  }

  function guarded(f) {
    stopFlag = false;
    written = 0;
    depth = 0;
    try {
      var r = f();
      if (!isNode && !isBrowser) return 0;
      return r === undefined ? 0 : r;
    } catch (e) {
      return report(e);
    }
  }

  function run(programFn) {
    function exec() { return guarded(function () { programFn(ensureRoot()); return 0; }); }
    if (isBrowser && document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', exec);
      return 0;
    }
    return exec();
  }

  function dispatch(id, args) {
    ensureRoot();
    return guarded(function () {
      var f = handlers[id];
      if (!f) fail(0, 'معالج حدث غير معروف: ' + id);
      var a = (args || []).map(function (x) { return x; });
      if (f.eeParams) { a.length = f.eeParams.length; for (var i = 0; i < a.length; i++) if (a[i] === undefined) a[i] = null; }
      call(f, a, 0);
      return 0;
    });
  }

  function toEE(v) {  // تحويل قيمة JS إلى قيمة EE#
    if (v === undefined || v === null) return null;
    if (typeof v === 'number' || typeof v === 'boolean' || typeof v === 'string' || typeof v === 'function') return v;
    if (Array.isArray(v)) return v.map(toEE);
    return String(v);
  }

  var EE = {
    version: VERSION,
    configure: function (o) {
      for (var k in o) config[k] = o[k];
      if (rootEnv) refreshHostVars();
      return EE;
    },
    use: function (s) { shell = s; if (rootEnv) refreshHostVars(); return EE; },
    io: io,
    run: run,
    dispatch: dispatch,
    stop: function () { stopFlag = true; },
    reset: function () { rootEnv = null; handlers = []; stopFlag = false; depth = 0; },
    invoke: function (name, args) {
      ensureRoot();
      var out = null;
      guarded(function () {
        var n = name.charAt(0) === '$' ? name.slice(1) : name;
        out = call(fnref(rootEnv, n, 0), (args || []).map(toEE), 0);
        return 0;
      });
      return out;
    },
    expose: function (name, fn) {
      ensureRoot();
      reg(name, function (a, ln) {
        try { return toEE(fn.apply(null, a)); }
        catch (e) { if (e instanceof EEError) throw e; fail(ln, '$' + name + ': ' + (e && e.message ? e.message : String(e))); }
      });
      return EE;
    },
    getVar: function (name) { ensureRoot(); var e = rootEnv.find(name); return e ? e.vars.get(name) : undefined; },
    setVar: function (name, v) { ensureRoot(); rootEnv.vars.set(name, toEE(v)); return EE; },
    toStr: toStr,
    // ── للشيفرة المُولَّدة ──
    _: {
      fail: fail, get: get, def: def, loopVar: loopVar, fnref: fnref, assign: assign, setIndex: setIndex,
      index: index, op: op, neg: neg, truthy: truthy, child: child, iter: iter, tick: tick,
      lambda: lambda, call: call
    }
  };
  // أسماء مختصرة للشيفرة المُولَّدة (تُقلل حجمها)
  for (var k in EE._) EE[k] = EE._[k];

  root.EE = EE;
  if (typeof module !== 'undefined' && module.exports) module.exports = EE;
})(typeof globalThis !== 'undefined' ? globalThis : (typeof window !== 'undefined' ? window : this));
