// اختبار تكامل JS ↔ EE# بلا متصفح: بيئة التشغيل + صدفة وهمية.
// الاستخدام: node shell_test.js <مسار ee>
const { execFileSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const assert = require('assert');

const ee = process.argv[2];
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ee-shell-'));
const src = path.join(dir, 'p.ee');
fs.writeFileSync(src, `
متغير <<عدد>> = 0
مفهوم $جمع(<<أ>>, <<ب>>)
  ارجع <<أ>> + <<ب>>
نهاية
مفهوم $عند_النقر(<<ق>>, <<مفتاح>>)
  <<عدد>> += 1
  $صدفة_نص("#out", "نقرات " + <<عدد>> + " " + <<ق>>)
نهاية
$صدفة_اربط("#btn", "click", $عند_النقر)
$اعرض $صدفة_نص("#out")
مفهوم $استعمل_js()
  ارجع $مضاعف(21)
نهاية
مفهوم $قائمة()
  ارجع [1, "ب", صح]
نهاية
`);
execFileSync(ee, ['translate', '--target', 'js', '-o', dir, src], { stdio: 'pipe' });

const EE = require(path.join(dir, 'ee-runtime.js'));
const out = [], err = [], calls = [];
EE.io.write = (s) => out.push(s);
EE.io.writeErr = (s) => err.push(s);
EE.configure({ platform: 'ويب' });
EE.use({
  call(cmd, args) {
    calls.push([cmd, args]);
    if (cmd === 'dom.text' && args.length === 1) return 'قديم';
    if (cmd === 'fail') throw new Error('فشل المضيف');
    return '';
  },
});
require(path.join(dir, 'program.js'));

assert.strictEqual(out.join(''), 'قديم\n');
const on = calls.find((c) => c[0] === 'dom.on');
assert.deepStrictEqual(on[1].slice(0, 2), ['#btn', 'click']);
const id = Number(on[1][2]);

// JS → EE#: إطلاق الحدث
assert.strictEqual(EE.dispatch(id, ['س']), 0);
assert.strictEqual(EE.dispatch(id, []), 0);
assert.deepStrictEqual(calls[calls.length - 1], ['dom.text', ['#out', 'نقرات 2 عدم']]);
assert.strictEqual(EE.dispatch(77, []), 1);
assert.ok(err.join('').includes('معالج حدث غير معروف'));

// JS → EE#: استدعاء مفهوم وقراءة متغير وكتابته
assert.strictEqual(EE.invoke('جمع', [2, 3]), 5);
assert.strictEqual(EE.invoke('$جمع', ['أ', 'ب']), 'أب');
assert.deepStrictEqual(EE.invoke('قائمة', []), [1, 'ب', true]);
assert.strictEqual(EE.getVar('عدد'), 2);
EE.setVar('عدد', 10);
assert.strictEqual(EE.getVar('عدد'), 10);

// EE# → JS: دالة JS مكشوفة
EE.expose('مضاعف', (x) => x * 2);
assert.strictEqual(EE.invoke('استعمل_js', []), 42);
EE.expose('يفشل', () => { throw new Error('خطأ JS'); });
err.length = 0;
EE.invoke('يفشل', []);
assert.ok(err.join('').includes('$يفشل: خطأ JS'), err.join(''));

// أخطاء المضيف تتحول إلى أخطاء EE# بسطرها
err.length = 0;
EE.reset();
EE.use({ call() { throw new Error('رفض'); } });
EE.run((S) => { EE.call(EE.fnref(S, 'صدفة_سجل', 3), ['x'], 3); });
assert.ok(err.join('').includes('(السطر 3): الصدفة (console.log): رفض'), err.join(''));

process.exitCode = 0;  // الأخطاء أعلاه مقصودة
console.log('اختبار تكامل JS نجح');
fs.rmSync(dir, { recursive: true, force: true });
