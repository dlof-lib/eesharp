// ═════════════════ مكوّنات الويب (100 مفهوم) — تُضمَّن داخل Interp::setup() ═════════════════
#define WEB(NAME, MINARGS, USAGE, ...)                                             \
  reg(NAME, [](Interp&, Args& a, int ln) -> Value {                                \
    needArgs(a, MINARGS, "$" NAME "(" USAGE ")", ln);                              \
    __VA_ARGS__                                                                    \
  })

// ───── التخطيط (8) ─────
WEB("صف", 0, "محتوى...", return mkHtml(tg("div", "ee-l-row", hrest(a, 0))););
WEB("عمود", 0, "محتوى...", return mkHtml(tg("div", "ee-l-col", hrest(a, 0))););
WEB("عمودان", 0, "محتوى...", return mkHtml(tg("div", "ee-l-cols c2", hrest(a, 0))););
WEB("ثلاثة_اعمدة", 0, "محتوى...", return mkHtml(tg("div", "ee-l-cols c3", hrest(a, 0))););
WEB("مركز", 0, "محتوى...", return mkHtml(tg("div", "ee-l-center", hrest(a, 0))););
WEB("حاوية_ضيقة", 0, "محتوى...", return mkHtml(tg("div", "ee-l-narrow", hrest(a, 0))););
WEB("مسافة", 0, "ارتفاع_بالبكسل", {
  double n = a.empty() ? 24 : needNum("مسافة", a[0], ln);
  n = std::max(0.0, std::min(400.0, n));
  return mkHtml("<div style=\"height:" + fmtNum(n) + "px\" aria-hidden=\"true\"></div>");
});
WEB("تخطيط_جانبي", 1, "جانب، محتوى...", {
  return mkHtml(tg("div", "ee-l-side", tg("aside", "", hs(a[0])) + tg("div", "", hrest(a, 1))));
});

// ───── النصوص (8) ─────
WEB("عنوان_فرعي", 0, "نص...", return mkHtml(tg("p", "ee-t-sub", hrest(a, 0))););
WEB("نص_كبير", 0, "نص...", return mkHtml(tg("p", "ee-t-lead", hrest(a, 0))););
WEB("نص_خافت", 0, "نص...", return mkHtml(tg("span", "ee-t-muted", hrest(a, 0))););
WEB("تمييز", 0, "نص...", return mkHtml(tg("mark", "ee-t-mark", hrest(a, 0))););
WEB("اختصار", 2, "نص، شرح", return mkHtml(tg("abbr", "ee-t-abbr", hs(a[0]), " title=\"" + ha(a[1]) + "\"")););
WEB("مفتاح", 0, "نص...", return mkHtml(tg("kbd", "ee-t-kbd", hrest(a, 0))););
WEB("اقتباس_بارز", 1, "نص، قائل", {
  std::string cap = a.size() > 1 ? tg("figcaption", "", "— " + hs(a[1])) : "";
  return mkHtml(tg("figure", "ee-t-pull", tg("div", "", hs(a[0])) + cap));
});
WEB("عنوان_مزخرف", 1, "نص، مستوى", {
  double lv = a.size() > 1 ? needNum("عنوان_مزخرف", a[1], ln) : 2;
  if (lv < 1 || lv > 6 || lv != std::floor(lv)) fail(ln, "مستوى العنوان من 1 إلى 6");
  return mkHtml(tg(("h" + std::to_string(static_cast<int>(lv))).c_str(), "ee-t-fancy", hs(a[0])));
});

// ───── التنبيهات والحالة (9) ─────
WEB("تنبيه_معلومة", 0, "نص...", return mkHtml(tg("div", "ee-a-alert info", tg("span", "i", "ℹ️") + tg("div", "", hrest(a, 0)), " role=\"alert\"")););
WEB("تنبيه_نجاح", 0, "نص...", return mkHtml(tg("div", "ee-a-alert ok", tg("span", "i", "✅") + tg("div", "", hrest(a, 0)), " role=\"alert\"")););
WEB("تنبيه_تحذير", 0, "نص...", return mkHtml(tg("div", "ee-a-alert warn", tg("span", "i", "⚠️") + tg("div", "", hrest(a, 0)), " role=\"alert\"")););
WEB("تنبيه_خطر", 0, "نص...", return mkHtml(tg("div", "ee-a-alert danger", tg("span", "i", "⛔") + tg("div", "", hrest(a, 0)), " role=\"alert\"")););
WEB("ملاحظة", 1, "عنوان، محتوى...", return mkHtml(tg("div", "ee-a-note", tg("strong", "", hs(a[0])) + hrest(a, 1))););
WEB("شارة", 1, "نص، نوع(نجاح|تحذير|خطر|معلومة|رمادي)", {
  return mkHtml(tg("span", "ee-a-badge " + kindClass(a.size() > 1 ? toStr(a[1]) : ""), hs(a[0])));
});
WEB("نقطة_حالة", 1, "نص، حالة(نجاح|تحذير|خطر|معلومة|رمادي)", {
  return mkHtml(tg("span", "ee-a-dot " + kindClass(a.size() > 1 ? toStr(a[1]) : "رمادي"), hs(a[0])));
});
WEB("شريط_اعلان", 0, "نص...", return mkHtml(tg("div", "ee-a-announce", hrest(a, 0))););
WEB("حالة_فارغة", 1, "عنوان، وصف، ايقونة", {
  std::string ic = a.size() > 2 ? hs(a[2]) : "📭";
  return mkHtml(tg("div", "ee-a-empty", tg("span", "i", ic) + tg("h3", "", hs(a[0])) + (a.size() > 1 ? tg("p", "", hs(a[1])) : "")));
});

// ───── الأزرار (7) ─────
{
  struct Btn { const char* name; const char* cls; };
  static const Btn btns[] = {{"زر_اساسي", "primary"}, {"زر_ثانوي", "second"}, {"زر_مخطط", "outline primary"},
                             {"زر_خطر", "danger"}, {"زر_نجاح", "ok"}};
  for (const Btn& b : btns) {
    std::string nm = b.name, cls = std::string("ee-b-btn ") + b.cls;
    reg(b.name, [nm, cls](Interp&, Args& a, int ln) -> Value {
      needArgs(a, 1, ("$" + nm + "(نص، رابط)").c_str(), ln);
      if (a.size() > 1 && a[1].t != VT::Null) return mkHtml(tg("a", cls, hs(a[0]), " href=\"" + hu(a[1]) + "\""));
      return mkHtml(tg("button", cls, hs(a[0]), " type=\"button\""));
    });
  }
}
WEB("مجموعة_ازرار", 0, "ازرار...", return mkHtml(tg("div", "ee-b-group", hrest(a, 0))););
WEB("زر_عائم", 1, "رمز، رابط", return mkHtml(tg("a", "ee-b-fab", hs(a[0]), " href=\"" + (a.size() > 1 ? hu(a[1]) : std::string("#")) + "\"")););

// ───── البطاقات (9) ─────
WEB("بطاقة_صورة", 2, "صورة، عنوان، وصف، رابط", {
  std::string body = tg("h3", "", hs(a[1]));
  if (a.size() > 2 && a[2].t != VT::Null) body += tg("p", "", hs(a[2]));
  if (a.size() > 3 && a[3].t != VT::Null) body += tg("a", "more", "اقرأ المزيد ←", " href=\"" + hu(a[3]) + "\"");
  return mkHtml(tg("article", "ee-c-card", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("div", "b", body)));
});
WEB("بطاقة_احصائية", 2, "قيمة، عنوان، ايقونة", {
  std::string in = (a.size() > 2 ? tg("span", "i", hs(a[2])) : "") + tg("span", "v", hs(a[0])) + tg("span", "l", hs(a[1]));
  return mkHtml(tg("div", "ee-c-card ee-c-stat", in));
});
WEB("بطاقة_ميزة", 3, "ايقونة، عنوان، وصف", {
  return mkHtml(tg("div", "ee-c-card ee-c-feat", tg("span", "i", hs(a[0])) + tg("h3", "", hs(a[1])) + tg("p", "", hs(a[2]))));
});
WEB("بطاقة_شهادة", 2, "نص، اسم، صفة", {
  std::string who = tg("strong", "", hs(a[1])) + (a.size() > 2 ? tg("span", "", hs(a[2])) : "");
  return mkHtml(tg("div", "ee-c-card ee-c-quote", tg("blockquote", "", hs(a[0])) + tg("div", "who", who)));
});
WEB("بطاقة_فريق", 3, "صورة، اسم، دور", {
  return mkHtml(tg("div", "ee-c-card ee-c-team", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("h3", "", hs(a[1])) + tg("p", "", hs(a[2]))));
});
WEB("بطاقة_سعر", 3, "اسم، سعر، ميزات، نص_زر، مميزة", {
  auto feats = listOf(a[2], "$بطاقة_سعر(اسم، سعر، [ميزات]، نص_زر، مميزة)", ln);
  std::string li;
  for (auto& f : feats) li += tg("li", "", hs(f));
  bool hot = a.size() > 4 && truthy(a[4]);
  std::string in = (hot ? tg("span", "tag", "الأكثر طلباً") : "") + tg("h3", "", hs(a[0])) + tg("div", "price", hs(a[1])) + tg("ul", "", li) +
                   tg("button", std::string("ee-b-btn ") + (hot ? "primary" : "outline primary"), a.size() > 3 ? hs(a[3]) : "اختر الباقة", " type=\"button\"");
  return mkHtml(tg("div", std::string("ee-c-card ee-c-price") + (hot ? " hot" : ""), in));
});
WEB("بطاقة_منتج", 3, "صورة، اسم، سعر، رابط", {
  std::string body = tg("h3", "", hs(a[1])) + tg("div", "price", hs(a[2]));
  if (a.size() > 3 && a[3].t != VT::Null) body += tg("a", "ee-b-btn primary", "اشترِ الآن", " href=\"" + hu(a[3]) + "\"");
  return mkHtml(tg("article", "ee-c-card", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("div", "b", body)));
});
WEB("بطاقة_مقال", 3, "صورة، عنوان، ملخص، تاريخ، رابط", {
  std::string body = (a.size() > 3 && a[3].t != VT::Null ? tg("span", "meta", hs(a[3])) : "") + tg("h3", "", hs(a[1])) + tg("p", "", hs(a[2]));
  if (a.size() > 4 && a[4].t != VT::Null) body += tg("a", "more", "متابعة القراءة ←", " href=\"" + hu(a[4]) + "\"");
  return mkHtml(tg("article", "ee-c-card", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("div", "b", body)));
});
WEB("بطاقة_رابط", 3, "عنوان، وصف، رابط", {
  return mkHtml(tg("a", "ee-c-card ee-c-link", tg("h3", "", hs(a[0])) + tg("p", "", hs(a[1])), " href=\"" + hu(a[2]) + "\""));
});

// ───── الأقسام (11) ─────
{
  struct Sec { const char* name; const char* cls; };
  static const Sec secs[] = {{"قسم_ميزات", ""}, {"قسم_اسعار", ""}, {"قسم_فريق", ""}, {"قسم_شهادات", "tint"}};
  for (const Sec& sc : secs) {
    std::string nm = sc.name, cls = std::string("ee-s-sec ") + sc.cls;
    reg(sc.name, [nm, cls](Interp&, Args& a, int ln) -> Value {
      std::string usage = "$" + nm + "(عنوان، [بطاقات]، وصف)";
      needArgs(a, 2, usage.c_str(), ln);
      auto cards = listOf(a[1], usage.c_str(), ln);
      std::string head = tg("h2", "", hs(a[0])) + (a.size() > 2 && a[2].t != VT::Null ? tg("p", "", hs(a[2])) : "");
      std::string grid;
      for (auto& c : cards) grid += hs(c);
      return mkHtml(tg("section", cls, tg("div", "in", tg("div", "ee-s-head", head) + tg("div", "ee-s-grid", grid))));
    });
  }
}
WEB("قسم_احصائيات", 1, "[بطاقات]", {
  auto cards = listOf(a[0], "$قسم_احصائيات([بطاقات])", ln);
  std::string grid;
  for (auto& c : cards) grid += hs(c);
  return mkHtml(tg("section", "ee-s-sec tint", tg("div", "in", tg("div", "ee-s-grid", grid))));
});
WEB("قسم_دعوة", 3, "عنوان، وصف، نص_زر، رابط", {
  std::string btn = tg("a", "ee-b-btn lg", hs(a[2]), " href=\"" + (a.size() > 3 ? hu(a[3]) : std::string("#")) + "\"");
  return mkHtml(tg("section", "ee-s-cta", tg("h2", "", hs(a[0])) + tg("p", "", hs(a[1])) + btn));
});
WEB("قسم_اسئلة", 2, "عنوان، [[سؤال، جواب]...]", {
  auto qs = listOf(a[1], "$قسم_اسئلة(عنوان، [[سؤال، جواب]...])", ln);
  std::string items;
  for (auto& q : qs) {
    auto& p = pairOf(q, 2, "$قسم_اسئلة(عنوان، [[سؤال، جواب]...])", ln);
    items += tg("details", "", tg("summary", "", hs(p[0])) + tg("div", "", hs(p[1])));
  }
  return mkHtml(tg("section", "ee-s-sec", tg("div", "in", tg("div", "ee-s-head", tg("h2", "", hs(a[0]))) + tg("div", "ee-s-faq", items))));
});
WEB("قسم_عنوان", 1, "عنوان، وصف", {
  return mkHtml(tg("div", "ee-s-head", tg("h2", "", hs(a[0])) + (a.size() > 1 ? tg("p", "", hs(a[1])) : "")));
});
WEB("بطل_صورة", 3, "عنوان، وصف، صورة، نص_زر، رابط", {
  std::string txt = tg("h1", "", hs(a[0])) + tg("p", "", hs(a[1]));
  if (a.size() > 3) txt += tg("a", "ee-b-btn primary lg", hs(a[3]), " href=\"" + (a.size() > 4 ? hu(a[4]) : std::string("#")) + "\"");
  return mkHtml(tg("section", "ee-s-hero2", tg("div", "", txt) + tg("img", "", "", " src=\"" + hu(a[2]) + "\" alt=\"" + ha(a[0]) + "\"")));
});
WEB("بطل_بسيط", 2, "عنوان، وصف، نص_زر، رابط", {
  std::string in = tg("h1", "", hs(a[0])) + tg("p", "", hs(a[1]));
  if (a.size() > 2) in += tg("a", "ee-b-btn primary lg", hs(a[2]), " href=\"" + (a.size() > 3 ? hu(a[3]) : std::string("#")) + "\"");
  return mkHtml(tg("section", "ee-s-hero3", in));
});
WEB("قسم_ملون", 1, "لون(فاتح|داكن|دافئ)، محتوى...", {
  std::string k = toStr(a[0]);
  std::string cls = k == "داكن" ? "dark" : (k == "دافئ" ? "warm" : "tint");
  return mkHtml(tg("section", "ee-s-sec " + cls, tg("div", "in", hrest(a, 1))));
});

// ───── التنقل (10) ─────
WEB("شعار", 1, "نص، رمز", {
  std::string sym = a.size() > 1 ? tg("b", "", hs(a[1])) : "";
  return mkHtml(tg("a", "ee-n-logo", sym + hs(a[0]), " href=\"#\""));
});
WEB("شريط_علوي", 2, "شعار، [[نص، رابط]...]", {
  auto links = listOf(a[1], "$شريط_علوي(شعار، [[نص، رابط]...])", ln);
  std::string ls;
  for (auto& l : links) {
    auto& p = pairOf(l, 2, "$شريط_علوي(شعار، [[نص، رابط]...])", ln);
    ls += tg("a", "", hs(p[0]), " href=\"" + hu(p[1]) + "\"");
  }
  std::string id = "ee-nt" + std::to_string(webUid());
  std::string logo = a[0].t == VT::Str && a[0].b ? a[0].s : tg("a", "ee-n-logo", hs(a[0]), " href=\"#\"");
  return mkHtml(tg("nav", "ee-n-bar", logo + "<input type=\"checkbox\" id=\"" + id + "\" class=\"ee-n-t\">" +
                                          tg("label", "ee-n-burger", "☰", " for=\"" + id + "\" aria-label=\"القائمة\"") + tg("div", "ee-n-links", ls)));
});
WEB("فتات_الخبز", 1, "[عناصر]", {
  auto items = listOf(a[0], "$فتات_الخبز([[نص، رابط]، ...، \"الصفحة الحالية\"])", ln);
  std::string li;
  for (auto& it : items) {
    if (it.t == VT::List && it.list->size() >= 2) li += tg("li", "", tg("a", "", hs((*it.list)[0]), " href=\"" + hu((*it.list)[1]) + "\""));
    else li += tg("li", "", hs(it), " aria-current=\"page\"");
  }
  return mkHtml(tg("nav", "", tg("ol", "ee-n-crumbs", li), " aria-label=\"مسار التنقل\""));
});
WEB("ترقيم_صفحات", 2, "الحالية، الإجمالي، قالب_رابط", {
  double cur = needNum("ترقيم_صفحات", a[0], ln), tot = needNum("ترقيم_صفحات", a[1], ln);
  if (tot < 1 || cur < 1 || cur > tot || tot > 10000 || cur != std::floor(cur) || tot != std::floor(tot)) fail(ln, "الحالية يجب أن تكون بين 1 والإجمالي");
  std::string tpl = a.size() > 2 ? toStr(a[2]) : "?page={}";
  auto href = [&](int n) {
    std::string h = tpl;
    size_t p = h.find("{}");
    if (p != std::string::npos) h.replace(p, 2, std::to_string(n));
    return " href=\"" + htmlEsc(safeUrl(h)) + "\"";
  };
  int c = static_cast<int>(cur), t = static_cast<int>(tot);
  std::string li;
  if (c > 1) li += tg("li", "", tg("a", "", "→", href(c - 1) + " aria-label=\"السابق\""));
  int last = 0;
  for (int p = 1; p <= t; p++) {
    if (p != 1 && p != t && std::abs(p - c) > 1) continue;
    if (p - last > 1) li += tg("li", "", tg("span", "", "…"));
    li += p == c ? tg("li", "", tg("span", "cur", std::to_string(p)), " aria-current=\"page\"") : tg("li", "", tg("a", "", std::to_string(p), href(p)));
    last = p;
  }
  if (c < t) li += tg("li", "", tg("a", "", "←", href(c + 1) + " aria-label=\"التالي\""));
  return mkHtml(tg("nav", "", tg("ul", "ee-n-pages", li), " aria-label=\"ترقيم الصفحات\""));
});
WEB("تبويبات", 1, "[[عنوان، محتوى]...]", {
  auto tabs = listOf(a[0], "$تبويبات([[عنوان، محتوى]...])", ln);
  std::string head, panes;
  for (size_t i = 0; i < tabs.size(); i++) {
    auto& p = pairOf(tabs[i], 2, "$تبويبات([[عنوان، محتوى]...])", ln);
    head += tg("button", i == 0 ? "on" : "", hs(p[0]), " type=\"button\" role=\"tab\"");
    panes += tg("div", "tp", hs(p[1]), i == 0 ? "" : " hidden");
  }
  return mkHtml(tg("div", "ee-n-tabs", tg("div", "tl", head, " role=\"tablist\"") + panes, " data-ee-tabs"));
});
WEB("قائمة_جانبية", 1, "[[نص، رابط]...]", {
  auto links = listOf(a[0], "$قائمة_جانبية([[نص، رابط]...])", ln);
  std::string li;
  for (auto& l : links) {
    auto& p = pairOf(l, 2, "$قائمة_جانبية([[نص، رابط]...])", ln);
    li += tg("li", "", tg("a", "", hs(p[0]), " href=\"" + hu(p[1]) + "\""));
  }
  return mkHtml(tg("nav", "", tg("ul", "ee-n-side", li)));
});
WEB("تذييل_كامل", 2, "شعار، وصف، [[عنوان، [[نص، رابط]...]]...]، حقوق", {
  std::string cols;
  if (a.size() > 2 && a[2].t == VT::List) {
    for (auto& c : *a[2].list) {
      auto& p = pairOf(c, 2, "$تذييل_كامل(شعار، وصف، [[عنوان، [[نص، رابط]...]]...]، حقوق)", ln);
      std::string li;
      for (auto& l : listOf(p[1], "$تذييل_كامل(...)", ln)) {
        auto& q = pairOf(l, 2, "$تذييل_كامل(...)", ln);
        li += tg("li", "", tg("a", "", hs(q[0]), " href=\"" + hu(q[1]) + "\""));
      }
      cols += tg("div", "", tg("h4", "", hs(p[0])) + tg("ul", "", li));
    }
  }
  std::string first = tg("div", "", tg("a", "ee-n-logo", hs(a[0]), " href=\"#\"") + tg("p", "ee-t-muted", hs(a[1])));
  std::string copy = a.size() > 3 ? tg("div", "copy", hs(a[3])) : "";
  return mkHtml(tg("footer", "ee-n-foot", tg("div", "in", first + cols) + copy));
});
WEB("زر_للاعلى", 0, "", return mkHtml(tg("a", "ee-n-top", "↑", " href=\"#\" aria-label=\"إلى الأعلى\"")););
WEB("روابط_اجتماعية", 1, "[[اسم، رابط]...]", {
  auto links = listOf(a[0], "$روابط_اجتماعية([[اسم، رابط]...])", ln);
  std::string r;
  for (auto& l : links) {
    auto& p = pairOf(l, 2, "$روابط_اجتماعية([[اسم، رابط]...])", ln);
    r += tg("a", "", hs(p[0]), " href=\"" + hu(p[1]) + "\" rel=\"noopener\"");
  }
  return mkHtml(tg("div", "ee-n-social", r));
});
WEB("حقوق_النشر", 1, "اسم، سنة", {
  std::string yr = a.size() > 1 ? toStr(a[1]) : "";
  return mkHtml(tg("div", "ee-n-foot ee-t-muted", tg("div", "copy", "© " + htmlEsc(yr) + (yr.empty() ? "" : " ") + hs(a[0]) + " — جميع الحقوق محفوظة")));
});

// ───── التفاعل (10) ─────
WEB("اكورديون", 1, "[[عنوان، محتوى]...]", {
  auto items = listOf(a[0], "$اكورديون([[عنوان، محتوى]...])", ln);
  std::string r;
  int uid = webUid();
  for (auto& it : items) {
    auto& p = pairOf(it, 2, "$اكورديون([[عنوان، محتوى]...])", ln);
    r += tg("details", "", tg("summary", "", hs(p[0])) + tg("div", "", hs(p[1])), " name=\"ee-acc" + std::to_string(uid) + "\"");
  }
  return mkHtml(tg("div", "ee-u-acc", r));
});
WEB("تفاصيل", 1, "عنوان، محتوى...", return mkHtml(tg("details", "ee-u-det", tg("summary", "", hs(a[0])) + tg("div", "", hrest(a, 1)))););
WEB("تبديل_الوضع", 0, "نص", {
  return mkHtml(tg("button", "ee-b-btn outline primary ee-u-theme", a.empty() ? std::string("◐ الوضع الفاتح/الداكن") : hs(a[0]), " type=\"button\" data-ee-theme"));
});
WEB("نافذة_منبثقة", 2, "معرف، عنوان، محتوى...", {
  std::string id = toStr(a[0]);
  for (char c : id) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) fail(ln, "المعرّف حروف لاتينية وأرقام و - و _ فقط");
  std::string head = tg("div", "h", tg("h3", "", hs(a[1])) + "<form method=\"dialog\"><button aria-label=\"إغلاق\">×</button></form>");
  return mkHtml(tg("dialog", "ee-u-modal", head + tg("div", "bd", hrest(a, 2)), " id=\"" + id + "\""));
});
WEB("زر_فتح_نافذة", 2, "معرف، نص", {
  return mkHtml(tg("button", "ee-b-btn primary", hs(a[1]), " type=\"button\" data-ee-open=\"" + ha(a[0]) + "\""));
});
WEB("شريط_تقدم", 1, "نسبة، نص", {
  double p = std::max(0.0, std::min(100.0, needNum("شريط_تقدم", a[0], ln)));
  std::string label = a.size() > 1 ? hs(a[1]) : "";
  return mkHtml(tg("div", "ee-u-prog", tg("div", "t", tg("span", "", label) + tg("span", "", fmtNum(p) + "%")) +
                                            tg("div", "tr", tg("div", "fl", "", " style=\"width:" + fmtNum(p) + "%\""), " role=\"progressbar\" aria-valuenow=\"" + fmtNum(p) + "\" aria-valuemin=\"0\" aria-valuemax=\"100\"")));
});
WEB("عداد_تصاعدي", 1, "قيمة، وصف", {
  double v = needNum("عداد_تصاعدي", a[0], ln);
  return mkHtml(tg("div", "", tg("span", "ee-u-count", "0", " data-ee-count=\"" + fmtNum(v) + "\"") + (a.size() > 1 ? tg("span", "ee-t-muted", hs(a[1]), " style=\"display:block;text-align:center\"") : "")));
});
WEB("مؤقت_عكسي", 1, "تاريخ_ISO مثل 2027-01-01T00:00:00Z", {
  std::string iso = toStr(a[0]);
  for (char c : iso) if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == ':' || c == 'T' || c == 'Z' || c == '+' || c == '.' || c == ' ')) fail(ln, "تاريخ غير صالح: اكتب مثل 2027-01-01T00:00:00Z");
  auto box = [](const char* label) { return tg("div", "", tg("b", "", "00") + tg("span", "", label)); };
  return mkHtml(tg("div", "ee-u-cd", box("يوم") + box("ساعة") + box("دقيقة") + box("ثانية"), " data-ee-countdown=\"" + htmlEsc(iso) + "\""));
});
WEB("زر_نسخ", 1, "نص_للنسخ، عنوان", {
  return mkHtml(tg("button", "ee-b-btn outline primary", a.size() > 1 ? hs(a[1]) : "نسخ", " type=\"button\" data-ee-copy=\"" + ha(a[0]) + "\""));
});
WEB("تلميح", 2, "نص، تلميح", return mkHtml(tg("span", "ee-u-tip", hs(a[0]), " tabindex=\"0\" data-tip=\"" + ha(a[1]) + "\"")););

// ───── البيانات (10) ─────
WEB("جدول_مخطط", 2, "[رؤوس]، [[صف]...]", {
  auto heads = listOf(a[0], "$جدول_مخطط([رؤوس]، [[صف]...])", ln);
  auto rows = listOf(a[1], "$جدول_مخطط([رؤوس]، [[صف]...])", ln);
  std::string th, body;
  for (auto& h : heads) th += tg("th", "", hs(h), " scope=\"col\"");
  for (auto& r : rows) {
    std::string td;
    for (auto& c : listOf(r, "كل صف قائمة خلايا", ln)) td += tg("td", "", hs(c));
    body += tg("tr", "", td);
  }
  return mkHtml("<div style=\"overflow-x:auto\">" + tg("table", "ee-d-tbl", tg("thead", "", tg("tr", "", th)) + tg("tbody", "", body)) + "</div>");
});
WEB("قائمة_وصف", 1, "[[مصطلح، وصف]...]", {
  std::string r;
  for (auto& it : listOf(a[0], "$قائمة_وصف([[مصطلح، وصف]...])", ln)) {
    auto& p = pairOf(it, 2, "$قائمة_وصف([[مصطلح، وصف]...])", ln);
    r += tg("dt", "", hs(p[0])) + tg("dd", "", hs(p[1]));
  }
  return mkHtml(tg("dl", "ee-d-dl", r));
});
WEB("خط_زمني", 1, "[[تاريخ، عنوان، وصف]...]", {
  std::string r;
  for (auto& it : listOf(a[0], "$خط_زمني([[تاريخ، عنوان، وصف]...])", ln)) {
    auto& p = pairOf(it, 2, "$خط_زمني([[تاريخ، عنوان، وصف]...])", ln);
    r += tg("li", "", tg("time", "", hs(p[0])) + tg("h4", "", hs(p[1])) + (p.size() > 2 ? tg("p", "", hs(p[2])) : ""));
  }
  return mkHtml(tg("ol", "ee-d-time", r));
});
WEB("تقييم_نجوم", 1, "قيمة، من", {
  double v = needNum("تقييم_نجوم", a[0], ln), n = a.size() > 1 ? needNum("تقييم_نجوم", a[1], ln) : 5;
  if (n < 1 || n > 10 || n != std::floor(n)) fail(ln, "عدد النجوم من 1 إلى 10");
  v = std::max(0.0, std::min(n, v));
  std::string stars;
  for (int i = 0; i < static_cast<int>(n); i++) stars += "★";
  return mkHtml(tg("span", "ee-d-stars", stars, " style=\"--v:" + fmtNum(v) + ";--n:" + fmtNum(n) + "\" role=\"img\" aria-label=\"تقييم " + fmtNum(v) + " من " + fmtNum(n) + "\""));
});
WEB("رسم_دائري", 2, "[تسميات]، [قيم]", {
  auto labs = listOf(a[0], "$رسم_دائري([تسميات]، [قيم])", ln);
  auto vals = listOf(a[1], "$رسم_دائري([تسميات]، [قيم])", ln);
  if (labs.empty() || labs.size() != vals.size()) fail(ln, "$رسم_دائري تتطلب قائمتين غير فارغتين بالطول نفسه");
  double tot = 0;
  std::vector<double> v;
  for (auto& x : vals) { double d = needNum("رسم_دائري", x, ln); if (d < 0) fail(ln, "قيم الرسم الدائري غير سالبة"); v.push_back(d); tot += d; }
  if (tot <= 0) fail(ln, "مجموع القيم يجب أن يكون موجباً");
  const double C = 2 * 3.14159265358979 * 70;
  std::string circles, leg;
  double off = 0;
  char buf[260];
  for (size_t i = 0; i < v.size(); i++) {
    double len = v[i] / tot * C;
    std::snprintf(buf, sizeof buf, "<circle r=\"70\" cx=\"110\" cy=\"110\" fill=\"none\" stroke=\"var(--c%d)\" stroke-width=\"34\" stroke-dasharray=\"%.2f %.2f\" stroke-dashoffset=\"%.2f\" transform=\"rotate(-90 110 110)\"/>",
                  static_cast<int>(i % 6) + 1, len, C - len, -off);
    circles += buf;
    off += len;
    std::snprintf(buf, sizeof buf, " style=\"--k:var(--c%d)\"", static_cast<int>(i % 6) + 1);
    leg += tg("li", "", tg("i", "", "", buf) + tg("span", "", hs(labs[i]) + " — " + fmtNum(v[i]) + " (" + fmtNum(std::round(v[i] / tot * 1000) / 10) + "%)"));
  }
  std::string svg = "<svg viewBox=\"0 0 220 220\" role=\"img\" aria-label=\"رسم دائري\">" + circles +
                    "<text x=\"110\" y=\"118\" text-anchor=\"middle\" style=\"font:800 26px system-ui;fill:var(--fg)\">" + htmlEsc(fmtNum(tot)) + "</text></svg>";
  return mkHtml(tg("div", "ee-d-chart", svg + tg("ul", "ee-d-leg", leg)));
});
WEB("رسم_خطي", 2, "[تسميات]، [قيم]", {
  auto labs = listOf(a[0], "$رسم_خطي([تسميات]، [قيم])", ln);
  auto vals = listOf(a[1], "$رسم_خطي([تسميات]، [قيم])", ln);
  if (labs.size() < 2 || labs.size() != vals.size()) fail(ln, "$رسم_خطي تتطلب قائمتين بالطول نفسه (نقطتان على الأقل)");
  std::vector<double> v;
  double mx = 0, mn = 1e300;
  for (auto& x : vals) { double d = needNum("رسم_خطي", x, ln); v.push_back(d); mx = std::max(mx, d); mn = std::min(mn, d); }
  mn = std::min(mn, 0.0);
  if (mx <= mn) mx = mn + 1;
  const double W = 600, H = 280, L = 20, R = 20, T = 30, B = 40;
  size_t n = v.size();
  auto X = [&](size_t i) { return L + (W - L - R) * static_cast<double>(i) / static_cast<double>(n - 1); };
  auto Y = [&](double d) { return H - B - (d - mn) / (mx - mn) * (H - T - B); };
  std::string pts, dots, xl;
  char buf[200];
  for (size_t i = 0; i < n; i++) {
    std::snprintf(buf, sizeof buf, "%.1f,%.1f ", X(i), Y(v[i]));
    pts += buf;
    std::snprintf(buf, sizeof buf, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"5\" fill=\"var(--accent)\" stroke=\"var(--card)\" stroke-width=\"2\"/>", X(i), Y(v[i]));
    dots += buf;
    std::snprintf(buf, sizeof buf, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" style=\"font:600 15px system-ui;fill:var(--fg)\">", X(i), Y(v[i]) - 12);
    dots += buf + htmlEsc(fmtNum(v[i])) + "</text>";
    std::snprintf(buf, sizeof buf, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" style=\"font:15px system-ui;fill:var(--muted)\">", X(i), H - 12);
    xl += buf + htmlEsc(toStr(labs[i])) + "</text>";
  }
  std::snprintf(buf, sizeof buf, "%.1f,%.1f %s%.1f,%.1f", X(0), Y(mn), pts.c_str(), X(n - 1), Y(mn));
  std::string area = buf;
  std::string svg = "<svg class=\"ee-d-line\" viewBox=\"0 0 600 280\" role=\"img\" aria-label=\"رسم خطي\">"
                    "<line x1=\"20\" x2=\"580\" y1=\"" + fmtNum(std::round(Y(mn))) + "\" y2=\"" + fmtNum(std::round(Y(mn))) + "\" stroke=\"var(--line)\"/>"
                    "<polygon points=\"" + area + "\" fill=\"var(--accent)\" opacity=\".13\"/>"
                    "<polyline points=\"" + pts + "\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"3.5\" stroke-linejoin=\"round\" stroke-linecap=\"round\"/>" + dots + xl + "</svg>";
  return mkHtml(svg);
});
WEB("شريط_نسبة", 2, "تسمية، نسبة", {
  double p = std::max(0.0, std::min(100.0, needNum("شريط_نسبة", a[1], ln)));
  return mkHtml(tg("div", "ee-d-meter", tg("div", "t", tg("span", "", hs(a[0])) + tg("span", "", fmtNum(p) + "%")) +
                                             tg("div", "tr", tg("div", "fl", "", " style=\"width:" + fmtNum(p) + "%\""))));
});
WEB("رقم_كبير", 1, "قيمة، وصف", {
  return mkHtml(tg("div", "ee-d-kpi", tg("b", "", hs(a[0])) + (a.size() > 1 ? tg("span", "", hs(a[1])) : "")));
});
WEB("خطوات", 1, "[[عنوان، وصف]...]", {
  std::string r;
  for (auto& it : listOf(a[0], "$خطوات([[عنوان، وصف]...])", ln)) {
    auto& p = pairOf(it, 1, "$خطوات([[عنوان، وصف]...])", ln);
    r += tg("li", "", tg("div", "", tg("h4", "", hs(p[0])) + (p.size() > 1 ? tg("p", "", hs(p[1])) : "")));
  }
  return mkHtml(tg("ol", "ee-d-steps", r));
});
WEB("قائمة_تحقق", 1, "[عناصر أو [نص، منجز]]", {
  std::string r;
  for (auto& it : listOf(a[0], "$قائمة_تحقق([\"مهمة\"، [\"مهمة منجزة\"، صح]])", ln)) {
    if (it.t == VT::List && it.list->size() >= 2) r += tg("li", truthy((*it.list)[1]) ? "on" : "", hs((*it.list)[0]));
    else r += tg("li", "", hs(it));
  }
  return mkHtml(tg("ul", "ee-d-check", r));
});

// ───── الوسائط (7) ─────
WEB("معرض_صور", 1, "[روابط الصور]", {
  std::string r;
  for (auto& u : listOf(a[0], "$معرض_صور([روابط])", ln)) r += tg("img", "", "", " src=\"" + hu(u) + "\" alt=\"\" loading=\"lazy\"");
  return mkHtml(tg("div", "ee-m-gal", r));
});
WEB("صورة_دائرية", 1, "مصدر، حجم، وصف", {
  double sz = a.size() > 1 ? needNum("صورة_دائرية", a[1], ln) : 64;
  sz = std::max(16.0, std::min(400.0, sz));
  return mkHtml(tg("img", "ee-m-av", "", " src=\"" + hu(a[0]) + "\" alt=\"" + (a.size() > 2 ? ha(a[2]) : std::string()) + "\" width=\"" + fmtNum(sz) + "\" height=\"" + fmtNum(sz) + "\" loading=\"lazy\""));
});
WEB("ايقونة", 1, "رمز", return mkHtml(tg("span", "ee-m-icon", hs(a[0]), " aria-hidden=\"true\"")););
WEB("فيديو_يوتيوب", 1, "معرف_الفيديو", {
  std::string id = toStr(a[0]);
  if (id.empty() || id.size() > 20) fail(ln, "معرّف الفيديو غير صالح");
  for (char c : id) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) fail(ln, "معرّف الفيديو غير صالح");
  return mkHtml(tg("div", "ee-m-yt", tg("iframe", "", "", " src=\"https://www.youtube-nocookie.com/embed/" + id + "\" title=\"فيديو\" loading=\"lazy\" allowfullscreen")));
});
WEB("فيديو", 1, "مصدر", return mkHtml(tg("video", "ee-m-vid", "", " src=\"" + hu(a[0]) + "\" controls preload=\"metadata\"")););
WEB("صورة_بعنوان", 2, "مصدر، وصف", {
  return mkHtml(tg("figure", "ee-m-fig", tg("img", "", "", " src=\"" + hu(a[0]) + "\" alt=\"" + ha(a[1]) + "\" loading=\"lazy\"") + tg("figcaption", "", hs(a[1]))));
});
WEB("فاصل_موجة", 0, "", {
  return mkHtml(tg("div", "ee-m-wave", "<svg viewBox=\"0 0 1440 60\" preserveAspectRatio=\"none\" aria-hidden=\"true\"><path fill=\"currentColor\" d=\"M0,30 C240,70 480,0 720,30 C960,60 1200,10 1440,30 L1440,60 L0,60 Z\"/></svg>"));
});

// ───── النماذج (11) ─────
{
  struct Fld { const char* name; const char* type; const char* dir; };
  static const Fld flds[] = {{"حقل_بريد", "email", " dir=\"ltr\" autocomplete=\"email\""}, {"حقل_سري", "password", " dir=\"ltr\" autocomplete=\"current-password\""},
                             {"حقل_رقم", "number", " dir=\"ltr\""}};
  for (const Fld& f : flds) {
    std::string nm = f.name, type = f.type, extra = f.dir;
    reg(f.name, [nm, type, extra](Interp&, Args& a, int ln) -> Value {
      needArgs(a, 2, ("$" + nm + "(معرف، تسمية)").c_str(), ln);
      return mkHtml(tg("div", "ee-f-g", tg("label", "", hs(a[1]), " for=\"" + ha(a[0]) + "\"") +
                                             tg("input", "", "", " id=\"" + ha(a[0]) + "\" name=\"" + ha(a[0]) + "\" type=\"" + type + "\"" + extra)));
    });
  }
}
WEB("قائمة_اختيار", 3, "معرف، تسمية، [خيارات]", {
  std::string opts;
  for (auto& o : listOf(a[2], "$قائمة_اختيار(معرف، تسمية، [خيارات أو [قيمة، نص]])", ln)) {
    if (o.t == VT::List && o.list->size() >= 2) opts += tg("option", "", hs((*o.list)[1]), " value=\"" + ha((*o.list)[0]) + "\"");
    else opts += tg("option", "", hs(o), " value=\"" + ha(o) + "\"");
  }
  return mkHtml(tg("div", "ee-f-g", tg("label", "", hs(a[1]), " for=\"" + ha(a[0]) + "\"") + tg("select", "", opts, " id=\"" + ha(a[0]) + "\" name=\"" + ha(a[0]) + "\"")));
});
WEB("مربع_اختيار", 2, "معرف، تسمية", {
  return mkHtml(tg("label", "ee-f-chk", tg("input", "", "", " type=\"checkbox\" id=\"" + ha(a[0]) + "\" name=\"" + ha(a[0]) + "\"") + tg("span", "", hs(a[1]))));
});
WEB("ازرار_راديو", 2, "اسم، [خيارات أو [قيمة، نص]]", {
  std::string r;
  for (auto& o : listOf(a[1], "$ازرار_راديو(اسم، [خيارات])", ln)) {
    std::string val = o.t == VT::List && o.list->size() >= 2 ? ha((*o.list)[0]) : ha(o);
    std::string txt = o.t == VT::List && o.list->size() >= 2 ? hs((*o.list)[1]) : hs(o);
    r += tg("label", "ee-f-chk", tg("input", "", "", " type=\"radio\" name=\"" + ha(a[0]) + "\" value=\"" + val + "\"") + tg("span", "", txt));
  }
  return mkHtml(tg("div", "ee-f-radio", r, " role=\"radiogroup\""));
});
WEB("مفتاح_تبديل", 2, "معرف، تسمية", {
  return mkHtml(tg("label", "ee-f-sw", tg("input", "", "", " type=\"checkbox\" role=\"switch\" id=\"" + ha(a[0]) + "\" name=\"" + ha(a[0]) + "\"") + tg("span", "", hs(a[1]))));
});
WEB("نموذج_اتصال", 0, "", {
  std::string f = tg("h3", "", "تواصل معنا") +
                  tg("div", "ee-f-g", tg("label", "", "الاسم", " for=\"cn\"") + tg("input", "", "", " id=\"cn\" name=\"name\" type=\"text\" required")) +
                  tg("div", "ee-f-g", tg("label", "", "البريد الإلكتروني", " for=\"ce\"") + tg("input", "", "", " id=\"ce\" name=\"email\" type=\"email\" dir=\"ltr\" required")) +
                  tg("div", "ee-f-g", tg("label", "", "الرسالة", " for=\"cm\"") + tg("textarea", "", "", " id=\"cm\" name=\"message\" rows=\"5\" required")) +
                  tg("button", "ee-b-btn primary", "إرسال", " type=\"submit\"");
  return mkHtml(tg("form", "ee-f-form", f, " action=\"#\" method=\"post\""));
});
WEB("نموذج_دخول", 0, "", {
  std::string f = tg("h3", "", "تسجيل الدخول") +
                  tg("div", "ee-f-g", tg("label", "", "البريد الإلكتروني", " for=\"le\"") + tg("input", "", "", " id=\"le\" name=\"email\" type=\"email\" dir=\"ltr\" required autocomplete=\"email\"")) +
                  tg("div", "ee-f-g", tg("label", "", "كلمة المرور", " for=\"lp\"") + tg("input", "", "", " id=\"lp\" name=\"password\" type=\"password\" dir=\"ltr\" required autocomplete=\"current-password\"")) +
                  tg("label", "ee-f-chk", tg("input", "", "", " type=\"checkbox\" name=\"remember\"") + tg("span", "", "تذكّرني")) +
                  tg("button", "ee-b-btn primary", "دخول", " type=\"submit\"");
  return mkHtml(tg("form", "ee-f-form", f, " action=\"#\" method=\"post\""));
});
WEB("نموذج_اشتراك", 0, "نص_الزر", {
  return mkHtml(tg("form", "ee-f-news", tg("input", "", "", " type=\"email\" name=\"email\" dir=\"ltr\" placeholder=\"بريدك الإلكتروني\" required aria-label=\"البريد الإلكتروني\"") +
                                              tg("button", "ee-b-btn primary", a.empty() ? std::string("اشترك") : hs(a[0]), " type=\"submit\""), " action=\"#\" method=\"post\""));
});
WEB("شريط_بحث", 0, "نص_ارشادي", {
  return mkHtml(tg("form", "ee-f-search", tg("input", "", "", " type=\"search\" name=\"q\" placeholder=\"" + (a.empty() ? std::string("ابحث…") : ha(a[0])) + "\" aria-label=\"بحث\"") +
                                              tg("button", "ee-b-btn primary", "بحث", " type=\"submit\""), " role=\"search\" action=\"#\""));
});
#undef WEB
