cmake_minimum_required(VERSION 3.22)
project(TWDFrameGrip LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build a Windows x64 DLL with the supplied cross-compilation preset.")
endif()
add_library(minhook STATIC third_party/minhook/src/buffer.c third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${generated}/embedded_bindings.h" "${generated}/additional_actions.inc"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_resources.py" "${generated}"
  DEPENDS tools/generate_resources.py config/frame_binding.json config/additional_actions.json
  VERBATIM)
add_library(TWDFrameGrip SHARED src/frame_grip.cpp src/action_bridge.cpp src/bindings.cpp
  "${generated}/embedded_bindings.h" "${generated}/additional_actions.inc")
target_include_directories(TWDFrameGrip PRIVATE src third_party/openvr third_party/picojson "${generated}")
target_link_libraries(TWDFrameGrip PRIVATE minhook bcrypt)
set_target_properties(TWDFrameGrip PROPERTIES PREFIX "" SUFFIX ".asi")
if(MINGW)
  target_compile_options(TWDFrameGrip PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
  target_link_options(TWDFrameGrip PRIVATE -static -Wl,--gc-sections -Wl,--no-insert-timestamp)
  set_source_files_properties(src/bindings.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions")
endif()
