// اختبارات الجلسات والصدفة ومحرك الترجمة وواجهة C وصدفة سطح المكتب.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ee.h"
#include "ee_capi.h"
#include "ee_host.h"

static int g_fail = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
      g_fail++;                                                                  \
    }                                                                            \
  } while (0)
#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                              \
    std::string _a = (a), _b = (b);                                                                 \
    if (_a != _b) {                                                                                 \
      std::fprintf(stderr, "FAIL %s:%d: %s\n  got:      [%s]\n  expected: [%s]\n", __FILE__, __LINE__, #a, \
                   _a.c_str(), _b.c_str());                                                         \
      g_fail++;                                                                                     \
    }                                                                                               \
  } while (0)

static bool has(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

struct Rig {
  std::istringstream in;
  std::ostringstream out;
  ee::Session s;
  Rig() : s(in, out, out) {}
  std::string take() { std::string r = out.str(); out.str(""); out.clear(); return r; }
};

static void testSession() {
  Rig r;
  CHECK(r.s.eval("متغير <<س>> = 5") == 0);
  CHECK(r.s.eval("مفهوم $زد(<<ن>>)\n  ارجع <<ن>> + <<س>>\nنهاية") == 0);
  CHECK(r.s.eval("$اعرض $زد(10)") == 0);
  CHECK_EQ(r.take(), "15\n");
  CHECK(r.s.eval("متغير <<س>> = 6") == 1);  // إعادة تعريف خارج الطرفية خطأ
  CHECK(has(r.take(), "معرّف مسبقاً"));
  r.s.set_repl(true);
  CHECK(r.s.eval("متغير <<س>> = 7") == 0);
  CHECK(r.s.eval("<<س>> + 1", true) == 0);
  CHECK_EQ(r.take(), "8\n");
  CHECK(r.s.eval("\"نص\"", true) == 0);
  CHECK_EQ(r.take(), "\"نص\"\n");
  // الاستدعاء بالاسم
  CHECK(r.s.eval("مفهوم $مرحبا(<<اسم>>)\n  $اعرض \"أهلاً\", <<اسم>>\nنهاية") == 0);
  CHECK(r.s.call("مرحبا", {"سارة"}) == 0);
  CHECK_EQ(r.take(), "أهلاً سارة\n");
  CHECK(r.s.call("غير_موجود", {}) == 1);
  r.take();
}

static void testComplete() {
  CHECK(ee::Session::is_complete("$اعرض 1"));
  CHECK(!ee::Session::is_complete("لو صح\n  $اعرض 1"));
  CHECK(ee::Session::is_complete("لو صح\n  $اعرض 1\nنهاية لو"));
  CHECK(!ee::Session::is_complete("$اعرض ("));
  CHECK(!ee::Session::is_complete("/* تعليق"));
  CHECK(ee::Session::is_complete("$اعرض <<مجهول>> ؛ ؛"));  // الأخطاء الأخرى «مكتملة» وتظهر عند التنفيذ
}

static void testShell() {
  Rig r;
  std::vector<std::pair<std::string, std::vector<std::string>>> log;
  r.s.set_platform("ويب");
  r.s.set_shell([&](const std::string& cmd, const std::vector<std::string>& a) -> std::string {
    log.push_back({cmd, a});
    if (cmd == "dom.text" && a.size() == 1) return "قيمة_قديمة";
    if (cmd == "boom") throw std::runtime_error("انفجار");
    return "";
  });
  CHECK(r.s.eval("$اعرض <<منصة>>, <<مضيف>>") == 0);
  CHECK_EQ(r.take(), "ويب صح\n");
  CHECK(r.s.eval("$اعرض $صدفة_نص(\"#a\")") == 0);
  CHECK_EQ(r.take(), "قيمة_قديمة\n");
  CHECK(log.back().first == "dom.text" && log.back().second.size() == 1 && log.back().second[0] == "#a");
  CHECK(r.s.eval("$صدفة_نص(\"#a\", 42)") == 0);
  CHECK(log.back().second.size() == 2 && log.back().second[1] == "42");
  CHECK(r.s.eval("$صدفة(\"أمر.عام\", \"س\", ١٫٥, صح, [1, 2])") == 0);
  CHECK(log.back().first == "أمر.عام" && log.back().second.size() == 4);
  CHECK_EQ(log.back().second[2], "صح");
  CHECK_EQ(log.back().second[3], "[1، 2]");
  // أخطاء المضيف تصبح أخطاء EE#
  CHECK(r.s.eval("$صدفة(\"boom\")") == 1);
  CHECK(has(r.take(), "الصدفة (boom): انفجار"));
  CHECK(r.s.eval("$صدفة_نص()") == 1);
  CHECK(has(r.take(), "يتوقع 1 إلى 2 معاملات"));
  CHECK(r.s.eval("$صدفة(\"x\", $اعرض)") == 1);
  CHECK(has(r.take(), "لا يمكن تمرير مفهوم"));
  // الأحداث والمعالجات
  log.clear();
  CHECK(r.s.eval("متغير <<عدد>> = 0\n"
                 "مفهوم $عند_النقر(<<قيمة>>)\n"
                 "  <<عدد>> += 1\n"
                 "  $صدفة_نص(\"#out\", \"نقرات: \" + <<عدد>> + \" \" + <<قيمة>>)\n"
                 "نهاية\n"
                 "$صدفة_اربط(\"#btn\", \"click\", $عند_النقر)") == 0);
  CHECK(log.size() == 1 && log[0].first == "dom.on" && log[0].second.size() == 3);
  CHECK_EQ(log[0].second[0], "#btn");
  CHECK_EQ(log[0].second[1], "click");
  int id = std::atoi(log[0].second[2].c_str());
  CHECK(r.s.dispatch(id, {"س", "زائد"}) == 0);  // معاملات زائدة تُهمل
  CHECK(r.s.dispatch(id, {}) == 0);              // ناقصة تصبح عدم
  CHECK(log.back().first == "dom.text");
  CHECK_EQ(log.back().second[1], "نقرات: 2 عدم");
  CHECK(r.s.dispatch(99, {}) == 1);
  CHECK(has(r.take(), "معالج حدث غير معروف"));
  CHECK(r.s.eval("$صدفة_اربط(\"#a\", \"click\", 5)") == 1);
  CHECK(has(r.take(), "يجب أن يكون مفهوماً"));
  CHECK(r.s.eval("$صدفة_انتظر(100, $عند_النقر)") == 0);
  CHECK(log.back().first == "timer.after" && log.back().second[0] == "100");
}

static void testTranslate() {
  ee::TranslateOptions o;
  const std::string src = "متغير <<أ>> = \"مرحباً\"\n$اعرض <<أ>>, 2 + 3\n";
  o.target = "js";
  auto r = ee::translate(src, o);
  CHECK(r.ok && r.files.size() == 2);
  CHECK(has(r.files[0].second, "EE.run(function (S) {"));
  CHECK(has(r.files[0].second, "EE.def(S, \"أ\", \"مرحباً\", 1);"));
  CHECK(has(r.files[0].second, "EE.op(\"+\", 2, 3, 2)"));
  o.standalone = true;
  r = ee::translate(src, o);
  CHECK(r.ok && r.files.size() == 1 && has(r.files[0].second, "VERSION") && has(r.files[0].second, "EE.run("));
  o.standalone = false;

  o.target = "web";
  o.name = "تطبيق <تجريبي>";
  r = ee::translate(src, o);
  CHECK(r.ok);
  bool idx = false, rt = false, sh = false, pj = false;
  for (auto& f : r.files) {
    if (f.first == "index.html") { idx = true; CHECK(has(f.second, "تطبيق &lt;تجريبي&gt;")); CHECK(has(f.second, "platform: \"ويب\"")); }
    if (f.first == "ee-runtime.js") rt = true;
    if (f.first == "ee-shell.js") sh = true;
    if (f.first == "program.js") pj = true;
  }
  CHECK(idx && rt && sh && pj);

  o.target = "android";
  o.name = "تطبيقي";
  o.package_id = "com.example.demo";
  r = ee::translate(src, o);
  CHECK(r.ok);
  bool act = false, assets = false, manifest = false, gradle = false;
  for (auto& f : r.files) {
    if (f.first == "app/src/main/java/com/example/demo/MainActivity.java") { act = has(f.second, "package com.example.demo;"); }
    if (f.first == "app/src/main/assets/index.html") assets = has(f.second, "platform: \"اندرويد\"");
    if (f.first == "app/src/main/AndroidManifest.xml") manifest = true;
    if (f.first == "app/build.gradle") gradle = has(f.second, "namespace 'com.example.demo'");
  }
  CHECK(act && assets && manifest && gradle);
  o.package_id = "bad package!";
  r = ee::translate(src, o);
  CHECK(!r.ok && has(r.error, "معرّف الحزمة غير صالح"));
  o.package_id = "com.example.demo";

  o.target = "desktop";
  r = ee::translate("$اعرض \"س\\\"ص\" // ؟\n", o);
  CHECK(r.ok);
  for (auto& f : r.files)
    if (f.first == "main.cpp") { CHECK(has(f.second, "ee::DesktopHost")); CHECK(has(f.second, "\\\"")); }

  o.target = "nope";
  r = ee::translate(src, o);
  CHECK(!r.ok && has(r.error, "هدف الترجمة غير معروف"));

  // الأخطاء النحوية تحمل السطر
  o.target = "js";
  r = ee::translate("$اعرض 1\nلو صح\n", o);
  CHECK(!r.ok && has(r.error, "السطر 2") && has(r.error, "لم تُغلق"));

  // HTML
  std::string html =
      "<!doctype html><html><head><title>x</title></head><body>\n<button id=\"b\">زر</button>\n"
      "<script>var keep = 1;</script>\n"
      "<script type=\"text/ee\">\n$صدفة_نص(\"#b\", \"مرحبا\")\n</script>\n"
      "<script type='text/ee' src=\"extra.ee\"></script>\n</body></html>";
  o.read_file = [](const std::string& p, std::string& c) { if (p != "extra.ee") return false; c = "$اعرض 7"; return true; };
  o.page_name = "page.html";
  auto h = ee::translate_html(html, o);
  CHECK(h.ok && h.files.size() == 3 && h.files[0].first == "page.html");
  const std::string& page = h.files[0].second;
  CHECK(has(page, "<script>var keep = 1;</script>"));
  CHECK(!has(page, "text/ee"));
  CHECK(has(page, "ee-runtime.js") && has(page, "EEShell.install(EE)"));
  CHECK(has(page, "EE.call(EE.fnref(S, \"صدفة_نص\", 5)"));
  CHECK(has(page, "EE.fnref(S, \"اعرض\", 1)"));
  // الزمن: ee-runtime يُحقن مرة واحدة فقط
  CHECK(page.find("ee-runtime.js") == page.rfind("ee-runtime.js"));
  o.read_file = nullptr;
  h = ee::translate_html(html, o);
  CHECK(!h.ok && has(h.error, "extra.ee"));
  h = ee::translate_html("<p>لا شيء</p>", o);
  CHECK(!h.ok && has(h.error, "لم يُعثر"));
  h = ee::translate_html("<script type=\"text/ee\">لو</script>", o);
  CHECK(!h.ok && has(h.error, "text/ee"));
}

static const char* capiCb(const char* cmd, const char* args, int nargs, void* user) {
  static std::string res;
  std::string c = cmd;
  *static_cast<std::string*>(user) += c + "(";
  const char* p = args;
  for (int i = 0; i < nargs; i++) {
    *static_cast<std::string*>(user) += std::string(i ? "|" : "") + p;
    p += std::string(p).size() + 1;
  }
  *static_cast<std::string*>(user) += ")";
  if (c == "err") { res = std::string("\x01") + "فشل المضيف"; return res.c_str(); }
  res = "رد:" + c;
  return res.c_str();
}

static void testCApi() {
  ee_session* s = ee_session_new();
  std::string seen;
  ee_set_shell(s, capiCb, &seen);
  ee_set_platform(s, "ويب");
  ee_set_input(s, "ليلى\n");
  CHECK(ee_eval(s, "متغير <<ا>> = $اسأل()\n$اعرض $صدفة_نص(\"#x\", <<ا>>), <<منصة>>") == 0);
  CHECK_EQ(ee_take_output(s), "رد:dom.text ويب\n");
  CHECK_EQ(seen, "dom.text(#x|ليلى)");
  CHECK(ee_eval(s, "$صدفة(\"err\")") == 1);
  CHECK(has(ee_take_output(s), "فشل المضيف"));
  CHECK(ee_is_complete("لو صح") == 0);
  CHECK(ee_eval(s, "مفهوم $ن(<<أ>>, <<ب>>)\n$اعرض <<أ>>, <<ب>>\nنهاية") == 0);
  const char args[] = {'1', 0, '2', 0};
  CHECK(ee_call(s, "ن", args, 2) == 0);
  CHECK_EQ(ee_take_output(s), "1 2\n");
  ee_session_free(s);
}

static void testDesktopHost() {
  std::string store = "unit_test_store.tmp";
  std::remove(store.c_str());
  Rig r;
  {
    ee::DesktopHost h(r.s, r.out, true, store);
    r.s.set_platform("سطح_المكتب");
    CHECK(r.s.eval("$صدفة_خزن(\"مفتاح\", \"قيمة\\nمتعددة\")\n$اعرض $صدفة_اقرأ_مخزن(\"مفتاح\")") == 0);
    CHECK_EQ(r.take(), "قيمة\nمتعددة\n");
    CHECK(r.s.eval("$صدفة_ملف_اكتب(\"unit_test_file.tmp\", \"محتوى\")\n$اعرض $صدفة_ملف_اقرأ(\"unit_test_file.tmp\")") == 0);
    CHECK_EQ(r.take(), "محتوى\n");
    std::remove("unit_test_file.tmp");
    CHECK(r.s.eval("$صدفة_ملف_اقرأ(\"لا_يوجد_هذا_الملف.tmp\")") == 1);
    CHECK(has(r.take(), "تعذّر فتح الملف"));
#ifndef _WIN32
    CHECK(r.s.eval("$اعرض $صدفة_نظام(\"echo مرحبا\")") == 0);
    CHECK_EQ(r.take(), "مرحبا\n\n");
#endif
    CHECK(r.s.eval("$صدفة_نص(\"#a\")") == 1);
    CHECK(has(r.take(), "خاص بالويب"));
    // المؤقتات
    CHECK(r.s.eval("متغير <<ن>> = 0\n"
                   "متغير <<مؤقت>> = 0\n"
                   "مفهوم $خطوة()\n  <<ن>> += 1\n  $اعرض \"خطوة\", <<ن>>\n  لو <<ن>> = 3\n    $صدفة_الغ_مؤقت(<<مؤقت>>)\n  نهاية لو\nنهاية\n"
                   "<<مؤقت>> = $صدفة_كرر_كل(5, $خطوة)\n"
                   "$صدفة_انتظر(1, مفهوم()\n  $اعرض \"مرة واحدة\"\nنهاية)") == 0);
    CHECK(h.run_events() == 0);
    CHECK_EQ(r.take(), "مرة واحدة\nخطوة 1\nخطوة 2\nخطوة 3\n");
  }
  {  // المخزن محفوظ بين التشغيلات
    Rig r2;
    ee::DesktopHost h2(r2.s, r2.out, false, store);
    CHECK(r2.s.eval("$اعرض $صدفة_اقرأ_مخزن(\"مفتاح\")") == 0);
    CHECK_EQ(r2.take(), "قيمة\nمتعددة\n");
    CHECK(r2.s.eval("$صدفة_نظام(\"echo x\")") == 1);
    CHECK(has(r2.take(), "معطّل"));
  }
  std::remove(store.c_str());
}

int main() {
  testSession();
  testComplete();
  testShell();
  testTranslate();
  testCApi();
  testDesktopHost();
  if (g_fail) {
    std::fprintf(stderr, "%d فشل\n", g_fail);
    return 1;
  }
  std::puts("كل اختبارات الوحدة نجحت");
  return 0;
}
