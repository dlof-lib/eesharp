/* EE# — واجهة C للمحرك (لتضمينه في WebAssembly أو أي لغة تدعم FFI).
 * كل النصوص UTF-8. المؤشرات المرجعة صالحة حتى الاستدعاء التالي لنفس الجلسة.
 *
 * الصدفة: يمرّر المحرك للمضيف أمراً ومعاملاته مفصولة بـ '\0' (كل معامل متبوع بـ '\0'، وعددها nargs).
 * يعيد المضيف نصاً. لإبلاغ خطأ يعيد نصاً يبدأ بالبايت 0x01 يليه نص الرسالة.
 */
#ifndef EE_CAPI_H
#define EE_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ee_session ee_session;
typedef const char* (*ee_shell_cb)(const char* cmd, const char* args, int nargs, void* user);

const char* ee_version(void);
ee_session* ee_session_new(void);
void ee_session_free(ee_session* s);

void ee_set_platform(ee_session* s, const char* platform);
void ee_set_shell(ee_session* s, ee_shell_cb cb, void* user);
void ee_set_input(ee_session* s, const char* text);  /* مدخلات $اقرأ */

int ee_eval(ee_session* s, const char* source);       /* 0 نجاح، 1 خطأ */
int ee_is_complete(const char* source);
int ee_call(ee_session* s, const char* name, const char* args, int nargs);
int ee_dispatch(ee_session* s, int handler_id, const char* args, int nargs);

/* المخرجات (stdout + الأخطاء بالترتيب) منذ آخر استدعاء؛ تُفرَّغ بعد القراءة */
const char* ee_take_output(ee_session* s);
void ee_stop(void);

#ifdef __cplusplus
}
#endif
#endif
