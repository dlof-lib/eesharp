#!/bin/sh
# الاستخدام: run_js_test.sh <ee> <node> <program.ee> <expected>
# يترجم البرنامج إلى JS مستقل ويشغّله بـ node ثم يقارن بالناتج المتوقع نفسه الذي يطابقه المفسّر.
ee="$1"; node="$2"; prog="$3"; exp="$4"
in="${exp%.expected}.in"
[ -f "$in" ] || in=/dev/null
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
if ! "$ee" translate --target js --standalone -o "$tmp" "$prog" > "$tmp/translate.log" 2> "$tmp/translate.err"; then
  # أخطاء الترجمة (نحوية) يجب أن تطابق رسالة المفسّر نفسها
  { echo; cat "$tmp/translate.err"; } | diff - "$exp" > /dev/null && exit 0
  # البرنامج قد يطبع قبل الخطأ في المفسّر لكن النحوي يفشل قبل أي تنفيذ؛ قارن الخطأ فقط
  tail -n 1 "$tmp/translate.err" | diff - "$(tail -n 1 "$exp")"
  exit $?
fi
"$node" "$tmp/program.js" < "$in" 2>&1 | diff - "$exp"
