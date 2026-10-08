package com.eesharp.ide;

/** جسر JNI إلى مفسّر EE# المكتوب بـ C++. */
final class Native {
    static {
        System.loadLibrary("eejni");
    }

    /** يشغّل البرنامج ويعيد المخرجات (والأخطاء) كبايتات UTF-8. */
    static native byte[] run(byte[] source, byte[] input);

    /** يطلب إيقاف البرنامج الجاري تشغيله. */
    static native void stop();

    private Native() {}
}
