// ═════════════════ مكتبة الدوال العامة (100 دالة) — تُضمَّن داخل Interp::setup() ═════════════════
#define EE_M1(NAME, EXPR)                                  \
  reg(NAME, [](Interp&, Args& a, int ln) {                 \
    argc(NAME, a, 1, 1, ln);                               \
    double x = needNum(NAME, a[0], ln);                    \
    return mkNum(EXPR);                                    \
  })
#define EE_ISTYPE(NAME, COND)                              \
  reg(NAME, [](Interp&, Args& a, int ln) {                 \
    argc(NAME, a, 1, 1, ln);                               \
    const Value& v = a[0];                                 \
    return mkBool(COND);                                   \
  })

// ───── رياضيات (30) ─────
EE_M1("جيب", std::sin(x));
EE_M1("جيب_تمام", std::cos(x));
EE_M1("ظل", std::tan(x));
EE_M1("قوس_جيب", std::asin(x));
EE_M1("قوس_جيب_تمام", std::acos(x));
EE_M1("قوس_ظل", std::atan(x));
EE_M1("لوغ10", std::log10(x));
EE_M1("لوغ2", std::log2(x));
EE_M1("اسي", std::exp(x));
EE_M1("علامة", (x > 0) - (x < 0));
EE_M1("قطع", std::trunc(x));
EE_M1("كسر", x - std::trunc(x));
EE_M1("راديان", x * 3.14159265358979323846 / 180.0);
EE_M1("درجات", x * 180.0 / 3.14159265358979323846);
EE_M1("جذر_تكعيبي", std::cbrt(x));
reg("لوغاريتم", [](Interp&, Args& a, int ln) {
  argc("لوغاريتم", a, 1, 2, ln);
  double x = needNum("لوغاريتم", a[0], ln);
  if (a.size() == 1) return mkNum(std::log(x));
  return mkNum(std::log(x) / std::log(needNum("لوغاريتم", a[1], ln)));
});
reg("وتر", [](Interp&, Args& a, int ln) {
  argc("وتر", a, 2, 2, ln);
  return mkNum(std::hypot(needNum("وتر", a[0], ln), needNum("وتر", a[1], ln)));
});
reg("باقي_موجب", [](Interp&, Args& a, int ln) {
  argc("باقي_موجب", a, 2, 2, ln);
  double x = needNum("باقي_موجب", a[0], ln), m = needNum("باقي_موجب", a[1], ln);
  if (m == 0) fail(ln, "القسمة على صفر");
  double r = std::fmod(x, m);
  if (r < 0) r += std::fabs(m);
  return mkNum(r);
});
reg("حصر", [](Interp&, Args& a, int ln) {  // clamp(x, أدنى, أعلى)
  argc("حصر", a, 3, 3, ln);
  double x = needNum("حصر", a[0], ln), lo = needNum("حصر", a[1], ln), hi = needNum("حصر", a[2], ln);
  if (lo > hi) fail(ln, "الحد الأدنى أكبر من الأعلى في $حصر");
  return mkNum(std::max(lo, std::min(hi, x)));
});
reg("استيفاء", [](Interp&, Args& a, int ln) {  // lerp(أ، ب، ت)
  argc("استيفاء", a, 3, 3, ln);
  double p = needNum("استيفاء", a[0], ln), q = needNum("استيفاء", a[1], ln), t = needNum("استيفاء", a[2], ln);
  return mkNum(p + (q - p) * t);
});
reg("قاسم_مشترك", [](Interp&, Args& a, int ln) {
  argc("قاسم_مشترك", a, 2, 2, ln);
  long long x = std::llabs(static_cast<long long>(needNum("قاسم_مشترك", a[0], ln))), y = std::llabs(static_cast<long long>(needNum("قاسم_مشترك", a[1], ln)));
  while (y) { long long t = x % y; x = y; y = t; }
  return mkNum(static_cast<double>(x));
});
reg("مضاعف_مشترك", [](Interp&, Args& a, int ln) {
  argc("مضاعف_مشترك", a, 2, 2, ln);
  long long x = std::llabs(static_cast<long long>(needNum("مضاعف_مشترك", a[0], ln))), y = std::llabs(static_cast<long long>(needNum("مضاعف_مشترك", a[1], ln)));
  if (!x || !y) return mkNum(0);
  long long g = x, h = y;
  while (h) { long long t = g % h; g = h; h = t; }
  return mkNum(static_cast<double>(x / g) * static_cast<double>(y));
});
reg("مضروب", [](Interp&, Args& a, int ln) {
  argc("مضروب", a, 1, 1, ln);
  double n = needNum("مضروب", a[0], ln);
  if (n < 0 || n != std::floor(n)) fail(ln, "$مضروب يتطلب عدداً صحيحاً غير سالب (وللأعداد الكبيرة استعمل $كبير)");
  if (n > 170) fail(ln, "$مضروب بالأرقام العادية يتجاوز 170 (جرّب $كبير وحلقة)");
  double r = 1;
  for (int i = 2; i <= static_cast<int>(n); i++) r *= i;
  return mkNum(r);
});
reg("توافيق", [](Interp&, Args& a, int ln) {  // C(n, k)
  argc("توافيق", a, 2, 2, ln);
  double n = needNum("توافيق", a[0], ln), k = needNum("توافيق", a[1], ln);
  if (n < 0 || k < 0 || k > n || n != std::floor(n) || k != std::floor(k)) fail(ln, "$توافيق(ن، ك) تتطلب 0 ≤ ك ≤ ن صحيحين");
  k = std::min(k, n - k);
  double r = 1;
  for (double i = 1; i <= k; i++) r = r * (n - k + i) / i;
  return mkNum(std::round(r));
});
reg("تباديل", [](Interp&, Args& a, int ln) {  // P(n, k)
  argc("تباديل", a, 2, 2, ln);
  double n = needNum("تباديل", a[0], ln), k = needNum("تباديل", a[1], ln);
  if (n < 0 || k < 0 || k > n || n != std::floor(n) || k != std::floor(k)) fail(ln, "$تباديل(ن، ك) تتطلب 0 ≤ ك ≤ ن صحيحين");
  double r = 1;
  for (double i = 0; i < k; i++) r *= n - i;
  return mkNum(r);
});
reg("فيبوناتشي", [](Interp&, Args& a, int ln) {
  argc("فيبوناتشي", a, 1, 1, ln);
  double n = needNum("فيبوناتشي", a[0], ln);
  if (n < 0 || n != std::floor(n) || n > 78) fail(ln, "$فيبوناتشي يقبل 0 إلى 78 (فوقها استعمل $كبير)");
  double p = 0, q = 1;
  for (int i = 0; i < static_cast<int>(n); i++) { double t = p + q; p = q; q = t; }
  return mkNum(p);
});
reg("هل_اولي", [](Interp&, Args& a, int ln) {
  argc("هل_اولي", a, 1, 1, ln);
  double d = needNum("هل_اولي", a[0], ln);
  if (d != std::floor(d) || d > 9e15) fail(ln, "$هل_اولي يتطلب عدداً صحيحاً (وللضخم $اولي_كبير)");
  long long n = static_cast<long long>(d);
  if (n < 2) return mkBool(false);
  if (n % 2 == 0) return mkBool(n == 2);
  for (long long i = 3; i * i <= n; i += 2)
    if (n % i == 0) return mkBool(false);
  return mkBool(true);
});
reg("عوامل_اولية", [](Interp&, Args& a, int ln) {
  argc("عوامل_اولية", a, 1, 1, ln);
  double d = needNum("عوامل_اولية", a[0], ln);
  if (d < 2 || d != std::floor(d) || d > 9e15) fail(ln, "$عوامل_اولية يتطلب عدداً صحيحاً ≥ 2");
  long long n = static_cast<long long>(d);
  auto out = std::make_shared<std::vector<Value>>();
  for (long long p = 2; p * p <= n; p += (p == 2 ? 1 : 2))
    while (n % p == 0) { out->push_back(mkNum(static_cast<double>(p))); n /= p; }
  if (n > 1) out->push_back(mkNum(static_cast<double>(n)));
  return mkListP(out);
});
reg("قواسم", [](Interp&, Args& a, int ln) {
  argc("قواسم", a, 1, 1, ln);
  double d = needNum("قواسم", a[0], ln);
  if (d < 1 || d != std::floor(d) || d > 9e12) fail(ln, "$قواسم يتطلب عدداً صحيحاً موجباً");
  long long n = static_cast<long long>(d);
  std::vector<long long> lo, hi;
  for (long long i = 1; i * i <= n; i++)
    if (n % i == 0) { lo.push_back(i); if (i != n / i) hi.push_back(n / i); }
  auto out = std::make_shared<std::vector<Value>>();
  for (long long v : lo) out->push_back(mkNum(static_cast<double>(v)));
  for (size_t i = hi.size(); i-- > 0;) out->push_back(mkNum(static_cast<double>(hi[i])));
  return mkListP(out);
});
reg("مجموع_ارقام", [](Interp&, Args& a, int ln) {
  argc("مجموع_ارقام", a, 1, 1, ln);
  std::string s = a[0].t == VT::Big ? a[0].big->str() : fmtNum(needNum("مجموع_ارقام", a[0], ln));
  double sum = 0;
  for (char c : s) if (c >= '0' && c <= '9') sum += c - '0';
  return mkNum(sum);
});

// ───── إحصاء (6) ─────
reg("متوسط", [](Interp&, Args& a, int ln) {
  argc("متوسط", a, 1, 1, ln);
  if (a[0].t != VT::List || a[0].list->empty()) fail(ln, "$متوسط يتطلب قائمة أرقام غير فارغة");
  double s = 0;
  for (auto& x : *a[0].list) s += needNum("متوسط", x, ln);
  return mkNum(s / static_cast<double>(a[0].list->size()));
});
reg("وسيط", [](Interp&, Args& a, int ln) {
  argc("وسيط", a, 1, 1, ln);
  if (a[0].t != VT::List || a[0].list->empty()) fail(ln, "$وسيط يتطلب قائمة أرقام غير فارغة");
  std::vector<double> v;
  for (auto& x : *a[0].list) v.push_back(needNum("وسيط", x, ln));
  std::sort(v.begin(), v.end());
  size_t n = v.size();
  return mkNum(n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2);
});
reg("منوال", [](Interp&, Args& a, int ln) {
  argc("منوال", a, 1, 1, ln);
  needList("منوال", a[0], ln);
  if (a[0].list->empty()) fail(ln, "$منوال يتطلب قائمة غير فارغة");
  std::vector<std::string> keys;
  std::vector<int> cnt;
  std::vector<size_t> first;
  for (size_t i = 0; i < a[0].list->size(); i++) {
    std::string k = toStr((*a[0].list)[i], true);
    size_t j = 0;
    while (j < keys.size() && keys[j] != k) j++;
    if (j == keys.size()) { keys.push_back(k); cnt.push_back(0); first.push_back(i); }
    cnt[j]++;
  }
  size_t best = 0;
  for (size_t j = 1; j < cnt.size(); j++) if (cnt[j] > cnt[best]) best = j;
  return (*a[0].list)[first[best]];
});
reg("تباين", [](Interp&, Args& a, int ln) {
  argc("تباين", a, 1, 1, ln);
  if (a[0].t != VT::List || a[0].list->empty()) fail(ln, "$تباين يتطلب قائمة أرقام غير فارغة");
  double m = 0;
  for (auto& x : *a[0].list) m += needNum("تباين", x, ln);
  m /= static_cast<double>(a[0].list->size());
  double s = 0;
  for (auto& x : *a[0].list) s += (x.n - m) * (x.n - m);
  return mkNum(s / static_cast<double>(a[0].list->size()));
});
reg("انحراف_معياري", [](Interp&, Args& a, int ln) {
  argc("انحراف_معياري", a, 1, 1, ln);
  if (a[0].t != VT::List || a[0].list->empty()) fail(ln, "$انحراف_معياري يتطلب قائمة أرقام غير فارغة");
  double m = 0;
  for (auto& x : *a[0].list) m += needNum("انحراف_معياري", x, ln);
  m /= static_cast<double>(a[0].list->size());
  double s = 0;
  for (auto& x : *a[0].list) s += (x.n - m) * (x.n - m);
  return mkNum(std::sqrt(s / static_cast<double>(a[0].list->size())));
});
reg("نسبة_مئوية", [](Interp&, Args& a, int ln) {
  argc("نسبة_مئوية", a, 2, 2, ln);
  double w = needNum("نسبة_مئوية", a[1], ln);
  if (w == 0) fail(ln, "القسمة على صفر");
  return mkNum(needNum("نسبة_مئوية", a[0], ln) / w * 100.0);
});

// ───── نصوص (28) ─────
reg("حروف_كبيرة", [](Interp&, Args& a, int ln) {
  argc("حروف_كبيرة", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$حروف_كبيرة يتطلب نصاً");
  std::string s = a[0].s;
  for (char& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
  return mkStr(s);
});
reg("حروف_صغيرة", [](Interp&, Args& a, int ln) {
  argc("حروف_صغيرة", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$حروف_صغيرة يتطلب نصاً");
  std::string s = a[0].s;
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return mkStr(s);
});
reg("اقتطع", [](Interp&, Args& a, int ln) {
  argc("اقتطع", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$اقتطع يتطلب نصاً");
  const std::string& t = a[0].s;
  size_t b = 0, e = t.size();
  while (b < e && std::isspace(static_cast<unsigned char>(t[b]))) b++;
  while (e > b && std::isspace(static_cast<unsigned char>(t[e - 1]))) e--;
  return mkStr(t.substr(b, e - b));
});
reg("يبدا_نص", [](Interp&, Args& a, int ln) {
  argc("يبدا_نص", a, 2, 2, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "$يبدا_نص(نص، بداية) يتطلب نصين");
  return mkBool(a[0].s.compare(0, a[1].s.size(), a[1].s) == 0);
});
reg("ينتهي_نص", [](Interp&, Args& a, int ln) {
  argc("ينتهي_نص", a, 2, 2, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "$ينتهي_نص(نص، نهاية) يتطلب نصين");
  const std::string &s = a[0].s, &e = a[1].s;
  return mkBool(s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0);
});
reg("استبدل", [](Interp&, Args& a, int ln) {
  argc("استبدل", a, 3, 3, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str || a[2].t != VT::Str) fail(ln, "$استبدل(نص، قديم، جديد) يتطلب ثلاثة نصوص");
  if (a[1].s.empty()) return a[0];
  std::string r;
  const std::string& s = a[0].s;
  size_t i = 0;
  while (i < s.size()) {
    if (s.compare(i, a[1].s.size(), a[1].s) == 0) { r += a[2].s; i += a[1].s.size(); }
    else r += s[i++];
    if (r.size() > 50000000) fail(ln, "النص الناتج كبير جداً");
  }
  return mkStr(r);
});
reg("فصل", [](Interp&, Args& a, int ln) {
  argc("فصل", a, 2, 2, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "$فصل(نص، فاصل) يتطلب نصين");
  auto out = std::make_shared<std::vector<Value>>();
  const std::string &s = a[0].s, &sep = a[1].s;
  if (sep.empty()) { for (auto& c : splitChars(s)) out->push_back(mkStr(c)); return mkListP(out); }
  size_t b = 0;
  for (;;) {
    size_t e = s.find(sep, b);
    if (e == std::string::npos) { out->push_back(mkStr(s.substr(b))); break; }
    out->push_back(mkStr(s.substr(b, e - b)));
    b = e + sep.size();
  }
  return mkListP(out);
});
reg("اجمع_نص", [](Interp&, Args& a, int ln) {
  argc("اجمع_نص", a, 1, 2, ln);
  needList("اجمع_نص", a[0], ln);
  std::string sep = a.size() == 2 ? toStr(a[1]) : "";
  std::string r;
  for (size_t i = 0; i < a[0].list->size(); i++) r += (i ? sep : "") + toStr((*a[0].list)[i]);
  return mkStr(r);
});
reg("كرر_نص", [](Interp&, Args& a, int ln) {
  argc("كرر_نص", a, 2, 2, ln);
  if (a[0].t != VT::Str) fail(ln, "$كرر_نص(نص، عدد) يتطلب نصاً");
  double n = needNum("كرر_نص", a[1], ln);
  if (n < 0 || n != std::floor(n) || n * static_cast<double>(a[0].s.size()) > 50e6) fail(ln, "عدد التكرار غير صالح أو النص الناتج كبير جداً");
  std::string r;
  for (long long i = 0; i < static_cast<long long>(n); i++) r += a[0].s;
  return mkStr(r);
});
reg("موضع_نص", [](Interp&, Args& a, int ln) {
  argc("موضع_نص", a, 2, 2, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str) fail(ln, "$موضع_نص(نص، جزء) يتطلب نصين");
  size_t p = a[0].s.find(a[1].s);
  if (p == std::string::npos) return mkNum(-1);
  return mkNum(static_cast<double>(cpCount(a[0].s.substr(0, p))));
});
reg("حرف_كود", [](Interp&, Args& a, int ln) {
  argc("حرف_كود", a, 1, 1, ln);
  if (a[0].t != VT::Str || a[0].s.empty()) fail(ln, "$حرف_كود يتطلب حرفاً");
  size_t l;
  return mkNum(static_cast<double>(decodeAt(a[0].s, 0, l)));
});
reg("كود_حرف", [](Interp&, Args& a, int ln) {
  argc("كود_حرف", a, 1, 1, ln);
  double n = needNum("كود_حرف", a[0], ln);
  if (n < 1 || n > 0x10FFFF || n != std::floor(n)) fail(ln, "رمز يونيكود غير صالح");
  std::string o;
  XmlParser::putUtf8(o, static_cast<uint32_t>(n));
  return mkStr(o);
});
reg("املأ_يمين", [](Interp&, Args& a, int ln) {
  argc("املأ_يمين", a, 2, 3, ln);
  return mkStr(padStr(toStr(a[0]), static_cast<long long>(needNum("املأ_يمين", a[1], ln)), a.size() == 3 ? toStr(a[2]) : " ", false));
});
reg("املأ_يسار", [](Interp&, Args& a, int ln) {
  argc("املأ_يسار", a, 2, 3, ln);
  return mkStr(padStr(toStr(a[0]), static_cast<long long>(needNum("املأ_يسار", a[1], ln)), a.size() == 3 ? toStr(a[2]) : " ", true));
});
reg("عدد_كلمات", [](Interp&, Args& a, int ln) {
  argc("عدد_كلمات", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$عدد_كلمات يتطلب نصاً");
  size_t n = 0;
  bool in = false;
  for (unsigned char c : a[0].s) {
    bool sp = std::isspace(c);
    if (!sp && !in) n++;
    in = !sp;
  }
  return mkNum(static_cast<double>(n));
});
reg("عدد_مرات", [](Interp&, Args& a, int ln) {
  argc("عدد_مرات", a, 2, 2, ln);
  if (a[0].t != VT::Str || a[1].t != VT::Str || a[1].s.empty()) fail(ln, "$عدد_مرات(نص، جزء) يتطلب نصين والجزء غير فارغ");
  size_t n = 0, p = 0;
  while ((p = a[0].s.find(a[1].s, p)) != std::string::npos) { n++; p += a[1].s.size(); }
  return mkNum(static_cast<double>(n));
});
reg("ازل_تشكيل", [](Interp&, Args& a, int ln) {
  argc("ازل_تشكيل", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$ازل_تشكيل يتطلب نصاً");
  std::string o;
  for (size_t i = 0; i < a[0].s.size();) {
    size_t l;
    uint32_t cp = decodeAt(a[0].s, i, l);
    if (!((cp >= 0x64B && cp <= 0x652) || cp == 0x670 || cp == 0x640)) o.append(a[0].s, i, l);
    i += l;
  }
  return mkStr(o);
});
reg("رابط_نظيف", [](Interp&, Args& a, int ln) {  // slug
  argc("رابط_نظيف", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$رابط_نظيف يتطلب نصاً");
  std::string o;
  bool dash = false;
  for (size_t i = 0; i < a[0].s.size();) {
    size_t l;
    uint32_t cp = decodeAt(a[0].s, i, l);
    bool keep = false;
    std::string piece;
    if (cp < 0x80) {
      if (std::isalnum(static_cast<int>(cp))) { keep = true; piece = std::string(1, static_cast<char>(std::tolower(static_cast<int>(cp)))); }
    } else if (digitValue(cp) >= 0) { keep = true; piece = a[0].s.substr(i, l); }
    else if (isIdentStart(cp) && !((cp >= 0x64B && cp <= 0x652) || cp == 0x640)) { keep = true; piece = a[0].s.substr(i, l); }
    if (keep) { if (dash && !o.empty()) o += '-'; o += piece; dash = false; }
    else dash = true;
    i += l;
  }
  return mkStr(o);
});
reg("هل_متناظر", [](Interp&, Args& a, int ln) {
  argc("هل_متناظر", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$هل_متناظر يتطلب نصاً");
  std::vector<std::string> v;
  for (auto& c : splitChars(a[0].s)) {
    size_t l;
    uint32_t cp = decodeAt(c, 0, l);
    if (isIdentStart(cp) || digitValue(cp) >= 0 || (cp < 0x80 && std::isalnum(static_cast<int>(cp))))
      if (!((cp >= 0x64B && cp <= 0x652) || cp == 0x640)) v.push_back(cp < 0x80 ? std::string(1, static_cast<char>(std::tolower(static_cast<int>(cp)))) : c);
  }
  for (size_t i = 0; i < v.size() / 2; i++) if (v[i] != v[v.size() - 1 - i]) return mkBool(false);
  return mkBool(true);
});
reg("ثنائي", [](Interp&, Args& a, int ln) {
  argc("ثنائي", a, 1, 1, ln);
  double d = needNum("ثنائي", a[0], ln);
  if (d != std::floor(d) || std::fabs(d) > 9e15) fail(ln, "$ثنائي يتطلب عدداً صحيحاً");
  unsigned long long n = static_cast<unsigned long long>(std::fabs(d));
  std::string s;
  do { s.insert(s.begin(), static_cast<char>('0' + (n & 1))); n >>= 1; } while (n);
  return mkStr(d < 0 ? "-" + s : s);
});
reg("من_ثنائي", [](Interp&, Args& a, int ln) {
  argc("من_ثنائي", a, 1, 1, ln);
  if (a[0].t != VT::Str) fail(ln, "$من_ثنائي يتطلب نصاً مثل \"1011\"");
  std::string t = toAsciiDigits(a[0].s);
  bool neg = !t.empty() && t[0] == '-';
  double v = 0;
  size_t cnt = 0;
  for (size_t i = neg ? 1 : 0; i < t.size(); i++, cnt++) {
    if (t[i] != '0' && t[i] != '1') fail(ln, "نص ثنائي غير صالح");
    v = v * 2 + (t[i] - '0');
  }
  if (!cnt || cnt > 53) fail(ln, "نص ثنائي غير صالح أو أطول من 53 خانة");
  return mkNum(neg ? -v : v);
});
reg("ست_عشري", [](Interp&, Args& a, int ln) {
  argc("ست_عشري", a, 1, 1, ln);
  double d = needNum("ست_عشري", a[0], ln);
  if (d != std::floor(d) || std::fabs(d) > 9e15) fail(ln, "$ست_عشري يتطلب عدداً صحيحاً");
  char buf[32];
  std::snprintf(buf, sizeof buf, "%llx", static_cast<unsigned long long>(std::fabs(d)));
  return mkStr(std::string(d < 0 ? "-" : "") + buf);
});
reg("تجزئة", [](Interp&, Args& a, int ln) {  // FNV-1a 32 بت
  argc("تجزئة", a, 1, 1, ln);
  uint32_t h = 2166136261u;
  for (unsigned char c : toStr(a[0])) { h ^= c; h *= 16777619u; }
  char buf[16];
  std::snprintf(buf, sizeof buf, "%08x", h);
  return mkStr(buf);
});
reg("رمز_url", [](Interp&, Args& a, int ln) {
  argc("رمز_url", a, 1, 1, ln);
  std::string o;
  char buf[8];
  for (unsigned char c : toStr(a[0])) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += static_cast<char>(c);
    else { std::snprintf(buf, sizeof buf, "%%%02X", c); o += buf; }
  }
  return mkStr(o);
});
reg("هرب_html", [](Interp&, Args& a, int ln) {
  argc("هرب_html", a, 1, 1, ln);
  return mkStr(htmlEsc(toStr(a[0])));
});
reg("رقم_بالحروف", [](Interp&, Args& a, int ln) {
  argc("رقم_بالحروف", a, 1, 1, ln);
  double d = needNum("رقم_بالحروف", a[0], ln);
  if (d != std::floor(d) || std::fabs(d) >= 1e12) fail(ln, "$رقم_بالحروف يتطلب عدداً صحيحاً أقل من تريليون");
  return mkStr(numToWords(static_cast<long long>(d)));
});
reg("تنسيق_رقم", [](Interp&, Args& a, int ln) {
  argc("تنسيق_رقم", a, 1, 2, ln);
  std::string s = fmtNum(needNum("تنسيق_رقم", a[0], ln));
  std::string sep = a.size() == 2 ? toStr(a[1]) : ",";
  bool neg = !s.empty() && s[0] == '-';
  if (neg) s.erase(0, 1);
  size_t dot = s.find('.');
  std::string ip = s.substr(0, dot), fp = dot == std::string::npos ? "" : s.substr(dot);
  std::string o;
  for (size_t i = 0; i < ip.size(); i++) {
    if (i && (ip.size() - i) % 3 == 0) o += sep;
    o += ip[i];
  }
  return mkStr((neg ? "-" : "") + o + fp);
});
reg("ابتر", [](Interp&, Args& a, int ln) {
  argc("ابتر", a, 2, 3, ln);
  auto ch = splitChars(toStr(a[0]));
  long long n = static_cast<long long>(needNum("ابتر", a[1], ln));
  if (n < 0 || static_cast<long long>(ch.size()) <= n) return mkStr(toStr(a[0]));
  std::string r;
  for (long long i = 0; i < n; i++) r += ch[static_cast<size_t>(i)];
  return mkStr(r + (a.size() == 3 ? toStr(a[2]) : "…"));
});

// ───── قوائم (22) ─────
reg("ادرج", [](Interp&, Args& a, int ln) {
  argc("ادرج", a, 3, 3, ln);
  needList("ادرج", a[0], ln);
  double i = needNum("ادرج", a[1], ln);
  long long n = static_cast<long long>(a[0].list->size()), p = static_cast<long long>(i);
  if (p < 0) p += n;
  if (i != std::floor(i) || p < 0 || p > n) fail(ln, "موضع الإدراج خارج الحدود");
  a[0].list->insert(a[0].list->begin() + p, a[2]);
  return a[0];
});
reg("احذف_عند", [](Interp&, Args& a, int ln) {
  argc("احذف_عند", a, 2, 2, ln);
  needList("احذف_عند", a[0], ln);
  double i = needNum("احذف_عند", a[1], ln);
  long long n = static_cast<long long>(a[0].list->size()), p = static_cast<long long>(i);
  if (p < 0) p += n;
  if (i != std::floor(i) || p < 0 || p >= n) fail(ln, "موضع الحذف خارج الحدود");
  Value r = (*a[0].list)[static_cast<size_t>(p)];
  a[0].list->erase(a[0].list->begin() + p);
  return r;
});
reg("اول", [](Interp&, Args& a, int ln) {
  argc("اول", a, 1, 1, ln);
  needList("اول", a[0], ln);
  return a[0].list->empty() ? Value() : a[0].list->front();
});
reg("اخر", [](Interp&, Args& a, int ln) {
  argc("اخر", a, 1, 1, ln);
  needList("اخر", a[0], ln);
  return a[0].list->empty() ? Value() : a[0].list->back();
});
reg("فريد", [](Interp&, Args& a, int ln) {
  argc("فريد", a, 1, 1, ln);
  needList("فريد", a[0], ln);
  std::unordered_set<std::string> seen;
  auto out = std::make_shared<std::vector<Value>>();
  for (auto& x : *a[0].list) if (seen.insert(toStr(x, true)).second) out->push_back(x);
  return mkListP(out);
});
reg("سطح", [](Interp&, Args& a, int ln) {
  argc("سطح", a, 1, 2, ln);
  needList("سطح", a[0], ln);
  double depth = a.size() == 2 ? needNum("سطح", a[1], ln) : 1e9;
  auto out = std::make_shared<std::vector<Value>>();
  std::function<void(const Value&, double)> go = [&](const Value& v, double d) {
    for (auto& x : *v.list) {
      if (x.t == VT::List && d > 0) go(x, d - 1);
      else out->push_back(x);
      if (out->size() > 50000000) fail(ln, "القائمة الناتجة كبيرة جداً");
    }
  };
  go(a[0], depth);
  return mkListP(out);
});
reg("قطع_قائمة", [](Interp&, Args& a, int ln) {
  argc("قطع_قائمة", a, 2, 2, ln);
  needList("قطع_قائمة", a[0], ln);
  double n = needNum("قطع_قائمة", a[1], ln);
  if (n < 1 || n != std::floor(n)) fail(ln, "حجم القطعة عدد صحيح موجب");
  auto out = std::make_shared<std::vector<Value>>();
  size_t k = static_cast<size_t>(n);
  for (size_t i = 0; i < a[0].list->size(); i += k) {
    auto part = std::make_shared<std::vector<Value>>(a[0].list->begin() + static_cast<long>(i),
                                                     a[0].list->begin() + static_cast<long>(std::min(i + k, a[0].list->size())));
    out->push_back(mkListP(part));
  }
  return mkListP(out);
});
reg("خذ", [](Interp&, Args& a, int ln) {
  argc("خذ", a, 2, 2, ln);
  needList("خذ", a[0], ln);
  double n = std::max(0.0, needNum("خذ", a[1], ln));
  size_t k = std::min(static_cast<size_t>(n), a[0].list->size());
  return mkListP(std::make_shared<std::vector<Value>>(a[0].list->begin(), a[0].list->begin() + static_cast<long>(k)));
});
reg("اسقط", [](Interp&, Args& a, int ln) {
  argc("اسقط", a, 2, 2, ln);
  needList("اسقط", a[0], ln);
  double n = std::max(0.0, needNum("اسقط", a[1], ln));
  size_t k = std::min(static_cast<size_t>(n), a[0].list->size());
  return mkListP(std::make_shared<std::vector<Value>>(a[0].list->begin() + static_cast<long>(k), a[0].list->end()));
});
reg("موضع", [](Interp&, Args& a, int ln) {
  argc("موضع", a, 2, 2, ln);
  needList("موضع", a[0], ln);
  for (size_t i = 0; i < a[0].list->size(); i++) if (equals((*a[0].list)[i], a[1])) return mkNum(static_cast<double>(i));
  return mkNum(-1);
});
reg("عدد_قيمة", [](Interp&, Args& a, int ln) {
  argc("عدد_قيمة", a, 2, 2, ln);
  needList("عدد_قيمة", a[0], ln);
  size_t n = 0;
  for (auto& x : *a[0].list) if (equals(x, a[1])) n++;
  return mkNum(static_cast<double>(n));
});
reg("اخلط", [](Interp& I, Args& a, int ln) {
  argc("اخلط", a, 1, 1, ln);
  needList("اخلط", a[0], ln);
  auto v = std::make_shared<std::vector<Value>>(*a[0].list);
  std::shuffle(v->begin(), v->end(), I.rng);
  return mkListP(v);
});
reg("اختر", [](Interp& I, Args& a, int ln) {
  argc("اختر", a, 1, 1, ln);
  needList("اختر", a[0], ln);
  if (a[0].list->empty()) return Value();
  return (*a[0].list)[std::uniform_int_distribution<size_t>(0, a[0].list->size() - 1)(I.rng)];
});
reg("تقاطع", [](Interp&, Args& a, int ln) {
  argc("تقاطع", a, 2, 2, ln);
  needList("تقاطع", a[0], ln);
  needList("تقاطع", a[1], ln);
  std::unordered_set<std::string> in;
  for (auto& x : *a[1].list) in.insert(toStr(x, true));
  std::unordered_set<std::string> seen;
  auto out = std::make_shared<std::vector<Value>>();
  for (auto& x : *a[0].list) { std::string k = toStr(x, true); if (in.count(k) && seen.insert(k).second) out->push_back(x); }
  return mkListP(out);
});
reg("اتحاد", [](Interp&, Args& a, int ln) {
  argc("اتحاد", a, 2, 2, ln);
  needList("اتحاد", a[0], ln);
  needList("اتحاد", a[1], ln);
  std::unordered_set<std::string> seen;
  auto out = std::make_shared<std::vector<Value>>();
  for (auto* l : {a[0].list.get(), a[1].list.get()})
    for (auto& x : *l) if (seen.insert(toStr(x, true)).second) out->push_back(x);
  return mkListP(out);
});
reg("فرق", [](Interp&, Args& a, int ln) {
  argc("فرق", a, 2, 2, ln);
  needList("فرق", a[0], ln);
  needList("فرق", a[1], ln);
  std::unordered_set<std::string> ex;
  for (auto& x : *a[1].list) ex.insert(toStr(x, true));
  auto out = std::make_shared<std::vector<Value>>();
  for (auto& x : *a[0].list) if (!ex.count(toStr(x, true))) out->push_back(x);
  return mkListP(out);
});
reg("تكرارات", [](Interp&, Args& a, int ln) {
  argc("تكرارات", a, 1, 1, ln);
  needList("تكرارات", a[0], ln);
  std::unordered_map<std::string, size_t> idx;
  auto out = std::make_shared<std::vector<Value>>();
  for (auto& x : *a[0].list) {
    std::string k = toStr(x, true);
    auto it = idx.find(k);
    if (it == idx.end()) {
      idx[k] = out->size();
      auto pr = std::make_shared<std::vector<Value>>();
      pr->push_back(x);
      pr->push_back(mkNum(1));
      out->push_back(mkListP(pr));
    } else {
      (*(*out)[it->second].list)[1].n += 1;
    }
  }
  return mkListP(out);
});
reg("ادر", [](Interp&, Args& a, int ln) {  // تدوير القائمة: موجب لليسار
  argc("ادر", a, 2, 2, ln);
  needList("ادر", a[0], ln);
  auto v = std::make_shared<std::vector<Value>>(*a[0].list);
  if (!v->empty()) {
    long long n = static_cast<long long>(v->size()), k = static_cast<long long>(needNum("ادر", a[1], ln)) % n;
    if (k < 0) k += n;
    std::rotate(v->begin(), v->begin() + k, v->end());
  }
  return mkListP(v);
});
reg("املأ", [](Interp&, Args& a, int ln) {
  argc("املأ", a, 2, 2, ln);
  double n = needNum("املأ", a[0], ln);
  if (n < 0 || n != std::floor(n) || n > 10e6) fail(ln, "حجم $املأ يجب أن يكون عدداً صحيحاً بين 0 و10000000");
  return mkListP(std::make_shared<std::vector<Value>>(static_cast<size_t>(n), a[1]));
});
reg("تراكمي", [](Interp&, Args& a, int ln) {
  argc("تراكمي", a, 1, 1, ln);
  needList("تراكمي", a[0], ln);
  auto out = std::make_shared<std::vector<Value>>();
  Value acc;
  for (size_t i = 0; i < a[0].list->size(); i++) {
    acc = i == 0 ? (*a[0].list)[0] : binop("+", acc, (*a[0].list)[i], ln);
    out->push_back(acc);
  }
  return mkListP(out);
});
reg("اعلى", [](Interp&, Args& a, int ln) {
  argc("اعلى", a, 1, 1, ln);
  needList("اعلى", a[0], ln);
  if (a[0].list->empty()) return Value();
  Value m = (*a[0].list)[0];
  for (auto& x : *a[0].list) if (truthy(binop(">", x, m, ln))) m = x;
  return m;
});
reg("ادنى", [](Interp&, Args& a, int ln) {
  argc("ادنى", a, 1, 1, ln);
  needList("ادنى", a[0], ln);
  if (a[0].list->empty()) return Value();
  Value m = (*a[0].list)[0];
  for (auto& x : *a[0].list) if (truthy(binop("<", x, m, ln))) m = x;
  return m;
});

// ───── تواريخ (8): الأوقات بالثواني منذ 1970 بالتوقيت العالمي ─────
reg("الان", [](Interp&, Args& a, int ln) {
  argc("الان", a, 0, 0, ln);
  return mkNum(static_cast<double>(std::time(nullptr)));
});
reg("سنة_من", [](Interp&, Args& a, int ln) {
  argc("سنة_من", a, 1, 1, ln);
  long long ts = static_cast<long long>(std::floor(needNum("سنة_من", a[0], ln)));
  long long days = ts >= 0 ? ts / 86400 : -((-ts + 86399) / 86400), y; unsigned m, d;
  civilFromDays(days, y, m, d);
  return mkNum(static_cast<double>(y));
});
reg("شهر_من", [](Interp&, Args& a, int ln) {
  argc("شهر_من", a, 1, 1, ln);
  long long ts = static_cast<long long>(std::floor(needNum("شهر_من", a[0], ln)));
  long long days = ts >= 0 ? ts / 86400 : -((-ts + 86399) / 86400), y; unsigned m, d;
  civilFromDays(days, y, m, d);
  return mkNum(m);
});
reg("يوم_من", [](Interp&, Args& a, int ln) {
  argc("يوم_من", a, 1, 1, ln);
  long long ts = static_cast<long long>(std::floor(needNum("يوم_من", a[0], ln)));
  long long days = ts >= 0 ? ts / 86400 : -((-ts + 86399) / 86400), y; unsigned m, d;
  civilFromDays(days, y, m, d);
  return mkNum(d);
});
reg("اسم_الشهر", [](Interp&, Args& a, int ln) {
  argc("اسم_الشهر", a, 1, 1, ln);
  static const char* M[] = {"يناير", "فبراير", "مارس", "أبريل", "مايو", "يونيو", "يوليو", "أغسطس", "سبتمبر", "أكتوبر", "نوفمبر", "ديسمبر"};
  double n = needNum("اسم_الشهر", a[0], ln);
  if (n < 1 || n > 12 || n != std::floor(n)) fail(ln, "رقم الشهر من 1 إلى 12");
  return mkStr(M[static_cast<int>(n) - 1]);
});
reg("اسم_اليوم", [](Interp&, Args& a, int ln) {  // 0 = الأحد
  argc("اسم_اليوم", a, 1, 1, ln);
  static const char* D[] = {"الأحد", "الاثنين", "الثلاثاء", "الأربعاء", "الخميس", "الجمعة", "السبت"};
  double n = needNum("اسم_اليوم", a[0], ln);
  if (n < 0 || n > 6 || n != std::floor(n)) fail(ln, "رقم اليوم من 0 (الأحد) إلى 6 (السبت)");
  return mkStr(D[static_cast<int>(n)]);
});
reg("تاريخ_منسق", [](Interp&, Args& a, int ln) {
  argc("تاريخ_منسق", a, 1, 1, ln);
  static const char* M[] = {"يناير", "فبراير", "مارس", "أبريل", "مايو", "يونيو", "يوليو", "أغسطس", "سبتمبر", "أكتوبر", "نوفمبر", "ديسمبر"};
  static const char* D[] = {"الأحد", "الاثنين", "الثلاثاء", "الأربعاء", "الخميس", "الجمعة", "السبت"};
  long long ts = static_cast<long long>(std::floor(needNum("تاريخ_منسق", a[0], ln)));
  long long days = ts >= 0 ? ts / 86400 : -((-ts + 86399) / 86400), y; unsigned m, d;
  civilFromDays(days, y, m, d);
  int wd = static_cast<int>(((days % 7) + 11) % 7);  // 1970-01-01 كان خميساً
  return mkStr(std::string(D[wd]) + " " + std::to_string(d) + " " + M[m - 1] + " " + std::to_string(y));
});
reg("بناء_وقت", [](Interp&, Args& a, int ln) {  // (سنة، شهر، يوم، ساعة، دقيقة، ثانية)
  argc("بناء_وقت", a, 3, 6, ln);
  double v[6] = {0, 1, 1, 0, 0, 0};
  for (size_t i = 0; i < a.size(); i++) v[i] = needNum("بناء_وقت", a[i], ln);
  if (v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31) fail(ln, "شهر أو يوم غير صالح في $بناء_وقت");
  long long days = daysFromCivil(static_cast<long long>(v[0]), static_cast<unsigned>(v[1]), static_cast<unsigned>(v[2]));
  return mkNum(static_cast<double>(days) * 86400 + v[3] * 3600 + v[4] * 60 + v[5]);
});

// ───── أنواع (4) وألوان (2) ─────
EE_ISTYPE("هل_نص", v.t == VT::Str);
EE_ISTYPE("هل_رقم", v.t == VT::Num || v.t == VT::Big);
EE_ISTYPE("هل_قائمة", v.t == VT::List);
EE_ISTYPE("هل_عدم", v.t == VT::Null);
reg("لون_rgb", [](Interp&, Args& a, int ln) {
  argc("لون_rgb", a, 3, 3, ln);
  return mkStr(colorHex(needNum("لون_rgb", a[0], ln), needNum("لون_rgb", a[1], ln), needNum("لون_rgb", a[2], ln)));
});
reg("لون_hsl", [](Interp&, Args& a, int ln) {  // الدرجة 0-360، التشبع والإضاءة 0-100
  argc("لون_hsl", a, 3, 3, ln);
  double h = std::fmod(needNum("لون_hsl", a[0], ln), 360.0), s = needNum("لون_hsl", a[1], ln) / 100.0, l = needNum("لون_hsl", a[2], ln) / 100.0;
  if (h < 0) h += 360;
  s = std::max(0.0, std::min(1.0, s));
  l = std::max(0.0, std::min(1.0, l));
  double c = (1 - std::fabs(2 * l - 1)) * s, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1)), m = l - c / 2, r = 0, g = 0, b = 0;
  if (h < 60) { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else { r = c; b = x; }
  return mkStr(colorHex((r + m) * 255, (g + m) * 255, (b + m) * 255));
});
#undef EE_M1
#undef EE_ISTYPE
