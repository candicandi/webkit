add_definitions(-DHAVE_WEBKIT2=1)
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source

    # The WebKit2 Qt APIs depend on qwebkitglobal.h, which lives in WebKit
    "${WEBKIT_DIR}/qt/Api"

    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Platform/IPC"
    "${WEBKIT2_DIR}/Shared/qt"
    "${WEBKIT2_DIR}/UIProcess/API/qt"

    ${WTF_DIR}
)
include_directories(SYSTEM
    ${ICU_INCLUDE_DIRS}
    ${Qt6Quick_INCLUDE_DIRS}
    ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
)

add_library(qmlwebkitplugin MODULE plugin.cpp)
target_link_libraries(qmlwebkitplugin
    WebKit Qt6::Quick)

set(qmlwebkit_output_dir "${CMAKE_BINARY_DIR}/imports/QtWebKit")
set(qmlwebkit_install_dir "${QML_INSTALL_DIR}/QtWebKit")

set_target_properties(qmlwebkitplugin PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${qmlwebkit_output_dir}
    LIBRARY_OUTPUT_DIRECTORY ${qmlwebkit_output_dir}
    RUNTIME_OUTPUT_DIRECTORY ${qmlwebkit_output_dir}
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${qmlwebkit_output_dir}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${qmlwebkit_output_dir}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${qmlwebkit_output_dir}
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${qmlwebkit_output_dir}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${qmlwebkit_output_dir}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${qmlwebkit_output_dir}

    AUTOMOC ON
)

if (COMPILER_IS_GCC_OR_CLANG)
    target_compile_options(qmlwebkitplugin PRIVATE -frtti)
endif ()

add_custom_target(
    qmlwebkitplugin-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different qmldir "${CMAKE_BINARY_DIR}/imports/QtWebKit/qmldir"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different plugins.qmltypes "${CMAKE_BINARY_DIR}/imports/QtWebKit/plugins.qmltypes"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS qmlwebkitplugin
        DESTINATION ${qmlwebkit_install_dir})
install(FILES qmldir plugins.qmltypes
        DESTINATION ${qmlwebkit_install_dir})

add_subdirectory(experimental)
