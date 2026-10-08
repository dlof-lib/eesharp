package com.eesharp.ide;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * ماسح صيغة EE# (بلا اعتماد على أندرويد، لذلك يمكن اختباره على JVM).
 * يحدد مواضع: الكلمات المفتاحية، المفاهيم ($)، المتغيرات (<< >>)، النصوص، الأرقام، التعليقات، الفواصل.
 */
final class SyntaxScanner {
    static final int KEYWORD = 0;
    static final int CONCEPT = 1;
    static final int STRING = 2;
    static final int NUMBER = 3;
    static final int COMMENT = 4;
    static final int SEPARATOR = 5;
    static final int VARIABLE = 6;

    static final class Span {
        final int start;
        final int end;
        final int kind;

        Span(int start, int end, int kind) {
            this.start = start;
            this.end = end;
            this.kind = kind;
        }
    }

    private static final String WB = "(?<![\\p{L}\\p{N}_$])";
    private static final String WA = "(?![\\p{L}\\p{N}_])";

    // التعليقات والنصوص في مرور واحد من اليسار لليمين، حتى لا يختلط // داخل نص
    static final Pattern TEXTS = Pattern.compile(
            "//[^\\n]*|#[^\\n]*|/\\*[\\s\\S]*?(?:\\*/|$)"
                    + "|\"(?:\\\\.|[^\"\\\\\\n])*\"?|'(?:\\\\.|[^'\\\\\\n])*'?|“[^”\\n]*”?|«[^»\\n]*»?");

    private static final Pattern VARIABLES =
            Pattern.compile("<<[ \\t]*[\\p{L}_][\\p{L}\\p{N}_]*[ \\t]*>>");

    private static final Pattern CONCEPTS = Pattern.compile("\\$[\\p{L}_][\\p{L}\\p{N}_]*");

    private static final Pattern KEYWORDS = Pattern.compile(WB + "(?:"
            + "متغير|اذا|إذا|لو|والا|وإلا|غير_ذلك|طالما|كرر|لكل|في|مفهوم|دالة|وظيفة|ارجع|توقف|استمر"
            + "|صح|خطأ|خطا|عدم|و|او|أو|ليس|من|يساوي|لا|اكبر|أكبر|اصغر|أصغر"
            + "|برنامج|ابدأ|انتهى|نهاية" + ")" + WA);

    private static final Pattern NUMBERS =
            Pattern.compile("[0-9٠-٩۰-۹]+(?:[.٫][0-9٠-٩۰-۹]+)?");

    private static final Pattern SEPARATORS = Pattern.compile("[؛;]");

    private SyntaxScanner() {}

    static List<Span> scan(CharSequence text) {
        boolean[] used = new boolean[text.length()];
        List<Span> out = new ArrayList<>();

        Matcher m = TEXTS.matcher(text);
        while (m.find()) {
            if (m.end() <= m.start()) continue;
            char c = text.charAt(m.start());
            boolean comment = c == '/' || c == '#';
            add(out, used, m.start(), m.end(), comment ? COMMENT : STRING);
        }
        run(out, used, VARIABLES.matcher(text), VARIABLE);
        run(out, used, CONCEPTS.matcher(text), CONCEPT);
        run(out, used, KEYWORDS.matcher(text), KEYWORD);
        run(out, used, NUMBERS.matcher(text), NUMBER);
        run(out, used, SEPARATORS.matcher(text), SEPARATOR);
        return out;
    }

    private static void run(List<Span> out, boolean[] used, Matcher m, int kind) {
        while (m.find()) {
            if (m.end() <= m.start()) continue;
            add(out, used, m.start(), m.end(), kind);
        }
    }

    private static void add(List<Span> out, boolean[] used, int start, int end, int kind) {
        for (int i = start; i < end; i++) {
            if (used[i]) return; // مغطّى بنوع أعلى أولوية
        }
        for (int i = start; i < end; i++) used[i] = true;
        out.add(new Span(start, end, kind));
    }

    /** يحذف النصوص والتعليقات (يستبدلها بمسافات) لتحليل هيكل السطر دون التباس. */
    static String stripTexts(String line) {
        Matcher m = TEXTS.matcher(line);
        StringBuilder sb = new StringBuilder(line);
        while (m.find()) {
            for (int i = m.start(); i < m.end(); i++) sb.setCharAt(i, ' ');
        }
        return sb.toString();
    }
}
