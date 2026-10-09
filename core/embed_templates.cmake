# يولّد templates_gen.h: يضمّن ملفات templates/ داخل ملف C++ ليعمل المترجم دون ملفات خارجية.
# الاستخدام: include(embed_templates.cmake) من CMakeLists.txt (يعمل وقت الإعداد).
set(EE_TPL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
file(GLOB_RECURSE EE_TPL_FILES RELATIVE ${EE_TPL_DIR} ${EE_TPL_DIR}/*)
list(SORT EE_TPL_FILES)
set(EE_TPL_OUT "// مُولَّد تلقائياً من templates/ — لا تعدّله يدوياً\n#pragma once\nstatic const struct { const char* path; const char* data; } kTemplates[] = {\n")
foreach(f ${EE_TPL_FILES})
  file(READ ${EE_TPL_DIR}/${f} EE_TPL_CONTENT)
  string(APPEND EE_TPL_OUT "{\"${f}\", R\"EETPL(${EE_TPL_CONTENT})EETPL\"},\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${EE_TPL_DIR}/${f})
endforeach()
string(APPEND EE_TPL_OUT "{nullptr, nullptr}};\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gen/templates_gen.h "${EE_TPL_OUT}")
