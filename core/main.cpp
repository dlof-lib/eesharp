// EE# — واجهة سطر الأوامر
#include <fstream>
#include <iostream>
#include <sstream>

#include "ee.h"

static void usage() {
  std::cout << "EE# " << ee::version() << " — لغة برمجة عربية\n"
            << "الاستخدام:\n"
            << "  ee <ملف.ee> [وسائط...]  تشغيل برنامج (الوسائط عبر $وسائط)\n"
            << "  ee --version       عرض الإصدار\n"
            << "  ee --help          عرض هذه المساعدة\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return 2;
  }
  std::string a = argv[1];
  if (a == "--version" || a == "-v") {
    std::cout << "EE# " << ee::version() << "\n";
    return 0;
  }
  if (a == "--help" || a == "-h") {
    usage();
    return 0;
  }
  std::ifstream f(a, std::ios::binary);
  if (!f) {
    std::cerr << "تعذّر فتح الملف: " << a << "\n";
    return 2;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  std::string src = ss.str();
  // تجاهل علامة BOM إن وُجدت
  if (src.size() >= 3 && static_cast<unsigned char>(src[0]) == 0xEF &&
      static_cast<unsigned char>(src[1]) == 0xBB && static_cast<unsigned char>(src[2]) == 0xBF)
    src.erase(0, 3);
  ee::RunOptions opts;
  opts.allow_files = true;
  size_t slash = a.find_last_of("/\\");
  opts.base_dir = slash == std::string::npos ? "" : a.substr(0, slash);
  for (int i = 2; i < argc; i++) opts.args.push_back(argv[i]);
  return ee::run(src, std::cin, std::cout, std::cerr, 0, &opts);
}
