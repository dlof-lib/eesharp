#include "ee_host.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#define EE_POPEN _popen
#define EE_PCLOSE _pclose
#else
#define EE_POPEN popen
#define EE_PCLOSE pclose
#endif

namespace ee {

namespace {

void need(const std::vector<std::string>& a, size_t n, const std::string& cmd) {
  if (a.size() < n) throw std::runtime_error("الأمر " + cmd + " يحتاج " + std::to_string(n) + " معاملات");
}

std::string esc(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\t') o += "\\t";
    else o += c;
  }
  return o;
}
std::string unesc(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char n = s[++i];
      o += n == 'n' ? '\n' : (n == 't' ? '\t' : n);
    } else {
      o += s[i];
    }
  }
  return o;
}

}  // namespace

DesktopHost::DesktopHost(Session& session, std::ostream& out, bool allow_system, std::string store_path)
    : session_(session), out_(out), allow_system_(allow_system), store_path_(std::move(store_path)) {
  load_store();
  session_.set_shell([this](const std::string& cmd, const std::vector<std::string>& args) {
    return handle(cmd, args);
  });
}

void DesktopHost::load_store() {
  std::ifstream f(store_path_, std::ios::binary);
  std::string line;
  while (std::getline(f, line)) {
    size_t tab = line.find('\t');
    if (tab == std::string::npos) continue;
    store_[unesc(line.substr(0, tab))] = unesc(line.substr(tab + 1));
  }
}

void DesktopHost::save_store() const {
  std::ofstream f(store_path_, std::ios::binary | std::ios::trunc);
  if (!f) throw std::runtime_error("تعذّر حفظ المخزن: " + store_path_);
  for (auto& kv : store_) f << esc(kv.first) << '\t' << esc(kv.second) << '\n';
}

std::string DesktopHost::handle(const std::string& cmd, const std::vector<std::string>& a) {
  if (cmd == "console.log") { need(a, 1, cmd); out_ << a[0] << "\n"; out_.flush(); return ""; }
  if (cmd == "ui.alert") { need(a, 1, cmd); out_ << "[تنبيه] " << a[0] << "\n"; out_.flush(); return ""; }
  if (cmd == "ui.toast") { need(a, 1, cmd); out_ << "[رسالة] " << a[0] << "\n"; out_.flush(); return ""; }
  if (cmd == "ui.share") { need(a, 1, cmd); out_ << "[مشاركة] " << a[0] << "\n"; out_.flush(); return ""; }

  if (cmd == "fs.read") {
    need(a, 1, cmd);
    std::ifstream f(a[0], std::ios::binary);
    if (!f) throw std::runtime_error("تعذّر فتح الملف للقراءة: " + a[0]);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
  }
  if (cmd == "fs.write") {
    need(a, 2, cmd);
    std::ofstream f(a[0], std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("تعذّر فتح الملف للكتابة: " + a[0]);
    f << a[1];
    return "";
  }
  if (cmd == "os.env") {
    need(a, 1, cmd);
    const char* v = std::getenv(a[0].c_str());
    return v ? v : "";
  }
  if (cmd == "os.exec") {
    need(a, 1, cmd);
    if (!allow_system_) throw std::runtime_error("تنفيذ أوامر النظام معطّل في هذه الصدفة");
    out_.flush();
    FILE* p = EE_POPEN(a[0].c_str(), "r");
    if (!p) throw std::runtime_error("تعذّر تشغيل الأمر: " + a[0]);
    std::string res;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) res.append(buf, n);
    EE_PCLOSE(p);
    return res;
  }
  if (cmd == "store.set") {
    need(a, 2, cmd);
    store_[a[0]] = a[1];
    save_store();
    return "";
  }
  if (cmd == "store.get") {
    need(a, 1, cmd);
    auto it = store_.find(a[0]);
    return it == store_.end() ? "" : it->second;
  }
  if (cmd == "timer.after" || cmd == "timer.every") {
    need(a, 2, cmd);
    long long ms = std::atoll(a[0].c_str());
    if (ms < 0) ms = 0;
    if (cmd == "timer.every" && ms < 1) ms = 1;
    Timer t;
    t.id = ++timer_seq_;
    t.interval_ms = cmd == "timer.every" ? ms : 0;
    t.due = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    t.handler = std::atoi(a[1].c_str());
    timers_.push_back(t);
    return std::to_string(t.id);
  }
  if (cmd == "timer.clear") {
    need(a, 1, cmd);
    int id = std::atoi(a[0].c_str());
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [id](const Timer& t) { return t.id == id; }),
                  timers_.end());
    return "";
  }
  if (cmd.compare(0, 4, "dom.") == 0 || cmd == "js.eval")
    throw std::runtime_error("الأمر " + cmd + " خاص بالويب/أندرويد ولا يعمل في سطح المكتب");
  throw std::runtime_error("أمر صدفة غير معروف: " + cmd);
}

int DesktopHost::run_events() {
  while (!timers_.empty()) {
    auto it = std::min_element(timers_.begin(), timers_.end(),
                               [](const Timer& x, const Timer& y) { return x.due < y.due; });
    Timer t = *it;
    std::this_thread::sleep_until(t.due);
    // ربما أُلغي المؤقت أثناء الانتظار (لا يحدث في خيط واحد، لكن للأمان)
    if (t.interval_ms > 0) it->due = t.due + std::chrono::milliseconds(t.interval_ms);
    else timers_.erase(it);
    if (session_.dispatch(t.handler, {}) != 0) return 1;
  }
  return 0;
}

}  // namespace ee
