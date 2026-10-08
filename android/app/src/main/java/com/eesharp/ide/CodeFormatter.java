package com.eesharp.ide;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * تنسيق هيكل كود EE#: الإزاحة بحسب الكتل، وتسمية النهايات («نهاية لو»)، ومساعدات الإغلاق التلقائي.
 * الكتل تنتهي بـ «نهاية» (والتسمية اختيارية وتتحقق منها اللغة).
 */
final class CodeFormatter {
    private static final String INDENT = "  ";
    private static final String WA = "(?![\\p{L}\\p{N}_])";

    // رأس كتلة في أول السطر: لو، كرر، لكل، مفهوم
    private static final Pattern HEAD = Pattern.compile(
            "^(لو|اذا|إذا|كرر|طالما|لكل|مفهوم|دالة|وظيفة)" + WA);
    // الحالة البديلة: والا / والا لو
    private static final Pattern ELSE = Pattern.compile("^(?:والا|وإلا|غير_ذلك)" + WA);
    // غلاف البرنامج: محتواه بلا إزاحة إضافية
    private static final Pattern PROGRAM = Pattern.compile("^(?:برنامج|ابدأ)" + WA);
    // مفهوم مجهول في وسط السطر:  = مفهوم(<<س>>)
    private static final Pattern LAMBDA =
            Pattern.compile("(?<![\\p{L}\\p{N}_$])(?:مفهوم|دالة|وظيفة)(?=\\s*\\()");
    private static final Pattern END =
            Pattern.compile("(?<![\\p{L}\\p{N}_$])(?:نهاية|انتهى)" + WA);
    private static final Pattern END_ONLY = Pattern.compile("^نهاية$");

    private CodeFormatter() {}

    private static final int OPEN = 0;
    private static final int CLOSE = 1;

    /** حدث في السطر: فتح كتلة (وقد تحمل نوعها) أو إغلاقها. */
    private static final class Ev {
        final int pos;
        final int type;
        final String kind;

        Ev(int pos, int type, String kind) {
            this.pos = pos;
            this.type = type;
            this.kind = kind;
        }
    }

    /** النوع المعتمد للكتلة من كلمة رأسها. */
    static String kindOf(String word) {
        switch (word) {
            case "لو": case "اذا": case "إذا": return "لو";
            case "كرر": case "طالما": return "كرر";
            case "لكل": return "لكل";
            default: return "مفهوم";
        }
    }

    private static List<Ev> events(String clean) {
        List<Ev> ev = new ArrayList<>();
        Matcher h = HEAD.matcher(clean);
        boolean head = h.find();
        if (head) ev.add(new Ev(0, OPEN, kindOf(h.group(1))));
        Matcher m = LAMBDA.matcher(clean);
        while (m.find()) {
            if (m.start() == 0 && head) continue; // محسوب كرأس
            ev.add(new Ev(m.start(), OPEN, "مفهوم"));
        }
        m = END.matcher(clean);
        while (m.find()) ev.add(new Ev(m.start(), CLOSE, null));
        ev.sort((a, b) -> Integer.compare(a.pos, b.pos));
        return ev;
    }

    private static int net(String trimmedLine) {
        String clean = SyntaxScanner.stripTexts(trimmedLine).trim();
        int n = 0;
        for (Ev e : events(clean)) n += e.type == OPEN ? 1 : -1;
        return n;
    }

    /** هل يفتح هذا السطر كتلة تحتاج إزاحة للسطر التالي؟ (للإزاحة التلقائية عند Enter) */
    static boolean opensBlock(String trimmedLine) {
        int n = net(trimmedLine);
        if (n > 0) return true;
        return n == 0 && ELSE.matcher(SyntaxScanner.stripTexts(trimmedLine).trim()).find();
    }

    /**
     * إن كان السطر يفتح كتلة جديدة تحتاج «نهاية»، يعيد نوعها (لو، كرر، لكل، مفهوم)، وإلا null.
     * (والا لا تحتاج نهاية خاصة بها لأنها جزء من كتلة لو.)
     */
    static String blockToClose(String trimmedLine) {
        String clean = SyntaxScanner.stripTexts(trimmedLine).trim();
        List<Ev> ev = events(clean);
        int n = 0;
        String kind = null;
        for (Ev e : ev) {
            if (e.type == OPEN) {
                n++;
                kind = e.kind;
            } else {
                n--;
            }
        }
        return n > 0 ? kind : null;
    }

    /** هل يبدأ السطر بـ نهاية أو والا؟ (يُستعمل لمعرفة أن الكتلة مغلقة أصلاً) */
    static boolean startsClosing(String trimmedLine) {
        String clean = SyntaxScanner.stripTexts(trimmedLine).trim();
        return ELSE.matcher(clean).find() || END.matcher(clean).lookingAt();
    }

    /** يعيد ترتيب إزاحة الكود حسب الكتل، ويسمّي النهايات العارية بنوع كتلتها. */
    static String format(String code) {
        String[] lines = code.replace("\r", "").split("\n", -1);
        StringBuilder sb = new StringBuilder();
        ArrayDeque<String> stack = new ArrayDeque<>(); // "برنامج" أو نوع الكتلة
        int level = 0;

        for (int i = 0; i < lines.length; i++) {
            String t = lines[i].trim();
            boolean last = i == lines.length - 1;
            if (t.isEmpty()) {
                if (!last) sb.append('\n');
                continue;
            }
            String clean = SyntaxScanner.stripTexts(t).trim();
            List<Ev> ev = events(clean);
            boolean startsEnd = !ev.isEmpty() && ev.get(0).pos == 0 && ev.get(0).type == CLOSE;
            boolean startsElse = ELSE.matcher(clean).find();

            int printLevel;
            int from = 0;
            String closedKind = null;
            if (startsEnd) {
                if (!stack.isEmpty()) {
                    closedKind = stack.pop();
                    if (!closedKind.equals("برنامج")) level--;
                }
                from = 1;
                printLevel = level;
            } else if (startsElse) {
                printLevel = Math.max(0, level - 1);
            } else {
                printLevel = level;
            }

            // «نهاية» وحدها تُسمّى بنوع الكتلة التي تغلقها
            if (closedKind != null && END_ONLY.matcher(t).matches()) {
                t = "نهاية " + closedKind;
            }

            for (int k = 0; k < printLevel; k++) sb.append(INDENT);
            sb.append(t);
            if (!last) sb.append('\n');

            if (PROGRAM.matcher(clean).find()) stack.push("برنامج");
            for (int k = from; k < ev.size(); k++) {
                if (ev.get(k).type == OPEN) {
                    stack.push(ev.get(k).kind);
                    level++;
                } else if (!stack.isEmpty()) {
                    if (!stack.pop().equals("برنامج")) level--;
                }
            }
        }
        return sb.toString();
    }
}
