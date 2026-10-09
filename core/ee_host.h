// EE# — صدفة سطح المكتب: مضيف يوفّر ملفات ونظاماً وبيئة ومؤقتات وتخزيناً لبرنامج EE#.
#pragma once

#include <chrono>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "ee.h"

namespace ee {

class DesktopHost {
 public:
  // يركّب نفسه كصدفة للجلسة. allow_system=false يعطّل $صدفة_نظام.
  DesktopHost(Session& session, std::ostream& out, bool allow_system = true,
              std::string store_path = ".ee_store");

  // ينتظر المؤقتات المجدولة وينفّذ معالجاتها حتى لا يبقى مؤقت (أو يحدث خطأ).
  // القيمة: 0 نجاح، 1 خطأ في معالج.
  int run_events();

  std::string handle(const std::string& cmd, const std::vector<std::string>& args);

 private:
  struct Timer {
    int id;
    std::chrono::steady_clock::time_point due;
    long long interval_ms;  // 0 = مرة واحدة
    int handler;
  };

  void load_store();
  void save_store() const;

  Session& session_;
  std::ostream& out_;
  bool allow_system_;
  std::string store_path_;
  std::map<std::string, std::string> store_;
  std::vector<Timer> timers_;
  int timer_seq_ = 0;
};

}  // namespace ee
