// جسر JNI بين Java ومفسّر EE#
// نمرّر البيانات كمصفوفات بايتات UTF-8 لتجنّب مشاكل "Modified UTF-8" في JNI.
#include <jni.h>

#include <sstream>
#include <string>

#include "ee.h"

namespace {
std::string toStd(JNIEnv* env, jbyteArray a) {
  if (!a) return std::string();
  jsize n = env->GetArrayLength(a);
  std::string s(static_cast<size_t>(n), '\0');
  if (n > 0) env->GetByteArrayRegion(a, 0, n, reinterpret_cast<jbyte*>(&s[0]));
  return s;
}
}  // namespace

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_eesharp_ide_Native_run(JNIEnv* env, jclass, jbyteArray src, jbyteArray input) {
  ee::set_default_platform("اندرويد");  // <<منصة>> داخل المحرر
  std::string source = toStd(env, src);
  std::istringstream in(toStd(env, input));
  std::ostringstream out;  // المخرجات والأخطاء في مجرى واحد للحفاظ على الترتيب
  ee::run(source, in, out, out, 2 * 1024 * 1024);
  std::string result = out.str();
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(result.size()));
  if (!result.empty())
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(result.size()),
                            reinterpret_cast<const jbyte*>(result.data()));
  return arr;
}

extern "C" JNIEXPORT void JNICALL Java_com_eesharp_ide_Native_stop(JNIEnv*, jclass) {
  ee::request_stop();
}
