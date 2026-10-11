// ═════════════════ مكوّنات الويب — الدفعة الثانية (40 مكوّناً، عائلة ee-x-) ═════════════════
#define WEB(NAME, MINARGS, USAGE, ...)                                             \
  reg(NAME, [=](Interp&, Args& a, int ln) -> Value {                                \
    needArgs(a, MINARGS, "$" NAME "(" USAGE ")", ln);                              \
    __VA_ARGS__                                                                    \
  })
{
  // ترميز نسبي لروابط المشاركة
  auto enc = [](const std::string& s) {
    std::string o;
    char b[4];
    for (unsigned char c : s) {
      if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += static_cast<char>(c);
      else { std::snprintf(b, sizeof b, "%%%02X", c); o += b; }
    }
    return o;
  };
  // لون حر: #hex أو rgb()/hsl() أو اسم عربي أو اسم CSS لاتيني — أي لون بلا حد
  auto cssColor = [](const Value& v, int ln) -> std::string {
    static const std::unordered_map<std::string, std::string> ar = {
        {"احمر", "#e53935"}, {"ازرق", "#1e66d0"}, {"اخضر", "#2e9e4f"}, {"اصفر", "#f5c518"}, {"برتقالي", "#f28c28"},
        {"بنفسجي", "#7b4fd6"}, {"وردي", "#ec5f9a"}, {"بني", "#7a4b2a"}, {"رمادي", "#8a8f98"}, {"اسود", "#16181b"},
        {"ابيض", "#ffffff"}, {"ذهبي", "#d4a017"}, {"فضي", "#c0c4cc"}, {"تركوازي", "#17b8b0"}, {"سماوي", "#4fc3f7"},
        {"زيتي", "#6b7a2e"}, {"عنابي", "#8e1b3a"}, {"كحلي", "#14284b"}, {"نيلي", "#3f3fa8"}, {"ليموني", "#c6e03a"},
        {"خوخي", "#ffab91"}, {"لافندر", "#b39ddb"}, {"فيروزي", "#26c6da"}, {"قرمزي", "#c2185b"}, {"زمردي", "#1faa6e"},
        {"ياقوتي", "#b0173f"}, {"عاجي", "#f6f0dc"}, {"رملي", "#d9c29a"}, {"نحاسي", "#b86f3a"}, {"اقحواني", "#f4e04d"}};
    std::string c = toStr(v);
    while (!c.empty() && c.front() == ' ') c.erase(0, 1);
    while (!c.empty() && c.back() == ' ') c.pop_back();
    if (c.empty() || c.size() > 60) fail(ln, "لون غير صالح");
    if (c[0] == '#') {
      size_t n = c.size() - 1;
      if (n != 3 && n != 4 && n != 6 && n != 8) fail(ln, "اللون السداسي يكون #abc أو #aabbcc");
      for (size_t i = 1; i < c.size(); i++) if (!std::isxdigit(static_cast<unsigned char>(c[i]))) fail(ln, "اللون السداسي غير صالح: " + c);
      return c;
    }
    size_t lp = c.find('(');
    if (lp != std::string::npos) {
      std::string fn = c.substr(0, lp);
      if (fn != "rgb" && fn != "rgba" && fn != "hsl" && fn != "hsla") fail(ln, "الدوال المسموحة: rgb وrgba وhsl وhsla");
      if (c.back() != ')' || c.find('(', lp + 1) != std::string::npos || c.find(')') != c.size() - 1) fail(ln, "لون غير صالح: " + c);
      for (size_t i = lp + 1; i + 1 < c.size(); i++) {
        char ch = c[i];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == ' ' || ch == ',' || ch == '.' || ch == '%' || ch == '/' || ch == '-' || ch == 'd' || ch == 'e' || ch == 'g'))
          fail(ln, "لون غير صالح: " + c);
      }
      return c;
    }
    bool latin = true;
    for (char ch : c) if (!std::isalpha(static_cast<unsigned char>(ch)) || static_cast<unsigned char>(ch) >= 128) latin = false;
    if (latin) {
      std::string l = c;
      for (char& ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      return l;
    }
    std::string k = normalizeKw(c);
    for (auto& e : ar) if (normalizeKw(e.first) == k) return e.second;
    fail(ln, "لون غير معروف: " + c + " (استعمل #hex أو rgb() أو hsl() أو اسماً مثل احمر، ذهبي، كحلي، أو red)");
  };
  auto isLight = [](const std::string& c) {
    if (c.compare(0, 3, "rgb") == 0 || c.compare(0, 3, "hsl") == 0) {
      std::vector<double> n;
      std::string cur;
      for (size_t i = c.find('(') + 1; i < c.size(); i++) {
        char ch = c[i];
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') cur += ch;
        else { if (!cur.empty()) n.push_back(std::atof(cur.c_str())); cur.clear(); }
      }
      if (n.size() < 3) return false;
      if (c[0] == 'h') return n[2] > 65;
      return (0.299 * n[0] + 0.587 * n[1] + 0.114 * n[2]) > 165;
    }
    if (c.size() < 4 || c[0] != '#') return false;
    auto hv = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) ? ch - '0' : (std::tolower(ch) - 'a' + 10); };
    double r, g, b;
    if (c.size() == 4 || c.size() == 5) { r = hv(c[1]) * 17; g = hv(c[2]) * 17; b = hv(c[3]) * 17; }
    else { r = hv(c[1]) * 16 + hv(c[2]); g = hv(c[3]) * 16 + hv(c[4]); b = hv(c[5]) * 16 + hv(c[6]); }
    return (0.299 * r + 0.587 * g + 0.114 * b) > 165;
  };
  // يحوّل (اسم تدرج جاهز | لون | [ألوان...]) إلى صنف + أنماط
  auto gradSpec = [cssColor, isLight](const Value& v, int ln, std::string& cls, std::string& style) {
    static const char* presets[] = {"بنفسجي", "برتقالي", "اخضر", "ازرق", "وردي", "داكن"};
    cls = "";
    style = "";
    if (v.t == VT::Str) {
      for (const char* n : presets) if (v.s == n) { cls = std::string(" g-") + n; return; }
    }
    std::vector<std::string> cs;
    if (v.t == VT::List) {
      if (v.list->size() < 2 || v.list->size() > 12) fail(ln, "التدرج يحتاج من لونين إلى 12 لوناً");
      for (auto& x : *v.list) cs.push_back(cssColor(x, ln));
    } else {
      std::string c1 = cssColor(v, ln);
      cs = {c1, "color-mix(in srgb," + c1 + " 62%,#000)"};
    }
    std::string stops;
    for (size_t i = 0; i < cs.size(); i++) stops += (i ? "," : "") + cs[i];
    style = " style=\"--g1:" + cs.front() + ";--g2:" + cs.back() + ";--gr:linear-gradient(135deg," + stops + ")\"";
    if (isLight(cs.front())) cls = " light";
  };
  auto textOn = [cssColor, isLight](const Value& v, int ln, std::string& c) {
    c = cssColor(v, ln);
    return isLight(c) ? "#1b1c1e" : "#ffffff";
  };
  auto digitsOnly = [](const std::string& s) {
    std::string o;
    for (char c : s) if (std::isdigit(static_cast<unsigned char>(c)) || (c == '+' && o.empty())) o += c;
    return o;
  };

  WEB("لون_الموقع", 2, "لون، محتوى...", {
    std::string c, fg = textOn(a[0], ln, c);
    return mkHtml(tg("div", "ee-x-theme", hrest(a, 1), " style=\"--accent:" + c + ";--accent-fg:" + fg + ";--info:" + c + "\""));
  });
  WEB("زر_ملون", 2, "نص، لون، رابط", {
    std::string c, fg = textOn(a[1], ln, c);
    std::string st = " style=\"background:" + c + ";border-color:" + c + ";color:" + fg + "\"";
    if (a.size() > 2 && a[2].t != VT::Null) return mkHtml(tg("a", "ee-b-btn", hs(a[0]), " href=\"" + hu(a[2]) + "\"" + st));
    return mkHtml(tg("button", "ee-b-btn", hs(a[0]), " type=\"button\"" + st));
  });
  WEB("شارة_ملونة", 2, "نص، لون", {
    std::string c, fg = textOn(a[1], ln, c);
    return mkHtml(tg("span", "ee-a-badge", hs(a[0]), " style=\"background:" + c + ";color:" + fg + ";border-color:" + c + "\""));
  });
  WEB("نص_ملون", 2, "نص، لون", {
    std::string c = cssColor(a[1], ln);
    return mkHtml(tg("span", "", hs(a[0]), " style=\"color:" + c + "\""));
  });
  WEB("نص_متدرج", 2, "نص، لون1، لون2", {
    std::string cls, st;
    Value pair = a[1];
    if (a.size() > 2) pair = mkListP(std::make_shared<std::vector<Value>>(std::vector<Value>{a[1], a[2]}));
    gradSpec(pair, ln, cls, st);
    return mkHtml(tg("span", "ee-x-gtext", hs(a[0]), st));
  });
  WEB("شبكة_متجاوبة", 1, "حد_ادنى_بكسل، عناصر...", {
    double n = std::max(100.0, std::min(600.0, needNum("شبكة_متجاوبة", a[0], ln)));
    return mkHtml(tg("div", "ee-x-grid", hrest(a, 1), " style=\"grid-template-columns:repeat(auto-fit,minmax(min(" + fmtNum(n) + "px,100%),1fr))\""));
  });
  WEB("مجموعة_رأسية", 0, "عناصر...", return mkHtml(tg("div", "ee-x-stack", hrest(a, 0))););
  WEB("فاصل_بعنوان", 0, "نص", {
    return a.empty() ? mkHtml(tg("div", "ee-x-div plain", "", " role=\"separator\"")) : mkHtml(tg("div", "ee-x-div", tg("span", "", hs(a[0])), " role=\"separator\""));
  });
  WEB("خلفية_متدرجة", 1, "لون أو [ألوان]، محتوى...", {
    std::string cls, st;
    gradSpec(a[0], ln, cls, st);
    return mkHtml(tg("section", "ee-x-gbg" + cls, hrest(a, 1), st));
  });
  WEB("صندوق_زجاجي", 0, "محتوى...", return mkHtml(tg("div", "ee-x-glass", hrest(a, 0))););
  WEB("بطاقة_متدرجة", 2, "عنوان، وصف، لون", {
    std::string cls, st;
    gradSpec(a.size() > 2 ? a[2] : mkStr("بنفسجي"), ln, cls, st);
    return mkHtml(tg("div", "ee-x-gcard" + cls, tg("h3", "", hs(a[0])) + tg("p", "", hs(a[1])), st));
  });
  WEB("شريط_متحرك", 1, "[نصوص]", {
    std::string r;
    for (auto& v : listOf(a[0], "$شريط_متحرك([نصوص])", ln)) r += tg("span", "", hs(v));
    return mkHtml(tg("div", "ee-x-marq", tg("div", "", r + r), " aria-hidden=\"true\""));
  });
  WEB("سلايدر", 1, "[عناصر]", {
    std::string r;
    for (auto& v : listOf(a[0], "$سلايدر([عناصر])", ln)) r += hs(v);
    return mkHtml(tg("div", "ee-x-slider", r, " tabindex=\"0\""));
  });
  WEB("وسوم", 1, "[نصوص]", {
    std::string r;
    for (auto& v : listOf(a[0], "$وسوم([نصوص])", ln)) r += tg("span", "", hs(v));
    return mkHtml(tg("div", "ee-x-tags", r));
  });
  WEB("كاتب", 2, "صورة، اسم، تاريخ", {
    return mkHtml(tg("div", "ee-x-by", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"\" loading=\"lazy\"") +
                                         tg("div", "", tg("b", "", hs(a[1])) + (a.size() > 2 ? tg("small", "", hs(a[2])) : ""))));
  });
  WEB("قائمة_مزايا", 1, "[[ايقونة، نص]...]", {
    std::string r;
    for (auto& v : listOf(a[0], "$قائمة_مزايا([[ايقونة، نص]...])", ln)) {
      auto& p = pairOf(v, 2, "$قائمة_مزايا([[ايقونة، نص]...])", ln);
      r += tg("li", "", tg("span", "i", hs(p[0])) + tg("span", "", hs(p[1])));
    }
    return mkHtml(tg("ul", "ee-x-pros", r));
  });
  WEB("جدول_مقارنة", 2, "[باقات]، [[ميزة، [قيم]]...]", {
    auto heads = listOf(a[0], "$جدول_مقارنة([باقات]، [[ميزة، [قيم]]...])", ln);
    std::string th = tg("th", "", "الميزة"), body;
    for (auto& h : heads) th += tg("th", "", hs(h));
    for (auto& row : listOf(a[1], "$جدول_مقارنة([باقات]، [[ميزة، [قيم]]...])", ln)) {
      auto& p = pairOf(row, 2, "$جدول_مقارنة(...)", ln);
      std::string td = tg("td", "", hs(p[0]));
      for (auto& c : listOf(p[1], "$جدول_مقارنة: قيم كل صف قائمة", ln)) {
        if (c.t == VT::Bool) td += c.b ? tg("td", "y", "✓") : tg("td", "n", "—");
        else td += tg("td", "", hs(c));
      }
      body += tg("tr", "", td);
    }
    return mkHtml("<div style=\"overflow-x:auto\">" + tg("table", "ee-x-cmp", tg("thead", "", tg("tr", "", th)) + tg("tbody", "", body)) + "</div>");
  });
  WEB("شاشة_هاتف", 0, "محتوى...", return mkHtml(tg("div", "ee-x-phone", tg("div", "", hrest(a, 0)))););
  WEB("نافذة_متصفح", 1, "عنوان، محتوى...", {
    return mkHtml(tg("div", "ee-x-brw", tg("div", "bar", "<i></i><i></i><i></i>" + tg("span", "", hs(a[0]))) + tg("div", "bd", hrest(a, 1))));
  });
  WEB("كتلة_كود", 1, "نص، عنوان", {
    std::string code = toStr(a[0]);
    return mkHtml(tg("div", "ee-x-code", tg("div", "h", tg("span", "", a.size() > 1 ? hs(a[1]) : "code") +
                                                             tg("button", "", "نسخ", " type=\"button\" data-ee-copy=\"" + htmlEsc(code) + "\"")) +
                                             tg("pre", "", tg("code", "", htmlEsc(code)))));
  });
  WEB("طرفية", 1, "[اوامر]", {
    std::string r;
    for (auto& v : listOf(a[0], "$طرفية([اوامر])", ln)) r += tg("div", "", htmlEsc(toStr(v)));
    return mkHtml(tg("div", "ee-x-term", r));
  });
  WEB("تحميل", 0, "", return mkHtml(tg("span", "ee-x-spin", "", " role=\"status\" aria-label=\"جارٍ التحميل\"")););
  WEB("هيكل_تحميل", 0, "عدد_الاسطر", {
    double n = a.empty() ? 3 : std::max(1.0, std::min(12.0, needNum("هيكل_تحميل", a[0], ln)));
    std::string r;
    for (int i = 0; i < static_cast<int>(n); i++) r += "<i></i>";
    return mkHtml(tg("div", "ee-x-skel", r, " aria-hidden=\"true\""));
  });
  WEB("تقدم_دائري", 1, "نسبة، وصف", {
    double p = std::max(0.0, std::min(100.0, needNum("تقدم_دائري", a[0], ln)));
    const double C = 2 * 3.14159265358979 * 52;
    char buf[300];
    std::snprintf(buf, sizeof buf, "<svg viewBox=\"0 0 120 120\" role=\"img\" aria-label=\"%s%%\"><circle cx=\"60\" cy=\"60\" r=\"52\" fill=\"none\" stroke=\"var(--line)\" stroke-width=\"10\"/>"
                                   "<circle cx=\"60\" cy=\"60\" r=\"52\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"10\" stroke-linecap=\"round\" stroke-dasharray=\"%.2f %.2f\"/></svg>",
                  fmtNum(p).c_str(), C * p / 100, C);
    return mkHtml(tg("div", "ee-x-ring", std::string(buf) + tg("b", "", fmtNum(p) + "%") + (a.size() > 1 ? tg("small", "", hs(a[1])) : "")));
  });
  WEB("اعمدة_افقية", 2, "[تسميات]، [قيم]", {
    auto labs = listOf(a[0], "$اعمدة_افقية([تسميات]، [قيم])", ln);
    auto vals = listOf(a[1], "$اعمدة_افقية([تسميات]، [قيم])", ln);
    if (labs.size() != vals.size() || labs.empty()) fail(ln, "$اعمدة_افقية تتطلب قائمتين غير فارغتين بالطول نفسه");
    double mx = 0;
    std::vector<double> v;
    for (auto& x : vals) { double d = needNum("اعمدة_افقية", x, ln); if (d < 0) fail(ln, "القيم غير سالبة"); v.push_back(d); mx = std::max(mx, d); }
    if (mx <= 0) mx = 1;
    std::string r;
    for (size_t i = 0; i < v.size(); i++)
      r += tg("div", "r", tg("span", "", hs(labs[i])) + tg("div", "t", tg("i", "", "", " style=\"width:" + fmtNum(std::round(v[i] / mx * 1000) / 10) + "%;background:var(--c" + std::to_string(i % 6 + 1) + ")\"")) + tg("span", "", fmtNum(v[i])));
    return mkHtml(tg("div", "ee-x-hbar", r));
  });
  WEB("خط_صغير", 2, "[قيم]", {
    auto vals = listOf(a[0], "$خط_صغير([قيم])", ln);
    if (vals.size() < 2) fail(ln, "$خط_صغير تتطلب قيمتين على الأقل");
    std::vector<double> v;
    double mn = 1e300, mx = -1e300;
    for (auto& x : vals) { double d = needNum("خط_صغير", x, ln); v.push_back(d); mn = std::min(mn, d); mx = std::max(mx, d); }
    if (mx == mn) mx = mn + 1;
    std::string pts;
    char buf[64];
    for (size_t i = 0; i < v.size(); i++) {
      std::snprintf(buf, sizeof buf, "%.1f,%.1f ", 140.0 * static_cast<double>(i) / static_cast<double>(v.size() - 1), 36 - (v[i] - mn) / (mx - mn) * 32);
      pts += buf;
    }
    return mkHtml("<svg class=\"ee-x-spark\" viewBox=\"0 0 140 40\" role=\"img\" aria-label=\"اتجاه\"><polyline points=\"" + pts +
                  "\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/></svg>");
  });
  WEB("سجل_تغييرات", 1, "[[اصدار، تاريخ، [بنود]]...]", {
    std::string r;
    for (auto& e : listOf(a[0], "$سجل_تغييرات([[اصدار، تاريخ، [بنود]]...])", ln)) {
      auto& p = pairOf(e, 3, "$سجل_تغييرات([[اصدار، تاريخ، [بنود]]...])", ln);
      std::string li;
      for (auto& b : listOf(p[2], "$سجل_تغييرات: البنود قائمة", ln)) li += tg("li", "", hs(b));
      r += tg("div", "e", tg("h4", "", hs(p[0])) + tg("time", "", hs(p[1])) + tg("ul", "", li));
    }
    return mkHtml(tg("div", "ee-x-log", r));
  });
  WEB("بطاقة_اتصال", 1, "هاتف، بريد، عنوان", {
    std::string r;
    std::string ph = digitsOnly(toStr(a[0]));
    if (!ph.empty()) r += tg("a", "", tg("b", "", "📞") + tg("span", "", ha(a[0]), " dir=\"ltr\""), " href=\"tel:" + ph + "\"");
    if (a.size() > 1 && a[1].t != VT::Null) r += tg("a", "", tg("b", "", "✉️") + tg("span", "", ha(a[1]), " dir=\"ltr\""), " href=\"mailto:" + ha(a[1]) + "\"");
    if (a.size() > 2 && a[2].t != VT::Null) r += tg("div", "", tg("b", "", "📍") + tg("span", "", hs(a[2])));
    return mkHtml(tg("div", "ee-x-contact", r));
  });
  WEB("ساعات_عمل", 1, "[[يوم، وقت]...]", {
    std::string r;
    for (auto& e : listOf(a[0], "$ساعات_عمل([[يوم، وقت]...])", ln)) {
      auto& p = pairOf(e, 2, "$ساعات_عمل([[يوم، وقت]...])", ln);
      r += tg("tr", "", tg("td", "", hs(p[0])) + tg("td", "", hs(p[1])));
    }
    return mkHtml(tg("table", "ee-x-hours", tg("tbody", "", r)));
  });
  WEB("شعارات", 1, "[اسماء]", {
    std::string r;
    for (auto& v : listOf(a[0], "$شعارات([اسماء])", ln)) r += tg("span", "", hs(v));
    return mkHtml(tg("div", "ee-x-logos", r));
  });
  WEB("نسبة_ابعاد", 2, "نسبة(16:9)، محتوى...", {
    std::string k = toStr(a[0]);
    static const std::pair<const char*, const char*> m[] = {{"16:9", "56.25%"}, {"4:3", "75%"}, {"1:1", "100%"}, {"21:9", "42.86%"}, {"3:2", "66.67%"}};
    const char* pct = nullptr;
    for (auto& e : m) if (k == e.first) pct = e.second;
    if (!pct) fail(ln, "النسب المتاحة: 16:9 و4:3 و1:1 و21:9 و3:2");
    return mkHtml(tg("div", "ee-x-ar", hrest(a, 1), std::string(" style=\"--r:") + pct + "\""));
  });
  reg("رابط_مشاركة", [enc](Interp&, Args& a, int ln) -> Value {
    needArgs(a, 2, "$رابط_مشاركة(منصة، رابط، نص)", ln);
    std::string k = toStr(a[0]), u = enc(safeUrl(toStr(a[1]))), t = a.size() > 2 ? enc(toStr(a[2])) : "";
    std::string href, label;
    if (k == "تويتر" || k == "اكس") { href = "https://twitter.com/intent/tweet?url=" + u + "&text=" + t; label = "تويتر / X"; }
    else if (k == "فيسبوك") { href = "https://www.facebook.com/sharer/sharer.php?u=" + u; label = "فيسبوك"; }
    else if (k == "واتساب") { href = "https://wa.me/?text=" + t + "%20" + u; label = "واتساب"; }
    else if (k == "تلغرام") { href = "https://t.me/share/url?url=" + u + "&text=" + t; label = "تلغرام"; }
    else if (k == "بريد") { href = "mailto:?subject=" + t + "&body=" + u; label = "بريد"; }
    else fail(ln, "المنصات: تويتر، اكس، فيسبوك، واتساب، تلغرام، بريد");
    return mkHtml(tg("a", "", label, " href=\"" + htmlEsc(href) + "\" rel=\"noopener\" target=\"_blank\""));
  });
  WEB("مجموعة_مشاركة", 0, "روابط...", return mkHtml(tg("div", "ee-x-share", hrest(a, 0))););
  WEB("ظهور", 0, "محتوى...", return mkHtml(tg("div", "ee-x-reveal", hrest(a, 0), " data-ee-reveal")););
  WEB("تعليق", 3, "صورة، اسم، نص، وقت", {
    std::string head = tg("b", "", hs(a[1])) + (a.size() > 3 ? " " + tg("small", "", hs(a[3])) : "");
    return mkHtml(tg("div", "ee-x-cm", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"\" loading=\"lazy\"") + tg("div", "b", head + tg("p", "", hs(a[2])))));
  });
  WEB("قائمة_منسدلة", 2, "عنوان، [[نص، رابط]...]", {
    std::string r;
    for (auto& e : listOf(a[1], "$قائمة_منسدلة(عنوان، [[نص، رابط]...])", ln)) {
      auto& p = pairOf(e, 2, "$قائمة_منسدلة(...)", ln);
      r += tg("a", "", hs(p[0]), " href=\"" + hu(p[1]) + "\"");
    }
    return mkHtml(tg("details", "ee-x-dd", tg("summary", "", hs(a[0])) + tg("div", "", r)));
  });
  WEB("شريط_ركن", 1, "نص، محتوى...", return mkHtml(tg("div", "ee-x-corner", tg("b", "", hs(a[0])) + hrest(a, 1))););
  WEB("بطاقة_افقية", 3, "صورة، عنوان، وصف، رابط", {
    std::string in = tg("h3", "", hs(a[1])) + tg("p", "", hs(a[2]));
    if (a.size() > 3 && a[3].t != VT::Null) in += tg("a", "", "التفاصيل ←", " href=\"" + hu(a[3]) + "\"");
    return mkHtml(tg("article", "ee-x-hcard", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("div", "", in)));
  });
  WEB("قسم_انقسام", 3, "صورة، عنوان، وصف، عكس", {
    bool rev = a.size() > 3 && truthy(a[3]);
    return mkHtml(tg("section", std::string("ee-x-split") + (rev ? " rev" : ""),
                     tg("div", "", tg("h2", "", hs(a[1])) + tg("p", "", hs(a[2]))) + tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"")));
  });
  WEB("اقتباس_جانبي", 1, "نص...", return mkHtml(tg("blockquote", "ee-x-quote", hrest(a, 0))););
  WEB("زر_واتساب", 1, "رقم، نص", {
    std::string n = digitsOnly(toStr(a[0]));
    if (n.size() < 6) fail(ln, "رقم واتساب غير صالح (مع رمز الدولة)");
    if (n[0] == '+') n.erase(0, 1);
    return mkHtml(tg("a", "ee-x-wa", "💬 " + (a.size() > 1 ? hs(a[1]) : std::string("تواصل عبر واتساب")), " href=\"https://wa.me/" + n + "\" rel=\"noopener\" target=\"_blank\""));
  });
  reg("خريطة_موقع", [enc](Interp&, Args& a, int ln) -> Value {
    needArgs(a, 1, "$خريطة_موقع(عنوان)", ln);
    return mkHtml(tg("div", "ee-x-map", tg("iframe", "", "", " src=\"https://maps.google.com/maps?q=" + enc(toStr(a[0])) + "&output=embed\" title=\"خريطة\" loading=\"lazy\"")));
  });
  WEB("شريط_كوكيز", 1, "نص، نص_الزر", {
    return mkHtml(tg("div", "ee-x-cookie", tg("p", "", hs(a[0])) + tg("button", "ee-b-btn primary", a.size() > 1 ? hs(a[1]) : "موافق", " type=\"button\" onclick=\"this.parentNode.hidden=true\""), " role=\"dialog\" aria-label=\"إشعار\""));
  });
  WEB("قبل_بعد", 2, "صورة_قبل، صورة_بعد", {
    auto fig = [&](const Value& u, const char* cap) { return tg("figure", "", tg("img", "", "", " src=\"" + hu(u) + "\" alt=\"" + cap + "\" loading=\"lazy\"") + tg("figcaption", "", cap)); };
    return mkHtml(tg("div", "ee-x-ba", fig(a[0], "قبل") + fig(a[1], "بعد")));
  });
  WEB("وقت_القراءة", 1, "نص", {
    std::string t = toStr(a[0]);
    size_t words = 0;
    bool in = false;
    for (char c : t) {
      bool sp = c == ' ' || c == '\n' || c == '\t' || c == '\r';
      if (!sp && !in) words++;
      in = !sp;
    }
    int mins = std::max(1, static_cast<int>(std::ceil(static_cast<double>(words) / 180.0)));
    return mkHtml(tg("span", "ee-x-read", "⏱ " + std::to_string(mins) + (mins == 1 ? " دقيقة قراءة" : (mins == 2 ? " دقيقتان قراءة" : (mins <= 10 ? " دقائق قراءة" : " دقيقة قراءة")))));
  });
  WEB("بطاقة_حدث", 3, "يوم، شهر، عنوان، مكان", {
    return mkHtml(tg("div", "ee-x-event", tg("div", "d", tg("b", "", hs(a[0])) + tg("small", "", hs(a[1]))) +
                                              tg("div", "", tg("h3", "", hs(a[2])) + (a.size() > 3 ? tg("p", "", "📍 " + hs(a[3])) : ""))));
  });
}
#undef WEB
