// EE# — الواجهة الأمامية المشتركة: أدوات UTF-8 + المحلل اللفظي + الشجرة النحوية + المحلل النحوي.
// يستعملها المنفّذ (ee.cpp) ومحرك الترجمة (translate.cpp).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace ee {
namespace {
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

enum class EK { Num, Str, Bool, Null, Var, List, Unary, Binary, And, Or, Assign, Index, Call, Lambda, FnRef, BigLit };

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

}  // namespace
}  // namespace ee
