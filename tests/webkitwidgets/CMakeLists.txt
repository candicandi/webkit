remove_definitions(-DQT_ASCII_CAST_WARNINGS)

add_definitions(-DTESTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/")

if (ENABLE_TEST_SUPPORT)
    add_definitions(-DHAVE_QTTESTSUPPORT)
endif ()

set(QtWK1ApiTests_LIBRARIES
    ${Qt6Gui_LIBRARIES}
    ${Qt6Network_LIBRARIES}
    ${Qt6Test_LIBRARIES}
    ${Qt6Widgets_LIBRARIES}
    WebKitWidgets
)

# Inspired by EFL WK2 tests
set(QtWK1ApiTests_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/tests
)

set(QtWK1ApiTests
    hybridPixmap
    qgraphicswebview
    qobjectbridge
    qwebelement
    qwebframe
    qwebhistory
    qwebhistoryinterface
    qwebinspector
    qwebpage
    qwebsecurityorigin
    qwebview
)

set(tst_hybridPixmap_SOURCES hybridPixmap/widget.cpp)
qt_wrap_ui(tst_hybridPixmap_SOURCES hybridPixmap/widget.ui)

foreach (testName ${QtWK1ApiTests})
    list(APPEND tst_${testName}_SOURCES ${testName}/tst_${testName}.cpp)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${testName}/tst_${testName}.qrc")
        qt_add_resources(tst_${testName}_SOURCES ${testName}/tst_${testName}.qrc)
    endif ()

    add_executable(tst_${testName} ${tst_${testName}_SOURCES})

    target_include_directories(tst_${testName} PRIVATE
        ${testName}
        "${CMAKE_BINARY_DIR}/include"
        "${CMAKE_BINARY_DIR}/include/QtWebKit"
        "${CMAKE_BINARY_DIR}/include/QtWebKitWidgets"
    )

    target_include_directories(tst_${testName} SYSTEM PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(tst_${testName} ${QtWK1ApiTests_LIBRARIES})
    set_target_properties(tst_${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${QtWK1ApiTests_RUNTIME_OUTPUT_DIRECTORY} AUTOMOC ON)

    add_test(${testName} "${QtWK1ApiTests_RUNTIME_OUTPUT_DIRECTORY}/tst_${testName}")
    set_tests_properties(${testName} PROPERTIES TIMEOUT 60)
endforeach ()
