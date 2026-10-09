package {{PACKAGE}};

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.KeyEvent;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/**
 * قشرة التطبيق: WebView يعرض صفحة HTML الناتجة من محرك ترجمة EE#،
 * وكائن EEAndroid هو جانب أندرويد من «الصدفة» (يقرؤه ee-shell.js).
 */
public class MainActivity extends Activity {

    private WebView web;

    @Override
    @SuppressWarnings("SetJavaScriptEnabled")
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        web = new WebView(this);
        setContentView(web);

        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);          // يلزم $صدفة_خزن
        s.setAllowFileAccess(false);           // لا يؤثر على android_asset
        s.setAllowContentAccess(false);
        s.setAllowFileAccessFromFileURLs(false);
        s.setAllowUniversalAccessFromFileURLs(false);

        web.setWebViewClient(new WebViewClient());
        web.addJavascriptInterface(new Bridge(this), "EEAndroid");
        web.loadUrl("file:///android_asset/index.html");
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && web.canGoBack()) {
            web.goBack();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onDestroy() {
        web.removeJavascriptInterface("EEAndroid");
        web.destroy();
        super.onDestroy();
    }

    /** الأوامر المتاحة لبرنامج EE#: ui.toast, ui.share, fs.read, fs.write (داخل مجلد التطبيق الخاص فقط). */
    static class Bridge {
        private final Activity activity;

        Bridge(Activity a) { activity = a; }

        @JavascriptInterface
        public void toast(final String text) {
            activity.runOnUiThread(new Runnable() {
                @Override public void run() { Toast.makeText(activity, text, Toast.LENGTH_SHORT).show(); }
            });
        }

        @JavascriptInterface
        public void share(String text) {
            Intent i = new Intent(Intent.ACTION_SEND);
            i.setType("text/plain");
            i.putExtra(Intent.EXTRA_TEXT, text);
            activity.startActivity(Intent.createChooser(i, null));
        }

        private File fileFor(String name) throws IOException {
            if (name == null || name.isEmpty() || name.contains("/") || name.contains("\\") || name.startsWith(".")) {
                throw new IOException("اسم ملف غير مسموح: " + name + " (اسم بسيط بلا مسارات)");
            }
            return new File(activity.getDir("ee", Context.MODE_PRIVATE), name);
        }

        @JavascriptInterface
        public String readFile(String name) {
            try (FileInputStream in = new FileInputStream(fileFor(name))) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                return out.toString("UTF-8");
            } catch (IOException e) {
                throw new RuntimeException("تعذّرت قراءة الملف: " + name);
            }
        }

        @JavascriptInterface
        public void writeFile(String name, String content) {
            try (FileOutputStream out = new FileOutputStream(fileFor(name))) {
                out.write(content.getBytes(StandardCharsets.UTF_8));
            } catch (IOException e) {
                throw new RuntimeException("تعذّرت كتابة الملف: " + name);
            }
        }
    }
}
