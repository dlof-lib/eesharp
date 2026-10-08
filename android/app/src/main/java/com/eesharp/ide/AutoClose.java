package com.eesharp.ide;

/**
 * الإغلاق التلقائي أثناء الكتابة (بلا اعتماد على أندرويد، لذلك يمكن اختباره على JVM):
 *  - ( [ " و << تُغلق تلقائياً، وكتابة الإغلاق فوق إغلاق موجود تتخطّاه بدل تكراره.
 *  - Enter بعد سطر يفتح كتلة (لو، كرر، لكل، مفهوم) يضيف الإزاحة ويكتب «نهاية <النوع>» تحتها.
 *  - Enter بعد والا يضيف إزاحة فقط، وبعد أي سطر آخر يحافظ على إزاحته.
 */
final class AutoClose {
    private static final String INDENT = "  ";

    /** تعديل مطلوب: استبدال [pos, pos+deleteLen) بـ insert ثم وضع المؤشر عند caret. */
    static final class Edit {
        final int pos;
        final int deleteLen;
        final String insert;
        final int caret;

        Edit(int pos, int deleteLen, String insert, int caret) {
            this.pos = pos;
            this.deleteLen = deleteLen;
            this.insert = insert;
            this.caret = caret;
        }
    }

    private AutoClose() {}

    /**
     * يُستدعى بعد إدخال حرف واحد في الموضع pos من النص الكامل text.
     * يعيد null إن لم يلزم أي تعديل.
     */
    static Edit onTyped(String text, int pos) {
        if (pos < 0 || pos >= text.length()) return null;
        char c = text.charAt(pos);
        int len = text.length();
        int lineStart = text.lastIndexOf('\n', pos - 1) + 1;
        char next = pos + 1 < len ? text.charAt(pos + 1) : '\0';

        switch (c) {
            case '\n':
                return onEnter(text, pos, lineStart);

            case '(':
                return pair(pos, next, ")");
            case '[':
                return pair(pos, next, "]");

            case ')':
            case ']':
                return next == c ? skip(pos) : null;

            case '"': {
                int quotes = 0;
                for (int i = lineStart; i < pos; i++) if (text.charAt(i) == '"') quotes++;
                if (quotes % 2 == 1) return next == '"' ? skip(pos) : null; // إغلاق نص
                return pair(pos, next, "\"");                                // فتح نص
            }

            case '<':
                // << تفتح متغيراً: تُكمَل بـ >>
                if (pos >= 1 && text.charAt(pos - 1) == '<' && !(pos >= 2 && text.charAt(pos - 2) == '<')) {
                    return new Edit(pos + 1, 0, ">>", pos + 1);
                }
                return null;

            case '>': {
                if (next != '>') return null;
                String prefix = text.substring(lineStart, pos);
                return prefix.lastIndexOf("<<") > prefix.lastIndexOf(">>") ? skip(pos) : null;
            }

            default:
                return null;
        }
    }

    /** إغلاق زوج، إلا إن كان الحرف التالي حرفاً أو رقماً (حتى لا نفسد كلمة موجودة). */
    private static Edit pair(int pos, char next, String closer) {
        if (Character.isLetterOrDigit(next)) return null;
        return new Edit(pos + 1, 0, closer, pos + 1);
    }

    /** تخطّي الحرف المكتوب لأن مثله موجود بعده: نحذف المكتوب ونضع المؤشر بعد الموجود. */
    private static Edit skip(int pos) {
        return new Edit(pos, 1, "", pos + 1);
    }

    private static Edit onEnter(String text, int pos, int lineStart) {
        String prev = text.substring(lineStart, pos);
        int indentLen = 0;
        while (indentLen < prev.length() && (prev.charAt(indentLen) == ' ' || prev.charAt(indentLen) == '\t')) {
            indentLen++;
        }
        String indent = prev.substring(0, indentLen);
        String trimmed = prev.trim();

        boolean deeper = CodeFormatter.opensBlock(trimmed);
        String toClose = CodeFormatter.blockToClose(trimmed);
        boolean addEnd = toClose != null && !hasEndBelow(text, pos + 1, indentLen);

        String insert = indent + (deeper ? INDENT : "");
        int caret = pos + 1 + insert.length();
        if (addEnd) insert += "\n" + indent + "نهاية " + toClose;
        if (insert.isEmpty()) return null;
        return new Edit(pos + 1, 0, insert, caret);
    }

    /**
     * هل السطر التالي غير الفارغ يغلق هذه الكتلة أصلاً (نهاية/والا بنفس الإزاحة)،
     * أو أنه جسم موجود بإزاحة أعمق (فنترك للمستخدم موضع الإغلاق)؟
     */
    private static boolean hasEndBelow(String text, int from, int indentLen) {
        int i = from;
        while (i < text.length()) {
            int eol = text.indexOf('\n', i);
            if (eol < 0) eol = text.length();
            String line = text.substring(i, eol);
            if (!line.trim().isEmpty()) {
                int ind = 0;
                while (ind < line.length() && (line.charAt(ind) == ' ' || line.charAt(ind) == '\t')) ind++;
                if (ind > indentLen) return true;
                return ind == indentLen && CodeFormatter.startsClosing(line.trim());
            }
            i = eol + 1;
        }
        return false;
    }
}
