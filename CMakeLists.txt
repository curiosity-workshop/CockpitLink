cmake_minimum_required(VERSION 3.20)

project(CockpitLink
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COCKPITLINK_BUILD_XPLANE_PLUGIN
    "Build the CockpitLink X-Plane plugin target"
    OFF)

set(COCKPITLINK_XPLANE_SDK_PATH
    ""
    CACHE PATH
    "Path to the X-Plane SDK root")

set(COCKPITLINK_XPLANE_ROOT
    ""
    CACHE PATH
    "Optional local X-Plane installation root for plugin deployment")

set(COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR
    ""
    CACHE PATH
    "Optional exact directory where CockpitLink.xpl should be copied after build")

if(NOT COCKPITLINK_XPLANE_SDK_PATH AND DEFINED ENV{XPLANE_SDK_ROOT})
    set(COCKPITLINK_XPLANE_SDK_PATH
        "$ENV{XPLANE_SDK_ROOT}"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

if(NOT COCKPITLINK_XPLANE_SDK_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xplane_sdk/CHeaders/XPLM/XPLMDataAccess.h")
    set(COCKPITLINK_XPLANE_SDK_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xplane_sdk"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

add_library(CockpitLinkHost
    host/src/logging/SerialTraceLogger.cpp
    host/src/protocol/Frame.cpp
    host/src/protocol/FrameParser.cpp
    host/src/protocol/Payloads.cpp
    host/src/protocol/TraceFormatter.cpp
    host/src/serial/SerialDeviceClassifier.cpp
    host/src/serial/WindowsSerialEnumerator.cpp
    host/src/serial/WindowsSerialTransport.cpp
)

target_include_directories(CockpitLinkHost
    PUBLIC
        host/include
)

if(WIN32)
    target_link_libraries(CockpitLinkHost
        PUBLIC
            Setupapi
    )
endif()

add_executable(CockpitLinkProbe
    host/apps/CockpitLinkProbe.cpp
)

target_link_libraries(CockpitLinkProbe
    PRIVATE
        CockpitLinkHost
)

enable_testing()

add_executable(CockpitLinkProtocolTests
    host/tests/protocol/FrameParserTests.cpp
)

target_link_libraries(CockpitLinkProtocolTests
    PRIVATE
        CockpitLinkHost
)

add_test(
    NAME CockpitLinkProtocolTests
    COMMAND CockpitLinkProtocolTests
)

add_executable(CockpitLinkCatalogTests
    host/tests/catalog/BehaviorCatalogValidationTests.cpp
)

add_test(
    NAME CockpitLinkCatalogTests
    COMMAND CockpitLinkCatalogTests
        ${CMAKE_CURRENT_SOURCE_DIR}/catalog/base-behaviors.json
)

if(COCKPITLINK_BUILD_XPLANE_PLUGIN)
    if(NOT COCKPITLINK_XPLANE_SDK_PATH)
        message(FATAL_ERROR
            "COCKPITLINK_XPLANE_SDK_PATH must be set when "
            "COCKPITLINK_BUILD_XPLANE_PLUGIN is ON.")
    endif()

    set(COCKPITLINK_XPLANE_SDK_INCLUDE_DIRS
        "${COCKPITLINK_XPLANE_SDK_PATH}/CHeaders/XPLM"
        "${COCKPITLINK_XPLANE_SDK_PATH}/CHeaders/Widgets"
    )

    add_library(CockpitLinkXPlanePlugin MODULE
        xplane/CockpitLinkXPlanePlugin.cpp
    )

    target_link_libraries(CockpitLinkXPlanePlugin
        PRIVATE
            CockpitLinkHost
    )

    target_include_directories(CockpitLinkXPlanePlugin
        PRIVATE
            host/include
            ${COCKPITLINK_XPLANE_SDK_INCLUDE_DIRS}
    )

    target_compile_definitions(CockpitLinkXPlanePlugin
        PRIVATE
            XPLM200=1
            XPLM210=1
            XPLM300=1
            XPLM301=1
            XPLM400=1
    )

    set_target_properties(CockpitLinkXPlanePlugin
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "CockpitLink"
            SUFFIX ".xpl"
    )

    if(WIN32)
        target_compile_definitions(CockpitLinkXPlanePlugin
            PRIVATE
                IBM=1
                APL=0
                LIN=0
        )

        target_link_libraries(CockpitLinkXPlanePlugin
            PRIVATE
                "${COCKPITLINK_XPLANE_SDK_PATH}/Libraries/Win/XPLM_64.lib"
                "${COCKPITLINK_XPLANE_SDK_PATH}/Libraries/Win/XPWidgets_64.lib"
                Setupapi
        )

        set(COCKPITLINK_XPLANE_PLUGIN_ABI_DIR "win_x64")
    elseif(APPLE)
        target_compile_definitions(CockpitLinkXPlanePlugin
            PRIVATE
                IBM=0
                APL=1
                LIN=0
        )

        set(COCKPITLINK_XPLANE_PLUGIN_ABI_DIR "mac_x64")
    else()
        target_compile_definitions(CockpitLinkXPlanePlugin
            PRIVATE
                IBM=0
                APL=0
                LIN=1
        )

        set(COCKPITLINK_XPLANE_PLUGIN_ABI_DIR "lin_x64")
    endif()

    if(NOT COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR AND COCKPITLINK_XPLANE_ROOT)
        set(COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR
            "${COCKPITLINK_XPLANE_ROOT}/Resources/plugins/CockpitLink/${COCKPITLINK_XPLANE_PLUGIN_ABI_DIR}"
            CACHE PATH
            "Optional exact directory where CockpitLink.xpl should be copied after build"
            FORCE)
    endif()

    if(COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR)
        add_custom_command(
            TARGET CockpitLinkXPlanePlugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:CockpitLinkXPlanePlugin>"
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/$<TARGET_FILE_NAME:CockpitLinkXPlanePlugin>"
            COMMENT
                    "Deploying CockpitLink.xpl to ${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}"
        )
    endif()
endif()
