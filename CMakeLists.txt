cmake_minimum_required(VERSION 3.16)
project(NWSpeech)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(3rd/sherpa-onnx/include)
include_directories(src)
include_directories($ENV{INCLUDE})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    link_directories(3rd/sherpa-onnx/lib/debug)
else()
    link_directories(3rd/sherpa-onnx/lib/release)
endif()

list(APPEND CMAKE_PREFIX_PATH $ENV{QtDir})
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

set(CORE_SOURCES
    src/core/audio_capture.cpp
    src/core/audio_resampler.cpp
    src/core/voice_activity_detector.cpp
    src/core/speech_recognizer.cpp
    src/core/file_saver.cpp
    src/config.cpp
)

set(CORE_HEADERS
    src/core/audio_capture.h
    src/core/audio_resampler.h
    src/core/voice_activity_detector.h
    src/core/speech_recognizer.h
    src/core/file_saver.h
    src/config.h
)

set(SERVICE_SOURCES
    src/service/speech_recognition_service.cpp
    src/service/offline_recognition_service.cpp
)

set(SERVICE_HEADERS
    src/service/speech_recognition_service.h
    src/service/offline_recognition_service.h
)

set(UI_SOURCES
    src/ui/main_window.cpp
    src/ui/settings_dialog.cpp
    src/ui/floating_window.cpp
)

set(UI_HEADERS
    src/ui/main_window.h
    src/ui/settings_dialog.h
    src/ui/floating_window.h
)

add_executable(nwspeech WIN32
    src/main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${SERVICE_SOURCES}
    ${SERVICE_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    resources.qrc
    app.rc
)

target_link_libraries(nwspeech PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    sherpa-onnx-cxx-api
    sherpa-onnx-c-api
    onnxruntime
)

function(copy_sherpa_dlls target)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/debug/sherpa-onnx-cxx-api.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/debug/sherpa-onnx-c-api.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/debug/onnxruntime.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/debug/onnxruntime_providers_shared.dll
            $<TARGET_FILE_DIR:${target}>
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/release/sherpa-onnx-cxx-api.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/release/sherpa-onnx-c-api.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/release/onnxruntime.dll
            ${CMAKE_SOURCE_DIR}/3rd/sherpa-onnx/lib/release/onnxruntime_providers_shared.dll
            $<TARGET_FILE_DIR:${target}>
        )
    endif()
endfunction()

copy_sherpa_dlls(nwspeech)

if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET nwspeech POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:nwspeech>"
            COMMENT "Running windeployqt for nwspeech"
        )
    endif()
endif()
