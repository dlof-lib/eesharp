// ───────────── مساعدات مكتبة الدوال العامة (تُضمَّن قبل المنفّذ) ─────────────
// النصوص UTF-8، والعمليات بوحدات الترميز (الحروف) لا بالبايتات.

size_t cpCount(const std::string& s) {
  size_t n = 0;
  for (size_t i = 0; i < s.size();) {
    size_t l;
    decodeAt(s, i, l);
    i += l;
    n++;
  }
  return n;
}

std::string padStr(const std::string& s, long long width, const std::string& fill, bool left) {
  size_t n = cpCount(s);
  if (width <= 0 || static_cast<long long>(n) >= width) return s;
  auto f = splitChars(fill.empty() ? std::string(" ") : fill);
  std::string pad;
  for (long long i = 0; i < width - static_cast<long long>(n); i++) pad += f[static_cast<size_t>(i) % f.size()];
  return left ? pad + s : s + pad;
}

std::string numToWords(long long n) {
  static const char* ones[] = {"صفر", "واحد", "اثنان", "ثلاثة", "أربعة", "خمسة", "ستة", "سبعة", "ثمانية", "تسعة", "عشرة",
                               "أحد عشر", "اثنا عشر", "ثلاثة عشر", "أربعة عشر", "خمسة عشر", "ستة عشر", "سبعة عشر",
                               "ثمانية عشر", "تسعة عشر"};
  static const char* tens[] = {"", "", "عشرون", "ثلاثون", "أربعون", "خمسون", "ستون", "سبعون", "ثمانون", "تسعون"};
  static const char* hund[] = {"", "مائة", "مائتان", "ثلاثمائة", "أربعمائة", "خمسمائة", "ستمائة", "سبعمائة", "ثمانمائة", "تسعمائة"};
  if (n == 0) return ones[0];
  if (n < 0) return "سالب " + numToWords(-n);
  // مجموعة من 1 إلى 999
  auto group = [&](int g) {
    std::string r;
    int h = g / 100, rest = g % 100;
    if (h) r += hund[h];
    if (rest) {
      if (!r.empty()) r += " و";
      if (rest < 20) r += ones[rest];
      else {
        int o = rest % 10;
        if (o) r += std::string(ones[o]) + " و";
        r += tens[rest / 10];
      }
    }
    return r;
  };
  struct Scale { long long v; const char *one, *two, *few, *many; };
  static const Scale scales[] = {{1000000000LL, "مليار", "ملياران", "مليارات", "ملياراً"},
                                 {1000000LL, "مليون", "مليونان", "ملايين", "مليوناً"},
                                 {1000LL, "ألف", "ألفان", "آلاف", "ألفاً"}};
  std::vector<std::string> parts;
  for (const Scale& sc : scales) {
    long long g = n / sc.v;
    n %= sc.v;
    if (!g) continue;
    if (g == 1) { parts.push_back(sc.one); continue; }
    if (g == 2) { parts.push_back(sc.two); continue; }
    int gi = static_cast<int>(g), r = gi % 100;
    std::string gw = group(gi);
    if (r == 0 && gi / 100 == 2) gw = "مائتا";  // مائتا ألف
    if (r >= 3 && r <= 10) parts.push_back(gw + " " + sc.few);
    else if (r >= 11) parts.push_back(gw + " " + sc.many);
    else parts.push_back(gw + " " + sc.one);
  }
  if (n) parts.push_back(group(static_cast<int>(n)));
  std::string r;
  for (size_t i = 0; i < parts.size(); i++) r += (i ? " و" : "") + parts[i];
  return r;
}

// تاريخ ميلادي (خوارزمية Howard Hinnant): أيام منذ 1970-01-01
long long daysFromCivil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}
void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
  z += 719468;
  const long long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = static_cast<long long>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y += m <= 2;
}

std::string colorHex(double r, double g, double b) {
  auto c = [](double v) { return static_cast<int>(std::max(0.0, std::min(255.0, std::round(v)))); };
  char buf[16];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", c(r), c(g), c(b));
  return buf;
}

