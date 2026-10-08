// EE# — مفسّر لغة البرمجة العربية (C++17)
// المراحل: محلل لفظي (lex) ← محلل نحوي (Parser) ← منفّذ شجري (Interp)

#include "ee.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ee {

const char* version() { return "0.1.0"; }

namespace {

std::atomic<bool> g_stop{false};

struct EEError {
  int line;
  std::string msg;
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
  static const char* ops2[] = {"==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "%="};

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

    // معاملات من حرفين
    bool matched = false;
    if (i + 1 < s.size()) {
      std::string two = s.substr(i, 2);
      for (const char* o : ops2) {
        if (two == o) { push(TK::Op, two, line); i += 2; matched = true; break; }
      }
    }
    if (matched) continue;

    if (cp > 0 && cp < 0x80 && std::strchr("+-*/%<>=()[],;!", static_cast<int>(cp))) {
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

enum class EK { Num, Str, Bool, Null, Var, List, Unary, Binary, And, Or, Assign, Index, Call, Lambda, FnRef };

struct Expr {
  EK k = EK::Null;
  int line = 0;
  double num = 0;
  std::string s;  // نص / اسم / معامل
  bool b = false;
  EP l, r;
  std::vector<EP> items;
  std::shared_ptr<FuncDef> fn;
};

enum class SK { Expr, Var, If, While, For, Block, Func, Return, Break, Continue };

struct Stmt {
  SK k = SK::Expr;
  int line = 0;
  std::string name;
  EP e;
  std::vector<SP> body;
  std::vector<SP> elseBody;
  bool hasElse = false;
  std::shared_ptr<FuncDef> fn;
};

struct FuncDef {
  std::string name;
  std::vector<std::string> params;
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
                          "نهاية", "انتهى"})
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
        isKw("نهاية|انتهى|والا|غير_ذلك")) {
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

  // معاملات المفهوم: (<<أ>>, <<ب>>)  أو بلا أقواس على نفس السطر:  <<أ>>, <<ب>>
  std::vector<std::string> parseParams(int line) {
    std::vector<std::string> ps;
    if (matchOp("(")) {
      while (!isOp(")") && cur().k != TK::End) {
        ps.push_back(expectVar("كمعامل"));
        if (!matchOp(",")) break;
      }
      expectOp(")");
    } else {
      while (cur().k == TK::Var && cur().line == line) {
        ps.push_back(cur().text);
        adv();
        if (!matchOp(",")) break;
      }
    }
    return ps;
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
      case TK::Op: return tk.text == "[" || tk.text == "-" || tk.text == "!";
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
      fd->params = parseParams(line);
      fd->body = parseBody("نهاية|انتهى");
      expectEnd("مفهوم", line);
      s->name = fd->name;
      s->fn = fd;
      return s;
    }
    if (isKw("ارجع")) {
      adv();
      auto s = mkS(SK::Return, line);
      if (cur().k == TK::End || isOp(";") || cur().line != line || isKw("نهاية|انتهى|والا|غير_ذلك"))
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
        if (left->k != EK::Var && left->k != EK::Index)
          fail(cur().line, "لا يمكن الإسناد إلى هذا التعبير: الإسناد يكون لمتغير مثل <<س>> أو لعنصر قائمة مثل <<ق>>[0]");
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
    while (isOp("+") || isOp("-")) {
      std::string op = cur().text;
      int line = cur().line;
      adv();
      l = bin(EK::Binary, op, l, parseMul(), line);
    }
    return l;
  }
  EP parseMul() {
    EP l = parseUnary();
    while (isOp("*") || isOp("/") || isOp("%")) {
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
  EP parsePrimary() {
    const Token& tk = cur();
    int line = tk.line;
    switch (tk.k) {
      case TK::Num: {
        EP e = mkE(EK::Num, line);
        e->num = tk.num;
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
          fd->params = parseParams(line);
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
        if (isOp("(")) {
          adv();
          EP e = parseExpr();
          expectOp(")");
          return e;
        }
        if (isOp("[")) {
          adv();
          EP e = mkE(EK::List, line);
          while (!isOp("]") && cur().k != TK::End) {
            e->items.push_back(parseExpr());
            if (!matchOp(",")) break;
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

// ───────────────────────── القيم ─────────────────────────

struct Env;
struct FuncObj;
struct NativeObj;
struct Interp;

enum class VT { Null, Num, Bool, Str, List, Func, Native };

struct Value {
  VT t = VT::Null;
  double n = 0;
  bool b = false;
  std::string s;
  std::shared_ptr<std::vector<Value>> list;
  std::shared_ptr<FuncObj> fn;
  std::shared_ptr<NativeObj> nat;
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
  std::shared_ptr<Env> parent;
  Value* find(const std::string& n) {
    for (Env* e = this; e; e = e->parent.get()) {
      auto it = e->vars.find(n);
      if (it != e->vars.end()) return &it->second;
    }
    return nullptr;
  }
};

Value mkNum(double d) { Value v; v.t = VT::Num; v.n = d; return v; }
Value mkBool(bool b) { Value v; v.t = VT::Bool; v.b = b; return v; }
Value mkStr(const std::string& s) { Value v; v.t = VT::Str; v.s = s; return v; }
Value mkListP(std::shared_ptr<std::vector<Value>> l) { Value v; v.t = VT::List; v.list = std::move(l); return v; }

std::string typeName(const Value& v) {
  switch (v.t) {
    case VT::Null: return "عدم";
    case VT::Num: return "رقم";
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
    case VT::Bool: return v.b ? "صح" : "خطأ";
    case VT::Str: return quote ? "\"" + v.s + "\"" : v.s;
    case VT::List: {
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

bool equals(const Value& a, const Value& b) {
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
  }
  return false;
}

Value binop(const std::string& op, const Value& a, const Value& b, int line) {
  if (op == "==") return mkBool(equals(a, b));
  if (op == "!=") return mkBool(!equals(a, b));
  if (op == "+") {
    if (a.t == VT::Num && b.t == VT::Num) return mkNum(a.n + b.n);
    if (a.t == VT::Str || b.t == VT::Str) return mkStr(toStr(a) + toStr(b));
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

// ───────────────────────── المنفّذ ─────────────────────────

enum class Flow { Normal, Break, Continue, Return };

struct Interp {
  std::istream& in;
  std::ostream& out;
  size_t maxOut;
  size_t written = 0;
  std::shared_ptr<Env> globals = std::make_shared<Env>();
  std::unordered_map<std::string, std::string> nativeAlias;
  int depth = 0;
  Value retVal;
  std::mt19937 rng{std::random_device{}()};

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
  static double needNum(const char* name, const Value& v, int ln) {
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

    // مرادفات عربية أبسط
    alias("اعرض", "اطبع");
    alias("اسأل", "اقرأ");
    alias("اسأل_رقم", "اقرأ_رقم");
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
    if (f.t != VT::Func) fail(line, "هذه القيمة ليست مفهوماً قابلاً للاستدعاء (النوع: " + typeName(f) + ")");
    const FuncDef& d = *f.fn->def;
    if (args.size() != d.params.size()) {
      fail(line, "المفهوم " + (d.name.empty() ? std::string("المجهول") : d.name) + " يتوقع " + std::to_string(d.params.size()) +
                     " معاملات لكن أُعطيت " + std::to_string(args.size()));
    }
    if (depth >= 1000) fail(line, "تجاوز الحد الأقصى لعمق الاستدعاءات (تكرار لا نهائي؟)");
    auto env = std::make_shared<Env>();
    env->parent = f.fn->closure;
    for (size_t i = 0; i < d.params.size(); i++) env->vars[d.params[i]] = args[i];
    depth++;
    Flow fl;
    try {
      fl = execList(d.body, env);
    } catch (...) {
      depth--;
      throw;
    }
    depth--;
    if (fl == Flow::Return) {
      Value r = std::move(retVal);
      retVal = Value();
      return r;
    }
    if (fl == Flow::Break || fl == Flow::Continue) fail(line, "'توقف' أو 'استمر' خارج حلقة");
    return Value();
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
      case EK::Bool: return mkBool(e->b);
      case EK::Null: return Value();
      case EK::Var: {
        Value* v = env->find(e->s);
        if (!v) fail(e->line, "المتغير <<" + e->s + ">> غير معرّف");
        return *v;
      }
      case EK::FnRef: {
        Value* v = lookupFn(e->s, env);
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
      case EK::Assign: {
        const EP& target = e->l;
        Value rhs = eval(e->r, env);
        std::string op = e->s;
        if (target->k == EK::Var) {
          Value* slot = env->find(target->s);
          if (!slot) fail(e->line, "المتغير <<" + target->s + ">> غير معرّف (عرّفه أولاً بـ: متغير <<" + target->s + ">> = ...)");
          if (op != "=") rhs = binop(op.substr(0, 1), *slot, rhs, e->line);
          *slot = rhs;
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
    if (env->vars.count(name))
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
      case SK::Var:
        define(env, s->name, eval(s->e, env), s->line);
        return Flow::Normal;
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
    }
    return Flow::Normal;
  }
};

}  // namespace

void request_stop() { g_stop.store(true); }

int run(const std::string& source, std::istream& in, std::ostream& out, std::ostream& err,
        std::size_t max_output) {
  g_stop.store(false);
  try {
    Parser ps;
    ps.t = lex(source);
    std::vector<SP> prog = ps.parseProgram();
    Interp I(in, out, max_output);
    I.setup();
    Flow f = I.execList(prog, I.globals);
    if (f == Flow::Break || f == Flow::Continue) fail(0, "'توقف' أو 'استمر' خارج حلقة");
    out.flush();
    return 0;
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
