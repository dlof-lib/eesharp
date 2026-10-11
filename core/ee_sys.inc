// ═════════════════ النظام: الملفات والوحدات والوسائط — تُضمَّن داخل Interp::setup() ═════════════════
{
  auto needFiles = [](Interp& I, int ln) {
    if (!I.opt.allow_files) fail(ln, "دوال الملفات والاستيراد غير متاحة في هذا الوضع (تعمل عند التشغيل من سطر الأوامر)");
  };
  auto resolve = [](Interp& I, const std::string& p) {
    if (p.empty() || p[0] == '/' || (p.size() > 1 && p[1] == ':')) return p;
    const std::string& d = I.dirStack.empty() ? std::string() : I.dirStack.back();
    return d.empty() ? p : d + "/" + p;
  };
  auto dirOf = [](const std::string& p) {
    size_t i = p.find_last_of("/\\");
    return i == std::string::npos ? std::string() : p.substr(0, i);
  };
  auto readAll = [](const std::string& path, int ln) {
    std::ifstream f(path, std::ios::binary);
    if (!f) fail(ln, "تعذّر فتح الملف: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.size() > 32u * 1024 * 1024) fail(ln, "الملف كبير جداً: " + path);
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
      s.erase(0, 3);
    return s;
  };

  reg("استورد", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $استورد(\"مسار.ee\")");
    needFiles(I, ln);
    std::string path = resolve(I, a[0].s);
    if (I.imported.count(path)) return Value();
    if (I.dirStack.size() > 50) fail(ln, "استيراد متداخل أكثر من اللازم");
    I.imported.insert(path);
    std::string src = readAll(path, ln);
    I.dirStack.push_back(dirOf(path));
    try {
      Parser ps;
      ps.t = lex(src);
      std::vector<SP> prog = ps.parseProgram();
      Flow f = I.execList(prog, I.globals);
      if (f == Flow::Break || f == Flow::Continue) fail(ln, "'توقف' أو 'استمر' خارج حلقة");
    } catch (EEError& e) {
      I.dirStack.pop_back();
      e.msg = "في الوحدة " + path + (e.line > 0 ? " (السطر " + std::to_string(e.line) + ")" : "") + ": " + e.msg;
      e.line = ln;
      throw;
    } catch (...) {
      I.dirStack.pop_back();
      throw;
    }
    I.dirStack.pop_back();
    return Value();
  });
  reg("اقرأ_ملف", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $اقرأ_ملف(\"مسار\")");
    needFiles(I, ln);
    return mkStr(readAll(resolve(I, a[0].s), ln));
  });
  reg("اسطر_ملف", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $اسطر_ملف(\"مسار\")");
    needFiles(I, ln);
    std::string s = readAll(resolve(I, a[0].s), ln);
    Value r;
    r.t = VT::List;
    r.list = std::make_shared<std::vector<Value>>();
    std::istringstream is(s);
    std::string line;
    while (std::getline(is, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      r.list->push_back(mkStr(line));
    }
    return r;
  });
  for (int mode = 0; mode < 2; mode++) {
    reg(mode ? "الحق_ملف" : "اكتب_ملف", [=](Interp& I, Args& a, int ln) -> Value {
      if (a.size() != 2 || a[0].t != VT::Str) fail(ln, mode ? "الاستخدام: $الحق_ملف(\"مسار\"، نص)" : "الاستخدام: $اكتب_ملف(\"مسار\"، نص)");
      needFiles(I, ln);
      std::string path = resolve(I, a[0].s);
      std::ofstream f(path, std::ios::binary | (mode ? std::ios::app : std::ios::trunc));
      if (!f) fail(ln, "تعذّرت الكتابة في الملف: " + path);
      f << toStr(a[1]);
      return Value();
    });
  }
  reg("يوجد_ملف", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $يوجد_ملف(\"مسار\")");
    needFiles(I, ln);
    std::ifstream f(resolve(I, a[0].s), std::ios::binary);
    return mkBool(static_cast<bool>(f));
  });
  reg("احذف_ملف", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $احذف_ملف(\"مسار\")");
    needFiles(I, ln);
    return mkBool(std::remove(resolve(I, a[0].s).c_str()) == 0);
  });
  reg("وسائط", [](Interp& I, Args&, int) -> Value {
    Value r;
    r.t = VT::List;
    r.list = std::make_shared<std::vector<Value>>();
    for (auto& s : I.opt.args) r.list->push_back(mkStr(s));
    return r;
  });
  reg("اخرج", [](Interp&, Args& a, int ln) -> Value {
    int code = 0;
    if (!a.empty()) {
      if (a[0].t != VT::Num) fail(ln, "رمز الخروج يجب أن يكون رقماً");
      code = static_cast<int>(a[0].n);
    }
    throw ExitSig{code};
  });
  reg("متغير_بيئة", [=](Interp& I, Args& a, int ln) -> Value {
    if (a.size() != 1 || a[0].t != VT::Str) fail(ln, "الاستخدام: $متغير_بيئة(\"الاسم\")");
    needFiles(I, ln);
    const char* v = std::getenv(a[0].s.c_str());
    return v ? mkStr(v) : Value();
  });
}
