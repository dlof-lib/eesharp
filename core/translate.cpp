// EE# — محرك الترجمة: يحوّل شجرة البرنامج إلى JavaScript ويُنتج مشاريع الويب وأندرويد وسطح المكتب.
// يشترك مع المفسّر في المحلل اللفظي والنحوي (front.h) فتتطابق الأخطاء النحوية تماماً.

#include <cstdio>
#include <sstream>

#include "ee.h"
#include "front.h"
#include "gen/templates_gen.h"

namespace ee {
namespace {

// ───────────── القوالب ─────────────

std::string rawTemplate(const std::string& path) {
  for (size_t i = 0; kTemplates[i].path; i++)
    if (path == kTemplates[i].path) return kTemplates[i].data;
  return std::string();
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
  if (from.empty()) return s;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

using Vars = std::vector<std::pair<std::string, std::string>>;

std::string tpl(const std::string& path, const Vars& vars = {}) {
  std::string s = rawTemplate(path);
  for (auto& kv : vars) s = replaceAll(s, "{{" + kv.first + "}}", kv.second);
  return s;
}

std::string htmlEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

std::string xmlEscape(const std::string& s) {
  std::string o = htmlEscape(s);
  return replaceAll(o, "'", "\\'");
}

// ───────────── مولّد JavaScript ─────────────

struct Gen {
  int uid = 0;
  int ind = 0;
  int lineOffset = 0;  // لصفحات HTML: يُضاف إلى أرقام الأسطر لتشير إلى موضعها في الصفحة

  std::string pad() const { return std::string(static_cast<size_t>(ind) * 2, ' '); }
  std::string fresh() { return "S" + std::to_string(++uid); }
  std::string ln(int l) const { return std::to_string(l + lineOffset); }

  static std::string q(const std::string& s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size(); i++) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
          if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            o += buf;
          } else if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80 &&
                     (static_cast<unsigned char>(s[i + 2]) == 0xA8 || static_cast<unsigned char>(s[i + 2]) == 0xA9)) {
            o += static_cast<unsigned char>(s[i + 2]) == 0xA8 ? "\\u2028" : "\\u2029";
            i += 2;
          } else {
            o += static_cast<char>(c);
          }
      }
    }
    return o + "\"";
  }

  static std::string num(double d) {
    if (d == std::floor(d) && std::fabs(d) < 1e15) {
      char buf[32];
      std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
      return buf;
    }
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
  }

  std::string lambda(const std::shared_ptr<FuncDef>& fd, const std::string& S) {
    std::string params = "[";
    for (size_t i = 0; i < fd->params.size(); i++) params += (i ? ", " : "") + q(fd->params[i]);
    params += "]";
    std::string inner = fresh();
    std::string out = "EE.lambda(" + S + ", " + q(fd->name) + ", " + params + ", function (" + inner + ") {\n";
    ind++;
    stmts(fd->body, inner, false, out);
    ind--;
    out += pad() + "})";
    return out;
  }

  std::string expr(const EP& e, const std::string& S) {
    switch (e->k) {
      case EK::Num: return num(e->num);
      case EK::Str: return q(e->s);
      case EK::BigLit: {  // عدد صحيح طويل: BigInt أصلي في JS (بلا أصفار بادئة)
        size_t nz = e->s.find_first_not_of('0');
        return (nz == std::string::npos ? std::string("0") : e->s.substr(nz)) + "n";
      }
      case EK::Bool: return e->b ? "true" : "false";
      case EK::Null: return "null";
      case EK::Var: return "EE.get(" + S + ", " + q(e->s) + ", " + ln(e->line) + ")";
      case EK::FnRef: return "EE.fnref(" + S + ", " + q(e->s) + ", " + ln(e->line) + ")";
      case EK::List: {
        std::string o = "[";
        for (size_t i = 0; i < e->items.size(); i++) o += (i ? ", " : "") + expr(e->items[i], S);
        return o + "]";
      }
      case EK::Unary:
        if (e->s == "-") return "EE.neg(" + expr(e->l, S) + ", " + ln(e->line) + ")";
        return "!EE.truthy(" + expr(e->l, S) + ")";
      case EK::And: return "(EE.truthy(" + expr(e->l, S) + ") && EE.truthy(" + expr(e->r, S) + "))";
      case EK::Or: return "(EE.truthy(" + expr(e->l, S) + ") || EE.truthy(" + expr(e->r, S) + "))";
      case EK::Binary:
        return "EE.op(" + q(e->s) + ", " + expr(e->l, S) + ", " + expr(e->r, S) + ", " + ln(e->line) + ")";
      case EK::Index: return "EE.index(" + expr(e->l, S) + ", " + expr(e->r, S) + ", " + ln(e->line) + ")";
      case EK::Call: {
        std::string o = "EE.call(" + expr(e->l, S) + ", [";
        for (size_t i = 0; i < e->items.size(); i++) o += (i ? ", " : "") + expr(e->items[i], S);
        return o + "], " + ln(e->line) + ")";
      }
      case EK::Lambda: return lambda(e->fn, S);
      case EK::Assign:
        if (e->l->k == EK::Var)
          return "EE.assign(" + S + ", " + q(e->l->s) + ", " + q(e->s) + ", " + expr(e->r, S) + ", " + ln(e->line) + ")";
        return "EE.setIndex(" + expr(e->r, S) + ", " + expr(e->l->l, S) + ", " + expr(e->l->r, S) + ", " + q(e->s) +
               ", " + ln(e->line) + ")";
    }
    return "null";
  }

  void stmts(const std::vector<SP>& body, const std::string& S, bool inLoop, std::string& out) {
    for (const SP& s : body) stmt(s, S, inLoop, out);
  }

  void stmt(const SP& s, const std::string& S, bool inLoop, std::string& out) {
    switch (s->k) {
      case SK::Expr: out += pad() + expr(s->e, S) + ";\n"; break;
      case SK::Var:
        out += pad() + "EE.def(" + S + ", " + q(s->name) + ", " + expr(s->e, S) + ", " + ln(s->line) + ");\n";
        break;
      case SK::Func:
        out += pad() + "EE.def(" + S + ", " + q(s->name) + ", " + lambda(s->fn, S) + ", " + ln(s->line) + ");\n";
        break;
      case SK::Block: break;  // جملة فارغة (؛)
      case SK::If: {
        out += pad() + "if (EE.truthy(" + expr(s->e, S) + ")) {\n";
        ind++;
        std::string c = fresh();
        out += pad() + "const " + c + " = EE.child(" + S + ");\n";
        stmts(s->body, c, inLoop, out);
        ind--;
        out += pad() + "}";
        if (s->hasElse) {
          out += " else {\n";
          ind++;
          std::string c2 = fresh();
          out += pad() + "const " + c2 + " = EE.child(" + S + ");\n";
          stmts(s->elseBody, c2, inLoop, out);
          ind--;
          out += pad() + "}";
        }
        out += "\n";
        break;
      }
      case SK::While: {
        out += pad() + "while (EE.truthy(" + expr(s->e, S) + ")) {\n";
        ind++;
        std::string c = fresh();
        out += pad() + "EE.tick(" + ln(s->line) + ");\n";
        out += pad() + "const " + c + " = EE.child(" + S + ");\n";
        stmts(s->body, c, true, out);
        ind--;
        out += pad() + "}\n";
        break;
      }
      case SK::For: {
        std::string item = "it" + std::to_string(++uid);
        out += pad() + "for (const " + item + " of EE.iter(" + expr(s->e, S) + ", " + ln(s->line) + ")) {\n";
        ind++;
        std::string c = fresh(), c2 = fresh();
        out += pad() + "EE.tick(" + ln(s->line) + ");\n";
        out += pad() + "const " + c + " = EE.child(" + S + ");\n";
        out += pad() + "EE.loopVar(" + c + ", " + q(s->name) + ", " + item + ");\n";
        out += pad() + "{\n";
        ind++;
        out += pad() + "const " + c2 + " = EE.child(" + c + ");\n";
        stmts(s->body, c2, true, out);
        ind--;
        out += pad() + "}\n";
        ind--;
        out += pad() + "}\n";
        break;
      }
      case SK::Return:
        out += pad() + "return " + (s->e ? expr(s->e, S) : std::string("null")) + ";\n";
        break;
      case SK::Break:
      case SK::Continue:
        if (inLoop) out += pad() + (s->k == SK::Break ? "break;\n" : "continue;\n");
        else out += pad() + "EE.fail(" + ln(s->line) + ", \"'توقف' أو 'استمر' خارج حلقة\");\n";
        break;
    }
  }

  std::string program(const std::vector<SP>& prog) {
    std::string out = "EE.run(function (S) {\n";
    ind = 1;
    stmts(prog, "S", false, out);
    ind = 0;
    out += "});\n";
    return out;
  }
};

std::string formatError(const EEError& e) {
  std::string m = "⚠ خطأ";
  if (e.line > 0) m += " (السطر " + std::to_string(e.line) + ")";
  return m + ": " + e.msg;
}

std::string stripBom(const std::string& s) {
  if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
      static_cast<unsigned char>(s[2]) == 0xBF)
    return s.substr(3);
  return s;
}

// نص C++ حرفي آمن (بايتات UTF-8 تُكتب ثمانية بثلاثة أرقام لتفادي التباس الأرقام السداسية)
std::string cppLiteral(const std::string& s) {
  std::string o;
  size_t col = 0;
  o += "\"";
  for (unsigned char c : s) {
    if (col > 90 && c != '\n') { o += "\"\n    \""; col = 0; }
    switch (c) {
      case '\n': o += "\\n\"\n    \""; col = 0; continue;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '?': o += "\\?"; break;
      default:
        if (c < 0x20 || c >= 0x7F) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\%03o", c);
          o += buf;
        } else {
          o += static_cast<char>(c);
        }
    }
    col++;
  }
  return o + "\"";
}

std::string packagePath(const std::string& pkg) { return replaceAll(pkg, ".", "/"); }

bool validPackage(const std::string& p) {
  if (p.empty()) return false;
  bool segStart = true;
  for (char c : p) {
    if (c == '.') {
      if (segStart) return false;
      segStart = true;
    } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || (!segStart && std::isdigit(static_cast<unsigned char>(c)))) {
      segStart = false;
    } else {
      return false;
    }
  }
  return !segStart;
}

TranslateResult failure(const std::string& msg) {
  TranslateResult r;
  r.ok = false;
  r.error = msg;
  return r;
}

const char* kJsBanner = "// مُولَّد بواسطة محرك ترجمة EE# — لا تعدّله يدوياً (عدّل ملف .ee ثم أعد الترجمة)\n";

}  // namespace

namespace {
bool translateWithOffset(const std::string& source, int offset, std::string& js, std::string& error) {
  try {
    Parser ps;
    ps.t = lex(stripBom(source));
    std::vector<SP> prog = ps.parseProgram();
    Gen g;
    g.lineOffset = offset;
    js = g.program(prog);
    return true;
  } catch (EEError e) {
    if (e.line > 0) e.line += offset;
    error = formatError(e);
    return false;
  }
}
}  // namespace

bool translate_to_js(const std::string& source, std::string& js, std::string& error) {
  return translateWithOffset(source, 0, js, error);
}

TranslateResult translate(const std::string& source, const TranslateOptions& opt) {
  std::string js, err;
  if (!translate_to_js(source, js, err)) return failure(err);
  std::string program = kJsBanner + js;

  TranslateResult r;
  r.ok = true;
  const std::string& t = opt.target;
  std::string title = htmlEscape(opt.name);

  if (t == "js") {
    if (opt.standalone) r.files.push_back({"program.js", tpl("runtime/ee-runtime.js") + "\n" + program});
    else {
      r.files.push_back({"program.js", program});
      r.files.push_back({"ee-runtime.js", tpl("runtime/ee-runtime.js")});
    }
    return r;
  }

  auto webFiles = [&](const std::string& prefix, const std::string& platform) {
    r.files.push_back({prefix + "index.html", tpl("web/index.html", {{"TITLE", title}, {"PLATFORM", platform}})});
    r.files.push_back({prefix + "program.js", program});
    r.files.push_back({prefix + "ee-runtime.js", tpl("runtime/ee-runtime.js")});
    r.files.push_back({prefix + "ee-shell.js", tpl("runtime/ee-shell.js")});
  };

  if (t == "web") {
    webFiles("", "ويب");
    r.files.push_back({"program.ee", source});
    return r;
  }

  if (t == "android") {
    if (!validPackage(opt.package_id)) return failure("⚠ خطأ: معرّف الحزمة غير صالح: " + opt.package_id + " (مثال: com.example.myapp)");
    Vars v{{"PACKAGE", opt.package_id}, {"PACKAGE_PATH", packagePath(opt.package_id)}, {"APPNAME", xmlEscape(opt.name)}};
    for (const char* f : {"settings.gradle", "build.gradle", "gradle.properties", ".github/workflows/android.yml"})
      r.files.push_back({f, tpl(std::string("android/") + f, v)});
    r.files.push_back({"app/build.gradle", tpl("android/app/build.gradle", v)});
    r.files.push_back({"app/src/main/AndroidManifest.xml", tpl("android/app/src/main/AndroidManifest.xml", v)});
    r.files.push_back({"app/src/main/res/values/strings.xml", tpl("android/app/src/main/res/values/strings.xml", v)});
    r.files.push_back({"app/src/main/res/values/themes.xml", tpl("android/app/src/main/res/values/themes.xml", v)});
    r.files.push_back({"app/src/main/java/" + packagePath(opt.package_id) + "/MainActivity.java",
                       tpl("android/app/src/main/java/MainActivity.java", v)});
    webFiles("app/src/main/assets/", "اندرويد");
    r.files.push_back({"app/src/main/assets/program.ee", source});
    return r;
  }

  if (t == "desktop") {
    r.files.push_back({"main.cpp", tpl("desktop/main.cpp", {{"NAME", opt.name}, {"SOURCE", cppLiteral(stripBom(source))}})});
    r.files.push_back({"CMakeLists.txt", tpl("desktop/CMakeLists.txt")});
    r.files.push_back({"program.ee", source});
    r.files.push_back({"README.txt", tpl("desktop/README.txt")});
    return r;
  }

  return failure("⚠ خطأ: هدف الترجمة غير معروف: " + t + " (المتاح: js, web, android, desktop)");
}

TranslateResult translate_html(const std::string& html, const TranslateOptions& opt) {
  auto lower = [](std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  };
  std::string low = lower(html);
  std::string out;
  size_t pos = 0;
  int count = 0;
  bool injected = false;
  std::string runtimeTags =
      "<script src=\"ee-runtime.js\"></script>\n<script src=\"ee-shell.js\"></script>\n"
      "<script>EE.configure({ platform: \"ويب\" }); EEShell.install(EE);</script>\n";

  for (;;) {
    size_t st = low.find("<script", pos);
    if (st == std::string::npos) break;
    size_t gt = low.find('>', st);
    if (gt == std::string::npos) break;
    std::string tag = html.substr(st, gt - st + 1);
    std::string ltag = low.substr(st, gt - st + 1);

    auto attr = [&](const char* name) -> std::string {
      size_t a = 0;
      std::string key = std::string(name) + "=";
      while ((a = ltag.find(key, a)) != std::string::npos) {
        if (a > 0 && (ltag[a - 1] == ' ' || ltag[a - 1] == '\t' || ltag[a - 1] == '\n')) break;
        a += key.size();
      }
      if (a == std::string::npos) return "";
      a += key.size();
      if (a >= tag.size()) return "";
      char qc = tag[a];
      if (qc == '"' || qc == '\'') {
        size_t e = tag.find(qc, a + 1);
        return e == std::string::npos ? "" : tag.substr(a + 1, e - a - 1);
      }
      size_t e = a;
      while (e < tag.size() && tag[e] != ' ' && tag[e] != '>' && tag[e] != '/') e++;
      return tag.substr(a, e - a);
    };

    std::string type = lower(attr("type"));
    size_t close = low.find("</script>", gt);
    if (close == std::string::npos) break;
    size_t endTag = close + 9;

    if (type == "text/ee" || type == "text/eesharp") {
      std::string src = html.substr(gt + 1, close - gt - 1);
      std::string srcAttr = attr("src");
      if (!srcAttr.empty()) {
        if (!opt.read_file || !opt.read_file(srcAttr, src))
          return failure("⚠ خطأ: تعذّرت قراءة ملف الشيفرة: " + srcAttr);
      }
      std::string js, err;
      int offset = 0;
      if (srcAttr.empty()) offset = static_cast<int>(std::count(html.begin(), html.begin() + static_cast<long>(gt) + 1, '\n'));
      if (!translateWithOffset(src, offset, js, err))
        return failure(err + "  [في <script type=\"text/ee\"> رقم " + std::to_string(count + 1) + (srcAttr.empty() ? "" : " — الملف " + srcAttr) + "]");
      out += html.substr(pos, st - pos);
      if (!injected) { out += runtimeTags; injected = true; }
      out += "<script>\n" + std::string(kJsBanner) + js + "</script>";
      count++;
    } else {
      out += html.substr(pos, endTag - pos);
    }
    pos = endTag;
  }
  out += html.substr(pos);
  if (count == 0) return failure("⚠ خطأ: لم يُعثر على <script type=\"text/ee\"> في الصفحة");

  TranslateResult r;
  r.ok = true;
  r.files.push_back({opt.page_name, out});
  r.files.push_back({"ee-runtime.js", tpl("runtime/ee-runtime.js")});
  r.files.push_back({"ee-shell.js", tpl("runtime/ee-shell.js")});
  return r;
}

}  // namespace ee
