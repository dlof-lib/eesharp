#!/bin/sh
# يبني محرك EE# الحقيقي (C++) إلى WebAssembly. يتطلب Emscripten (emcc).  [تجريبي: لم يُختبر في بيئة التطوير]
# الناتج: ee.js + ee.wasm بجوار ee-wasm.js و ee-shell.js
set -e
cd "$(dirname "$0")/../../core"
emcc ee.cpp ee_capi.cpp -std=c++17 -O2 -fexceptions \
  -sMODULARIZE=1 -sEXPORT_NAME=EEModule -sALLOW_TABLE_GROWTH=1 -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS='["_malloc","_free","_ee_version","_ee_session_new","_ee_session_free","_ee_set_platform","_ee_set_shell","_ee_set_input","_ee_eval","_ee_is_complete","_ee_call","_ee_dispatch","_ee_take_output","_ee_stop"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","UTF8ToString","lengthBytesUTF8","addFunction","stringToNewUTF8","HEAPU8"]' \
  -o ../web/wasm/ee.js
echo "تم: web/wasm/ee.js و ee.wasm"
