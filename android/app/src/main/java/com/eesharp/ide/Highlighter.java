package com.eesharp.ide;

import android.graphics.Color;
import android.graphics.Typeface;
import android.text.Editable;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;

import java.util.List;

/** تلوين صيغة EE# داخل المحرر، بلوحتين مريحتين للعين: فاتحة وداكنة. */
final class Highlighter {
    // الترتيب يطابق ثوابت SyntaxScanner:
    // كلمة مفتاحية، مفهوم $، نص، رقم، تعليق، فاصل، متغير <<>>
    private static final int[] LIGHT = {
        Color.parseColor("#6D28D9"),
        Color.parseColor("#0369A1"),
        Color.parseColor("#B45309"),
        Color.parseColor("#047857"),
        Color.parseColor("#9CA3AF"),
        Color.parseColor("#C4C8CF"),
        Color.parseColor("#0F766E"),
    };
    private static final int[] DARK = {
        Color.parseColor("#C4B5FD"),
        Color.parseColor("#7DD3FC"),
        Color.parseColor("#FCD34D"),
        Color.parseColor("#6EE7B7"),
        Color.parseColor("#6B7280"),
        Color.parseColor("#4B5563"),
        Color.parseColor("#5EEAD4"),
    };

    private Highlighter() {}

    static void apply(Editable text, boolean dark) {
        int[] c = dark ? DARK : LIGHT;
        for (ForegroundColorSpan s : text.getSpans(0, text.length(), ForegroundColorSpan.class)) {
            text.removeSpan(s);
        }
        for (StyleSpan s : text.getSpans(0, text.length(), StyleSpan.class)) {
            text.removeSpan(s);
        }
        List<SyntaxScanner.Span> spans = SyntaxScanner.scan(text.toString());
        for (SyntaxScanner.Span sp : spans) {
            text.setSpan(new ForegroundColorSpan(c[sp.kind]), sp.start, sp.end,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            if (sp.kind == SyntaxScanner.KEYWORD || sp.kind == SyntaxScanner.CONCEPT) {
                text.setSpan(new StyleSpan(Typeface.BOLD), sp.start, sp.end,
                        Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
    }
}
