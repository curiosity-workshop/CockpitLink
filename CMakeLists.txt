cmake_minimum_required(VERSION 3.20)

project(CockpitLink
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COCKPITLINK_BUILD_XPLANE_PLUGIN
    "Build the CockpitLink X-Plane plugin target"
    OFF)

option(COCKPITLINK_BUILD_MSFS_PROBE
    "Build the CockpitLink MSFS SimConnect connection probe"
    OFF)

set(COCKPITLINK_MSFS_SDK_PATH
    ""
    CACHE PATH
    "Path to the Microsoft Flight Simulator SDK root")

if(NOT COCKPITLINK_MSFS_SDK_PATH AND DEFINED ENV{MSFS_SDK_ROOT})
    set(COCKPITLINK_MSFS_SDK_PATH
        "$ENV{MSFS_SDK_ROOT}"
        CACHE PATH
        "Path to the Microsoft Flight Simulator SDK root"
        FORCE)
endif()

if(WIN32 AND NOT COCKPITLINK_MSFS_SDK_PATH AND
    EXISTS "C:/MSFS SDK/SimConnect SDK/include/SimConnect.h")
    set(COCKPITLINK_MSFS_SDK_PATH
        "C:/MSFS SDK"
        CACHE PATH
        "Path to the Microsoft Flight Simulator SDK root"
        FORCE)
endif()

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

if(COCKPITLINK_BUILD_MSFS_PROBE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "The native SimConnect probe currently requires Windows.")
    endif()

    set(COCKPITLINK_SIMCONNECT_ROOT
        "${COCKPITLINK_MSFS_SDK_PATH}/SimConnect SDK")
    set(COCKPITLINK_SIMCONNECT_INCLUDE_DIR
        "${COCKPITLINK_SIMCONNECT_ROOT}/include")
    set(COCKPITLINK_SIMCONNECT_LIBRARY
        "${COCKPITLINK_SIMCONNECT_ROOT}/lib/SimConnect.lib")
    set(COCKPITLINK_SIMCONNECT_DLL
        "${COCKPITLINK_SIMCONNECT_ROOT}/lib/SimConnect.dll")

    if(NOT EXISTS "${COCKPITLINK_SIMCONNECT_INCLUDE_DIR}/SimConnect.h" OR
        NOT EXISTS "${COCKPITLINK_SIMCONNECT_LIBRARY}" OR
        NOT EXISTS "${COCKPITLINK_SIMCONNECT_DLL}")
        message(FATAL_ERROR
            "COCKPITLINK_MSFS_SDK_PATH must point to an SDK containing "
            "SimConnect SDK/include/SimConnect.h and SimConnect SDK/lib.")
    endif()

    add_executable(CockpitLinkMSFSProbe
        msfs/CockpitLinkMSFSProbe.cpp
    )

    add_executable(CockpitLinkMSFS
        msfs/CockpitLinkMSFS.cpp
    )

    target_include_directories(CockpitLinkMSFSProbe
        PRIVATE
            "${COCKPITLINK_SIMCONNECT_INCLUDE_DIR}"
    )

    target_include_directories(CockpitLinkMSFS
        PRIVATE
            "${COCKPITLINK_SIMCONNECT_INCLUDE_DIR}"
    )

    target_link_libraries(CockpitLinkMSFSProbe
        PRIVATE
            "${COCKPITLINK_SIMCONNECT_LIBRARY}"
    )

    target_link_libraries(CockpitLinkMSFS
        PRIVATE
            CockpitLinkHost
            "${COCKPITLINK_SIMCONNECT_LIBRARY}"
    )

    add_custom_command(
        TARGET CockpitLinkMSFSProbe
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${COCKPITLINK_SIMCONNECT_DLL}"
                "$<TARGET_FILE_DIR:CockpitLinkMSFSProbe>/SimConnect.dll"
        COMMENT "Copying the SimConnect runtime beside CockpitLinkMSFSProbe"
    )

    add_custom_command(
        TARGET CockpitLinkMSFS
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${COCKPITLINK_SIMCONNECT_DLL}"
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/SimConnect.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/catalog"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/catalog/base-behaviors.json"
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/catalog/base-behaviors.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/profiles/aircraft"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/profiles/local"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/profiles/templates"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/profiles/aircraft/msfs-king-air-350i.json"
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/profiles/aircraft/msfs-king-air-350i.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/profiles/templates/user-profile.json"
                "$<TARGET_FILE_DIR:CockpitLinkMSFS>/profiles/templates/user-profile.json"
        COMMENT "Packaging CockpitLink MSFS runtime"
    )
endif()

if(NOT COCKPITLINK_XPLANE_SDK_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xplane_sdk/CHeaders/XPLM/XPLMDataAccess.h")
    set(COCKPITLINK_XPLANE_SDK_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xplane_sdk"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

add_library(CockpitLinkHost
    host/src/catalog/BehaviorCatalog.cpp
    host/src/logging/SerialTraceLogger.cpp
    host/src/protocol/Frame.cpp
    host/src/protocol/FrameParser.cpp
    host/src/protocol/Payloads.cpp
    host/src/protocol/TraceFormatter.cpp
    host/src/runtime/DeviceRuntime.cpp
    host/src/serial/SerialDeviceClassifier.cpp
    host/src/serial/WindowsSerialEnumerator.cpp
    host/src/serial/WindowsSerialTransport.cpp
    host/src/transport/TransportSession.cpp
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

target_link_libraries(CockpitLinkCatalogTests
    PRIVATE
        CockpitLinkHost
)

add_test(
    NAME CockpitLinkCatalogTests
    COMMAND CockpitLinkCatalogTests
        ${CMAKE_CURRENT_SOURCE_DIR}/catalog/base-behaviors.json
        ${CMAKE_CURRENT_SOURCE_DIR}/profiles/aircraft/msfs-king-air-350i.json
        ${CMAKE_CURRENT_SOURCE_DIR}/profiles/templates/user-profile.json
)

add_executable(CockpitLinkTransportTests
    host/tests/transport/TransportSessionTests.cpp
)

target_link_libraries(CockpitLinkTransportTests
    PRIVATE
        CockpitLinkHost
)

add_test(
    NAME CockpitLinkTransportTests
    COMMAND CockpitLinkTransportTests
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

    add_custom_command(
        TARGET CockpitLinkXPlanePlugin
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkXPlanePlugin>/catalog"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/catalog/base-behaviors.json"
                "$<TARGET_FILE_DIR:CockpitLinkXPlanePlugin>/catalog/base-behaviors.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkXPlanePlugin>/profiles/aircraft"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/profiles/aircraft"
                "$<TARGET_FILE_DIR:CockpitLinkXPlanePlugin>/profiles/aircraft"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:CockpitLinkXPlanePlugin>/profiles/local"
        COMMENT "Copying CockpitLink catalog and aircraft profiles"
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
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/catalog"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/catalog/base-behaviors.json"
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/catalog/base-behaviors.json"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/profiles/aircraft"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/profiles/aircraft"
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/profiles/aircraft"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}/profiles/local"
            COMMENT
                    "Deploying CockpitLink.xpl to ${COCKPITLINK_XPLANE_PLUGIN_DEPLOY_DIR}"
        )
    endif()
endif()
