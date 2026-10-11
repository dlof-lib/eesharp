// EE# — مفسّر لغة البرمجة العربية (C++17)
// المراحل: محلل لفظي (lex) ← محلل نحوي (Parser) ← منفّذ شجري (Interp)

#include "ee.h"

#include <algorithm>
#include <ctime>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <fstream>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ee {

const char* version() { return "0.2.0"; }

namespace {

std::atomic<bool> g_stop{false};

struct Value;
struct EEError {
  int line;
  std::string msg;
  std::shared_ptr<Value> val;  // القيمة المرمية بـ $ارم (إن وُجدت)
  EEError(int l, std::string m, std::shared_ptr<Value> v = nullptr) : line(l), msg(std::move(m)), val(std::move(v)) {}
};
[[noreturn]] void fail(int line, const std::string& msg) { throw EEError{line, msg}; }

// ───────────────────────── مساعدات UTF-8 ─────────────────────────

uint32_t decodeAt(const std::string& s, size_t i, size_t& len) {
  unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) { len = 1; return c; }
  if ((c >> 5) == 6 && i + 1 < s.size()) {
    len = 2;
    return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
  }
  if ((c >> 4) == 14 && i + 2 < s.size()) {
    len = 3;
    return ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
  }
  if ((c >> 3) == 30 && i + 3 < s.size()) {
    len = 4;
    return ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
  }
  len = 1;
  return c;
}

std::vector<std::string> splitChars(const std::string& s) {
  std::vector<std::string> r;
  size_t i = 0;
  while (i < s.size()) {
    size_t len;
    decodeAt(s, i, len);
    r.push_back(s.substr(i, len));
    i += len;
  }
  return r;
}

// قيمة الرقم: ASCII أو عربي-هندي (٠-٩) أو فارسي (۰-۹)
int digitValue(uint32_t cp) {
  if (cp >= '0' && cp <= '9') return static_cast<int>(cp - '0');
  if (cp >= 0x660 && cp <= 0x669) return static_cast<int>(cp - 0x660);
  if (cp >= 0x6F0 && cp <= 0x6F9) return static_cast<int>(cp - 0x6F0);
  return -1;
}

// رموز غير مرئية (علامات الاتجاه ...) تُعامل كمسافات
bool isInvisible(uint32_t cp) {
  return cp == 0xA0 || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x202A && cp <= 0x202E) ||
         (cp >= 0x2066 && cp <= 0x2069) || cp == 0xFEFF;
}

bool isIdentStart(uint32_t cp) {
  if (cp < 0x80) return cp == '_' || std::isalpha(static_cast<int>(cp));
  if (isInvisible(cp) || digitValue(cp) >= 0) return false;
  switch (cp) {
    case 0x60C: case 0x61B: case 0x61F: case 0x66A: case 0x66B: case 0x66C: case 0x66D:
    case 0xD7: case 0xF7: case 0x2212: case 0x2260: case 0x2264: case 0x2265: case 0x2190:
    case 0xAB: case 0xBB: case 0x2018: case 0x2019: case 0x201C: case 0x201D:
      return false;
    default:
      return true;
  }
}

// توحيد الهمزات وحذف التشكيل والتطويل (لمطابقة الكلمات المفتاحية فقط)
std::string normalizeKw(const std::string& s) {
  std::string o;
  size_t i = 0;
  while (i < s.size()) {
    size_t len;
    uint32_t cp = decodeAt(s, i, len);
    if (cp == 0x623 || cp == 0x625 || cp == 0x622) {
      o += "\xD8\xA7";
    } else if (cp == 0x640 || (cp >= 0x64B && cp <= 0x652)) {
      // تطويل أو تشكيل: يُحذف
    } else {
      o.append(s, i, len);
    }
    i += len;
  }
  return o;
}

std::string toAsciiDigits(const std::string& s) {
  std::string o;
  size_t i = 0;
  while (i < s.size()) {
    size_t len;
    uint32_t cp = decodeAt(s, i, len);
    int d = digitValue(cp);
    if (d >= 0) o += static_cast<char>('0' + d);
    else if (cp == 0x66B) o += '.';
    else o.append(s, i, len);
    i += len;
  }
  return o;
}

std::string toArabicDigits(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c >= '0' && c <= '9') {
      o += "\xD9";
      o += static_cast<char>(0xA0 + (c - '0'));
    } else if (c == '.') {
      o += "\xD9\xAB";
    } else {
      o += c;
    }
  }
  return o;
}

// ───────────────────────── المحلل اللفظي ─────────────────────────

enum class TK { Num, Str, Id, Var, Fn, Op, End };
struct Token {
  TK k = TK::End;
  std::string text, norm;
  double num = 0;
  int line = 1;
};

std::vector<Token> lex(const std::string& s) {
  std::vector<Token> out;
  size_t i = 0;
  int line = 1;
  auto push = [&](TK k, const std::string& text, int ln) {
    Token t;
    t.k = k;
    t.text = text;
    t.line = ln;
    out.push_back(t);
  };
  static const char* ops2[] = {"==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "=>"};

  while (i < s.size()) {
    size_t len;
    uint32_t cp = decodeAt(s, i, len);
    if (cp == '\n') { line++; i++; continue; }
    if (cp == ' ' || cp == '\t' || cp == '\r' || isInvisible(cp)) { i += len; continue; }

    // تعليقات: // و # حتى نهاية السطر
    if ((cp == '/' && i + 1 < s.size() && s[i + 1] == '/') || cp == '#') {
      while (i < s.size() && s[i] != '\n') i++;
      continue;
    }
    // تعليق متعدد الأسطر /* ... */
    if (cp == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      int start = line;
      i += 2;
      bool closed = false;
      while (i < s.size()) {
        if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '/') { i += 2; closed = true; break; }
        if (s[i] == '\n') line++;
        i++;
      }
      if (!closed) fail(start, "تعليق غير مغلق (ينقصه */)");
      continue;
    }

    // أرقام
    if (digitValue(cp) >= 0) {
      std::string num;
      int ln = line;
      bool dot = false;
      while (i < s.size()) {
        size_t l2;
        uint32_t c = decodeAt(s, i, l2);
        int d = digitValue(c);
        if (d >= 0) {
          num += static_cast<char>('0' + d);
          i += l2;
        } else if (!dot && (c == '.' || c == 0x66B) && i + l2 < s.size()) {
          size_t l3;
          uint32_t nx = decodeAt(s, i + l2, l3);
          if (digitValue(nx) >= 0) { dot = true; num += '.'; i += l2; }
          else break;
        } else {
          break;
        }
      }
      Token t;
      t.k = TK::Num;
      t.text = num;
      t.num = std::strtod(num.c_str(), nullptr);
      t.line = ln;
      out.push_back(t);
      continue;
    }

    // نصوص: "..." '...' “...” «...»
    uint32_t closeCp = 0;
    if (cp == '"') closeCp = '"';
    else if (cp == '\'') closeCp = '\'';
    else if (cp == 0x201C) closeCp = 0x201D;
    else if (cp == 0x2018) closeCp = 0x2019;
    else if (cp == 0xAB) closeCp = 0xBB;
    if (closeCp) {
      int ln = line;
      i += len;
      std::string val;
      bool closed = false;
      while (i < s.size()) {
        size_t l2;
        uint32_t c = decodeAt(s, i, l2);
        if (c == closeCp) { i += l2; closed = true; break; }
        if (c == '\\' && i + 1 < s.size()) {
          char e = s[i + 1];
          i += 2;
          switch (e) {
            case 'n': val += '\n'; break;
            case 't': val += '\t'; break;
            case 'r': val += '\r'; break;
            case '0': val += '\0'; break;
            case '\\': val += '\\'; break;
            case '"': val += '"'; break;
            case '\'': val += '\''; break;
            default: val += '\\'; val += e;
          }
          continue;
        }
        if (c == '\n') line++;
        val.append(s, i, l2);
        i += l2;
      }
      if (!closed) fail(ln, "نص غير مغلق (ينقصه علامة اقتباس)");
      push(TK::Str, val, ln);
      continue;
    }

    // معرّفات وكلمات مفتاحية
    if (isIdentStart(cp)) {
      int ln = line;
      std::string id;
      while (i < s.size()) {
        size_t l2;
        uint32_t c = decodeAt(s, i, l2);
        if (isIdentStart(c) || digitValue(c) >= 0) { id.append(s, i, l2); i += l2; }
        else break;
      }
      Token t;
      t.k = TK::Id;
      t.text = id;
      t.norm = normalizeKw(id);
      t.line = ln;
      out.push_back(t);
      continue;
    }

    // الفاصلة والفاصلة المنقوطة العربيتان
    if (cp == 0x60C) { push(TK::Op, ",", line); i += len; continue; }
    if (cp == 0x61B) { push(TK::Op, ";", line); i += len; continue; }

    // $مفهوم : الدالة أو الأمر يبدأ بعلامة $
    if (cp == '$') {
      int ln = line;
      size_t j = i + 1;
      std::string id;
      bool first = true, ok = true;
      while (j < s.size()) {
        size_t l2;
        uint32_t c = decodeAt(s, j, l2);
        if (first && !isIdentStart(c)) { ok = false; break; }
        if (isIdentStart(c) || digitValue(c) >= 0) { id.append(s, j, l2); j += l2; first = false; }
        else break;
      }
      if (!ok || id.empty()) fail(ln, "بعد علامة $ يجب كتابة اسم المفهوم مباشرة، مثل $اعرض");
      Token t;
      t.k = TK::Fn;
      t.text = id;
      t.norm = normalizeKw(id);
      t.line = ln;
      out.push_back(t);
      i = j;
      continue;
    }

    // <<متغير>> : المتغير يُكتب بين << >>
    if (cp == '<' && i + 1 < s.size() && s[i + 1] == '<') {
      int ln = line;
      size_t j = i + 2;
      while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) j++;
      std::string id;
      bool first = true, ok = true;
      while (j < s.size()) {
        size_t l2;
        uint32_t c = decodeAt(s, j, l2);
        if (first && !isIdentStart(c)) { ok = false; break; }
        if (isIdentStart(c) || digitValue(c) >= 0) { id.append(s, j, l2); j += l2; first = false; }
        else break;
      }
      while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) j++;
      if (ok && !id.empty() && s.compare(j, 2, ">>") != 0 && j < s.size()) {
        size_t l3;
        uint32_t nx = decodeAt(s, j, l3);
        if (isIdentStart(nx)) {
          std::string word2;
          size_t k = j;
          while (k < s.size()) {
            size_t l4;
            uint32_t c4 = decodeAt(s, k, l4);
            if (isIdentStart(c4) || digitValue(c4) >= 0) { word2.append(s, k, l4); k += l4; }
            else break;
          }
          fail(ln, "اسم المتغير لا يحتوي مسافات: استعمل _ بين الكلمات، مثل <<" + id + "_" + word2 + ">>");
        }
      }
      if (!ok || id.empty() || s.compare(j, 2, ">>") != 0)
        fail(ln, "المتغير يُكتب هكذا: <<اسم>> أو <<اسم_من_كلمتين>> (حروف وأرقام و _ فقط، ويبدأ بحرف)");
      Token t;
      t.k = TK::Var;
      t.text = id;
      t.norm = normalizeKw(id);
      t.line = ln;
      out.push_back(t);
      i = j + 2;
      continue;
    }

    // رموز رياضية يونيكود: × ÷ − ٪ ≠ ≤ ≥ ←   (و ×= ÷= −= للإسناد المركّب)
    {
      const char* mapped = nullptr;
      switch (cp) {
        case 0xD7: mapped = "*"; break;
        case 0xF7: mapped = "/"; break;
        case 0x2212: mapped = "-"; break;
        case 0x66A: case 0x25: mapped = "%"; break;
        case 0x2260: mapped = "!="; break;
        case 0x2264: mapped = "<="; break;
        case 0x2265: mapped = ">="; break;
        case 0x2190: mapped = "="; break;
        case 0x61F: mapped = "?"; break;
        default: break;
      }
      if (mapped) {
        std::string op = mapped;
        size_t adv = len;
        if (op.size() == 1 && op != "=" && i + len < s.size() && s[i + len] == '=' &&
            (cp == 0xD7 || cp == 0xF7 || cp == 0x2212 || cp == 0x66A)) {
          op += '=';
          adv += 1;
        }
        push(TK::Op, op, line);
        i += adv;
        continue;
      }
    }

    // معاملات من حرفين
    bool matched = false;
    if (i + 1 < s.size()) {
      std::string two = s.substr(i, 2);
      for (const char* o : ops2) {
        if (two == o) { push(TK::Op, two, line); i += 2; matched = true; break; }
      }
    }
    if (matched) continue;

    if (cp > 0 && cp < 0x80 && std::strchr("+-*/%<>=()[],;!.:?&|", static_cast<int>(cp))) {
      push(TK::Op, std::string(1, static_cast<char>(cp)), line);
      i++;
      continue;
    }
    fail(line, "رمز غير معروف");
  }
  push(TK::End, "", line);
  return out;
}

// ───────────────────────── الشجرة النحوية ─────────────────────────

struct Expr;
struct Stmt;
struct FuncDef;
using EP = std::shared_ptr<Expr>;
using SP = std::shared_ptr<Stmt>;

// وصف نوع (TypeScript): رقم | نص | قائمة[رقم] | رقم او نص | رقم؟ | أي | اسم صنف
struct TypeSpec;
using TP = std::shared_ptr<TypeSpec>;
struct TypeSpec {
  enum K { Any, Name, ListOf, Union, Opt } k = Any;
  std::string name, norm;
  std::vector<TP> parts;
};

enum class EK { Num, Str, Bool, Null, Var, List, Unary, Binary, And, Or, Assign, Index, Call, Lambda, FnRef, BigLit,
                Member, Deref, AddrOf, Comp };

struct Expr {
  EK k = EK::Null;
  int line = 0;
  double num = 0;
  std::string s;  // نص / اسم / معامل
  std::string key;  // مفتاح البحث المخزّن "$"+اسم للمفاهيم (تسريع)
  bool b = false;
  EP l, r;
  std::vector<EP> items;
  std::shared_ptr<FuncDef> fn;
};

enum class SK { Expr, Var, If, While, For, Block, Func, Return, Break, Continue, Class, Try };

struct Stmt {
  SK k = SK::Expr;
  int line = 0;
  std::string name;
  EP e;
  std::vector<SP> body;
  std::vector<SP> elseBody;
  bool hasElse = false;
  std::shared_ptr<FuncDef> fn;
  TP ty;                 // نوع المتغير المعلن
  std::string parent;    // صنف: اسم الأب
  std::vector<SP> members;
};

struct FuncDef {
  std::string name;
  std::vector<std::string> params;
  std::vector<TP> ptypes;  // نوع كل معامل (nullptr = بلا نوع)
  TP rtype;                // نوع القيمة المرجعة
  std::vector<SP> body;
};

EP mkE(EK k, int line) {
  auto e = std::make_shared<Expr>();
  e->k = k;
  e->line = line;
  return e;
}
SP mkS(SK k, int line) {
  auto s = std::make_shared<Stmt>();
  s->k = k;
  s->line = line;
  return s;
}

bool isReserved(const std::string& norm) {
  static const std::unordered_set<std::string> kw = [] {
    std::unordered_set<std::string> s;
    for (const char* w : {"متغير", "اذا", "لو", "والا", "غير_ذلك", "طالما", "كرر", "لكل", "في",
                          "دالة", "وظيفة", "مفهوم", "ارجع", "توقف", "استمر", "صح", "خطأ", "عدم",
                          "و", "او", "ليس", "يساوي", "من", "لا", "اكبر", "اصغر", "برنامج", "ابدأ",
                          "نهاية", "انتهى", "صنف", "يرث", "حاول", "امسك", "اخيرا"})
      s.insert(normalizeKw(w));
    return s;
  }();
  return kw.count(norm) > 0;
}

// ───────────────────────── المحلل النحوي ─────────────────────────
//
// صيغة EE#:
//   المفهوم (دالة أو أمر) يبدأ بـ $          مثل  $اعرض   $جمع
//   المتغير يُكتب بين << >>                  مثل  <<الاسم>>
//   القيمة نص بين علامتي اقتباس أو رقم       مثل  "مرحبا"   ٣٫٥
//   كل كتلة (لو، كرر، لكل، مفهوم، برنامج) تنتهي بكلمة: نهاية

struct Parser {
  std::vector<Token> t;
  size_t p = 0;
  bool eqSign = true;  // هل = مساواة؟ (خطأ فقط أثناء قراءة هدف الإسناد في بداية الجملة)

  const Token& cur() const { return t[p]; }
  const Token& peek(size_t n = 1) const { return t[std::min(p + n, t.size() - 1)]; }
  void adv() { if (cur().k != TK::End) p++; }
  bool isOp(const char* o) const { return cur().k == TK::Op && cur().text == o; }

  // يقبل عدة كلمات مفصولة بـ | (مرادفات)
  static bool kwMatch(const Token& tk, const char* w) {
    if (tk.k != TK::Id) return false;
    std::string all = w;
    size_t start = 0;
    while (start <= all.size()) {
      size_t bar = all.find('|', start);
      std::string one = all.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
      if (tk.norm == normalizeKw(one)) return true;
      if (bar == std::string::npos) break;
      start = bar + 1;
    }
    return false;
  }
  bool isKw(const char* w) const { return kwMatch(cur(), w); }
  bool nextKw(const char* w) const { return kwMatch(peek(), w); }
  bool matchOp(const char* o) {
    if (isOp(o)) { adv(); return true; }
    return false;
  }

  static std::string describe(const Token& tk) {
    switch (tk.k) {
      case TK::End: return "نهاية البرنامج";
      case TK::Var: return "<<" + tk.text + ">>";
      case TK::Fn: return "$" + tk.text;
      case TK::Str: return "نص";
      default: return "'" + tk.text + "'";
    }
  }

  void expectOp(const char* o) {
    if (!matchOp(o)) fail(cur().line, std::string("متوقع '") + o + "' لكن وُجد " + describe(cur()));
  }

  std::string expectVar(const char* where) {
    if (cur().k != TK::Var)
      fail(cur().line, std::string("متوقع متغير مكتوب هكذا <<اسم>> ") + where + " لكن وُجد " + describe(cur()));
    std::string n = cur().text;
    adv();
    return n;
  }

  // النوع المعتمد للكتلة من كلمة التسمية بعد «نهاية» (nullptr إن لم تكن كلمة كتلة)
  static const char* blockKindOf(const Token& tk) {
    if (kwMatch(tk, "لو|اذا")) return "لو";
    if (kwMatch(tk, "كرر|طالما")) return "كرر";
    if (kwMatch(tk, "لكل")) return "لكل";
    if (kwMatch(tk, "مفهوم|دالة|وظيفة")) return "مفهوم";
    if (kwMatch(tk, "برنامج|ابدأ")) return "برنامج";
    if (kwMatch(tk, "صنف")) return "صنف";
    if (kwMatch(tk, "حاول")) return "حاول";
    return nullptr;
  }

  // نهاية الكتلة: «نهاية» وبعدها تسمية اختيارية تطابق نوع الكتلة:  نهاية لو ، نهاية كرر ...
  // التسمية تُقرأ فقط إن كانت آخر ما في السطر، وإن لم تطابق الكتلة المفتوحة يُبلَّغ بخطأ واضح.
  void expectEnd(const char* kind, int startLine) {
    if (isKw("نهاية|انتهى")) {
      int endLine = cur().line;
      adv();
      const char* label = cur().k == TK::Id && cur().line == endLine ? blockKindOf(cur()) : nullptr;
      bool lastOnLine = peek().k == TK::End || peek().line != endLine || (peek().k == TK::Op && peek().text == ";");
      if (label && lastOnLine) {
        if (std::string(label) != kind)
          fail(endLine, "'نهاية " + cur().text + "' لا تطابق الكتلة '" + kind + "' التي بدأت في السطر " +
                            std::to_string(startLine) + " (المتوقع: نهاية " + kind + ")");
        adv();
      }
      return;
    }
    fail(cur().line, std::string("الكتلة '") + kind + "' التي بدأت في السطر " + std::to_string(startLine) +
                         " لم تُغلق: ينقصها 'نهاية " + kind + "' (وُجد " + describe(cur()) + ")");
  }

  // بعد كل جملة بسيطة: سطر جديد، أو فاصل ؛ ، أو نهاية كتلة
  void endStatement() {
    if (cur().k == TK::End || isOp(";") || cur().line != t[p - 1].line ||
        isKw("نهاية|انتهى|والا|غير_ذلك|امسك|اخيرا")) {
      matchOp(";");
      return;
    }
    fail(cur().line, "رمز غير متوقع " + describe(cur()) +
                         " بعد نهاية الجملة (اكتب كل جملة في سطر، أو افصل بينها بـ ؛)");
  }

  std::vector<SP> parseBody(const char* stops) {
    std::vector<SP> v;
    while (cur().k != TK::End && !isKw(stops)) v.push_back(parseStatement());
    return v;
  }

  // ── الأنواع (TypeScript): رقم | نص | قائمة[رقم] | رقم او نص | رقم؟ | أي | اسم_صنف ──
  TP parseTypeAtom() {
    if (cur().k != TK::Id && cur().k != TK::Fn)
      fail(cur().line, "متوقع اسم نوع مثل رقم أو نص أو $صنف بعد ':' لكن وُجد " + describe(cur()));
    auto t = std::make_shared<TypeSpec>();
    t->k = TypeSpec::Name;
    t->name = cur().text;  // اسم الصنف يُقبل بـ $ أو بدونها
    t->norm = cur().k == TK::Fn ? normalizeKw(cur().text) : cur().norm;
    adv();
    if (t->norm == normalizeKw("أي")) t->k = TypeSpec::Any;
    if (isOp("[") && t->norm == normalizeKw("قائمة")) {
      adv();
      auto l = std::make_shared<TypeSpec>();
      l->k = TypeSpec::ListOf;
      l->parts.push_back(parseType());
      expectOp("]");
      t = l;
    }
    while (isOp("?")) {
      adv();
      auto o = std::make_shared<TypeSpec>();
      o->k = TypeSpec::Opt;
      o->parts.push_back(t);
      t = o;
    }
    return t;
  }
  TP parseType() {
    TP t = parseTypeAtom();
    if (isKw("او") || isOp("|")) {
      auto u = std::make_shared<TypeSpec>();
      u->k = TypeSpec::Union;
      u->parts.push_back(t);
      while (isKw("او") || isOp("|")) {
        adv();
        u->parts.push_back(parseTypeAtom());
      }
      t = u;
    }
    return t;
  }

  // معاملات المفهوم: (<<أ>>: رقم, <<ب>>): رقم   أو بلا أقواس على نفس السطر:  <<أ>>, <<ب>>
  void parseParams(int line, FuncDef& fd) {
    auto one = [&]() {
      fd.params.push_back(expectVar("كمعامل"));
      if (matchOp(":")) fd.ptypes.push_back(parseType());
      else fd.ptypes.push_back(nullptr);
    };
    if (matchOp("(")) {
      while (!isOp(")") && cur().k != TK::End) {
        one();
        if (!matchOp(",")) break;
      }
      expectOp(")");
      if (matchOp(":")) fd.rtype = parseType();
    } else {
      while (cur().k == TK::Var && cur().line == line) {
        one();
        if (!matchOp(",")) break;
      }
    }
  }

  // غلاف البرنامج الاختياري:  برنامج الاسم ... نهاية   (أو ابدأ ... انتهى)
  std::vector<SP> parseProgram() {
    std::vector<SP> v;
    if (isKw("برنامج|ابدأ")) {
      int line = cur().line;
      adv();
      while (cur().k != TK::End && cur().line == line) adv();  // عنوان البرنامج (اختياري)
      v = parseBody("نهاية|انتهى");
      expectEnd("برنامج", line);
      while (matchOp(";")) {}
      if (cur().k != TK::End) fail(cur().line, "لا يمكن كتابة كود بعد نهاية البرنامج");
      return v;
    }
    while (cur().k != TK::End) v.push_back(parseStatement());
    return v;
  }

  bool startsExpr(const Token& tk) const {
    switch (tk.k) {
      case TK::Num: case TK::Str: case TK::Var: case TK::Fn: return true;
      case TK::Id: return kwMatch(tk, "صح|خطأ|عدم|ليس|مفهوم|دالة|وظيفة") || !isReserved(tk.norm);
      case TK::Op: return tk.text == "[" || tk.text == "-" || tk.text == "!" || tk.text == "*" || tk.text == "&";
      default: return false;
    }
  }

  void parseIfRest(const SP& s, int line) {
    s->body = parseBody("والا|غير_ذلك|نهاية|انتهى");
    if (isKw("والا|غير_ذلك")) {
      int elseLine = cur().line;
      adv();
      s->hasElse = true;
      if (isKw("اذا|لو") && cur().line == elseLine) {  // والا لو ... : تتابع شروط بنهاية واحدة
        int l2 = cur().line;
        adv();
        SP nested = mkS(SK::If, l2);
        nested->e = parseExpr();
        parseIfRest(nested, line);
        s->elseBody.push_back(nested);
      } else {
        s->elseBody = parseBody("نهاية|انتهى");
        expectEnd("لو", line);
      }
    } else {
      expectEnd("لو", line);
    }
  }

  SP parseStatement() {
    int line = cur().line;
    if (isOp(";")) { adv(); return mkS(SK::Block, line); }

    if (isKw("برنامج|ابدأ")) fail(line, "'برنامج' يجب أن تكون في أول الملف فقط");

    if (isKw("متغير")) {
      adv();
      auto s = mkS(SK::Var, line);
      s->name = expectVar("بعد 'متغير'");
      if (matchOp(":")) s->ty = parseType();
      if (matchOp("=")) s->e = parseExpr();
      else s->e = mkE(EK::Null, line);
      endStatement();
      return s;
    }
    if (isKw("اذا|لو")) {
      adv();
      auto s = mkS(SK::If, line);
      s->e = parseExpr();
      parseIfRest(s, line);
      return s;
    }
    if (isKw("طالما|كرر")) {
      adv();
      auto s = mkS(SK::While, line);
      s->e = parseExpr();
      s->body = parseBody("نهاية|انتهى");
      expectEnd("كرر", line);
      return s;
    }
    if (isKw("لكل")) {
      adv();
      auto s = mkS(SK::For, line);
      s->name = expectVar("بعد 'لكل'");
      if (!isKw("في")) fail(cur().line, "متوقع 'في' في حلقة لكل لكن وُجد " + describe(cur()));
      adv();
      s->e = parseExpr();
      s->body = parseBody("نهاية|انتهى");
      expectEnd("لكل", line);
      return s;
    }
    if (isKw("مفهوم|دالة|وظيفة") && peek().k == TK::Fn) {
      adv();
      auto s = mkS(SK::Func, line);
      auto fd = std::make_shared<FuncDef>();
      fd->name = "$" + cur().text;
      adv();
      parseParams(line, *fd);
      fd->body = parseBody("نهاية|انتهى");
      expectEnd("مفهوم", line);
      s->name = fd->name;
      s->fn = fd;
      return s;
    }
    if (isKw("صنف") && peek().k == TK::Fn) {
      adv();
      auto s = mkS(SK::Class, line);
      s->name = "$" + cur().text;
      adv();
      if (isKw("يرث")) {
        adv();
        if (cur().k != TK::Fn) fail(cur().line, "متوقع اسم الصنف الأب مثل $حيوان بعد 'يرث' لكن وُجد " + describe(cur()));
        s->parent = "$" + cur().text;
        adv();
      }
      while (cur().k != TK::End && !isKw("نهاية|انتهى")) {
        if ((isKw("مفهوم|دالة|وظيفة") && peek().k == TK::Fn) || isKw("متغير")) s->members.push_back(parseStatement());
        else if (isOp(";")) adv();
        else fail(cur().line, "داخل الصنف يُسمح بتعريف المفاهيم ('مفهوم $اسم ...') والحقول ('متغير <<اسم>> = قيمة') فقط، لكن وُجد " + describe(cur()));
      }
      expectEnd("صنف", line);
      return s;
    }
    if (isKw("حاول")) {
      adv();
      auto s = mkS(SK::Try, line);
      matchOp(";");
      s->body = parseBody("امسك|اخيرا|نهاية|انتهى");
      if (isKw("امسك")) {
        adv();
        s->hasElse = true;
        if (cur().k == TK::Var && cur().line == t[p - 1].line) s->name = expectVar("بعد 'امسك'");
        s->elseBody = parseBody("اخيرا|نهاية|انتهى");
      }
      if (isKw("اخيرا")) {
        adv();
        s->members = parseBody("نهاية|انتهى");
        s->ty = std::make_shared<TypeSpec>();  // علامة: يوجد اخيرا
      }
      if (!s->hasElse && !s->ty) fail(line, "'حاول' تحتاج 'امسك' أو 'اخيرا'");
      expectEnd("حاول", line);
      return s;
    }
    if (isKw("ارجع")) {
      adv();
      auto s = mkS(SK::Return, line);
      if (cur().k == TK::End || isOp(";") || cur().line != line || isKw("نهاية|انتهى|والا|غير_ذلك|امسك|اخيرا"))
        s->e = nullptr;
      else
        s->e = parseExpr();
      endStatement();
      return s;
    }
    if (isKw("توقف")) { adv(); endStatement(); return mkS(SK::Break, line); }
    if (isKw("استمر")) { adv(); endStatement(); return mkS(SK::Continue, line); }

    // أمر بصيغة مبسطة:  $اعرض "نص", <<س>>   (بلا أقواس)
    if (cur().k == TK::Fn) {
      bool callForm = peek().k == TK::Op && peek().text == "(" && peek().line == cur().line;
      if (!callForm) {
        EP c = mkE(EK::Call, line);
        EP f = mkE(EK::FnRef, line);
        f->s = cur().text;
        c->l = f;
        adv();
        if (cur().line == line && startsExpr(cur())) {
          c->items.push_back(parseExpr());
          while (matchOp(",")) c->items.push_back(parseExpr());
        }
        auto s = mkS(SK::Expr, line);
        s->e = c;
        endStatement();
        return s;
      }
    }

    auto s = mkS(SK::Expr, line);
    s->e = parseSimple();
    endStatement();
    return s;
  }

  // ── التعابير ──
  // داخل أي تعبير (شرط، وسيط، قيمة) العلامة = تعني المساواة:  لو <<س>> = 5
  EP parseExpr() {
    bool saved = eqSign;
    eqSign = true;
    EP e = parseOr();
    eqSign = saved;
    return e;
  }

  // في بداية الجملة: <<س>> = قيمة  إسناد، (ما بعد = تعبير عادي، فتعني = فيه مساواة)
  EP parseSimple() {
    bool saved = eqSign;
    eqSign = false;
    EP left = parseOr();
    eqSign = saved;
    static const char* aops[] = {"=", "+=", "-=", "*=", "/=", "%="};
    for (const char* o : aops) {
      if (isOp(o)) {
        bool okTarget = left->k == EK::Var || left->k == EK::Index || left->k == EK::Deref ||
                        (left->k == EK::Member && !left->b);
        if (!okTarget)
          fail(cur().line, "لا يمكن الإسناد إلى هذا التعبير: الإسناد يكون لمتغير مثل <<س>> أو لعنصر قائمة <<ق>>[0] أو لحقل <<ك>>.<<اسم>> أو عبر مؤشر *<<م>>");
        int line = cur().line;
        adv();
        EP right = parseExpr();
        EP a = mkE(EK::Assign, line);
        a->s = o;
        a->l = left;
        a->r = right;
        return a;
      }
    }
    return left;
  }

  EP bin(EK k, const std::string& op, EP l, EP r, int line) {
    EP e = mkE(k, line);
    e->s = op;
    e->l = std::move(l);
    e->r = std::move(r);
    return e;
  }

  EP parseOr() {
    EP l = parseAnd();
    while (isKw("او") || isOp("||")) {
      int line = cur().line;
      adv();
      l = bin(EK::Or, "or", l, parseAnd(), line);
    }
    return l;
  }
  EP parseAnd() {
    EP l = parseNot();
    while (isKw("و") || isOp("&&")) {
      int line = cur().line;
      adv();
      l = bin(EK::And, "and", l, parseNot(), line);
    }
    return l;
  }

  // ليس: أقل أولوية من المقارنة، فـ «ليس س يساوي 3» تعني «ليس (س يساوي 3)»
  EP parseNot() {
    if (isKw("ليس")) {
      int line = cur().line;
      adv();
      EP e = mkE(EK::Unary, line);
      e->s = "!";
      e->l = parseNot();
      return e;
    }
    return parseEquality();
  }

  // المساواة: =  ==  !=  أو بالكلمات: لا يساوي (و يساوي مقبولة أيضاً)
  EP parseEquality() {
    EP l = parseComparison();
    for (;;) {
      int line = cur().line;
      std::string op;
      if (isOp("==")) { op = "=="; adv(); }
      else if (isOp("!=")) { op = "!="; adv(); }
      else if (isOp("=") && eqSign) { op = "=="; adv(); }
      else if (isKw("يساوي")) { op = "=="; adv(); }
      else if (isKw("لا") && nextKw("يساوي")) { op = "!="; adv(); adv(); }
      else if (isKw("لا") && peek().k == TK::Op && peek().text == "=" && eqSign) { op = "!="; adv(); adv(); }
      else break;
      l = bin(EK::Binary, op, l, parseComparison(), line);
    }
    return l;
  }

  // المقارنة: < > <= >= أو بالكلمات: اكبر من، اصغر من، اكبر من او يساوي، اصغر من او يساوي
  EP parseComparison() {
    EP l = parseAdditive();
    for (;;) {
      int line = cur().line;
      std::string op;
      if (isOp("<") || isOp(">") || isOp("<=") || isOp(">=")) {
        op = cur().text;
        adv();
      } else if ((isKw("اكبر") || isKw("اصغر")) && nextKw("من")) {
        bool greater = isKw("اكبر");
        adv();
        adv();
        bool orEq = false;
        if (isKw("او") && nextKw("يساوي")) { adv(); adv(); orEq = true; }
        op = greater ? (orEq ? ">=" : ">") : (orEq ? "<=" : "<");
      } else {
        break;
      }
      l = bin(EK::Binary, op, l, parseAdditive(), line);
    }
    return l;
  }
  EP parseAdditive() {
    EP l = parseMul();
    while ((isOp("+") || isOp("-")) && cur().line == t[p - 1].line) {
      std::string op = cur().text;
      int line = cur().line;
      adv();
      l = bin(EK::Binary, op, l, parseMul(), line);
    }
    return l;
  }
  EP parseMul() {
    EP l = parseUnary();
    while ((isOp("*") || isOp("/") || isOp("%")) && cur().line == t[p - 1].line) {
      std::string op = cur().text;
      int line = cur().line;
      adv();
      l = bin(EK::Binary, op, l, parseUnary(), line);
    }
    return l;
  }
  EP parseUnary() {
    int line = cur().line;
    if (isOp("-")) {
      adv();
      EP e = mkE(EK::Unary, line);
      e->s = "-";
      e->l = parseUnary();
      return e;
    }
    if (isOp("!")) {
      adv();
      EP e = mkE(EK::Unary, line);
      e->s = "!";
      e->l = parseUnary();
      return e;
    }
    if (isOp("*")) {  // فكّ المؤشر (C):  *<<م>>
      adv();
      EP e = mkE(EK::Deref, line);
      e->l = parseUnary();
      return e;
    }
    if (isOp("&")) {  // عنوان (C):  &<<س>>  &<<ق>>[2]  &<<ك>>.<<حقل>>
      adv();
      EP e = mkE(EK::AddrOf, line);
      e->l = parseUnary();
      if (e->l->k != EK::Var && e->l->k != EK::Index && !(e->l->k == EK::Member && !e->l->b))
        fail(line, "العلامة & تأخذ عنوان متغير <<س>> أو عنصر <<ق>>[0] أو حقل <<ك>>.<<اسم>>");
      return e;
    }
    return parsePostfix();
  }
  EP parsePostfix() {
    EP e = parsePrimary();
    for (;;) {
      int line = cur().line;
      bool sameLine = line == t[p - 1].line;  // لا يمتد الاستدعاء أو الفهرسة إلى سطر جديد
      if (isOp("(") && sameLine) {
        adv();
        EP c = mkE(EK::Call, line);
        c->l = e;
        while (!isOp(")") && cur().k != TK::End) {
          c->items.push_back(parseExpr());
          if (!matchOp(",")) break;
        }
        expectOp(")");
        e = c;
      } else if (isOp(".") && sameLine) {
        adv();
        EP m = mkE(EK::Member, line);
        m->l = e;
        if (cur().k == TK::Var) { m->s = cur().text; m->b = false; adv(); }
        else if (cur().k == TK::Fn) { m->s = "$" + cur().text; m->b = true; adv(); }
        else fail(cur().line, "بعد النقطة يُكتب اسم حقل <<اسم>> أو مفهوم $اسم لكن وُجد " + describe(cur()));
        e = m;
      } else if (isOp("[") && sameLine) {
        adv();
        EP ix = mkE(EK::Index, line);
        ix->l = e;
        ix->r = parseExpr();
        expectOp("]");
        e = ix;
      } else {
        break;
      }
    }
    return e;
  }
  // هل نحن عند ( <<أ>>, <<ب>> ) => ؟
  bool isArrowParams() const {
    size_t q = p + 1;
    while (q < t.size() && t[q].k == TK::Var) {
      q++;
      if (q < t.size() && t[q].k == TK::Op && t[q].text == ",") q++;
      else break;
    }
    return q + 1 < t.size() && t[q].k == TK::Op && t[q].text == ")" && t[q + 1].k == TK::Op && t[q + 1].text == "=>";
  }
  EP arrowBody(const std::shared_ptr<FuncDef>& fd, int line) {
    expectOp("=>");
    EP e = mkE(EK::Lambda, line);
    SP r = mkS(SK::Return, line);
    r->e = parseExpr();
    fd->body.push_back(r);
    e->fn = fd;
    return e;
  }
  // بايثون: [ع لكل <<س>> في ق لو شرط لكل <<ص>> في ...]
  EP parseComprehension(EP elem, int line) {
    struct Clause { std::string var; EP iter, cond; };
    std::vector<Clause> cl;
    while (isKw("لكل")) {
      adv();
      Clause c;
      c.var = expectVar("بعد 'لكل'");
      if (!isKw("في")) fail(cur().line, "متوقع 'في' في التوليد لكن وُجد " + describe(cur()));
      adv();
      c.iter = parseExpr();
      if (isKw("اذا|لو")) { adv(); c.cond = parseExpr(); }
      cl.push_back(c);
    }
    expectOp("]");
    EP inner = elem;
    for (size_t i = cl.size(); i-- > 0;) {
      EP c = mkE(EK::Comp, line);
      c->s = cl[i].var;
      c->l = inner;
      c->r = cl[i].iter;
      if (cl[i].cond) c->items.push_back(cl[i].cond);
      c->b = i + 1 != cl.size();  // الطبقة الخارجية تسطّح نتائج الداخلية
      inner = c;
    }
    return inner;
  }
  EP parsePrimary() {
    const Token& tk = cur();
    int line = tk.line;
    switch (tk.k) {
      case TK::Num: {
        bool isInt = tk.text.find('.') == std::string::npos;
        EP e = mkE(isInt && tk.text.size() > 15 ? EK::BigLit : EK::Num, line);
        e->num = tk.num;
        e->s = tk.text;
        adv();
        return e;
      }
      case TK::Str: {
        EP e = mkE(EK::Str, line);
        e->s = tk.text;
        adv();
        return e;
      }
      case TK::Var: {
        if (peek().k == TK::Op && peek().text == "=>") {  // <<س>> => تعبير   (دالة سهمية كـ JS)
          auto fd = std::make_shared<FuncDef>();
          fd->params.push_back(tk.text);
          fd->ptypes.push_back(nullptr);
          adv();
          return arrowBody(fd, line);
        }
        EP e = mkE(EK::Var, line);
        e->s = tk.text;
        adv();
        return e;
      }
      case TK::Fn: {
        EP e = mkE(EK::FnRef, line);
        e->s = tk.text;
        adv();
        return e;
      }
      case TK::Id: {
        if (isKw("صح")) { adv(); EP e = mkE(EK::Bool, line); e->b = true; return e; }
        if (isKw("خطأ")) { adv(); EP e = mkE(EK::Bool, line); e->b = false; return e; }
        if (isKw("عدم")) { adv(); return mkE(EK::Null, line); }
        if (isKw("مفهوم|دالة|وظيفة")) {  // مفهوم مجهول:  مفهوم(<<س>>) ... نهاية
          adv();
          EP e = mkE(EK::Lambda, line);
          auto fd = std::make_shared<FuncDef>();
          fd->name = "";
          parseParams(line, *fd);
          fd->body = parseBody("نهاية|انتهى");
          expectEnd("مفهوم", line);
          e->fn = fd;
          return e;
        }
        if (isReserved(tk.norm)) fail(line, "الكلمة '" + tk.text + "' في غير موضعها");
        fail(line, "الكلمة '" + tk.text + "' غير مفهومة: المتغير يُكتب <<" + tk.text +
                       ">> والمفهوم يُكتب $" + tk.text + " والنص يُكتب بين علامتي اقتباس");
      }
      case TK::Op: {
        if (isOp("(") && isArrowParams()) {  // (<<أ>>, <<ب>>) => تعبير
          auto fd = std::make_shared<FuncDef>();
          adv();
          while (cur().k == TK::Var) {
            fd->params.push_back(cur().text);
            fd->ptypes.push_back(nullptr);
            adv();
            if (!matchOp(",")) break;
          }
          expectOp(")");
          return arrowBody(fd, line);
        }
        if (isOp("(")) {
          adv();
          EP e = parseExpr();
          expectOp(")");
          return e;
        }
        if (isOp("[")) {
          adv();
          EP e = mkE(EK::List, line);
          if (!isOp("]")) {
            EP first = parseExpr();
            if (isKw("لكل")) return parseComprehension(first, line);  // [تعبير لكل <<س>> في ق لو شرط]
            e->items.push_back(first);
            while (matchOp(",")) {
              if (isOp("]")) break;
              e->items.push_back(parseExpr());
            }
          }
          expectOp("]");
          return e;
        }
        break;
      }
      default:
        break;
    }
    fail(line, "تعبير غير متوقع: " + describe(tk));
  }
};

// ───────────────────────── الأعداد الكبيرة (BigInt) ─────────────────────────
//
// عدد صحيح بلا حدّ: إشارة + أجزاء بالأساس 10^9 (الأصغر أولاً، والصفر = قائمة فارغة).
// يدعم + - * وقسمة طويلة (قاطعة نحو الصفر كـ C++)، وأسّاً سريعاً، وأسّاً بالباقي،
// واختبار أولية Miller–Rabin. هذا الصنف هو الجزء "الصعب" الذي يختصره EE# في أسطر قليلة.

struct BigInt {
  static constexpr uint32_t BASE = 1000000000u;
  static constexpr size_t MAX_LIMBS = 120000;  // ≈ مليون خانة عشرية
  bool neg = false;
  std::vector<uint32_t> d;

  bool zero() const { return d.empty(); }
  void trim() {
    while (!d.empty() && d.back() == 0) d.pop_back();
    if (d.empty()) neg = false;
  }
  static void checkSize(const BigInt& r) {
    if (r.d.size() > MAX_LIMBS) throw EEError{0, "النتيجة كبيرة جداً (أكثر من مليون خانة)"};
  }
  static void checkStop() {
    if (g_stop.load()) throw EEError{0, "تم إيقاف البرنامج"};
  }

  static BigInt fromInt(long long v) {
    BigInt r;
    unsigned long long u;
    if (v < 0) {
      r.neg = true;
      u = 0ULL - static_cast<unsigned long long>(v);
    } else {
      u = static_cast<unsigned long long>(v);
    }
    while (u) {
      r.d.push_back(static_cast<uint32_t>(u % BASE));
      u /= BASE;
    }
    r.trim();
    return r;
  }

  // يقبل: [+-] أرقام (عربية أو لاتينية) مع مسافات حول الرقم
  static bool parse(const std::string& in, BigInt& out) {
    std::string s = toAsciiDigits(in);
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    size_t b = 0, e = s.size();
    while (b < e && ws(s[b])) b++;
    while (e > b && ws(s[e - 1])) e--;
    bool negative = false;
    if (b < e && (s[b] == '-' || s[b] == '+')) {
      negative = s[b] == '-';
      b++;
    }
    if (b >= e) return false;
    for (size_t i = b; i < e; i++)
      if (s[i] < '0' || s[i] > '9') return false;
    out = BigInt();
    for (size_t hi = e; hi > b;) {
      size_t lo = hi >= b + 9 ? hi - 9 : b;
      uint32_t limb = 0;
      for (size_t i = lo; i < hi; i++) limb = limb * 10 + static_cast<uint32_t>(s[i] - '0');
      out.d.push_back(limb);
      hi = lo;
    }
    out.neg = negative;
    out.trim();
    checkSize(out);
    return true;
  }

  std::string str() const {
    if (d.empty()) return "0";
    std::string s = neg ? "-" : "";
    s += std::to_string(d.back());
    char buf[16];
    for (size_t i = d.size() - 1; i-- > 0;) {
      std::snprintf(buf, sizeof buf, "%09u", d[i]);
      s += buf;
    }
    return s;
  }

  double toDouble() const {
    double r = 0;
    for (size_t i = d.size(); i-- > 0;) r = r * BASE + d[i];
    return neg ? -r : r;
  }

  static int cmpAbs(const BigInt& a, const BigInt& b) {
    if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
    for (size_t i = a.d.size(); i-- > 0;)
      if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
    return 0;
  }
  static int cmp(const BigInt& a, const BigInt& b) {
    if (a.neg != b.neg) return a.neg ? -1 : 1;
    int c = cmpAbs(a, b);
    return a.neg ? -c : c;
  }

  static BigInt addAbs(const BigInt& a, const BigInt& b) {
    BigInt r;
    uint64_t carry = 0;
    size_t n = std::max(a.d.size(), b.d.size());
    for (size_t i = 0; i < n || carry; i++) {
      uint64_t cur = carry;
      if (i < a.d.size()) cur += a.d[i];
      if (i < b.d.size()) cur += b.d[i];
      r.d.push_back(static_cast<uint32_t>(cur % BASE));
      carry = cur / BASE;
    }
    return r;
  }
  // |a| >= |b|
  static BigInt subAbs(const BigInt& a, const BigInt& b) {
    BigInt r;
    int64_t borrow = 0;
    for (size_t i = 0; i < a.d.size(); i++) {
      int64_t cur = static_cast<int64_t>(a.d[i]) - borrow - (i < b.d.size() ? static_cast<int64_t>(b.d[i]) : 0);
      borrow = cur < 0 ? 1 : 0;
      if (cur < 0) cur += BASE;
      r.d.push_back(static_cast<uint32_t>(cur));
    }
    r.trim();
    return r;
  }

  static BigInt add(const BigInt& a, const BigInt& b) {
    BigInt r;
    if (a.neg == b.neg) {
      r = addAbs(a, b);
      r.neg = a.neg;
    } else if (cmpAbs(a, b) >= 0) {
      r = subAbs(a, b);
      r.neg = a.neg;
    } else {
      r = subAbs(b, a);
      r.neg = b.neg;
    }
    r.trim();
    checkSize(r);
    return r;
  }
  static BigInt sub(const BigInt& a, const BigInt& b) {
    BigInt nb = b;
    if (!nb.zero()) nb.neg = !nb.neg;
    return add(a, nb);
  }

  // |a| * m  حيث m <= BASE
  static BigInt mulSmall(const BigInt& a, uint32_t m) {
    BigInt r;
    uint64_t carry = 0;
    for (size_t i = 0; i < a.d.size() || carry; i++) {
      uint64_t cur = carry + (i < a.d.size() ? static_cast<uint64_t>(a.d[i]) * m : 0);
      r.d.push_back(static_cast<uint32_t>(cur % BASE));
      carry = cur / BASE;
    }
    r.trim();
    return r;
  }

  static BigInt mul(const BigInt& a, const BigInt& b) {
    if (a.zero() || b.zero()) return BigInt();
    BigInt r;
    if (b.d.size() == 1 || a.d.size() == 1) {
      r = b.d.size() == 1 ? mulSmall(a, b.d[0]) : mulSmall(b, a.d[0]);
    } else {
      r.d.assign(a.d.size() + b.d.size(), 0);
      for (size_t i = 0; i < a.d.size(); i++) {
        checkStop();
        uint64_t carry = 0;
        for (size_t j = 0; j < b.d.size(); j++) {
          uint64_t cur = r.d[i + j] + static_cast<uint64_t>(a.d[i]) * b.d[j] + carry;
          r.d[i + j] = static_cast<uint32_t>(cur % BASE);
          carry = cur / BASE;
        }
        r.d[i + b.d.size()] = static_cast<uint32_t>(carry);
      }
    }
    r.neg = a.neg != b.neg;
    r.trim();
    checkSize(r);
    return r;
  }

  // يقسم |a| على m (m <= BASE) في المكان، ويعيد الباقي
  static uint32_t divSmall(BigInt& a, uint32_t m) {
    uint64_t rem = 0;
    for (size_t i = a.d.size(); i-- > 0;) {
      uint64_t cur = a.d[i] + rem * BASE;
      a.d[i] = static_cast<uint32_t>(cur / m);
      rem = cur % m;
    }
    a.trim();
    return static_cast<uint32_t>(rem);
  }

  // قسمة طويلة: a = q*b + r  (q قاطع نحو الصفر، وإشارة r كإشارة a). b != 0
  static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
    BigInt qq, rr;
    if (cmpAbs(a, b) < 0) {
      rr = a;
    } else if (b.d.size() == 1) {
      qq = a;
      qq.neg = false;
      uint32_t rem = divSmall(qq, b.d[0]);
      rr = fromInt(rem);
      rr.neg = a.neg && rem != 0;
    } else {
      BigInt babs = b;
      babs.neg = false;
      BigInt rem;
      qq.d.assign(a.d.size(), 0);
      for (size_t i = a.d.size(); i-- > 0;) {
        checkStop();
        rem.d.insert(rem.d.begin(), a.d[i]);  // rem = rem*BASE + limb
        rem.trim();
        uint32_t lo = 0, hi = BASE - 1;       // أكبر رقم خارج قسمة لا يتجاوز rem
        while (lo < hi) {
          uint32_t mid = lo + (hi - lo + 1) / 2;
          if (cmpAbs(mulSmall(babs, mid), rem) <= 0) lo = mid;
          else hi = mid - 1;
        }
        if (lo) rem = subAbs(rem, mulSmall(babs, lo));
        qq.d[i] = lo;
      }
      rr = rem;
      rr.neg = a.neg && !rr.zero();
    }
    qq.neg = a.neg != b.neg;
    qq.trim();
    rr.trim();
    q = qq;
    r = rr;
  }

  static BigInt pow(BigInt base, unsigned long long e) {
    BigInt r = fromInt(1);
    while (e) {
      checkStop();
      if (e & 1) r = mul(r, base);
      e >>= 1;
      if (e) base = mul(base, base);
    }
    return r;
  }

  // base^e mod m  (بتربيع متكرر؛ لا ينتج أعداداً أكبر من m^2 أبداً)
  static BigInt modpow(BigInt base, BigInt e, BigInt m) {
    m.neg = false;
    if (m.zero()) throw EEError{0, "الباقي لا يمكن أن يكون صفراً"};
    if (e.neg) throw EEError{0, "الأس يجب أن يكون غير سالب"};
    BigInt q, r, one = fromInt(1), result;
    divmod(one, m, q, result);
    BigInt tmp;
    divmod(base, m, q, tmp);
    base = tmp.neg ? add(tmp, m) : tmp;
    while (!e.zero()) {
      checkStop();
      if (e.d[0] & 1u) {
        divmod(mul(result, base), m, q, r);
        result = r;
      }
      divSmall(e, 2);
      if (!e.zero()) {
        divmod(mul(base, base), m, q, r);
        base = r;
      }
    }
    return result;
  }

  // Miller–Rabin بالأسس 2..37: حتمي لكل n < 3.3×10^24، ومحتمل بيقين عالٍ جداً بعدها
  static bool isPrime(const BigInt& n) {
    static const unsigned small[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    if (n.neg || n.zero() || cmp(n, fromInt(2)) < 0) return false;
    for (unsigned p : small) {
      if (cmp(n, fromInt(p)) == 0) return true;
      BigInt t = n;
      if (divSmall(t, p) == 0) return false;
    }
    BigInt one = fromInt(1);
    BigInt nm1 = sub(n, one), dd = nm1;
    int s = 0;
    while (!(dd.d[0] & 1u)) {
      divSmall(dd, 2);
      s++;
    }
    for (unsigned a : small) {
      BigInt x = modpow(fromInt(a), dd, n);
      if (cmp(x, one) == 0 || cmp(x, nm1) == 0) continue;
      bool composite = true;
      for (int i = 1; i < s; i++) {
        BigInt q, r;
        divmod(mul(x, x), n, q, r);
        x = r;
        if (cmp(x, nm1) == 0) { composite = false; break; }
        if (cmp(x, one) == 0) break;
      }
      if (composite) return false;
    }
    return true;
  }
};

// ───────────────────────── القيم ─────────────────────────

struct Env;
struct FuncObj;
struct NativeObj;
struct Interp;

enum class VT { Null, Num, Bool, Str, List, Func, Native, Big, Class, Obj, Ptr };

struct ClassObj;
struct ObjectObj;
struct PtrObj;

struct Value {
  VT t = VT::Null;
  double n = 0;
  bool b = false;
  std::string s;
  std::shared_ptr<std::vector<Value>> list;
  std::shared_ptr<FuncObj> fn;
  std::shared_ptr<NativeObj> nat;
  std::shared_ptr<BigInt> big;
  std::shared_ptr<ClassObj> cls;
  std::shared_ptr<ObjectObj> obj;
  std::shared_ptr<PtrObj> ptr;
};

using Args = std::vector<Value>;

struct FuncObj {
  std::shared_ptr<FuncDef> def;
  std::shared_ptr<Env> closure;
};

struct NativeObj {
  std::string name;
  std::function<Value(Interp&, Args&, int)> fn;
};

struct Env {
  std::unordered_map<std::string, Value> vars;
  std::unordered_map<std::string, TP> types;  // أنواع المتغيرات المعلنة (TypeScript)
  std::shared_ptr<Env> parent;
  Value* find(const std::string& n) {
    for (Env* e = this; e; e = e->parent.get()) {
      auto it = e->vars.find(n);
      if (it != e->vars.end()) return &it->second;
    }
    return nullptr;
  }
};

// صنف (C++): حقول افتراضية + مفاهيم + أب واحد
struct ClassObj {
  std::string name;  // مع $
  std::shared_ptr<ClassObj> parent;
  std::shared_ptr<Env> closure;
  std::unordered_map<std::string, Value> methods;
  std::vector<std::pair<std::string, EP>> fields;
};
struct ObjectObj {
  std::shared_ptr<ClassObj> cls;
  std::unordered_map<std::string, Value> fields;
  std::vector<std::string> order;
};
// مؤشر (C): إلى متغير، أو عنصر قائمة، أو حقل كائن
struct PtrObj {
  enum K { Var, Elem, Field } k = Var;
  std::shared_ptr<Env> env;
  std::string name;
  std::shared_ptr<std::vector<Value>> list;
  long long idx = 0;
  std::shared_ptr<ObjectObj> obj;
};

std::shared_ptr<Env> findEnvSP(const std::shared_ptr<Env>& env, const std::string& n) {
  for (auto e = env; e; e = e->parent)
    if (e->vars.count(n)) return e;
  return nullptr;
}

Value mkNum(double d) { Value v; v.t = VT::Num; v.n = d; return v; }
Value mkBool(bool b) { Value v; v.t = VT::Bool; v.b = b; return v; }
Value mkStr(const std::string& s) { Value v; v.t = VT::Str; v.s = s; return v; }
Value mkBig(const BigInt& b) { Value v; v.t = VT::Big; v.big = std::make_shared<BigInt>(b); return v; }
Value mkListP(std::shared_ptr<std::vector<Value>> l) { Value v; v.t = VT::List; v.list = std::move(l); return v; }

std::string typeName(const Value& v) {
  switch (v.t) {
    case VT::Null: return "عدم";
    case VT::Num: return "رقم";
    case VT::Big: return "عدد_كبير";
    case VT::Class: return "صنف";
    case VT::Obj: return v.obj->cls->name.substr(1);
    case VT::Ptr: return "مؤشر";
    case VT::Bool: return "منطقي";
    case VT::Str: return "نص";
    case VT::List: return "قائمة";
    case VT::Func: case VT::Native: return "مفهوم";
  }
  return "؟";
}

bool truthy(const Value& v) {
  switch (v.t) {
    case VT::Null: return false;
    case VT::Num: return v.n != 0;
    case VT::Big: return !v.big->zero();
    case VT::Bool: return v.b;
    case VT::Str: return !v.s.empty();
    case VT::List: return !v.list->empty();
    default: return true;
  }
}

std::string fmtNum(double d) {
  if (std::isnan(d)) return "غير_رقم";
  if (std::isinf(d)) return d > 0 ? "ما_لا_نهاية" : "-ما_لا_نهاية";
  if (d == std::floor(d) && std::fabs(d) < 1e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
    return buf;
  }
  std::ostringstream o;
  o << std::setprecision(14) << d;
  return o.str();
}

std::string toStr(const Value& v, bool quote = false) {
  switch (v.t) {
    case VT::Null: return "عدم";
    case VT::Num: return fmtNum(v.n);
    case VT::Big: return v.big->str();
    case VT::Class: return "<صنف " + v.cls->name + ">";
    case VT::Ptr: return "<مؤشر>";
    case VT::Obj: {
      static int depth = 0;
      if (depth > 6) return v.obj->cls->name.substr(1) + "{...}";
      depth++;
      std::string s = v.obj->cls->name.substr(1) + "{";
      for (size_t i = 0; i < v.obj->order.size(); i++) {
        if (i) s += "، ";
        s += v.obj->order[i] + ": " + toStr(v.obj->fields[v.obj->order[i]], true);
      }
      depth--;
      return s + "}";
    }
    case VT::Bool: return v.b ? "صح" : "خطأ";
    case VT::Str: return quote ? "\"" + v.s + "\"" : v.s;
    case VT::List: {
      static int ldepth = 0;
      if (ldepth > 20) return "[...]";
      struct G { G() { ldepth++; } ~G() { ldepth--; } } guard;
      std::string s = "[";
      for (size_t i = 0; i < v.list->size(); i++) {
        if (i) s += "، ";
        s += toStr((*v.list)[i], true);
      }
      return s + "]";
    }
    case VT::Func: return "<مفهوم " + (v.fn->def->name.empty() ? std::string("مجهول") : v.fn->def->name) + ">";
    case VT::Native: return "<مفهوم مدمج $" + v.nat->name + ">";
  }
  return "";
}

// يحوّل رقماً عادياً صحيحاً إلى BigInt (إن أمكن)
bool numToBig(const Value& v, BigInt& out) {
  if (v.t == VT::Big) { out = *v.big; return true; }
  if (v.t == VT::Num && v.n == std::floor(v.n) && std::fabs(v.n) < 9e18) {
    out = BigInt::fromInt(static_cast<long long>(v.n));
    return true;
  }
  return false;
}

bool equals(const Value& a, const Value& b) {
  if (a.t == VT::Big || b.t == VT::Big) {
    BigInt x, y;
    if (!numToBig(a, x) || !numToBig(b, y)) return false;
    return BigInt::cmp(x, y) == 0;
  }
  if (a.t != b.t) return false;
  switch (a.t) {
    case VT::Null: return true;
    case VT::Num: return a.n == b.n;
    case VT::Bool: return a.b == b.b;
    case VT::Str: return a.s == b.s;
    case VT::List:
      if (a.list == b.list) return true;
      if (a.list->size() != b.list->size()) return false;
      for (size_t i = 0; i < a.list->size(); i++)
        if (!equals((*a.list)[i], (*b.list)[i])) return false;
      return true;
    case VT::Func: return a.fn == b.fn;
    case VT::Native: return a.nat == b.nat;
    case VT::Big: return false;
    case VT::Class: return a.cls == b.cls;
    case VT::Obj: return a.obj == b.obj;
    case VT::Ptr: {
      const PtrObj &x = *a.ptr, &y = *b.ptr;
      if (x.k != y.k) return false;
      if (x.k == PtrObj::Var) return x.env == y.env && x.name == y.name;
      if (x.k == PtrObj::Elem) return x.list == y.list && x.idx == y.idx;
      return x.obj == y.obj && x.name == y.name;
    }
  }
  return false;
}

Value bigBinop(const std::string& op, const Value& a, const Value& b, int line) {
  auto okT = [](const Value& v) { return v.t == VT::Big || v.t == VT::Num; };
  if (!okT(a) || !okT(b)) {
    if (op == "<" || op == ">" || op == "<=" || op == ">=")
      fail(line, "لا يمكن مقارنة " + typeName(a) + " مع " + typeName(b));
    fail(line, "العملية '" + op + "' تتطلب رقمين لكن وُجد " + typeName(a) + " و" + typeName(b));
  }
  BigInt x, y;
  bool cx = numToBig(a, x), cy = numToBig(b, y);
  try {
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
      int c;
      if (cx && cy) c = BigInt::cmp(x, y);
      else {
        double dx = a.t == VT::Big ? a.big->toDouble() : a.n, dy = b.t == VT::Big ? b.big->toDouble() : b.n;
        c = dx < dy ? -1 : (dx > dy ? 1 : 0);
      }
      if (op == "<") return mkBool(c < 0);
      if (op == ">") return mkBool(c > 0);
      if (op == "<=") return mkBool(c <= 0);
      return mkBool(c >= 0);
    }
    if (!cx || !cy)
      fail(line, "لا يمكن خلط عدد كبير مع رقم عشري أو ضخم جداً؛ حوّله أولاً بـ $كبير(...)");
    if (op == "+") return mkBig(BigInt::add(x, y));
    if (op == "-") return mkBig(BigInt::sub(x, y));
    if (op == "*") return mkBig(BigInt::mul(x, y));
    if (op == "/" || op == "%") {
      if (y.zero()) fail(line, "القسمة على صفر");
      BigInt q, r;
      BigInt::divmod(x, y, q, r);
      return mkBig(op == "/" ? q : r);
    }
  } catch (EEError& e) {
    if (e.line == 0) e.line = line;
    throw;
  }
  fail(line, "عملية غير معروفة: " + op);
}

// حساب المؤشرات (C):  م + ن ، م - ن ، م - م ، ومقارنة مؤشري القائمة نفسها
Value ptrBinop(const std::string& op, const Value& a, const Value& b, int line) {
  auto isElem = [](const Value& v) { return v.t == VT::Ptr && v.ptr->k == PtrObj::Elem; };
  auto intOf = [&](const Value& v, long long& out) {
    if (v.t != VT::Num || v.n != std::floor(v.n) || std::fabs(v.n) > 1e12) return false;
    out = static_cast<long long>(v.n);
    return true;
  };
  long long n;
  if ((op == "+" || op == "-") && isElem(a) && intOf(b, n)) {
    Value r = a;
    r.ptr = std::make_shared<PtrObj>(*a.ptr);
    r.ptr->idx += op == "+" ? n : -n;
    return r;
  }
  if (op == "+" && isElem(b) && intOf(a, n)) {
    Value r = b;
    r.ptr = std::make_shared<PtrObj>(*b.ptr);
    r.ptr->idx += n;
    return r;
  }
  if (isElem(a) && isElem(b) && a.ptr->list == b.ptr->list) {
    long long d = a.ptr->idx - b.ptr->idx;
    if (op == "-") return mkNum(static_cast<double>(d));
    if (op == "<") return mkBool(d < 0);
    if (op == ">") return mkBool(d > 0);
    if (op == "<=") return mkBool(d <= 0);
    if (op == ">=") return mkBool(d >= 0);
  }
  fail(line, "العملية '" + op + "' غير مسموحة على المؤشرات: المسموح م + عدد، م - عدد، م - م (في القائمة نفسها)، والمقارنة");
}

Value binop(const std::string& op, const Value& a, const Value& b, int line) {
  if (op == "==") return mkBool(equals(a, b));
  if (op == "!=") return mkBool(!equals(a, b));
  if ((a.t == VT::Ptr || b.t == VT::Ptr) && !((a.t == VT::Str || b.t == VT::Str) && op == "+"))
    return ptrBinop(op, a, b, line);
  if ((a.t == VT::Big || b.t == VT::Big) && a.t != VT::Str && b.t != VT::Str)
    return bigBinop(op, a, b, line);
  if (op == "+") {
    if (a.t == VT::Num && b.t == VT::Num) return mkNum(a.n + b.n);
    if (a.t == VT::Str || b.t == VT::Str) {
      Value r = mkStr(toStr(a) + toStr(b));
      r.b = a.t == VT::Str && b.t == VT::Str && a.b && b.b;  // دمج عنصرين آمنين يبقى آمناً
      return r;
    }
    if (a.t == VT::List && b.t == VT::List) {
      auto l = std::make_shared<std::vector<Value>>(*a.list);
      l->insert(l->end(), b.list->begin(), b.list->end());
      return mkListP(l);
    }
    fail(line, "لا يمكن جمع النوعين: " + typeName(a) + " و" + typeName(b));
  }
  if (op == "*" && ((a.t == VT::Str && b.t == VT::Num) || (a.t == VT::Num && b.t == VT::Str))) {
    const Value& sv = a.t == VT::Str ? a : b;
    double cnt = a.t == VT::Num ? a.n : b.n;
    if (cnt < 0 || cnt != std::floor(cnt)) fail(line, "عدد التكرار يجب أن يكون عدداً صحيحاً غير سالب");
    if (cnt * static_cast<double>(sv.s.size()) > 50e6) fail(line, "النص الناتج كبير جداً");
    std::string r;
    for (long long i = 0; i < static_cast<long long>(cnt); i++) r += sv.s;
    return mkStr(r);
  }
  if (op == "-" || op == "*" || op == "/" || op == "%") {
    if (a.t != VT::Num || b.t != VT::Num)
      fail(line, "العملية '" + op + "' تتطلب رقمين لكن وُجد " + typeName(a) + " و" + typeName(b));
    if (op == "-") return mkNum(a.n - b.n);
    if (op == "*") return mkNum(a.n * b.n);
    if (b.n == 0) fail(line, "القسمة على صفر");
    if (op == "/") return mkNum(a.n / b.n);
    return mkNum(std::fmod(a.n, b.n));
  }
  if (op == "<" || op == ">" || op == "<=" || op == ">=") {
    int c;
    if (a.t == VT::Num && b.t == VT::Num) c = a.n < b.n ? -1 : (a.n > b.n ? 1 : 0);
    else if (a.t == VT::Str && b.t == VT::Str) c = a.s < b.s ? -1 : (a.s > b.s ? 1 : 0);
    else fail(line, "لا يمكن مقارنة " + typeName(a) + " مع " + typeName(b));
    if (op == "<") return mkBool(c < 0);
    if (op == ">") return mkBool(c > 0);
    if (op == "<=") return mkBool(c <= 0);
    return mkBool(c >= 0);
  }
  fail(line, "عملية غير معروفة: " + op);
}

// ───────────────────────── XML ─────────────────────────
//
// محلل XML حقيقي: وسوم متداخلة، سمات، وسوم ذاتية الإغلاق، كيانات (&amp; &#1575; ...)،
// CDATA، تعليقات، تعليمات المعالجة، وأسماء عربية. العقدة = قائمة [وسم، سمات، أبناء، نص]
// حيث السمات قائمة أزواج [اسم، قيمة] والأبناء قائمة عقد.

bool xmlIsNode(const Value& v) {
  return v.t == VT::List && v.list->size() == 4 && (*v.list)[0].t == VT::Str && (*v.list)[1].t == VT::List &&
         (*v.list)[2].t == VT::List && (*v.list)[3].t == VT::Str;
}

Value xmlMakeNode(const std::string& tag, std::vector<Value> attrs, std::vector<Value> kids, const std::string& text) {
  auto at = std::make_shared<std::vector<Value>>(std::move(attrs));
  auto ch = std::make_shared<std::vector<Value>>(std::move(kids));
  auto n = std::make_shared<std::vector<Value>>();
  n->push_back(mkStr(tag));
  n->push_back(mkListP(at));
  n->push_back(mkListP(ch));
  n->push_back(mkStr(text));
  return mkListP(n);
}

struct XmlParser {
  const std::string& s;
  size_t i = 0;
  int line = 1;
  int depth = 0;
  explicit XmlParser(const std::string& src) : s(src) {}

  [[noreturn]] void err(const std::string& m) { fail(0, "XML (السطر " + std::to_string(line) + "): " + m); }
  bool eof() const { return i >= s.size(); }
  bool starts(const char* p) const { return s.compare(i, std::strlen(p), p) == 0; }
  void bump() {
    if (s[i] == '\n') line++;
    i++;
  }
  void skipWs() {
    while (!eof() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) bump();
  }
  void skipPast(const char* end, const char* what) {
    size_t e = s.find(end, i);
    if (e == std::string::npos) err(std::string(what) + " غير مغلق (ينقصه " + end + ")");
    while (i < e + std::strlen(end)) bump();
  }
  static bool nameChar(unsigned char c) {
    return c >= 0x80 || std::isalnum(c) || c == '_' || c == '-' || c == ':' || c == '.';
  }
  std::string name() {
    size_t b = i;
    if (eof() || !(static_cast<unsigned char>(s[i]) >= 0x80 || std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == ':'))
      err("متوقع اسم وسم أو سمة");
    while (!eof() && nameChar(static_cast<unsigned char>(s[i]))) i++;
    return s.substr(b, i - b);
  }
  static void putUtf8(std::string& o, uint32_t cp) {
    if (cp < 0x80) o += static_cast<char>(cp);
    else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
  }
  std::string decode(const std::string& raw) {
    std::string o;
    for (size_t k = 0; k < raw.size(); k++) {
      if (raw[k] != '&') { o += raw[k]; continue; }
      size_t semi = raw.find(';', k);
      if (semi == std::string::npos || semi - k > 10) err("كيان '&' غير مكتمل: اكتبه &amp;");
      std::string e = raw.substr(k + 1, semi - k - 1);
      if (e == "lt") o += '<';
      else if (e == "gt") o += '>';
      else if (e == "amp") o += '&';
      else if (e == "quot") o += '"';
      else if (e == "apos") o += '\'';
      else if (!e.empty() && e[0] == '#') {
        char* end = nullptr;
        unsigned long v = (e.size() > 1 && (e[1] == 'x' || e[1] == 'X')) ? std::strtoul(e.c_str() + 2, &end, 16)
                                                                         : std::strtoul(e.c_str() + 1, &end, 10);
        if (e.size() < 2 || *end != '\0' || v == 0 || v > 0x10FFFF) err("كيان رقمي غير صالح: &" + e + ";");
        putUtf8(o, static_cast<uint32_t>(v));
      } else err("كيان غير معروف: &" + e + ";");
      k = semi;
    }
    return o;
  }
  static std::string trim(const std::string& t) {
    size_t b = 0, e = t.size();
    while (b < e && std::isspace(static_cast<unsigned char>(t[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(t[e - 1]))) e--;
    return t.substr(b, e - b);
  }
  void misc() {  // تعليقات وتعليمات ومسافات خارج العناصر
    for (;;) {
      skipWs();
      if (starts("<!--")) skipPast("-->", "تعليق");
      else if (starts("<?")) skipPast("?>", "تعليمة معالجة");
      else if (starts("<!DOCTYPE") || starts("<!doctype")) skipPast(">", "DOCTYPE");
      else break;
    }
  }

  Value element() {
    if (++depth > 200) err("التداخل عميق جداً (أكثر من 200 مستوى)");
    int startLine = line;
    if (eof() || s[i] != '<') err("متوقع '<' لبدء وسم");
    i++;
    std::string tag = name();
    std::vector<Value> attrs;
    std::vector<std::string> seen;
    for (;;) {
      skipWs();
      if (eof()) err("الوسم <" + tag + "> الذي بدأ في السطر " + std::to_string(startLine) + " لم يُغلق");
      if (starts("/>")) { i += 2; depth--; return xmlMakeNode(tag, attrs, {}, ""); }
      if (s[i] == '>') { i++; break; }
      std::string an = name();
      for (auto& x : seen) if (x == an) err("السمة '" + an + "' مكررة في الوسم <" + tag + ">");
      seen.push_back(an);
      skipWs();
      if (eof() || s[i] != '=') err("بعد السمة '" + an + "' متوقع '='");
      i++;
      skipWs();
      if (eof() || (s[i] != '"' && s[i] != '\'')) err("قيمة السمة '" + an + "' يجب أن تكون بين علامتي اقتباس");
      char q = s[i];
      i++;
      size_t e = s.find(q, i);
      if (e == std::string::npos) err("قيمة السمة '" + an + "' غير مغلقة");
      std::string raw = s.substr(i, e - i);
      while (i < e) bump();
      i++;
      auto pr = std::make_shared<std::vector<Value>>();
      pr->push_back(mkStr(an));
      pr->push_back(mkStr(decode(raw)));
      attrs.push_back(mkListP(pr));
    }
    std::vector<Value> kids;
    std::string text;
    for (;;) {
      if (eof()) err("الوسم <" + tag + "> الذي بدأ في السطر " + std::to_string(startLine) + " لم يُغلق");
      if (starts("</")) {
        i += 2;
        std::string close = name();
        skipWs();
        if (eof() || s[i] != '>') err("متوقع '>' في إغلاق الوسم");
        i++;
        if (close != tag)
          err("الوسم </" + close + "> لا يطابق <" + tag + "> المفتوح في السطر " + std::to_string(startLine));
        break;
      }
      if (starts("<!--")) { skipPast("-->", "تعليق"); continue; }
      if (starts("<![CDATA[")) {
        i += 9;
        size_t e = s.find("]]>", i);
        if (e == std::string::npos) err("CDATA غير مغلق (ينقصه ]]>)");
        text += s.substr(i, e - i);
        while (i < e + 3) bump();
        continue;
      }
      if (starts("<?")) { skipPast("?>", "تعليمة معالجة"); continue; }
      if (s[i] == '<') { kids.push_back(element()); continue; }
      size_t b = i;
      while (!eof() && s[i] != '<') bump();
      text += decode(s.substr(b, i - b));
    }
    depth--;
    return xmlMakeNode(tag, attrs, kids, trim(text));
  }

  Value parse() {
    misc();
    if (eof()) err("المستند فارغ: لا يوجد وسم جذر");
    Value root = element();
    misc();
    if (!eof()) err("نص زائد بعد الوسم الجذر");
    return root;
  }
};

std::string xmlEscape(const std::string& t, bool attr) {
  std::string o;
  for (char c : t) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"' && attr) o += "&quot;";
    else o += c;
  }
  return o;
}

void xmlWrite(const Value& n, std::string& out, bool pretty, int level) {
  std::string pad = pretty ? std::string(static_cast<size_t>(level) * 2, ' ') : "";
  const auto& a = *n.list;
  out += pad + "<" + a[0].s;
  for (auto& at : *a[1].list)
    if (at.t == VT::List && at.list->size() == 2)
      out += " " + toStr((*at.list)[0]) + "=\"" + xmlEscape(toStr((*at.list)[1]), true) + "\"";
  bool hasKids = !a[2].list->empty();
  if (!hasKids && a[3].s.empty()) { out += "/>"; if (pretty) out += "\n"; return; }
  out += ">";
  if (hasKids) {
    if (pretty) out += "\n";
    if (!a[3].s.empty()) out += (pretty ? pad + "  " : "") + xmlEscape(a[3].s, false) + (pretty ? "\n" : "");
    for (auto& k : *a[2].list) xmlWrite(k, out, pretty, level + 1);
    out += pad;
  } else {
    out += xmlEscape(a[3].s, false);
  }
  out += "</" + a[0].s + ">";
  if (pretty) out += "\n";
}

void xmlFind(const Value& n, const std::string& tag, std::vector<Value>& out) {
  for (auto& k : *(*n.list)[2].list) {
    if (tag == "*" || (*k.list)[0].s == tag) out.push_back(k);
    xmlFind(k, tag, out);
  }
}

// ───────────────────────── الويب: بناء صفحات HTML كاملة ─────────────────────────
//
// عناصر الصفحة نصوص «آمنة» (Value نص مع b = true). النص العادي الذي يُمرَّر لأي عنصر
// يُهرَّب تلقائياً (فلا حقن HTML)، والعناصر المبنية بالمفاهيم تُدرج كما هي، والقوائم تُسطَّح.

std::string htmlEsc(const std::string& t) {
  std::string o;
  o.reserve(t.size());
  for (char c : t) {
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

Value mkHtml(const std::string& s) {
  Value v = mkStr(s);
  v.b = true;
  return v;
}

// قيمة → HTML: عنصر آمن كما هو، قائمة تُسطَّح، عدم لا شيء، وغيره نص مُهرَّب
std::string htmlOf(const Value& v) {
  if (v.t == VT::Null) return "";
  if (v.t == VT::Str && v.b) return v.s;
  if (v.t == VT::List) {
    std::string o;
    for (auto& x : *v.list) o += htmlOf(x);
    return o;
  }
  return htmlEsc(toStr(v));
}

// روابط javascript: تُستبدل بـ # (حماية من الحقن عند عرض نص غير موثوق)
std::string safeUrl(const std::string& u) {
  std::string t;
  for (char c : u) if (!std::isspace(static_cast<unsigned char>(c))) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (t.rfind("javascript:", 0) == 0 || t.rfind("vbscript:", 0) == 0) return "#";
  return u;
}

std::string htmlAttrs(const Value& attrs, int ln) {
  std::string o;
  if (attrs.t == VT::Null) return o;
  if (attrs.t != VT::List) fail(ln, "الصفات قائمة أزواج: [[\"class\"، \"بطاقة\"]، ...]");
  for (auto& p : *attrs.list) {
    if (p.t != VT::List || p.list->size() != 2 || (*p.list)[0].t != VT::Str)
      fail(ln, "كل صفة زوج [اسم، قيمة] مثل [\"class\"، \"بطاقة\"]");
    std::string k = (*p.list)[0].s;
    for (char c : k)
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == ':'))
        fail(ln, "اسم صفة غير صالح: " + k);
    const Value& v = (*p.list)[1];
    if (v.t == VT::Bool) { if (v.b) o += " " + k; continue; }
    if (v.t == VT::Null) continue;
    std::string val = toStr(v);
    if (k == "href" || k == "src" || k == "action") val = safeUrl(val);
    o += " " + k + "=\"" + htmlEsc(val) + "\"";
  }
  return o;
}

bool htmlVoid(const std::string& tag) {
  static const std::unordered_set<std::string> v = {"br", "hr", "img", "input", "meta", "link"};
  return v.count(tag) > 0;
}

Value htmlElement(const std::string& tag, const Value& attrs, const std::string& inner, int ln) {
  for (char c : tag)
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-')) fail(ln, "اسم وسم غير صالح: " + tag);
  std::string o = "<" + tag + htmlAttrs(attrs, ln) + ">";
  if (htmlVoid(tag)) return mkHtml(o);
  return mkHtml(o + inner + "</" + tag + ">");
}

// يضيف صفة إلى أول وسم في عنصر جاهز (للتنسيق السريع: $صنف و$نمط و$معرف)
Value htmlWithAttr(const Value& el, const std::string& name, const std::string& val, int ln) {
  if (el.t != VT::Str || !el.b || el.s.size() < 2 || el.s[0] != '<' || el.s[1] == '/' || el.s[1] == '!')
    fail(ln, "هذا المفهوم يتطلب عنصراً مبنياً بمفاهيم الويب (مثل $فقرة)");
  size_t e = 1;
  while (e < el.s.size() && (std::isalnum(static_cast<unsigned char>(el.s[e])) || el.s[e] == '-')) e++;
  std::string r = el.s;
  // إن وُجدت الصفة أصلاً في الوسم الأول نضمّ القيمة إليها
  size_t close = r.find('>');
  std::string head = r.substr(0, close == std::string::npos ? r.size() : close);
  std::string key = " " + name + "=\"";
  size_t k = head.find(key);
  if (k != std::string::npos && (name == "class" || name == "style")) {
    size_t at = k + key.size();
    r.insert(at, htmlEsc(val) + (name == "class" ? " " : ";"));
    return mkHtml(r);
  }
  r.insert(e, " " + name + "=\"" + htmlEsc(name == "href" || name == "src" ? safeUrl(val) : val) + "\"");
  return mkHtml(r);
}

const char* kWebCss = R"CSS(
:root{--bg:#fbf8f2;--fg:#2b2a27;--muted:#6f6b62;--card:#ffffff;--line:#e6dfd2;--accent:#2f6f8f;--accent-fg:#fff;--radius:14px;--ok:#2e7d52;--warn:#b7791f;--danger:#c0392b;--info:#2f6f8f;--c1:#2f6f8f;--c2:#d98a3d;--c3:#5b9a6b;--c4:#a5527d;--c5:#7a6bbd;--c6:#c75c4a}
:root[data-theme="dark"]{--bg:#1c1d1f;--fg:#e8e6e1;--muted:#a4a099;--card:#26282b;--line:#383b3f;--accent:#6bb3d3;--accent-fg:#10202a;--ok:#5ec48d;--warn:#e0b04f;--danger:#ee7a6a;--info:#6bb3d3;--c1:#6bb3d3;--c2:#e8a85e;--c3:#7fc08f;--c4:#cf7fa8;--c5:#9d90dd;--c6:#e58a79}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#1c1d1f;--fg:#e8e6e1;--muted:#a4a099;--card:#26282b;--line:#383b3f;--accent:#6bb3d3;--accent-fg:#10202a;--ok:#5ec48d;--warn:#e0b04f;--danger:#ee7a6a;--info:#6bb3d3;--c1:#6bb3d3;--c2:#e8a85e;--c3:#7fc08f;--c4:#cf7fa8;--c5:#9d90dd;--c6:#e58a79}}
html{scroll-behavior:smooth}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--fg);font:17px/1.8 system-ui,-apple-system,"Segoe UI",Tahoma,"Noto Naskh Arabic",sans-serif}
main,header.bar,footer.foot{max-width:860px;margin:0 auto;padding:20px 18px}
h1,h2,h3,h4,h5,h6{line-height:1.35;margin:1.4em 0 .5em}
h1{font-size:2rem}h2{font-size:1.5rem}h3{font-size:1.25rem}
p{margin:.7em 0}
a{color:var(--accent);text-underline-offset:3px}
img,svg{max-width:100%;height:auto}
nav.nav{display:flex;flex-wrap:wrap;gap:6px 18px;padding:14px 18px;border-bottom:1px solid var(--line);background:var(--card);position:sticky;top:0}
nav.nav a{text-decoration:none;font-weight:600}
section.hero{padding:56px 18px;text-align:center;background:linear-gradient(135deg,var(--accent),color-mix(in srgb,var(--accent) 55%,#000));color:var(--accent-fg)}
section.hero h1{margin:0 0 .3em;font-size:2.4rem}
section.hero p{margin:0 auto;max-width:620px;opacity:.92}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--radius);padding:16px 20px;margin:16px 0;box-shadow:0 1px 3px rgba(0,0,0,.05)}
.card>h3:first-child{margin-top:.2em}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:16px}
.grid .card{margin:0}
button,.btn{font:inherit;cursor:pointer;border:0;border-radius:10px;padding:.55em 1.2em;background:var(--accent);color:var(--accent-fg);font-weight:600}
button:hover{filter:brightness(1.08)}
input,textarea,select{font:inherit;width:100%;padding:.6em .8em;border:1px solid var(--line);border-radius:10px;background:var(--card);color:var(--fg)}
label{display:block;margin:.8em 0 .25em;font-weight:600}
table{width:100%;border-collapse:collapse;margin:1em 0;background:var(--card);border-radius:var(--radius);overflow:hidden}
th,td{padding:.6em .9em;text-align:start;border-bottom:1px solid var(--line)}
th{background:color-mix(in srgb,var(--accent) 14%,var(--card))}
tr:last-child td{border-bottom:0}
blockquote{margin:1em 0;padding:.2em 1em;border-inline-start:4px solid var(--accent);color:var(--muted)}
code,pre{font-family:ui-monospace,Consolas,monospace;direction:ltr;unicode-bidi:plaintext}
code{background:color-mix(in srgb,var(--accent) 12%,var(--card));padding:.1em .4em;border-radius:6px}
pre{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;overflow:auto}
footer.foot{color:var(--muted);text-align:center;border-top:1px solid var(--line);margin-top:30px}
.bars rect{fill:var(--accent)}.bars text{fill:var(--fg);font:17px system-ui}
)CSS";

Value mkAttrs(std::initializer_list<std::pair<std::string, std::string>> kv) {
  auto l = std::make_shared<std::vector<Value>>();
  for (auto& p : kv) {
    auto pr = std::make_shared<std::vector<Value>>();
    pr->push_back(mkStr(p.first));
    pr->push_back(mkStr(p.second));
    l->push_back(mkListP(pr));
  }
  return mkListP(l);
}

#include "ee_lib_helpers.inc"
#include "ee_web2_helpers.inc"

// ───────────────────────── المنفّذ ─────────────────────────

enum class Flow { Normal, Break, Continue, Return };

struct ExitSig { int code; };

struct Interp {
  int lastErrLine = 0;
  RunOptions opt;
  std::vector<std::string> dirStack;
  std::unordered_set<std::string> imported;
  std::istream& in;
  std::ostream& out;
  size_t maxOut;
  size_t written = 0;
  std::shared_ptr<Env> globals = std::make_shared<Env>();
  std::unordered_map<std::string, std::string> nativeAlias;
  int depth = 0;
  Value retVal;
  std::mt19937 rng{std::random_device{}()};
  std::unordered_set<const void*> heap;  // كتل $احجز الحيّة

  Interp(std::istream& i, std::ostream& o, size_t m) : in(i), out(o), maxOut(m) {}

  void emit(int line, const std::string& s) {
    written += s.size();
    if (maxOut && written > maxOut) fail(line, "تجاوز الحد الأقصى للمخرجات");
    out << s;
  }

  void reg(const std::string& name, std::function<Value(Interp&, Args&, int)> f) {
    Value v;
    v.t = VT::Native;
    v.nat = std::make_shared<NativeObj>();
    v.nat->name = name;
    v.nat->fn = std::move(f);
    globals->vars["$" + name] = v;
    nativeAlias[normalizeKw(name)] = "$" + name;
  }

  static void argc(const char* name, const Args& a, size_t lo, size_t hi, int ln) {
    if (a.size() < lo || a.size() > hi) {
      std::string exp = lo == hi ? std::to_string(lo) : std::to_string(lo) + " إلى " + std::to_string(hi);
      fail(ln, std::string("المفهوم $") + name + " يتوقع " + exp + " معاملات لكن أُعطيت " +
                   std::to_string(a.size()));
    }
  }
  static void needList(const char* name, const Value& v, int ln) {
    if (v.t != VT::List) fail(ln, std::string("المفهوم $") + name + " يتطلب قائمة لكن وُجد " + typeName(v));
  }
  static void needCallable(const char* name, const Value& v, int ln) {
    if (v.t != VT::Func && v.t != VT::Native && v.t != VT::Class)
      fail(ln, std::string("المفهوم $") + name + " يتطلب مفهوماً لكن وُجد " + typeName(v));
  }
  static void needNode(const char* name, const Value& v, int ln) {
    if (!xmlIsNode(v)) fail(ln, std::string("المفهوم $") + name + " يتطلب عقدة XML (من $حلل_xml أو $عقدة) لكن وُجد " + typeName(v));
  }
  static Value mkNativeV(const std::string& name, std::function<Value(Interp&, Args&, int)> f) {
    Value v;
    v.t = VT::Native;
    v.nat = std::make_shared<NativeObj>();
    v.nat->name = name;
    v.nat->fn = std::move(f);
    return v;
  }
  // يستدعي f(عنصر) أو f(عنصر، فهرس) إن كانت تتوقع معاملين (كـ JavaScript)
  static Value callEl(Interp& I, const Value& f, const Value& item, size_t idx, int ln) {
    Args a{item};
    if (f.t == VT::Func && f.fn->def->params.size() == 2) a.push_back(mkNum(static_cast<double>(idx)));
    return I.call(f, a, ln);
  }
  static Value toBigArg(const char* name, const Value& v, int ln) {
    if (v.t == VT::Big) return v;
    BigInt b;
    if (v.t == VT::Num && numToBig(v, b)) return mkBig(b);
    if (v.t == VT::Str && BigInt::parse(v.s, b)) return mkBig(b);
    fail(ln, std::string("المفهوم $") + name + " يتطلب عدداً صحيحاً (أو نصاً من أرقام) لكن وُجد " + typeName(v) +
                 (v.t == VT::Num ? " غير صحيح" : ""));
  }
  static double needNum(const char* name, const Value& v, int ln) {
    if (v.t == VT::Big) return v.big->toDouble();
    if (v.t != VT::Num) fail(ln, std::string("المفهوم $") + name + " يتطلب رقماً لكن وُجد " + typeName(v));
    return v.n;
  }

  void alias(const std::string& newName, const std::string& oldName) {
    globals->vars["$" + newName] = globals->vars["$" + oldName];
    nativeAlias[normalizeKw(newName)] = "$" + newName;
  }

  void setup() {
    globals->vars["باي"] = mkNum(3.14159265358979323846);

    reg("اطبع", [](Interp& I, Args& a, int ln) {
      std::string s;
      for (size_t i = 0; i < a.size(); i++) {
        if (i) s += ' ';
        s += toStr(a[i]);
      }
      s += '\n';
      I.emit(ln, s);
      return Value();
    });
    reg("اكتب", [](Interp& I, Args& a, int ln) {
      std::string s;
      for (size_t i = 0; i < a.size(); i++) {
        if (i) s += ' ';
        s += toStr(a[i]);
      }
      I.emit(ln, s);
      return Value();
    });
    reg("اقرأ", [](Interp& I, Args& a, int ln) {
      argc("اقرأ", a, 0, 1, ln);
      if (!a.empty()) I.emit(ln, toStr(a[0]));
      std::string line;
      if (!std::getline(I.in, line)) return Value();
      if (!line.empty() && line.back() == '\r') line.pop_back();
      return mkStr(line);
    });
    reg("اقرأ_رقم", [](Interp& I, Args& a, int ln) {
      argc("اقرأ_رقم", a, 0, 1, ln);
      if (!a.empty()) I.emit(ln, toStr(a[0]));
      std::string line;
      if (!std::getline(I.in, line)) return Value();
      std::string t = toAsciiDigits(line);
      char* end = nullptr;
      double d = std::strtod(t.c_str(), &end);
      if (end == t.c_str()) return Value();
      while (*end == ' ' || *end == '\t' || *end == '\r') end++;
      if (*end != '\0') return Value();
      return mkNum(d);
    });
    reg("ارم", [](Interp&, Args& a, int ln) -> Value {
      if (a.size() != 1) fail(ln, "الاستخدام: $ارم(قيمة)");
      throw EEError{ln, toStr(a[0]), std::make_shared<Value>(a[0])};
    });
    reg("سطر_الخطأ", [](Interp& I, Args&, int) -> Value { return mkNum(I.lastErrLine); });
    reg("طول", [](Interp&, Args& a, int ln) {
      argc("طول", a, 1, 1, ln);
      if (a[0].t == VT::Str) return mkNum(static_cast<double>(splitChars(a[0].s).size()));
      if (a[0].t == VT::List) return mkNum(static_cast<double>(a[0].list->size()));
      fail(ln, "المفهوم $طول يتطلب نصاً أو قائمة");
    });
    reg("اضف", [](Interp&, Args& a, int ln) {
      argc("اضف", a, 2, 2, ln);
      if (a[0].t != VT::List) fail(ln, "المفهوم $اضف يتطلب قائمة كمعامل أول");
      a[0].list->push_back(a[1]);
      return Value();
    });
    reg("ازل", [](Interp&, Args& a, int ln) {
      argc("ازل", a, 1, 1, ln);
      if (a[0].t != VT::List) fail(ln, "المفهوم $ازل يتطلب قائمة");
      if (a[0].list->empty()) fail(ln, "القائمة فارغة");
      Value v = a[0].list->back();
      a[0].list->pop_back();
      return v;
    });
    reg("مدى", [](Interp&, Args& a, int ln) {
      argc("مدى", a, 1, 3, ln);
      double from = 0, to, step = 1;
      if (a.size() == 1) to = needNum("مدى", a[0], ln);
      else {
        from = needNum("مدى", a[0], ln);
        to = needNum("مدى", a[1], ln);
        if (a.size() == 3) step = needNum("مدى", a[2], ln);
      }
      if (step == 0) fail(ln, "خطوة 'مدى' لا يمكن أن تكون صفراً");
      auto l = std::make_shared<std::vector<Value>>();
      for (double x = from; step > 0 ? x < to : x > to; x += step) {
        l->push_back(mkNum(x));
        if (l->size() > 10000000) fail(ln, "المدى كبير جداً");
      }
      return mkListP(l);
    });
    reg("جذر", [](Interp&, Args& a, int ln) {
      argc("جذر", a, 1, 1, ln);
      double x = needNum("جذر", a[0], ln);
      if (x < 0) fail(ln, "لا يمكن حساب جذر عدد سالب");
      return mkNum(std::sqrt(x));
    });
    reg("اس", [](Interp&, Args& a, int ln) {
      argc("اس", a, 2, 2, ln);
      return mkNum(std::pow(needNum("اس", a[0], ln), needNum("اس", a[1], ln)));
    });
    reg("مطلق", [](Interp&, Args& a, int ln) {
      argc("مطلق", a, 1, 1, ln);
      return mkNum(std::fabs(needNum("مطلق", a[0], ln)));
    });
    reg("ارضية", [](Interp&, Args& a, int ln) {
      argc("ارضية", a, 1, 1, ln);
      return mkNum(std::floor(needNum("ارضية", a[0], ln)));
    });
    reg("سقف", [](Interp&, Args& a, int ln) {
      argc("سقف", a, 1, 1, ln);
      return mkNum(std::ceil(needNum("سقف", a[0], ln)));
    });
    reg("تقريب", [](Interp&, Args& a, int ln) {
      argc("تقريب", a, 1, 1, ln);
      return mkNum(std::round(needNum("تقريب", a[0], ln)));
    });
    auto minmax = [](bool isMax) {
      return [isMax](Interp&, Args& a, int ln) {
        const char* nm = isMax ? "اكبر" : "اصغر";
        std::vector<Value> vals = a;
        if (a.size() == 1 && a[0].t == VT::List) vals = *a[0].list;
        if (vals.empty()) fail(ln, std::string("المفهوم $") + nm + " يحتاج قيمة واحدة على الأقل");
        double r = needNum(nm, vals[0], ln);
        for (auto& v : vals) {
          double x = needNum(nm, v, ln);
          r = isMax ? std::max(r, x) : std::min(r, x);
        }
        return mkNum(r);
      };
    };
    reg("اكبر", minmax(true));
    reg("اصغر", minmax(false));
    reg("نص", [](Interp&, Args& a, int ln) {
      argc("نص", a, 1, 1, ln);
      return mkStr(toStr(a[0]));
    });
    reg("رقم", [](Interp&, Args& a, int ln) {
      argc("رقم", a, 1, 1, ln);
      const Value& v = a[0];
      if (v.t == VT::Num) return v;
      if (v.t == VT::Big) return mkNum(v.big->toDouble());
      if (v.t == VT::Bool) return mkNum(v.b ? 1 : 0);
      if (v.t == VT::Str) {
        std::string t = toAsciiDigits(v.s);
        char* end = nullptr;
        double d = std::strtod(t.c_str(), &end);
        if (end == t.c_str()) return Value();
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (*end != '\0') return Value();
        return mkNum(d);
      }
      return Value();
    });
    reg("كبير", [](Interp&, Args& a, int ln) {
      argc("كبير", a, 1, 1, ln);
      return toBigArg("كبير", a[0], ln);
    });
    reg("اس_كبير", [](Interp&, Args& a, int ln) {
      argc("اس_كبير", a, 2, 2, ln);
      Value b = toBigArg("اس_كبير", a[0], ln);
      if (a[1].t != VT::Num || a[1].n < 0 || a[1].n != std::floor(a[1].n) || a[1].n > 1e9)
        fail(ln, "الأس في $اس_كبير يجب أن يكون عدداً صحيحاً غير سالب");
      try {
        return mkBig(BigInt::pow(*b.big, static_cast<unsigned long long>(a[1].n)));
      } catch (EEError& e) { if (e.line == 0) e.line = ln; throw; }
    });
    reg("اس_بالباقي", [](Interp&, Args& a, int ln) {
      argc("اس_بالباقي", a, 3, 3, ln);
      Value b = toBigArg("اس_بالباقي", a[0], ln), e = toBigArg("اس_بالباقي", a[1], ln),
            m = toBigArg("اس_بالباقي", a[2], ln);
      try {
        return mkBig(BigInt::modpow(*b.big, *e.big, *m.big));
      } catch (EEError& x) { if (x.line == 0) x.line = ln; throw; }
    });
    reg("اولي_كبير", [](Interp&, Args& a, int ln) {
      argc("اولي_كبير", a, 1, 1, ln);
      Value b = toBigArg("اولي_كبير", a[0], ln);
      try {
        return mkBool(BigInt::isPrime(*b.big));
      } catch (EEError& x) { if (x.line == 0) x.line = ln; throw; }
    });
    reg("نوع", [](Interp&, Args& a, int ln) {
      argc("نوع", a, 1, 1, ln);
      return mkStr(typeName(a[0]));
    });
    reg("قص", [](Interp&, Args& a, int ln) {
      argc("قص", a, 2, 3, ln);
      if (a[0].t != VT::Str) fail(ln, "المفهوم $قص يتطلب نصاً");
      auto ch = splitChars(a[0].s);
      long long n = static_cast<long long>(ch.size());
      long long from = static_cast<long long>(needNum("قص", a[1], ln));
      long long to = a.size() == 3 ? static_cast<long long>(needNum("قص", a[2], ln)) : n;
      if (from < 0) from += n;
      if (to < 0) to += n;
      from = std::max(0LL, std::min(from, n));
      to = std::max(0LL, std::min(to, n));
      std::string r;
      for (long long i = from; i < to; i++) r += ch[static_cast<size_t>(i)];
      return mkStr(r);
    });
    reg("فرز", [](Interp&, Args& a, int ln) {
      argc("فرز", a, 1, 1, ln);
      if (a[0].t != VT::List) fail(ln, "المفهوم $فرز يتطلب قائمة");
      auto l = std::make_shared<std::vector<Value>>(*a[0].list);
      bool allNum = true, allStr = true;
      for (auto& v : *l) {
        if (v.t != VT::Num) allNum = false;
        if (v.t != VT::Str) allStr = false;
      }
      if (!allNum && !allStr && !l->empty()) fail(ln, "لا يمكن فرز قائمة بأنواع مختلطة");
      if (allNum) std::sort(l->begin(), l->end(), [](const Value& x, const Value& y) { return x.n < y.n; });
      else std::sort(l->begin(), l->end(), [](const Value& x, const Value& y) { return x.s < y.s; });
      return mkListP(l);
    });
    reg("عكس", [](Interp&, Args& a, int ln) {
      argc("عكس", a, 1, 1, ln);
      if (a[0].t == VT::List) {
        auto l = std::make_shared<std::vector<Value>>(a[0].list->rbegin(), a[0].list->rend());
        return mkListP(l);
      }
      if (a[0].t == VT::Str) {
        auto ch = splitChars(a[0].s);
        std::string r;
        for (auto it = ch.rbegin(); it != ch.rend(); ++it) r += *it;
        return mkStr(r);
      }
      fail(ln, "المفهوم $عكس يتطلب قائمة أو نصاً");
    });
    reg("يحتوي", [](Interp&, Args& a, int ln) {
      argc("يحتوي", a, 2, 2, ln);
      if (a[0].t == VT::List) {
        for (auto& v : *a[0].list)
          if (equals(v, a[1])) return mkBool(true);
        return mkBool(false);
      }
      if (a[0].t == VT::Str && a[1].t == VT::Str) return mkBool(a[0].s.find(a[1].s) != std::string::npos);
      fail(ln, "المفهوم $يحتوي يتطلب (قائمة، قيمة) أو (نص، نص)");
    });
    reg("عشوائي", [](Interp& I, Args& a, int ln) {
      if (a.empty()) return mkNum(std::uniform_real_distribution<double>(0.0, 1.0)(I.rng));
      argc("عشوائي", a, 2, 2, ln);
      long long lo = static_cast<long long>(needNum("عشوائي", a[0], ln));
      long long hi = static_cast<long long>(needNum("عشوائي", a[1], ln));
      if (lo > hi) std::swap(lo, hi);
      return mkNum(static_cast<double>(std::uniform_int_distribution<long long>(lo, hi)(I.rng)));
    });
    reg("ارقام_عربية", [](Interp&, Args& a, int ln) {
      argc("ارقام_عربية", a, 1, 1, ln);
      return mkStr(toArabicDigits(toStr(a[0])));
    });

    // ═════ JavaScript: دوال من الدرجة الأولى، إغلاقات، تركيب، تطبيق جزئي، حفظ نتائج ═════
    reg("خريطة", [](Interp& I, Args& a, int ln) {
      argc("خريطة", a, 2, 2, ln);
      needList("خريطة", a[0], ln);
      auto out = std::make_shared<std::vector<Value>>();
      std::vector<Value> src = *a[0].list;
      for (size_t i = 0; i < src.size(); i++) out->push_back(callEl(I, a[1], src[i], i, ln));
      return mkListP(out);
    });
    reg("رشح", [](Interp& I, Args& a, int ln) {
      argc("رشح", a, 2, 2, ln);
      needList("رشح", a[0], ln);
      auto out = std::make_shared<std::vector<Value>>();
      std::vector<Value> src = *a[0].list;
      for (size_t i = 0; i < src.size(); i++)
        if (truthy(callEl(I, a[1], src[i], i, ln))) out->push_back(src[i]);
      return mkListP(out);
    });
    reg("اختزل", [](Interp& I, Args& a, int ln) {
      argc("اختزل", a, 2, 3, ln);
      needList("اختزل", a[0], ln);
      std::vector<Value> src = *a[0].list;
      size_t i = 0;
      Value acc;
      if (a.size() == 3) acc = a[2];
      else if (src.empty()) fail(ln, "$اختزل على قائمة فارغة تحتاج قيمة بداية: $اختزل(ق، مفهوم، بداية)");
      else acc = src[i++];
      for (; i < src.size(); i++) {
        Args args{acc, src[i]};
        acc = I.call(a[1], args, ln);
      }
      return acc;
    });
    reg("رتب_حسب", [](Interp& I, Args& a, int ln) {
      argc("رتب_حسب", a, 2, 2, ln);
      needList("رتب_حسب", a[0], ln);
      std::vector<Value> v = *a[0].list;
      Value f = a[1];
      std::stable_sort(v.begin(), v.end(), [&](const Value& x, const Value& y) {
        Args args{x, y};
        Value r = I.call(f, args, ln);
        return needNum("رتب_حسب", r, ln) < 0;
      });
      return mkListP(std::make_shared<std::vector<Value>>(std::move(v)));
    });
    reg("ابحث", [](Interp& I, Args& a, int ln) {
      argc("ابحث", a, 2, 2, ln);
      needList("ابحث", a[0], ln);
      std::vector<Value> src = *a[0].list;
      for (size_t i = 0; i < src.size(); i++)
        if (truthy(callEl(I, a[1], src[i], i, ln))) return src[i];
      return Value();
    });
    reg("اي", [](Interp& I, Args& a, int ln) {  // any
      argc("اي", a, 1, 2, ln);
      needList("اي", a[0], ln);
      std::vector<Value> src = *a[0].list;
      for (size_t i = 0; i < src.size(); i++)
        if (truthy(a.size() == 2 ? callEl(I, a[1], src[i], i, ln) : src[i])) return mkBool(true);
      return mkBool(false);
    });
    reg("كل", [](Interp& I, Args& a, int ln) {  // all
      argc("كل", a, 1, 2, ln);
      needList("كل", a[0], ln);
      std::vector<Value> src = *a[0].list;
      for (size_t i = 0; i < src.size(); i++)
        if (!truthy(a.size() == 2 ? callEl(I, a[1], src[i], i, ln) : src[i])) return mkBool(false);
      return mkBool(true);
    });
    reg("ركب", [](Interp&, Args& a, int ln) {  // ركب(f, g, h)(x) = f(g(h(x)))
      if (a.size() < 2) fail(ln, "المفهوم $ركب يتطلب مفهومين على الأقل");
      for (auto& f : a) needCallable("ركب", f, ln);
      std::vector<Value> fs = a;
      return mkNativeV("مركّب", [fs](Interp& I, Args& args, int l2) {
        Value r = I.call(fs.back(), args, l2);
        for (size_t i = fs.size() - 1; i-- > 0;) {
          Args one{r};
          r = I.call(fs[i], one, l2);
        }
        return r;
      });
    });
    reg("جزئي", [](Interp&, Args& a, int ln) {  // تطبيق جزئي (currying)
      if (a.empty()) fail(ln, "المفهوم $جزئي يتطلب مفهوماً ثم قيمه الأولى");
      needCallable("جزئي", a[0], ln);
      Value f = a[0];
      std::vector<Value> bound(a.begin() + 1, a.end());
      return mkNativeV("جزئي", [f, bound](Interp& I, Args& args, int l2) {
        Args all = bound;
        all.insert(all.end(), args.begin(), args.end());
        return I.call(f, all, l2);
      });
    });
    reg("احفظ", [](Interp&, Args& a, int ln) {  // memoize
      argc("احفظ", a, 1, 1, ln);
      needCallable("احفظ", a[0], ln);
      Value f = a[0];
      auto cache = std::make_shared<std::unordered_map<std::string, Value>>();
      return mkNativeV("محفوظ", [f, cache](Interp& I, Args& args, int l2) {
        std::string key;
        for (auto& x : args) key += toStr(x, true) + "\x1f";
        auto it = cache->find(key);
        if (it != cache->end()) return it->second;
        Value r = I.call(f, args, l2);
        (*cache)[key] = r;
        return r;
      });
    });

    // ═════ Python: zip و enumerate و sum (والتوليد [... لكل ... في ... لو ...] في المحلل) ═════
    reg("ضم", [](Interp&, Args& a, int ln) {
      if (a.empty()) fail(ln, "المفهوم $ضم يتطلب قائمتين أو أكثر");
      size_t n = SIZE_MAX;
      for (auto& l : a) { needList("ضم", l, ln); n = std::min(n, l.list->size()); }
      auto out = std::make_shared<std::vector<Value>>();
      for (size_t i = 0; i < n; i++) {
        auto row = std::make_shared<std::vector<Value>>();
        for (auto& l : a) row->push_back((*l.list)[i]);
        out->push_back(mkListP(row));
      }
      return mkListP(out);
    });
    reg("ترقيم", [](Interp&, Args& a, int ln) {
      argc("ترقيم", a, 1, 2, ln);
      needList("ترقيم", a[0], ln);
      double start = a.size() == 2 ? needNum("ترقيم", a[1], ln) : 0;
      auto out = std::make_shared<std::vector<Value>>();
      for (size_t i = 0; i < a[0].list->size(); i++) {
        auto row = std::make_shared<std::vector<Value>>();
        row->push_back(mkNum(start + static_cast<double>(i)));
        row->push_back((*a[0].list)[i]);
        out->push_back(mkListP(row));
      }
      return mkListP(out);
    });
    reg("مجموع", [](Interp&, Args& a, int ln) {
      argc("مجموع", a, 1, 2, ln);
      needList("مجموع", a[0], ln);
      Value acc = a.size() == 2 ? a[1] : mkNum(0);
      for (auto& x : *a[0].list) acc = binop("+", acc, x, ln);
      return acc;
    });

    // ═════ XML: محلل كامل + استعلام + كتابة ═════
    reg("حلل_xml", [](Interp&, Args& a, int ln) {
      argc("حلل_xml", a, 1, 1, ln);
      if (a[0].t != VT::Str) fail(ln, "المفهوم $حلل_xml يتطلب نصاً");
      try {
        XmlParser xp(a[0].s);
        return xp.parse();
      } catch (EEError& e) { if (e.line == 0) e.line = ln; throw; }
    });
    reg("اكتب_xml", [](Interp&, Args& a, int ln) {
      argc("اكتب_xml", a, 1, 2, ln);
      needNode("اكتب_xml", a[0], ln);
      std::string out;
      bool compact = a.size() == 2 && truthy(a[1]);
      xmlWrite(a[0], out, !compact, 0);
      while (!out.empty() && out.back() == '\n') out.pop_back();
      return mkStr(out);
    });
    reg("وسم", [](Interp&, Args& a, int ln) {
      argc("وسم", a, 1, 1, ln);
      needNode("وسم", a[0], ln);
      return (*a[0].list)[0];
    });
    reg("نص_عقدة", [](Interp&, Args& a, int ln) {
      argc("نص_عقدة", a, 1, 1, ln);
      needNode("نص_عقدة", a[0], ln);
      return (*a[0].list)[3];
    });
    reg("ابناء", [](Interp&, Args& a, int ln) {
      argc("ابناء", a, 1, 1, ln);
      needNode("ابناء", a[0], ln);
      return (*a[0].list)[2];
    });
    reg("سمات", [](Interp&, Args& a, int ln) {
      argc("سمات", a, 1, 1, ln);
      needNode("سمات", a[0], ln);
      return (*a[0].list)[1];
    });
    reg("سمة", [](Interp&, Args& a, int ln) {
      argc("سمة", a, 2, 3, ln);
      needNode("سمة", a[0], ln);
      std::string k = toStr(a[1]);
      for (auto& at : *(*a[0].list)[1].list)
        if (toStr((*at.list)[0]) == k) return (*at.list)[1];
      return a.size() == 3 ? a[2] : Value();
    });
    reg("ابحث_xml", [](Interp&, Args& a, int ln) {
      argc("ابحث_xml", a, 2, 2, ln);
      needNode("ابحث_xml", a[0], ln);
      std::string path = toStr(a[1]);
      std::vector<Value> cur;
      if (path.find('/') == std::string::npos) {
        xmlFind(a[0], path, cur);
      } else {
        cur.push_back(a[0]);
        size_t b = 0;
        while (b <= path.size()) {
          size_t e = path.find('/', b);
          std::string seg = path.substr(b, e == std::string::npos ? std::string::npos : e - b);
          if (!seg.empty()) {
            std::vector<Value> next;
            for (auto& n : cur)
              for (auto& k : *(*n.list)[2].list)
                if (seg == "*" || (*k.list)[0].s == seg) next.push_back(k);
            cur = next;
          }
          if (e == std::string::npos) break;
          b = e + 1;
        }
      }
      return mkListP(std::make_shared<std::vector<Value>>(cur));
    });
    reg("عقدة", [](Interp&, Args& a, int ln) {
      argc("عقدة", a, 1, 4, ln);
      std::vector<Value> attrs, kids;
      if (a.size() > 1) {
        needList("عقدة", a[1], ln);
        for (auto& p : *a[1].list) {
          if (p.t != VT::List || p.list->size() != 2) fail(ln, "سمات $عقدة قائمة أزواج: [[\"اسم\"، \"قيمة\"]، ...]");
          attrs.push_back(p);
        }
      }
      if (a.size() > 2) {
        needList("عقدة", a[2], ln);
        for (auto& k : *a[2].list) {
          needNode("عقدة", k, ln);
          kids.push_back(k);
        }
      }
      return xmlMakeNode(toStr(a[0]), attrs, kids, a.size() > 3 ? toStr(a[3]) : "");
    });

    // ═════ C: ذاكرة ومؤشرات (&م  *م  حساب المؤشرات) ═════
    reg("احجز", [](Interp& I, Args& a, int ln) {  // malloc
      argc("احجز", a, 1, 2, ln);
      double n = needNum("احجز", a[0], ln);
      if (n < 1 || n != std::floor(n) || n > 50e6) fail(ln, "حجم $احجز يجب أن يكون عدداً صحيحاً بين 1 و 50000000");
      auto l = std::make_shared<std::vector<Value>>(static_cast<size_t>(n), a.size() == 2 ? a[1] : mkNum(0));
      I.heap.insert(l.get());
      auto p = std::make_shared<PtrObj>();
      p->k = PtrObj::Elem;
      p->list = l;
      p->idx = 0;
      Value v;
      v.t = VT::Ptr;
      v.ptr = p;
      return v;
    });
    reg("حرر", [](Interp& I, Args& a, int ln) {  // free
      argc("حرر", a, 1, 1, ln);
      if (a[0].t == VT::Null) return Value();  // free(NULL) مسموح
      if (a[0].t != VT::Ptr || a[0].ptr->k != PtrObj::Elem || a[0].ptr->idx != 0 || !I.heap.count(a[0].ptr->list.get()))
        fail(ln, "تحرير غير صالح: يجب تمرير المؤشر الأصلي الذي أعاده $احجز، ولم يُحرَّر من قبل (double free)");
      I.heap.erase(a[0].ptr->list.get());
      a[0].ptr->list->clear();
      a[0].ptr->list->shrink_to_fit();
      return Value();
    });

    // ═════ الويب: صفحة كاملة بمفاهيم عربية (انظر قسم الويب في README) ═════
    {
      struct Simple { const char* name; const char* tag; const char* cls; };
      static const Simple simples[] = {
          {"فقرة", "p", ""},        {"حاوية", "div", ""},        {"قسم", "section", ""},
          {"رئيسي", "main", ""},    {"مقال", "article", ""},     {"رأس", "header", "bar"},
          {"تذييل", "footer", "foot"}, {"ابراز", "strong", ""},  {"مائل", "em", ""},
          {"كود", "code", ""},      {"كتلة_كود", "pre", ""},     {"اقتباس", "blockquote", ""},
          {"عنصر_قائمة", "li", ""}, {"شبكة", "div", "grid"},     {"نموذج", "form", ""},
          {"صغير", "small", ""},
      };
      for (const Simple& sp : simples) {
        std::string tag = sp.tag, cls = sp.cls;
        reg(sp.name, [tag, cls](Interp&, Args& a, int ln) {
          std::string in;
          for (auto& x : a) in += htmlOf(x);
          return htmlElement(tag, cls.empty() ? Value() : mkAttrs({{"class", cls}}), in, ln);
        });
      }
    }
    reg("وسم_html", [](Interp&, Args& a, int ln) {
      if (a.size() < 2) fail(ln, "المفهوم $وسم_html(وسم، صفات، محتوى...) يتطلب وسماً وصفات (أو عدم)");
      if (a[0].t != VT::Str) fail(ln, "اسم الوسم يجب أن يكون نصاً");
      std::string in;
      for (size_t i = 2; i < a.size(); i++) in += htmlOf(a[i]);
      return htmlElement(a[0].s, a[1], in, ln);
    });
    reg("خام", [](Interp&, Args& a, int ln) {
      argc("خام", a, 1, 1, ln);
      return mkHtml(toStr(a[0]));
    });
    reg("اسلوب", [](Interp&, Args& a, int ln) {
      argc("اسلوب", a, 1, 1, ln);
      std::string css = toStr(a[0]);
      if (css.find("</") != std::string::npos) fail(ln, "لا يُسمح بـ </ داخل الأسلوب");
      return mkHtml("<style>" + css + "</style>");
    });
    reg("سكربت", [](Interp&, Args& a, int ln) {
      argc("سكربت", a, 1, 1, ln);
      std::string js = toStr(a[0]);
      if (js.find("</") != std::string::npos) fail(ln, "لا يُسمح بـ </ داخل السكربت (اكتب <\\/ بدلها)");
      return mkHtml("<script>" + js + "</script>");
    });
    reg("عنوان", [](Interp&, Args& a, int ln) {
      argc("عنوان", a, 1, 2, ln);
      double lv = a.size() == 2 ? needNum("عنوان", a[1], ln) : 1;
      if (lv < 1 || lv > 6 || lv != std::floor(lv)) fail(ln, "مستوى العنوان من 1 إلى 6");
      return htmlElement("h" + std::to_string(static_cast<int>(lv)), Value(), htmlOf(a[0]), ln);
    });
    reg("رابط", [](Interp&, Args& a, int ln) {
      argc("رابط", a, 2, 2, ln);
      return htmlElement("a", mkAttrs({{"href", toStr(a[1])}}), htmlOf(a[0]), ln);
    });
    reg("صورة", [](Interp&, Args& a, int ln) {
      argc("صورة", a, 1, 2, ln);
      return htmlElement("img", mkAttrs({{"src", toStr(a[0])}, {"alt", a.size() == 2 ? toStr(a[1]) : ""}}), "", ln);
    });
    reg("زر", [](Interp&, Args& a, int ln) {
      argc("زر", a, 1, 2, ln);
      Value at = a.size() == 2 ? mkAttrs({{"type", "button"}, {"onclick", toStr(a[1])}}) : mkAttrs({{"type", "button"}});
      return htmlElement("button", at, htmlOf(a[0]), ln);
    });
    reg("سطر_جديد", [](Interp&, Args&, int ln) { return htmlElement("br", Value(), "", ln); });
    reg("فاصل", [](Interp&, Args&, int ln) { return htmlElement("hr", Value(), "", ln); });
    auto listFn = [](const char* nm, const char* tag) {
      std::string name = nm, t = tag;
      return [name, t](Interp&, Args& a, int ln) {
        argc(name.c_str(), a, 1, 1, ln);
        needList(name.c_str(), a[0], ln);
        std::string in;
        for (auto& x : *a[0].list) in += htmlElement("li", Value(), htmlOf(x), ln).s;
        return htmlElement(t, Value(), in, ln);
      };
    };
    reg("قائمة_نقطية", listFn("قائمة_نقطية", "ul"));
    reg("قائمة_مرقمة", listFn("قائمة_مرقمة", "ol"));
    reg("جدول", [](Interp&, Args& a, int ln) {
      argc("جدول", a, 2, 2, ln);
      needList("جدول", a[0], ln);
      needList("جدول", a[1], ln);
      std::string head, body;
      for (auto& h : *a[0].list) head += htmlElement("th", Value(), htmlOf(h), ln).s;
      for (auto& row : *a[1].list) {
        if (row.t != VT::List) fail(ln, "كل صف في $جدول قائمة خلايا");
        std::string cells;
        for (auto& c : *row.list) cells += htmlElement("td", Value(), htmlOf(c), ln).s;
        body += htmlElement("tr", Value(), cells, ln).s;
      }
      return htmlElement("table", Value(),
                         htmlElement("thead", Value(), htmlElement("tr", Value(), head, ln).s, ln).s +
                             htmlElement("tbody", Value(), body, ln).s,
                         ln);
    });
    reg("بطاقة", [](Interp&, Args& a, int ln) {
      if (a.empty()) fail(ln, "المفهوم $بطاقة(عنوان، محتوى...) يتطلب عنواناً");
      std::string in = htmlElement("h3", Value(), htmlOf(a[0]), ln).s;
      for (size_t i = 1; i < a.size(); i++) in += htmlOf(a[i]);
      return htmlElement("div", mkAttrs({{"class", "card"}}), in, ln);
    });
    reg("بطل", [](Interp&, Args& a, int ln) {
      argc("بطل", a, 1, 2, ln);
      std::string in = htmlElement("h1", Value(), htmlOf(a[0]), ln).s;
      if (a.size() == 2) in += htmlElement("p", Value(), htmlOf(a[1]), ln).s;
      return htmlElement("section", mkAttrs({{"class", "hero"}}), in, ln);
    });
    reg("تنقل", [](Interp&, Args& a, int ln) {
      argc("تنقل", a, 1, 1, ln);
      needList("تنقل", a[0], ln);
      std::string in;
      for (auto& l : *a[0].list) {
        if (l.t != VT::List || l.list->size() != 2) fail(ln, "روابط $تنقل أزواج: [[\"نص\"، \"#قسم\"]، ...]");
        in += htmlElement("a", mkAttrs({{"href", toStr((*l.list)[1])}}), htmlOf((*l.list)[0]), ln).s;
      }
      return htmlElement("nav", mkAttrs({{"class", "nav"}}), in, ln);
    });
    reg("حقل", [](Interp&, Args& a, int ln) {
      argc("حقل", a, 2, 3, ln);
      std::string id = toStr(a[0]);
      std::string type = a.size() == 3 ? toStr(a[2]) : "text";
      Value lab = htmlElement("label", mkAttrs({{"for", id}}), htmlOf(a[1]), ln);
      Value inp = type == "textarea" ? htmlElement("textarea", mkAttrs({{"id", id}, {"name", id}, {"rows", "4"}}), "", ln)
                                     : htmlElement("input", mkAttrs({{"id", id}, {"name", id}, {"type", type}}), "", ln);
      return mkHtml(lab.s + inp.s);
    });
    reg("رسم_اعمدة", [](Interp&, Args& a, int ln) {
      argc("رسم_اعمدة", a, 2, 2, ln);
      needList("رسم_اعمدة", a[0], ln);
      needList("رسم_اعمدة", a[1], ln);
      size_t n = a[0].list->size();
      if (n == 0 || n != a[1].list->size()) fail(ln, "$رسم_اعمدة تتطلب قائمتين غير فارغتين بالطول نفسه (التسميات، القيم)");
      double mx = 0;
      std::vector<double> vals;
      for (auto& v : *a[1].list) {
        double d = needNum("رسم_اعمدة", v, ln);
        vals.push_back(d);
        mx = std::max(mx, d);
      }
      if (mx <= 0) mx = 1;
      const double W = 600, H = 260, top = 28, bottom = 36, slot = (W - 20) / static_cast<double>(n);
      std::string svg = "<svg class=\"bars\" viewBox=\"0 0 600 260\" role=\"img\" aria-label=\"رسم بياني بالأعمدة\">";
      char buf[200];
      for (size_t i = 0; i < n; i++) {
        double h = vals[i] / mx * (H - top - bottom);
        double x = 10 + slot * static_cast<double>(i) + slot * 0.2, w = slot * 0.6, y = H - bottom - h;
        std::snprintf(buf, sizeof buf, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"5\"/>", x, y, w, h);
        svg += buf;
        std::snprintf(buf, sizeof buf, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\">", x + w / 2, y - 6);
        svg += buf + htmlEsc(fmtNum(vals[i])) + "</text>";
        std::snprintf(buf, sizeof buf, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\">", x + w / 2, H - 12);
        svg += buf + htmlEsc(toStr((*a[0].list)[i])) + "</text>";
      }
      return mkHtml(svg + "</svg>");
    });
    reg("صنف_css", [](Interp&, Args& a, int ln) {
      argc("صنف_css", a, 2, 2, ln);
      return htmlWithAttr(a[1], "class", toStr(a[0]), ln);
    });
    reg("نمط", [](Interp&, Args& a, int ln) {
      argc("نمط", a, 2, 2, ln);
      return htmlWithAttr(a[1], "style", toStr(a[0]), ln);
    });
    reg("معرف", [](Interp&, Args& a, int ln) {
      argc("معرف", a, 2, 2, ln);
      return htmlWithAttr(a[1], "id", toStr(a[0]), ln);
    });
    reg("صفحة", [](Interp&, Args& a, int ln) {
      if (a.empty()) fail(ln, "المفهوم $صفحة(عنوان، محتوى...) يتطلب عنواناً");
      std::string body;
      for (size_t i = 1; i < a.size(); i++) body += htmlOf(a[i]);
      std::string css = kWebCss, js;
      for (const WebChunk& c : kWebChunks)
        if (body.find(c.trigger) != std::string::npos) css += c.css;
      for (const WebJs& j : kWebJs)
        if (body.find(j.trigger) != std::string::npos) js += j.js;
      return mkHtml("<!DOCTYPE html>\n<html lang=\"ar\" dir=\"rtl\">\n<head>\n<meta charset=\"utf-8\">\n"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                    "<meta name=\"color-scheme\" content=\"light dark\">\n<title>" + htmlEsc(toStr(a[0])) +
                    "</title>\n<style>" + css + "</style>\n</head>\n<body>\n" + body + "\n" +
                    (js.empty() ? std::string() : "<script>" + js + "</script>\n") + "</body>\n</html>");
    });

#include "ee_lib.inc"
#include "ee_web2.inc"
#include "ee_web3.inc"
#include "ee_sys.inc"

    // مرادفات عربية أبسط
    alias("اعرض", "اطبع");
    alias("اسأل", "اقرأ");
    alias("اسأل_رقم", "اقرأ_رقم");
  }

  // ── الأنواع (TypeScript) ──
  static std::string describeType(const TypeSpec& t) {
    switch (t.k) {
      case TypeSpec::Any: return "أي";
      case TypeSpec::Name: return t.name;
      case TypeSpec::ListOf: return "قائمة[" + describeType(*t.parts[0]) + "]";
      case TypeSpec::Opt: return describeType(*t.parts[0]) + "؟";
      case TypeSpec::Union: {
        std::string r;
        for (size_t i = 0; i < t.parts.size(); i++) r += (i ? " او " : "") + describeType(*t.parts[i]);
        return r;
      }
    }
    return "؟";
  }
  bool typeOk(const TypeSpec& t, const Value& v, const std::shared_ptr<Env>& env, int line) {
    switch (t.k) {
      case TypeSpec::Any: return true;
      case TypeSpec::Opt: return v.t == VT::Null || typeOk(*t.parts[0], v, env, line);
      case TypeSpec::Union:
        for (auto& p : t.parts) if (typeOk(*p, v, env, line)) return true;
        return false;
      case TypeSpec::ListOf:
        if (v.t != VT::List) return false;
        for (auto& x : *v.list) if (!typeOk(*t.parts[0], x, env, line)) return false;
        return true;
      case TypeSpec::Name: {
        static const std::string nNum = normalizeKw("رقم"), nStr = normalizeKw("نص"), nBool = normalizeKw("منطقي"),
                                 nList = normalizeKw("قائمة"), nFn = normalizeKw("مفهوم"), nNull = normalizeKw("عدم"),
                                 nBig = normalizeKw("عدد_كبير"), nInt = normalizeKw("عدد"), nPtr = normalizeKw("مؤشر"),
                                 nObj = normalizeKw("كائن"), nCls = normalizeKw("صنف");
        const std::string& n = t.norm;
        if (n == nNum) return v.t == VT::Num;
        if (n == nStr) return v.t == VT::Str;
        if (n == nBool) return v.t == VT::Bool;
        if (n == nList) return v.t == VT::List;
        if (n == nFn) return v.t == VT::Func || v.t == VT::Native || v.t == VT::Class;
        if (n == nNull) return v.t == VT::Null;
        if (n == nBig) return v.t == VT::Big;
        if (n == nInt) return v.t == VT::Num || v.t == VT::Big;
        if (n == nPtr) return v.t == VT::Ptr;
        if (n == nObj) return v.t == VT::Obj;
        if (n == nCls) return v.t == VT::Class;
        Value* c = env->find("$" + t.name);
        if (!c || c->t != VT::Class)
          fail(line, "النوع '" + t.name + "' غير معروف: الأنواع رقم، نص، منطقي، قائمة، مفهوم، عدم، عدد_كبير، عدد، مؤشر، أي، أو اسم صنف معرّف");
        if (v.t != VT::Obj) return false;
        for (ClassObj* k = v.obj->cls.get(); k; k = k->parent.get())
          if (k == c->cls.get()) return true;
        return false;
      }
    }
    return false;
  }
  void checkType(const TP& t, const Value& v, const std::shared_ptr<Env>& env, int line, const std::string& what) {
    if (t && !typeOk(*t, v, env, line))
      fail(line, what + " نوعه " + describeType(*t) + " لكن القيمة من نوع " + typeName(v) + " (" + toStr(v, true) + ")");
  }

  // ── الأصناف (C++) ──
  static std::string methodKey(const std::string& n) {
    return normalizeKw(n.substr(1)) == normalizeKw("انشاء") ? std::string("$انشاء") : n;
  }
  static Value* findMethod(ClassObj* c, const std::string& name, ClassObj*& owner) {
    for (; c; c = c->parent.get()) {
      auto it = c->methods.find(name);
      if (it != c->methods.end()) { owner = c; return &it->second; }
    }
    return nullptr;
  }
  Value bindMethod(const Value& self, ClassObj* owner, const std::string& name, const Value& m) {
    auto env = std::make_shared<Env>();
    env->parent = m.fn->closure;
    env->vars["هذا"] = self;
    ClassObj* po = nullptr;
    Value* pm = owner->parent ? findMethod(owner->parent.get(), name, po) : nullptr;
    if (pm) env->vars["$الاب"] = bindMethod(self, po, name, *pm);
    Value r;
    r.t = VT::Func;
    r.fn = std::make_shared<FuncObj>();
    r.fn->def = m.fn->def;
    r.fn->closure = env;
    return r;
  }
  static void setField(ObjectObj& o, const std::string& n, const Value& v) {
    if (!o.fields.count(n)) o.order.push_back(n);
    o.fields[n] = v;
  }
  Value instantiate(const Value& cv, Args& args, int line) {
    auto o = std::make_shared<ObjectObj>();
    o->cls = cv.cls;
    Value ov;
    ov.t = VT::Obj;
    ov.obj = o;
    std::vector<ClassObj*> chain;
    for (ClassObj* c = cv.cls.get(); c; c = c->parent.get()) chain.insert(chain.begin(), c);
    for (ClassObj* c : chain) {
      auto fenv = std::make_shared<Env>();
      fenv->parent = c->closure;
      fenv->vars["هذا"] = ov;
      for (auto& f : c->fields) setField(*o, f.first, eval(f.second, fenv));
    }
    ClassObj* owner = nullptr;
    Value* ctor = findMethod(cv.cls.get(), "$انشاء", owner);
    if (ctor) {
      Value b = bindMethod(ov, owner, "$انشاء", *ctor);
      call(b, args, line);
    } else if (!args.empty()) {
      fail(line, "الصنف " + cv.cls->name + " ليس له مفهوم $انشاء لكن أُعطيت " + std::to_string(args.size()) + " معاملات");
    }
    return ov;
  }

  // ── المؤشرات (C) ──
  static Value* ptrSlot(const Value& pv, int line) {
    if (pv.t == VT::Null) fail(line, "فكّ مؤشر فارغ (عدم): خطأ تجزئة");
    if (pv.t != VT::Ptr) fail(line, "العلامة * تتطلب مؤشراً لكن وُجد " + typeName(pv));
    PtrObj& p = *pv.ptr;
    if (p.k == PtrObj::Var) {
      auto it = p.env->vars.find(p.name);
      if (it == p.env->vars.end()) fail(line, "مؤشر معلّق: المتغير لم يعد موجوداً");
      return &it->second;
    }
    if (p.k == PtrObj::Elem) {
      if (p.idx < 0 || p.idx >= static_cast<long long>(p.list->size()))
        fail(line, "وصول إلى ذاكرة غير صالحة: الموضع " + std::to_string(p.idx) + " خارج الحدود أو الكتلة محررة");
      return &(*p.list)[static_cast<size_t>(p.idx)];
    }
    auto it = p.obj->fields.find(p.name);
    if (it == p.obj->fields.end()) fail(line, "مؤشر معلّق: الحقل غير موجود");
    return &it->second;
  }

  // ── المفاهيم: $اسم (مع مطابقة مرنة للهمزات في المفاهيم المدمجة) ──
  Value* lookupFn(const std::string& name, const std::shared_ptr<Env>& env) {
    Value* v = env->find("$" + name);
    if (v) return v;
    auto it = nativeAlias.find(normalizeKw(name));
    if (it != nativeAlias.end()) return globals->find(it->second);
    return nullptr;
  }

  Value call(const Value& f, Args& args, int line) {
    if (g_stop.load()) fail(line, "تم إيقاف البرنامج");
    if (f.t == VT::Native) return f.nat->fn(*this, args, line);
    if (f.t == VT::Class) return instantiate(f, args, line);
    if (f.t != VT::Func) fail(line, "هذه القيمة ليست مفهوماً قابلاً للاستدعاء (النوع: " + typeName(f) + ")");
    const FuncDef& d = *f.fn->def;
    if (args.size() != d.params.size()) {
      fail(line, "المفهوم " + (d.name.empty() ? std::string("المجهول") : d.name) + " يتوقع " + std::to_string(d.params.size()) +
                     " معاملات لكن أُعطيت " + std::to_string(args.size()));
    }
    if (depth >= 1000) fail(line, "تجاوز الحد الأقصى لعمق الاستدعاءات (تكرار لا نهائي؟)");
    auto env = std::make_shared<Env>();
    env->parent = f.fn->closure;
    for (size_t i = 0; i < d.params.size(); i++) {
      env->vars[d.params[i]] = args[i];
      if (i < d.ptypes.size() && d.ptypes[i]) {
        checkType(d.ptypes[i], args[i], env, line,
                  "المعامل <<" + d.params[i] + ">> في المفهوم " + (d.name.empty() ? std::string("المجهول") : d.name));
        env->types[d.params[i]] = d.ptypes[i];
      }
    }
    depth++;
    Flow fl;
    try {
      fl = execList(d.body, env);
    } catch (...) {
      depth--;
      throw;
    }
    depth--;
    if (fl == Flow::Break || fl == Flow::Continue) fail(line, "'توقف' أو 'استمر' خارج حلقة");
    Value r;
    if (fl == Flow::Return) {
      r = std::move(retVal);
      retVal = Value();
    }
    if (d.rtype)
      checkType(d.rtype, r, env, line, "القيمة المرجعة من المفهوم " + (d.name.empty() ? std::string("المجهول") : d.name));
    return r;
  }

  static long long toIndex(const Value& v, size_t size, int line) {
    if (v.t != VT::Num) fail(line, "الفهرس يجب أن يكون رقماً");
    if (v.n != std::floor(v.n)) fail(line, "الفهرس يجب أن يكون عدداً صحيحاً");
    long long i = static_cast<long long>(v.n);
    long long n = static_cast<long long>(size);
    if (i < 0) i += n;
    if (i < 0 || i >= n) fail(line, "الفهرس خارج النطاق (الطول = " + std::to_string(n) + ")");
    return i;
  }

  Value eval(const EP& e, const std::shared_ptr<Env>& env) {
    switch (e->k) {
      case EK::Num: return mkNum(e->num);
      case EK::Str: return mkStr(e->s);
      case EK::BigLit: {
        BigInt b;
        if (!BigInt::parse(e->s, b)) fail(e->line, "رقم غير صالح");
        return mkBig(b);
      }
      case EK::Bool: return mkBool(e->b);
      case EK::Null: return Value();
      case EK::Var: {
        Value* v = env->find(e->s);
        if (!v) fail(e->line, "المتغير <<" + e->s + ">> غير معرّف");
        if (v->t == VT::Num) return mkNum(v->n);  // نسخ سريع للأرقام
        return *v;
      }
      case EK::FnRef: {
        if (e->key.empty()) e->key = "$" + e->s;
        Value* v = env->find(e->key);
        if (!v) v = lookupFn(e->s, env);
        if (!v && e->s == "الاب") fail(e->line, "لا يوجد مفهوم $الاب هنا: الصنف الأب لا يعرّف مفهوماً بالاسم نفسه");
        if (!v) fail(e->line, "المفهوم $" + e->s + " غير معرّف");
        return *v;
      }
      case EK::List: {
        auto l = std::make_shared<std::vector<Value>>();
        for (auto& it : e->items) l->push_back(eval(it, env));
        return mkListP(l);
      }
      case EK::Unary: {
        Value v = eval(e->l, env);
        if (e->s == "-") {
          if (v.t == VT::Big) {
            BigInt r = *v.big;
            if (!r.zero()) r.neg = !r.neg;
            return mkBig(r);
          }
          if (v.t != VT::Num) fail(e->line, "لا يمكن عكس إشارة قيمة من نوع " + typeName(v));
          return mkNum(-v.n);
        }
        return mkBool(!truthy(v));
      }
      case EK::And: {
        if (!truthy(eval(e->l, env))) return mkBool(false);
        return mkBool(truthy(eval(e->r, env)));
      }
      case EK::Or: {
        if (truthy(eval(e->l, env))) return mkBool(true);
        return mkBool(truthy(eval(e->r, env)));
      }
      case EK::Binary: {
        Value a = eval(e->l, env);
        Value b = eval(e->r, env);
        if (a.t == VT::Num && b.t == VT::Num) {  // مسار سريع للأرقام
          const std::string& o = e->s;
          char c0 = o[0];
          if (o.size() == 1) {
            switch (c0) {
              case '+': return mkNum(a.n + b.n);
              case '-': return mkNum(a.n - b.n);
              case '*': return mkNum(a.n * b.n);
              case '<': return mkBool(a.n < b.n);
              case '>': return mkBool(a.n > b.n);
              default: break;
            }
          } else if (o.size() == 2 && o[1] == '=') {
            switch (c0) {
              case '=': return mkBool(a.n == b.n);
              case '!': return mkBool(a.n != b.n);
              case '<': return mkBool(a.n <= b.n);
              case '>': return mkBool(a.n >= b.n);
              default: break;
            }
          }
        }
        return binop(e->s, a, b, e->line);
      }
      case EK::Index: {
        Value obj = eval(e->l, env);
        Value ix = eval(e->r, env);
        if (obj.t == VT::List) return (*obj.list)[static_cast<size_t>(toIndex(ix, obj.list->size(), e->line))];
        if (obj.t == VT::Str) {
          auto ch = splitChars(obj.s);
          return mkStr(ch[static_cast<size_t>(toIndex(ix, ch.size(), e->line))]);
        }
        fail(e->line, "لا يمكن استخدام الفهرس [] مع النوع " + typeName(obj));
      }
      case EK::Call: {
        Value f = eval(e->l, env);
        Args args;
        for (auto& it : e->items) args.push_back(eval(it, env));
        return call(f, args, e->line);
      }
      case EK::Lambda: {
        Value v;
        v.t = VT::Func;
        v.fn = std::make_shared<FuncObj>();
        v.fn->def = e->fn;
        v.fn->closure = env;
        return v;
      }
      case EK::Member: {
        Value o = eval(e->l, env);
        if (o.t != VT::Obj) fail(e->line, "النقطة تُستعمل مع الكائنات فقط لكن وُجد " + typeName(o));
        if (e->b) {
          ClassObj* owner = nullptr;
          Value* m = findMethod(o.obj->cls.get(), methodKey(e->s), owner);
          if (!m) fail(e->line, "الصنف " + o.obj->cls->name + " لا يحتوي المفهوم " + e->s);
          return bindMethod(o, owner, methodKey(e->s), *m);
        }
        auto it = o.obj->fields.find(e->s);
        if (it == o.obj->fields.end())
          fail(e->line, "الحقل <<" + e->s + ">> غير موجود في الكائن من الصنف " + o.obj->cls->name);
        return it->second;
      }
      case EK::Deref: return *ptrSlot(eval(e->l, env), e->line);
      case EK::AddrOf: {
        const EP& t = e->l;
        auto p = std::make_shared<PtrObj>();
        if (t->k == EK::Var) {
          auto en = findEnvSP(env, t->s);
          if (!en) fail(e->line, "المتغير <<" + t->s + ">> غير معرّف");
          p->k = PtrObj::Var;
          p->env = en;
          p->name = t->s;
        } else if (t->k == EK::Index) {
          Value ob = eval(t->l, env);
          Value ix = eval(t->r, env);
          if (ob.t != VT::List) fail(e->line, "& على عنصر يتطلب قائمة");
          p->k = PtrObj::Elem;
          p->list = ob.list;
          p->idx = toIndex(ix, ob.list->size(), e->line);
        } else {
          Value ob = eval(t->l, env);
          if (ob.t != VT::Obj) fail(e->line, "& على حقل يتطلب كائناً");
          if (!ob.obj->fields.count(t->s)) fail(e->line, "الحقل <<" + t->s + ">> غير موجود");
          p->k = PtrObj::Field;
          p->obj = ob.obj;
          p->name = t->s;
        }
        Value v;
        v.t = VT::Ptr;
        v.ptr = p;
        return v;
      }
      case EK::Comp: {
        Value it = eval(e->r, env);
        std::vector<Value> items;
        if (it.t == VT::List) items = *it.list;
        else if (it.t == VT::Str) for (auto& c : splitChars(it.s)) items.push_back(mkStr(c));
        else fail(e->line, "لا يمكن التكرار على النوع " + typeName(it));
        auto out = std::make_shared<std::vector<Value>>();
        for (auto& item : items) {
          if (g_stop.load()) fail(e->line, "تم إيقاف البرنامج");
          auto child = std::make_shared<Env>();
          child->parent = env;
          child->vars[e->s] = item;
          if (!e->items.empty() && !truthy(eval(e->items[0], child))) continue;
          Value v = eval(e->l, child);
          if (e->b) {
            if (v.t != VT::List) fail(e->line, "خطأ داخلي في التوليد المتداخل");
            out->insert(out->end(), v.list->begin(), v.list->end());
          } else {
            out->push_back(std::move(v));
          }
          if (out->size() > 50000000) fail(e->line, "القائمة الناتجة كبيرة جداً");
        }
        return mkListP(out);
      }
      case EK::Assign: {
        const EP& target = e->l;
        Value rhs = eval(e->r, env);
        std::string op = e->s;
        if (target->k == EK::Var) {
          auto en = findEnvSP(env, target->s);
          if (!en) fail(e->line, "المتغير <<" + target->s + ">> غير معرّف (عرّفه أولاً بـ: متغير <<" + target->s + ">> = ...)");
          Value* slot = &en->vars[target->s];
          if (op != "=") rhs = binop(op.substr(0, 1), *slot, rhs, e->line);
          auto ti = en->types.find(target->s);
          if (ti != en->types.end()) checkType(ti->second, rhs, env, e->line, "المتغير <<" + target->s + ">> معرّف");
          *slot = rhs;
          return rhs;
        }
        if (target->k == EK::Deref) {
          Value* slot = ptrSlot(eval(target->l, env), e->line);
          if (op != "=") rhs = binop(op.substr(0, 1), *slot, rhs, e->line);
          *slot = rhs;
          return rhs;
        }
        if (target->k == EK::Member) {
          Value o = eval(target->l, env);
          if (o.t != VT::Obj) fail(e->line, "إسناد الحقل يتطلب كائناً لكن وُجد " + typeName(o));
          if (op != "=") {
            auto it = o.obj->fields.find(target->s);
            if (it == o.obj->fields.end()) fail(e->line, "الحقل <<" + target->s + ">> غير موجود");
            rhs = binop(op.substr(0, 1), it->second, rhs, e->line);
          }
          setField(*o.obj, target->s, rhs);
          return rhs;
        }
        // إسناد إلى عنصر قائمة
        Value obj = eval(target->l, env);
        Value ix = eval(target->r, env);
        if (obj.t != VT::List) fail(e->line, "الإسناد بالفهرس مدعوم للقوائم فقط");
        size_t i = static_cast<size_t>(toIndex(ix, obj.list->size(), e->line));
        if (op != "=") rhs = binop(op.substr(0, 1), (*obj.list)[i], rhs, e->line);
        (*obj.list)[i] = rhs;
        return rhs;
      }
    }
    return Value();
  }

  void define(const std::shared_ptr<Env>& env, const std::string& name, const Value& v, int line) {
    auto ex = env->vars.find(name);
    bool builtinShadow = ex != env->vars.end() && ex->second.t == VT::Native && !name.empty() && name[0] == '$' &&
                         env == globals;  // يمكن للمستخدم إعادة تعريف مفهوم مدمج
    if (ex != env->vars.end() && !builtinShadow)
      fail(line, (!name.empty() && name[0] == '$' ? "المفهوم " + name : "المتغير <<" + name + ">>") +
                     " معرّف مسبقاً في هذا النطاق");
    env->vars[name] = v;
  }

  Flow execList(const std::vector<SP>& body, const std::shared_ptr<Env>& env) {
    for (auto& s : body) {
      Flow f = execStmt(s, env);
      if (f != Flow::Normal) return f;
    }
    return Flow::Normal;
  }

  Flow execBlock(const std::vector<SP>& body, const std::shared_ptr<Env>& env) {
    auto child = std::make_shared<Env>();
    child->parent = env;
    return execList(body, child);
  }

  Flow execStmt(const SP& s, const std::shared_ptr<Env>& env) {
    switch (s->k) {
      case SK::Expr:
        eval(s->e, env);
        return Flow::Normal;
      case SK::Var: {
        Value v = eval(s->e, env);
        checkType(s->ty, v, env, s->line, "المتغير <<" + s->name + ">> معرّف");
        define(env, s->name, v, s->line);
        if (s->ty) env->types[s->name] = s->ty;
        return Flow::Normal;
      }
      case SK::Class: {
        auto c = std::make_shared<ClassObj>();
        c->name = s->name;
        c->closure = env;
        if (!s->parent.empty()) {
          Value* pv = env->find(s->parent);
          if (!pv || pv->t != VT::Class) fail(s->line, "الصنف الأب " + s->parent + " غير معرّف");
          c->parent = pv->cls;
        }
        for (auto& m : s->members) {
          if (m->k == SK::Func) {
            std::string key = methodKey(m->name);
            if (c->methods.count(key)) fail(m->line, "المفهوم " + key + " مكرر في الصنف " + c->name);
            Value v;
            v.t = VT::Func;
            v.fn = std::make_shared<FuncObj>();
            v.fn->def = m->fn;
            v.fn->closure = env;
            c->methods[key] = v;
          } else {
            c->fields.push_back({m->name, m->e});
          }
        }
        Value v;
        v.t = VT::Class;
        v.cls = c;
        define(env, s->name, v, s->line);
        return Flow::Normal;
      }
      case SK::Func: {
        Value v;
        v.t = VT::Func;
        v.fn = std::make_shared<FuncObj>();
        v.fn->def = s->fn;
        v.fn->closure = env;
        define(env, s->name, v, s->line);
        return Flow::Normal;
      }
      case SK::Block:
        return execBlock(s->body, env);
      case SK::If: {
        if (truthy(eval(s->e, env))) return execBlock(s->body, env);
        if (s->hasElse) return execBlock(s->elseBody, env);
        return Flow::Normal;
      }
      case SK::While: {
        while (truthy(eval(s->e, env))) {
          if (g_stop.load()) fail(s->line, "تم إيقاف البرنامج");
          Flow f = execBlock(s->body, env);
          if (f == Flow::Break) break;
          if (f == Flow::Return) return f;
        }
        return Flow::Normal;
      }
      case SK::For: {
        Value it = eval(s->e, env);
        std::vector<Value> items;
        if (it.t == VT::List) items = *it.list;
        else if (it.t == VT::Str) for (auto& c : splitChars(it.s)) items.push_back(mkStr(c));
        else fail(s->line, "لا يمكن التكرار على النوع " + typeName(it));
        for (auto& item : items) {
          if (g_stop.load()) fail(s->line, "تم إيقاف البرنامج");
          auto child = std::make_shared<Env>();
          child->parent = env;
          child->vars[s->name] = item;
          Flow f = execBlock(s->body, child);
          if (f == Flow::Break) break;
          if (f == Flow::Return) return f;
        }
        return Flow::Normal;
      }
      case SK::Return:
        retVal = s->e ? eval(s->e, env) : Value();
        return Flow::Return;
      case SK::Break: return Flow::Break;
      case SK::Continue: return Flow::Continue;
      case SK::Try: {
        Flow f = Flow::Normal;
        std::exception_ptr pending;
        int savedDepth = depth;
        try {
          f = execBlock(s->body, env);
        } catch (EEError& e) {
          depth = savedDepth;
          if (g_stop.load() || !s->hasElse) {
            pending = std::current_exception();
          } else {
            try {
              auto child = std::make_shared<Env>();
              child->parent = env;
              lastErrLine = e.line;
              if (!s->name.empty()) child->vars[s->name] = e.val ? *e.val : mkStr(e.msg);
              f = execList(s->elseBody, child);
            } catch (...) {
              pending = std::current_exception();
            }
          }
        }
        if (s->ty) {
          Value savedRet = retVal;
          Flow ff = execBlock(s->members, env);
          if (ff != Flow::Normal) return ff;  // اخيرا تتغلب على ما قبلها
          retVal = savedRet;
        }
        if (pending) std::rethrow_exception(pending);
        return f;
      }
    }
    return Flow::Normal;
  }
};

}  // namespace

void request_stop() { g_stop.store(true); }

int run(const std::string& source, std::istream& in, std::ostream& out, std::ostream& err,
        std::size_t max_output, const RunOptions* opts) {
  g_stop.store(false);
  try {
    Parser ps;
    ps.t = lex(source);
    std::vector<SP> prog = ps.parseProgram();
    Interp I(in, out, max_output);
    if (opts) I.opt = *opts;
    I.dirStack.push_back(I.opt.base_dir);
    I.setup();
    Flow f = I.execList(prog, I.globals);
    if (f == Flow::Break || f == Flow::Continue) fail(0, "'توقف' أو 'استمر' خارج حلقة");
    out.flush();
    return 0;
  } catch (const ExitSig& x) {
    out.flush();
    return x.code;
  } catch (const EEError& e) {
    out.flush();
    err << "\n⚠ خطأ";
    if (e.line > 0) err << " (السطر " << e.line << ")";
    err << ": " << e.msg << "\n";
    err.flush();
    return 1;
  } catch (const std::exception& e) {
    out.flush();
    err << "\n⚠ خطأ داخلي: " << e.what() << "\n";
    err.flush();
    return 1;
  }
}

}  // namespace ee
