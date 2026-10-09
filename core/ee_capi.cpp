#include "ee_capi.h"

#include <sstream>
#include <string>
#include <vector>

#include "ee.h"

struct ee_session {
  std::istringstream in;
  std::ostringstream out;  // stdout والأخطاء في مجرى واحد لحفظ الترتيب
  ee::Session session;
  std::string taken;
  std::string shellResult;
  ee_session() : session(in, out, out, 4 * 1024 * 1024) {}
};

namespace {
std::vector<std::string> splitArgs(const char* args, int nargs) {
  std::vector<std::string> v;
  const char* p = args;
  for (int i = 0; i < nargs && p; i++) {
    v.emplace_back(p);
    p += v.back().size() + 1;
  }
  return v;
}
std::string joinArgs(const std::vector<std::string>& a) {
  std::string s;
  for (auto& x : a) { s += x; s += '\0'; }
  return s;
}
}  // namespace

extern "C" {

const char* ee_version(void) { return ee::version(); }
ee_session* ee_session_new(void) { return new ee_session(); }
void ee_session_free(ee_session* s) { delete s; }

void ee_set_platform(ee_session* s, const char* platform) { s->session.set_platform(platform ? platform : ""); }

void ee_set_shell(ee_session* s, ee_shell_cb cb, void* user) {
  if (!cb) { s->session.set_shell(nullptr); return; }
  s->session.set_shell([s, cb, user](const std::string& cmd, const std::vector<std::string>& args) {
    std::string packed = joinArgs(args);
    const char* r = cb(cmd.c_str(), packed.c_str(), static_cast<int>(args.size()), user);
    s->shellResult = r ? r : "";
    if (!s->shellResult.empty() && s->shellResult[0] == '\x01') throw std::runtime_error(s->shellResult.substr(1));
    return s->shellResult;
  });
}

void ee_set_input(ee_session* s, const char* text) {
  s->in.str(text ? text : "");
  s->in.clear();
}

int ee_eval(ee_session* s, const char* source) { return s->session.eval(source ? source : ""); }
int ee_is_complete(const char* source) { return ee::Session::is_complete(source ? source : "") ? 1 : 0; }
int ee_call(ee_session* s, const char* name, const char* args, int nargs) {
  return s->session.call(name ? name : "", splitArgs(args, nargs));
}
int ee_dispatch(ee_session* s, int id, const char* args, int nargs) {
  return s->session.dispatch(id, splitArgs(args, nargs));
}

const char* ee_take_output(ee_session* s) {
  s->taken = s->out.str();
  s->out.str("");
  s->out.clear();
  return s->taken.c_str();
}

void ee_stop(void) { ee::request_stop(); }

}  // extern "C"
