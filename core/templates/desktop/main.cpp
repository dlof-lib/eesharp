// {{NAME}} — تطبيق سطح مكتب مُولَّد بمحرك ترجمة EE#.
// البرنامج الأصلي في program.ee؛ عدّله ثم أعد الترجمة:  ee translate --target desktop program.ee
#include <iostream>

#include "ee.h"
#include "ee_host.h"

#ifdef _WIN32
#include <windows.h>
#endif

static const char* kSource =
    {{SOURCE}};

int main() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  ee::set_default_platform("سطح_المكتب");
  ee::Session session(std::cin, std::cout, std::cerr);
  ee::DesktopHost host(session, std::cout);  // الصدفة: ملفات ونظام وبيئة ومؤقتات وتخزين
  int rc = session.eval(kSource);
  if (rc == 0) rc = host.run_events();       // ينتظر المؤقتات المجدولة (صدفة_انتظر / صدفة_كرر_كل)
  return rc;
}
