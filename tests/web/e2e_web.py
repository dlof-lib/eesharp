#!/usr/bin/env python3
"""اختبار متصفح حقيقي (Chromium + Playwright) للصفحات المُترجَمة.
الاستخدام: e2e_web.py <مسار ee> [مسار مجلد examples/web]
"""
import glob
import os
import subprocess
import sys
import tempfile

from playwright.sync_api import sync_playwright

ee = os.path.abspath(sys.argv[1])
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
examples = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "examples", "web")

def chromium_path():
    for p in glob.glob("/opt/pw-browsers/chromium-*/chrome-linux/chrome"):
        return p
    return None

failures = []
def check(cond, msg):
    if not cond:
        failures.append(msg)
        print("FAIL:", msg)

with tempfile.TemporaryDirectory() as tmp, sync_playwright() as pw:
    page_dir = os.path.join(tmp, "page")
    app_dir = os.path.join(tmp, "app")
    subprocess.run([ee, "translate", "-o", page_dir, os.path.join(examples, "عداد.html")], check=True, capture_output=True)
    subprocess.run([ee, "translate", "--target", "web", "-o", app_dir, os.path.join(examples, "مرحبا.ee")], check=True, capture_output=True)

    kw = {"executable_path": chromium_path()} if chromium_path() else {}
    browser = pw.chromium.launch(**kw)

    # ── 1) صفحة HTML فيها <script type="text/ee"> ──
    page = browser.new_page()
    errors = []
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
    page.goto("file://" + os.path.join(page_dir, "عداد.html"))
    page.wait_for_function("document.querySelector('#العدد').textContent === '0'")
    check(page.inner_text("#العدد") == "0", "العدد الابتدائي 0")

    page.click("#سلم")
    check(page.inner_text("#تحية") == "اكتب اسمك أولاً", "تحية فارغة: " + page.inner_text("#تحية"))
    page.fill("#اسم", "  سارة ")
    page.click("#سلم")
    check(page.inner_text("#تحية") == "أهلاً يا سارة! المنصة: ويب", "التحية: " + page.inner_text("#تحية"))

    for _ in range(3):
        page.click("#زيادة")
    check(page.inner_text("#العدد") == "3", "العدد بعد 3 نقرات: " + page.inner_text("#العدد"))
    check(page.evaluate("localStorage.getItem('ee:عدد')") == "3", "التخزين المحلي")
    check(page.evaluate("EE.getVar('عدد')") == 3, "EE.getVar")
    check(page.evaluate("EE.invoke('زد', ['x']) , EE.getVar('عدد')") == 4, "EE.invoke من JS")
    check(page.inner_text("#العدد") == "4", "DOM بعد invoke")

    # المؤقت المتكرر
    page.wait_for_function("document.querySelector('#ساعة').textContent.includes('مرّت')", timeout=3500)
    check("ثانية" in page.inner_text("#ساعة"), "المؤقت: " + page.inner_text("#ساعة"))

    # الاستمرارية: إعادة تحميل الصفحة تقرأ العدد من المخزن
    page.reload()
    page.wait_for_function("document.querySelector('#العدد').textContent === '4'")
    check(page.inner_text("#العدد") == "4", "العدد بعد إعادة التحميل")
    page.click("#تصفير")
    check(page.inner_text("#العدد") == "0", "تصفير")
    check(not errors, "أخطاء الصفحة: %r" % errors)
    page.close()

    # ── 2) برنامج .ee → صفحة ويب جاهزة ──
    page = browser.new_page()
    errors = []
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.goto("file://" + os.path.join(app_dir, "index.html"))
    page.wait_for_selector("#ازرار")
    out = page.inner_text("#ee-output")
    check("المنصة: ويب" in out and "الإصدار:" in out, "مخرجات البرنامج: " + out)
    page.click("#ازرار")
    page.click("#ازرار")
    check(page.inner_text("#نتيجة") == "٢", "الأرقام العربية: " + page.inner_text("#نتيجة"))
    check(page.locator("div[role=status]").last.inner_text() == "ضغطة رقم 2", "رسالة toast")
    check(not errors, "أخطاء الصفحة: %r" % errors)
    page.close()

    # ── 3) خطأ وقت التشغيل يظهر في المخرجات بسطره ──
    bad = os.path.join(tmp, "bad.ee")
    with open(bad, "w", encoding="utf-8") as f:
        f.write('$اعرض "قبل"\n$اعرض 1 / 0\n')
    subprocess.run([ee, "translate", "-o", os.path.join(tmp, "bad"), bad], check=True, capture_output=True)
    page = browser.new_page()
    page.goto("file://" + os.path.join(tmp, "bad", "index.html"))
    page.wait_for_function("document.querySelector('#ee-output').textContent.includes('خطأ')")
    out = page.inner_text("#ee-output")
    check("قبل" in out and "(السطر 2): القسمة على صفر" in out, "خطأ التشغيل: " + out)
    page.close()
    browser.close()

if failures:
    print("%d فشل" % len(failures))
    sys.exit(1)
print("اختبار المتصفح نجح")
