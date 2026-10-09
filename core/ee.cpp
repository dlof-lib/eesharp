// EE# — مفسّر لغة البرمجة العربية (C++17)
// المراحل: محلل لفظي (lex) ← محلل نحوي (Parser) ← منفّذ شجري (Interp)

#include "ee.h"
#include "front.h"

#include <algorithm>
#include <atomic>
#include <chrono>
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

const char* version() { return "0.2.0"; }

namespace {

std::atomic<bool> g_stop{false};
std::string g_platform = "طرفية";


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

enum class VT { Null, Num, Bool, Str, List, Func, Native, Big };

struct Value {
  VT t = VT::Null;
  double n = 0;
  bool b = false;
  std::string s;
  std::shared_ptr<std::vector<Value>> list;
  std::shared_ptr<FuncObj> fn;
  std::shared_ptr<NativeObj> nat;
  std::shared_ptr<BigInt> big;
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
Value mkBig(const BigInt& b) { Value v; v.t = VT::Big; v.big = std::make_shared<BigInt>(b); return v; }
Value mkListP(std::shared_ptr<std::vector<Value>> l) { Value v; v.t = VT::List; v.list = std::move(l); return v; }

std::string typeName(const Value& v) {
  switch (v.t) {
    case VT::Null: return "عدم";
    case VT::Num: return "رقم";
    case VT::Big: return "عدد_كبير";
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

Value binop(const std::string& op, const Value& a, const Value& b, int line) {
  if (op == "==") return mkBool(equals(a, b));
  if (op == "!=") return mkBool(!equals(a, b));
  if ((a.t == VT::Big || b.t == VT::Big) && a.t != VT::Str && b.t != VT::Str)
    return bigBinop(op, a, b, line);
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
  std::string platform = g_platform;  // قيمة <<منصة>>
  ShellHandler shell;                  // الصدفة: جسر المضيف (قد تكون فارغة)
  std::vector<Value> handlers;         // مفاهيم ربطها البرنامج بأحداث المضيف
  bool replMode = false;               // الطرفية: السماح بإعادة تعريف المتغيرات العامة

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

  // ── الصدفة ──
  void refreshHostVars() {
    globals->vars["منصة"] = mkStr(platform);
    globals->vars["مضيف"] = mkBool(static_cast<bool>(shell));
  }

  Value shellCall(const std::string& cmd, const Args& a, int ln) {
    if (!shell)
      fail(ln, "لا توجد صدفة مضيفة في هذه البيئة (المنصة: " + platform + ") — الأمر: " + cmd);
    std::vector<std::string> sargs;
    for (const Value& v : a) {
      if (v.t == VT::Func || v.t == VT::Native)
        fail(ln, "لا يمكن تمرير مفهوم إلى الصدفة مباشرة؛ استعمل $صدفة_اربط أو $صدفة_انتظر");
      sargs.push_back(toStr(v));
    }
    try {
      return mkStr(shell(cmd, sargs));
    } catch (const EEError&) {
      throw;
    } catch (const std::exception& e) {
      fail(ln, "الصدفة (" + cmd + "): " + e.what());
    }
  }

  int addHandler(const Value& f, const char* who, int ln) {
    if (f.t != VT::Func && f.t != VT::Native)
      fail(ln, std::string("المعامل الأخير في $") + who + " يجب أن يكون مفهوماً (مثل $عند_النقر)");
    handlers.push_back(f);
    return static_cast<int>(handlers.size()) - 1;
  }

  void setupShell() {
    auto shellFn = [this](const std::string& name, const std::string& cmd, size_t lo, size_t hi) {
      reg(name, [name, cmd, lo, hi](Interp& I, Args& a, int ln) {
        argc(name.c_str(), a, lo, hi, ln);
        return I.shellCall(cmd, a, ln);
      });
    };
    reg("صدفة", [](Interp& I, Args& a, int ln) {
      argc("صدفة", a, 1, 64, ln);
      if (a[0].t != VT::Str) fail(ln, "المعامل الأول في $صدفة هو اسم الأمر (نص)، مثل \"dom.text\"");
      Args rest(a.begin() + 1, a.end());
      return I.shellCall(a[0].s, rest, ln);
    });
    reg("صدفة_متاح", [](Interp& I, Args& a, int ln) {
      argc("صدفة_متاح", a, 0, 0, ln);
      return mkBool(static_cast<bool>(I.shell));
    });
    reg("منصة", [](Interp& I, Args& a, int ln) {
      argc("منصة", a, 0, 0, ln);
      return mkStr(I.platform);
    });
    reg("على_منصة", [](Interp& I, Args& a, int ln) {
      argc("على_منصة", a, 1, 1, ln);
      return mkBool(normalizeKw(toStr(a[0])) == normalizeKw(I.platform));
    });
    // عناصر الصفحة (ويب / WebView)
    shellFn("صدفة_نص", "dom.text", 1, 2);
    shellFn("صدفة_قيمة", "dom.value", 1, 2);
    shellFn("صدفة_html", "dom.html", 1, 2);
    shellFn("صدفة_خاصية", "dom.attr", 2, 3);
    shellFn("صدفة_نمط", "dom.style", 3, 3);
    shellFn("صدفة_صنف", "dom.class", 3, 3);
    shellFn("صدفة_اضف", "dom.append", 2, 2);
    // واجهة وتخزين
    shellFn("صدفة_تنبيه", "ui.alert", 1, 1);
    shellFn("صدفة_رسالة", "ui.toast", 1, 1);
    shellFn("صدفة_سجل", "console.log", 1, 1);
    shellFn("صدفة_خزن", "store.set", 2, 2);
    shellFn("صدفة_اقرأ_مخزن", "store.get", 1, 1);
    shellFn("صدفة_js", "js.eval", 1, 1);
    // سطح المكتب
    shellFn("صدفة_ملف_اقرأ", "fs.read", 1, 1);
    shellFn("صدفة_ملف_اكتب", "fs.write", 2, 2);
    shellFn("صدفة_نظام", "os.exec", 1, 1);
    shellFn("صدفة_بيئة", "os.env", 1, 1);
    shellFn("صدفة_الغ_مؤقت", "timer.clear", 1, 1);
    // الأحداث: آخر معامل مفهوم يستدعيه المضيف لاحقاً
    reg("صدفة_اربط", [](Interp& I, Args& a, int ln) {
      argc("صدفة_اربط", a, 3, 3, ln);
      int id = I.addHandler(a[2], "صدفة_اربط", ln);
      Args h{a[0], a[1], mkNum(id)};
      return I.shellCall("dom.on", h, ln);
    });
    reg("صدفة_انتظر", [](Interp& I, Args& a, int ln) {
      argc("صدفة_انتظر", a, 2, 2, ln);
      int id = I.addHandler(a[1], "صدفة_انتظر", ln);
      Args h{a[0], mkNum(id)};
      return I.shellCall("timer.after", h, ln);
    });
    reg("صدفة_كرر_كل", [](Interp& I, Args& a, int ln) {
      argc("صدفة_كرر_كل", a, 2, 2, ln);
      int id = I.addHandler(a[1], "صدفة_كرر_كل", ln);
      Args h{a[0], mkNum(id)};
      return I.shellCall("timer.every", h, ln);
    });
  }

  // ── مفاهيم نصية إضافية ──
  void setupText() {
    reg("استبدل", [](Interp&, Args& a, int ln) {
      argc("استبدل", a, 3, 3, ln);
      if (a[0].t != VT::Str || a[1].t != VT::Str || a[2].t != VT::Str)
        fail(ln, "المفهوم $استبدل يتطلب ثلاثة نصوص: (النص، القديم، الجديد)");
      if (a[1].s.empty()) fail(ln, "النص القديم في $استبدل لا يمكن أن يكون فارغاً");
      std::string r;
      size_t pos = 0;
      for (;;) {
        size_t f = a[0].s.find(a[1].s, pos);
        if (f == std::string::npos) { r.append(a[0].s, pos, std::string::npos); break; }
        r.append(a[0].s, pos, f - pos);
        r += a[2].s;
        pos = f + a[1].s.size();
      }
      return mkStr(r);
    });
    reg("قسم", [](Interp&, Args& a, int ln) {
      argc("قسم", a, 2, 2, ln);
      if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "المفهوم $قسم يتطلب نصين: (النص، الفاصل)");
      auto l = std::make_shared<std::vector<Value>>();
      if (a[1].s.empty()) {
        for (auto& c : splitChars(a[0].s)) l->push_back(mkStr(c));
      } else {
        size_t pos = 0;
        for (;;) {
          size_t f = a[0].s.find(a[1].s, pos);
          if (f == std::string::npos) { l->push_back(mkStr(a[0].s.substr(pos))); break; }
          l->push_back(mkStr(a[0].s.substr(pos, f - pos)));
          pos = f + a[1].s.size();
        }
      }
      return mkListP(l);
    });
    reg("دمج", [](Interp&, Args& a, int ln) {
      argc("دمج", a, 1, 2, ln);
      if (a[0].t != VT::List) fail(ln, "المفهوم $دمج يتطلب قائمة");
      std::string sep = a.size() == 2 ? toStr(a[1]) : "";
      std::string r;
      for (size_t i = 0; i < a[0].list->size(); i++) {
        if (i) r += sep;
        r += toStr((*a[0].list)[i]);
      }
      return mkStr(r);
    });
    reg("قص_فراغ", [](Interp&, Args& a, int ln) {
      argc("قص_فراغ", a, 1, 1, ln);
      if (a[0].t != VT::Str) fail(ln, "المفهوم $قص_فراغ يتطلب نصاً");
      const std::string& s = a[0].s;
      size_t b = 0, e = s.size();
      auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
      while (b < e && ws(s[b])) b++;
      while (e > b && ws(s[e - 1])) e--;
      return mkStr(s.substr(b, e - b));
    });
    auto caseFn = [](bool upper) {
      return [upper](Interp&, Args& a, int ln) {
        argc(upper ? "حروف_كبيرة" : "حروف_صغيرة", a, 1, 1, ln);
        if (a[0].t != VT::Str) fail(ln, "هذا المفهوم يتطلب نصاً");
        std::string r = a[0].s;
        for (char& c : r)
          if (static_cast<unsigned char>(c) < 0x80)
            c = static_cast<char>(upper ? std::toupper(static_cast<unsigned char>(c))
                                        : std::tolower(static_cast<unsigned char>(c)));
        return mkStr(r);
      };
    };
    reg("حروف_كبيرة", caseFn(true));
    reg("حروف_صغيرة", caseFn(false));
    reg("يبدأ_ب", [](Interp&, Args& a, int ln) {
      argc("يبدأ_ب", a, 2, 2, ln);
      if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "المفهوم $يبدأ_ب يتطلب نصين");
      return mkBool(a[0].s.compare(0, a[1].s.size(), a[1].s) == 0);
    });
    reg("ينتهي_ب", [](Interp&, Args& a, int ln) {
      argc("ينتهي_ب", a, 2, 2, ln);
      if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "المفهوم $ينتهي_ب يتطلب نصين");
      const std::string &s = a[0].s, &t = a[1].s;
      return mkBool(s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0);
    });
    reg("موضع", [](Interp&, Args& a, int ln) {
      argc("موضع", a, 2, 2, ln);
      if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "المفهوم $موضع يتطلب نصين");
      size_t f = a[0].s.find(a[1].s);
      if (f == std::string::npos) return mkNum(-1);
      return mkNum(static_cast<double>(splitChars(a[0].s.substr(0, f)).size()));
    });
    reg("وقت", [](Interp&, Args& a, int ln) {
      argc("وقت", a, 0, 0, ln);
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
      return mkNum(static_cast<double>(ms));
    });
  }

  void setup() {
    globals->vars["باي"] = mkNum(3.14159265358979323846);
    globals->vars["اصدار"] = mkStr(version());
    refreshHostVars();

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

    // مرادفات عربية أبسط
    alias("اعرض", "اطبع");
    alias("اسأل", "اقرأ");
    alias("اسأل_رقم", "اقرأ_رقم");

    setupText();
    setupShell();
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
    if (replMode && env == globals) {
      env->vars[name] = v;
      return;
    }
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

void set_default_platform(const std::string& name) { g_platform = name; }

namespace {
void printError(std::ostream& out, std::ostream& err, const EEError& e) {
  out.flush();
  err << "\n⚠ خطأ";
  if (e.line > 0) err << " (السطر " << e.line << ")";
  err << ": " << e.msg << "\n";
  err.flush();
}
}  // namespace

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
    printError(out, err, e);
    return 1;
  } catch (const std::exception& e) {
    out.flush();
    err << "\n⚠ خطأ داخلي: " << e.what() << "\n";
    err.flush();
    return 1;
  }
}

// ───────────────────────── الجلسة ─────────────────────────

struct Session::Impl {
  Interp I;
  std::ostream& out;
  std::ostream& err;
  Impl(std::istream& in, std::ostream& o, std::ostream& e, size_t m) : I(in, o, m), out(o), err(e) {
    I.setup();
  }

  template <typename F>
  int guarded(F&& f) {
    g_stop.store(false);
    I.written = 0;
    I.depth = 0;
    try {
      f();
      out.flush();
      return 0;
    } catch (const EEError& e) {
      printError(out, err, e);
      return 1;
    } catch (const std::exception& e) {
      out.flush();
      err << "\n⚠ خطأ داخلي: " << e.what() << "\n";
      err.flush();
      return 1;
    }
  }
};

Session::Session(std::istream& in, std::ostream& out, std::ostream& err, std::size_t max_output)
    : impl_(new Impl(in, out, err, max_output)) {}
Session::~Session() = default;

void Session::set_platform(const std::string& name) {
  impl_->I.platform = name;
  impl_->I.refreshHostVars();
}
void Session::set_shell(ShellHandler handler) {
  impl_->I.shell = std::move(handler);
  impl_->I.refreshHostVars();
}
void Session::set_repl(bool on) { impl_->I.replMode = on; }

int Session::eval(const std::string& source, bool echo_result) {
  return impl_->guarded([&] {
    Parser ps;
    ps.t = lex(source);
    std::vector<SP> prog = ps.parseProgram();
    Interp& I = impl_->I;
    for (size_t i = 0; i < prog.size(); i++) {
      const SP& st = prog[i];
      if (echo_result && i + 1 == prog.size() && st->k == SK::Expr && st->e->k != EK::Assign) {
        Value v = I.eval(st->e, I.globals);
        if (v.t != VT::Null) I.emit(st->line, toStr(v, true) + "\n");
        break;
      }
      Flow f = I.execStmt(st, I.globals);
      if (f == Flow::Return) break;
      if (f == Flow::Break || f == Flow::Continue) fail(st->line, "'توقف' أو 'استمر' خارج حلقة");
    }
  });
}

bool Session::is_complete(const std::string& source) {
  try {
    Parser ps;
    ps.t = lex(source);
    ps.parseProgram();
    return true;
  } catch (const EEError& e) {
    return e.msg.find("لم تُغلق") == std::string::npos &&
           e.msg.find("نهاية البرنامج") == std::string::npos &&
           e.msg.find("تعليق غير مغلق") == std::string::npos;
  } catch (...) {
    return true;
  }
}

static Value hostArg(const std::string& s) { return mkStr(s); }

int Session::call(const std::string& name, const std::vector<std::string>& args) {
  return impl_->guarded([&] {
    Interp& I = impl_->I;
    std::string n = !name.empty() && name[0] == '$' ? name.substr(1) : name;
    Value* f = I.lookupFn(n, I.globals);
    if (!f) fail(0, "المفهوم $" + n + " غير معرّف");
    Value fv = *f;
    Args a;
    for (auto& x : args) a.push_back(hostArg(x));
    I.call(fv, a, 0);
  });
}

int Session::dispatch(int handler_id, const std::vector<std::string>& args) {
  return impl_->guarded([&] {
    Interp& I = impl_->I;
    if (handler_id < 0 || static_cast<size_t>(handler_id) >= I.handlers.size())
      fail(0, "معالج حدث غير معروف: " + std::to_string(handler_id));
    Value fv = I.handlers[static_cast<size_t>(handler_id)];
    Args a;
    for (auto& x : args) a.push_back(hostArg(x));
    if (fv.t == VT::Func) {  // المعالج يأخذ ما يحتاجه من معاملات الحدث
      size_t want = fv.fn->def->params.size();
      a.resize(want);
    }
    I.call(fv, a, 0);
  });
}

}  // namespace ee
