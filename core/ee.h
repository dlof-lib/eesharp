// EE# — لغة برمجة عربية بالكامل، منفّذة بلغة C++17
#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace ee {

// رقم الإصدار
const char* version();

// خيارات التشغيل لسطر الأوامر (الوضع الافتراضي: بلا ملفات، للأمان على الجوال)
struct RunOptions {
  bool allow_files = false;           // تفعيل دوال الملفات والاستيراد
  std::string base_dir;               // مجلد البرنامج (لتحليل مسارات الاستيراد النسبية)
  std::vector<std::string> args;      // وسائط البرنامج ($وسائط)
};

// تشغيل برنامج مكتوب بلغة EE#.
//  source     : نص البرنامج (UTF-8)
//  in         : مصدر المدخلات (لدوال اقرأ / اقرأ_رقم)
//  out, err   : مجرى المخرجات والأخطاء (يمكن أن يكونا نفس المجرى)
//  max_output : أقصى عدد بايتات للمخرجات (0 = بلا حد)
// القيمة المرجعة: 0 عند النجاح، 1 عند حدوث خطأ.
int run(const std::string& source, std::istream& in, std::ostream& out,
        std::ostream& err, std::size_t max_output = 0, const RunOptions* opts = nullptr);

// طلب إيقاف البرنامج الجاري تشغيله (آمن للاستدعاء من خيط آخر).
void request_stop();

}  // namespace ee
