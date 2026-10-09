// EE# — واجهة سطر الأوامر: تشغيل، صدفة تفاعلية، ومحرك الترجمة.
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ee.h"
#include "ee_host.h"

namespace fs = std::filesystem;

static void usage() {
  std::cout
      << "EE# " << ee::version() << " — لغة برمجة عربية\n"
      << "الاستخدام:\n"
      << "  ee <ملف.ee>                      تشغيل برنامج\n"
      << "  ee --host <ملف.ee>               تشغيل مع صدفة سطح المكتب (ملفات، نظام، مؤقتات، تخزين)\n"
      << "  ee --shell  (أو --صدفة)           صدفة تفاعلية (REPL)\n"
      << "  ee translate [خيارات] <دخل>      ترجمة برنامج .ee أو صفحة .html إلى منصة هدف\n"
      << "      --target js|web|android|desktop   الهدف (الافتراضي web)\n"
      << "      -o <مجلد>                         مجلد الناتج (الافتراضي out)\n"
      << "      --name <اسم>                      اسم التطبيق\n"
      << "      --package <معرّف>                 معرّف حزمة أندرويد (com.example.app)\n"
      << "      --standalone                      (js) دمج بيئة التشغيل في ملف واحد\n"
      << "  ee --version | --help\n";
}

static bool readFile(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB &&
      static_cast<unsigned char>(out[2]) == 0xBF)
    out.erase(0, 3);
  return true;
}

static int runFile(const std::string& path, bool host) {
  std::string src;
  if (!readFile(path, src)) {
    std::cerr << "تعذّر فتح الملف: " << path << "\n";
    return 2;
  }
  if (!host) return ee::run(src, std::cin, std::cout, std::cerr);
  ee::set_default_platform("سطح_المكتب");
  ee::Session s(std::cin, std::cout, std::cerr);
  ee::DesktopHost h(s, std::cout);
  int rc = s.eval(src);
  if (rc == 0) rc = h.run_events();
  return rc;
}

static int repl() {
  ee::Session s(std::cin, std::cout, std::cerr);
  s.set_repl(true);
  ee::DesktopHost host(s, std::cout);
  std::cout << "صدفة EE# " << ee::version() << "  —  :مساعدة للمساعدة، :خروج للخروج\n";
  std::string buf, line;
  for (;;) {
    std::cout << (buf.empty() ? "صدفة> " : "  ..> ") << std::flush;
    if (!std::getline(std::cin, line)) break;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (buf.empty()) {
      if (line == ":خروج" || line == ":q" || line == "exit" || line == "quit") break;
      if (line == ":مساعدة" || line == ":help") {
        std::cout << "اكتب أي جملة EE# فتُنفَّذ فوراً وتبقى المتغيرات والمفاهيم بين الأسطر.\n"
                     "التعبير الأخير تُعرض قيمته. الكتل (لو/كرر/لكل/مفهوم) تكتمل بـ «نهاية»،\n"
                     "وسطر فارغ أثناء الإدخال المتعدد يفرض التنفيذ.\n"
                     "أوامر الصدفة: $صدفة_ملف_اقرأ  $صدفة_ملف_اكتب  $صدفة_نظام  $صدفة_بيئة  $صدفة_خزن  $صدفة_انتظر\n";
        continue;
      }
    }
    if (!buf.empty() && line.empty()) {
      // إجبار التنفيذ ليظهر الخطأ
    } else {
      buf += (buf.empty() ? "" : "\n") + line;
      if (!ee::Session::is_complete(buf)) continue;
    }
    if (!buf.empty()) s.eval(buf, true);
    buf.clear();
  }
  std::cout << "\n";
  return 0;
}

static int translateCmd(int argc, char** argv) {
  ee::TranslateOptions opt;
  std::string input, outDir = "out";
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&](std::string& dst) {
      if (i + 1 >= argc) { std::cerr << "الخيار " << a << " يحتاج قيمة\n"; return false; }
      dst = argv[++i];
      return true;
    };
    if (a == "--target") { if (!next(opt.target)) return 2; }
    else if (a == "-o") { if (!next(outDir)) return 2; }
    else if (a == "--name") { if (!next(opt.name)) return 2; }
    else if (a == "--package") { if (!next(opt.package_id)) return 2; }
    else if (a == "--standalone") opt.standalone = true;
    else if (!a.empty() && a[0] == '-') { std::cerr << "خيار غير معروف: " << a << "\n"; return 2; }
    else input = a;
  }
  if (input.empty()) { usage(); return 2; }
  std::string src;
  if (!readFile(input, src)) { std::cerr << "تعذّر فتح الملف: " << input << "\n"; return 2; }

  fs::path inPath(input);
  std::string ext = inPath.extension().string();
  bool isHtml = ext == ".html" || ext == ".htm";
  if (opt.name == "EE Program") opt.name = inPath.stem().string();

  ee::TranslateResult r;
  if (isHtml) {
    opt.page_name = inPath.filename().string();
    fs::path dir = inPath.parent_path();
    opt.read_file = [dir](const std::string& p, std::string& out) { return readFile((dir / p).string(), out); };
    r = ee::translate_html(src, opt);
  } else {
    r = ee::translate(src, opt);
  }
  if (!r.ok) { std::cerr << r.error << "\n"; return 1; }

  for (auto& f : r.files) {
    fs::path p = fs::path(outDir) / fs::u8path(f.first);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    if (!o) { std::cerr << "تعذّرت الكتابة: " << p.string() << "\n"; return 2; }
    o << f.second;
    std::cout << "  " << p.string() << "\n";
  }
  std::cout << "تمت الترجمة (" << (isHtml ? "html" : opt.target) << ") إلى المجلد: " << outDir << "\n";
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 2; }
  std::string a = argv[1];
  if (a == "--version" || a == "-v") { std::cout << "EE# " << ee::version() << "\n"; return 0; }
  if (a == "--help" || a == "-h") { usage(); return 0; }
  if (a == "--shell" || a == "--صدفة" || a == "shell") return repl();
  if (a == "translate" || a == "ترجم") return translateCmd(argc, argv);
  if (a == "--host") {
    if (argc < 3) { usage(); return 2; }
    return runFile(argv[2], true);
  }
  return runFile(a, false);
}
