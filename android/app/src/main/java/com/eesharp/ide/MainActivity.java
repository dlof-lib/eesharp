package com.eesharp.ide;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** الشاشة الرئيسية: محرر أكواد EE# مع تلوين الصيغة وتنسيق الهيكل وتشغيل البرنامج. */
public class MainActivity extends Activity {
    private static final int REQ_OPEN = 1;
    private static final int REQ_SAVE = 2;
    private static final String PREFS = "ee_prefs";
    private static final String KEY_CODE = "code";
    private static final String KEY_STDIN = "stdin";
    private static final String KEY_DARK = "dark";

    // ألوان مريحة للعين: خلفية دافئة هادئة في الفاتح، وألوان ناعمة غير حادة في الداكن
    private static final int L_EDITOR_BG = Color.parseColor("#FAF8F3");
    private static final int L_EDITOR_FG = Color.parseColor("#2B2F3A");
    private static final int L_PANEL_BG = Color.parseColor("#ECE9E1");
    private static final int L_OUTPUT_BG = Color.parseColor("#1B2230");
    private static final int L_OUTPUT_FG = Color.parseColor("#B8F0CF");
    private static final int D_EDITOR_BG = Color.parseColor("#1E1E2E");
    private static final int D_EDITOR_FG = Color.parseColor("#CDD6F4");
    private static final int D_PANEL_BG = Color.parseColor("#2A2B3C");
    private static final int D_OUTPUT_BG = Color.parseColor("#11111B");
    private static final int D_OUTPUT_FG = Color.parseColor("#A6E3A1");

    // رموز وكلمات يصعب كتابتها على لوحة المفاتيح العربية.
    // العلامة \u0001 تحدد مكان المؤشر بعد الإدراج.
    private static final String CARET = "\u0001";
    private static final String[] SYMBOLS = {
        "$", "<<" + CARET + ">>", "_", "\"" + CARET + "\"", "(", ")", "[", "]", "،", "؛",
        "=", "+=", "+", "-", "*", "/", "%", "<", ">", "<=", ">=",
        "×", "÷", "−", "٪", "≠", "≤", "≥", "←", "×=", "÷=", "#", "//",
        "متغير <<" + CARET + ">> = ", "لو ", "والا", "كرر ", "لكل <<" + CARET + ">> في ",
        "مفهوم $", "ارجع ", "نهاية ", "توقف", "استمر",
        " اكبر من ", " اصغر من ", " = ", " لا = ", " و ", " او ", "ليس ",
        "صح", "خطأ", "عدم", "$اعرض ", "برنامج "
    };

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<Button> symbolButtons = new ArrayList<>();
    private EditText editor;
    private EditText stdin;
    private TextView output;
    private View root;
    private View symbolScroll;
    private volatile boolean running = false;
    private int sampleIndex = 0;
    private boolean dark = false;
    private boolean editing = false;     // تعديل برمجي: يتجاهله المراقب
    private int pendingPos = -1;         // موضع حرف واحد أُدخل للتو (للإغلاق التلقائي)

    private final Runnable highlightTask = new Runnable() {
        @Override
        public void run() {
            rehighlight();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        root = findViewById(R.id.root);
        symbolScroll = findViewById(R.id.symbolScroll);
        editor = findViewById(R.id.editor);
        stdin = findViewById(R.id.stdin);
        output = findViewById(R.id.output);

        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        dark = prefs.getBoolean(KEY_DARK, false);
        editor.setText(prefs.getString(KEY_CODE, Samples.ALL[0]));
        stdin.setText(prefs.getString(KEY_STDIN, ""));

        editor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                pendingPos = (!editing && before == 0 && count == 1) ? start : -1;
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (editing) return;
                if (pendingPos >= 0) {
                    int pos = pendingPos;
                    pendingPos = -1;
                    applyAutoClose(s, pos);
                }
                handler.removeCallbacks(highlightTask);
                handler.postDelayed(highlightTask, 150);
            }
        });

        buildSymbolBar();
        applyTheme();

        findViewById(R.id.btnRun).setOnClickListener(v -> runProgram());
        findViewById(R.id.btnStop).setOnClickListener(v -> Native.stop());
        findViewById(R.id.btnOpen).setOnClickListener(v -> openFile());
        findViewById(R.id.btnSave).setOnClickListener(v -> saveFile());
        findViewById(R.id.btnExample).setOnClickListener(v -> {
            setCode(Samples.ALL[sampleIndex % Samples.ALL.length].trim() + "\n");
            sampleIndex++;
        });
        findViewById(R.id.btnFormat).setOnClickListener(v -> {
            int caret = editor.getSelectionStart();
            setCode(CodeFormatter.format(editor.getText().toString()));
            editor.setSelection(Math.max(0, Math.min(caret, editor.getText().length())));
            Toast.makeText(this, R.string.formatted, Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.btnTheme).setOnClickListener(v -> {
            dark = !dark;
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_DARK, dark).apply();
            applyTheme();
        });
    }

    @Override
    protected void onPause() {
        super.onPause();
        // حفظ تلقائي للكود والمدخلات
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putString(KEY_CODE, editor.getText().toString())
                .putString(KEY_STDIN, stdin.getText().toString())
                .apply();
    }

    // ── التعديل والتلوين ──
    private void setCode(String text) {
        editing = true;
        try {
            editor.setText(text);
        } finally {
            editing = false;
        }
        rehighlight();
    }

    private void rehighlight() {
        editing = true;
        try {
            Highlighter.apply(editor.getText(), dark);
        } finally {
            editing = false;
        }
    }

    /**
     * الإغلاق التلقائي: الأقواس و" و<< تُغلق، وEnter بعد فتح كتلة يضيف الإزاحة و«نهاية <النوع>».
     * (المنطق في AutoClose ومُختبَر على JVM.)
     */
    private void applyAutoClose(Editable s, int pos) {
        AutoClose.Edit e = AutoClose.onTyped(s.toString(), pos);
        if (e == null) return;
        editing = true;
        try {
            s.replace(e.pos, e.pos + e.deleteLen, e.insert);
            editor.setSelection(Math.max(0, Math.min(e.caret, s.length())));
        } finally {
            editing = false;
        }
    }

    // ── السمة (فاتحة / داكنة) ──
    private void applyTheme() {
        int editorBg = dark ? D_EDITOR_BG : L_EDITOR_BG;
        int editorFg = dark ? D_EDITOR_FG : L_EDITOR_FG;
        int panelBg = dark ? D_PANEL_BG : L_PANEL_BG;
        root.setBackgroundColor(editorBg);
        editor.setBackgroundColor(editorBg);
        editor.setTextColor(editorFg);
        stdin.setBackgroundColor(panelBg);
        stdin.setTextColor(editorFg);
        stdin.setHintTextColor(dark ? Color.parseColor("#7F849C") : Color.parseColor("#8A8F98"));
        symbolScroll.setBackgroundColor(panelBg);
        for (Button b : symbolButtons) {
            b.setTextColor(editorFg);
        }
        output.setBackgroundColor(dark ? D_OUTPUT_BG : L_OUTPUT_BG);
        ((View) output.getParent()).setBackgroundColor(dark ? D_OUTPUT_BG : L_OUTPUT_BG);
        output.setTextColor(dark ? D_OUTPUT_FG : L_OUTPUT_FG);
        rehighlight();
    }

    // ── شريط الرموز ──
    private void buildSymbolBar() {
        LinearLayout bar = findViewById(R.id.symbolBar);
        for (final String sym : SYMBOLS) {
            Button b = new Button(this);
            String label = sym.replace(CARET, "");
            b.setText(label.trim().isEmpty() ? label : label.trim());
            b.setAllCaps(false);
            b.setBackgroundColor(Color.TRANSPARENT);
            b.setMinWidth(0);
            b.setMinimumWidth(0);
            b.setPadding(24, 0, 24, 0);
            b.setOnClickListener(v -> insert(sym));
            symbolButtons.add(b);
            bar.addView(b, new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT, 110));
        }
    }

    private void insert(String s) {
        int start = Math.max(editor.getSelectionStart(), 0);
        int end = Math.max(editor.getSelectionEnd(), 0);
        int lo = Math.min(start, end);
        int caret = s.indexOf(CARET);
        String text = s.replace(CARET, "");
        editing = true; // أزرار الشريط تدرج النص كما هو، بلا إغلاق تلقائي إضافي
        try {
            editor.getText().replace(lo, Math.max(start, end), text);
            editor.setSelection(Math.min(lo + (caret >= 0 ? caret : text.length()), editor.getText().length()));
        } finally {
            editing = false;
        }
        rehighlight();
    }

    // ── التشغيل ──
    private void runProgram() {
        if (running) return;
        running = true;
        output.setText(R.string.running);
        final String src = editor.getText().toString();
        final String in = stdin.getText().toString();

        // خيط بحجم مكدّس كبير ليتحمّل التعاود العميق
        Thread t = new Thread(null, () -> {
            byte[] result;
            try {
                result = Native.run(src.getBytes(StandardCharsets.UTF_8),
                        in.getBytes(StandardCharsets.UTF_8));
            } catch (Throwable e) {
                result = ("خطأ داخلي: " + e).getBytes(StandardCharsets.UTF_8);
            }
            final String text = new String(result, StandardCharsets.UTF_8);
            runOnUiThread(() -> {
                output.setText(text.isEmpty() ? getString(R.string.no_output) : text);
                running = false;
            });
        }, "ee-run", 16L * 1024 * 1024);
        t.start();
    }

    // ── فتح وحفظ الملفات (Storage Access Framework) ──
    private void openFile() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_OPEN);
    }

    private void saveFile() {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("application/octet-stream");
        i.putExtra(Intent.EXTRA_TITLE, "برنامج.ee");
        startActivityForResult(i, REQ_SAVE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            if (requestCode == REQ_OPEN) {
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new java.io.IOException("no stream");
                    ByteArrayOutputStream bos = new ByteArrayOutputStream();
                    byte[] buf = new byte[8192];
                    int n;
                    while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
                    String text = new String(bos.toByteArray(), StandardCharsets.UTF_8);
                    if (text.startsWith("﻿")) text = text.substring(1);
                    setCode(text);
                }
                Toast.makeText(this, R.string.opened, Toast.LENGTH_SHORT).show();
            } else if (requestCode == REQ_SAVE) {
                try (OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
                    if (out == null) throw new java.io.IOException("no stream");
                    out.write(editor.getText().toString().getBytes(StandardCharsets.UTF_8));
                }
                Toast.makeText(this, R.string.saved, Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, R.string.io_error, Toast.LENGTH_LONG).show();
        }
    }
}
