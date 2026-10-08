// EE# — لغة برمجة عربية بالكامل، منفّذة بلغة C++17
#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <string>

namespace ee {

// رقم الإصدار
const char* version();

// تشغيل برنامج مكتوب بلغة EE#.
//  source     : نص البرنامج (UTF-8)
//  in         : مصدر المدخلات (لدوال اقرأ / اقرأ_رقم)
//  out, err   : مجرى المخرجات والأخطاء (يمكن أن يكونا نفس المجرى)
//  max_output : أقصى عدد بايتات للمخرجات (0 = بلا حد)
// القيمة المرجعة: 0 عند النجاح، 1 عند حدوث خطأ.
int run(const std::string& source, std::istream& in, std::ostream& out,
        std::ostream& err, std::size_t max_output = 0);

// طلب إيقاف البرنامج الجاري تشغيله (آمن للاستدعاء من خيط آخر).
void request_stop();

}  // namespace ee
