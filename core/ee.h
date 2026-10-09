// EE# — لغة برمجة عربية بالكامل، منفّذة بلغة C++17
#pragma once

#include <cstddef>
#include <functional>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace ee {

// رقم الإصدار
const char* version();

// تشغيل برنامج مكتوب بلغة EE#.
//  source     : نص البرنامج (UTF-8)
//  in         : مصدر المدخلات (لدوال اقرأ / اقرأ_رقم)
//  out, err   : مجرى المخرجات والأخطاء (يمكن أن يكونا نفس المجرى)
//  max_output : أقصى عدد بايتات للمخرجات (0 = بلا حد)
// القيمة المرجعة: 0 عند النجاح، 1 عند حدوث خطأ.
int run(const std::string& source, std::istream& in, std::ostream& out,
        std::ostream& err, std::size_t max_output = 0);

// طلب إيقاف البرنامج الجاري تشغيله (آمن للاستدعاء من خيط آخر).
void request_stop();

// ══════════════ الصدفة (Shell): جسر بين EE# والمضيف ══════════════
// المضيف قد يكون: صفحة ويب (DOM/JS) أو WebView في أندرويد أو تطبيق سطح مكتب أو الطرفية.
// يرسل البرنامج أمراً نصياً (مثل "dom.text") مع معاملات نصية ويتلقى نصاً.
// إذا رمى المعالج std::exception تتحول رسالته إلى خطأ EE# عادي بسطر الاستدعاء.
using ShellHandler =
    std::function<std::string(const std::string& cmd, const std::vector<std::string>& args)>;

// المنصة الافتراضية للمتغير <<منصة>> : "طرفية" | "ويب" | "اندرويد" | "سطح_المكتب"
void set_default_platform(const std::string& name);

// جلسة تحتفظ بالمتغيرات والمفاهيم بين الاستدعاءات (لصفحات HTML والتطبيقات والطرفية التفاعلية).
class Session {
 public:
  Session(std::istream& in, std::ostream& out, std::ostream& err, std::size_t max_output = 0);
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  void set_platform(const std::string& name);
  void set_shell(ShellHandler handler);
  // في وضع الطرفية يُسمح بإعادة تعريف المتغيرات في النطاق العام.
  void set_repl(bool on);

  // تنفيذ كود. echo_result: يطبع قيمة آخر تعبير (للطرفية). القيمة: 0 نجاح، 1 خطأ.
  int eval(const std::string& source, bool echo_result = false);
  // هل الكود مكتمل؟ (false إذا كانت هناك كتلة مفتوحة تنتظر «نهاية»)
  static bool is_complete(const std::string& source);
  // استدعاء مفهوم معرّف باسمه ($اسم) بمعاملات نصية.
  int call(const std::string& name, const std::vector<std::string>& args);
  // يستدعيه المضيف عند وقوع حدث سبق ربطه بـ $صدفة_اربط / $صدفة_انتظر.
  int dispatch(int handler_id, const std::vector<std::string>& args);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ══════════════ محرك الترجمة (Translator) ══════════════
// يحوّل برنامج EE# إلى مشروع/ملفات تعمل على منصة هدف:
//   js       : ملف JavaScript (program.js) يحتاج ee-runtime.js (أو standalone)
//   web      : صفحة HTML + JS جاهزة للمتصفح
//   android  : مشروع Android Studio (WebView + جسر صدفة بـ Kotlin)
//   desktop  : مشروع C++/CMake لتطبيق سطح مكتب يضم البرنامج
struct TranslateOptions {
  std::string target = "web";
  std::string name = "EE Program";
  std::string package_id = "com.eesharp.app";
  bool standalone = false;  // js: دمج ee-runtime.js في الملف الناتج
  std::string page_name = "index.html";  // اسم صفحة HTML الناتجة
  // قارئ ملفات لـ <script type="text/ee" src="..."> (اختياري)
  std::function<bool(const std::string& path, std::string& content)> read_file;
};

struct TranslateResult {
  bool ok = false;
  std::string error;  // رسالة عربية منسّقة عند الفشل
  std::vector<std::pair<std::string, std::string>> files;  // المسار النسبي → المحتوى
};

TranslateResult translate(const std::string& source, const TranslateOptions& opt);
// يترجم صفحة HTML تحتوي <script type="text/ee"> ويُبقي باقي الصفحة كما هو.
TranslateResult translate_html(const std::string& html, const TranslateOptions& opt);
// ترجمة برنامج إلى جسم JS فقط (دون غلاف).
bool translate_to_js(const std::string& source, std::string& js, std::string& error);

}  // namespace ee
