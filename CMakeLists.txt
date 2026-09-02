cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING
        "Minimum supported macOS version" FORCE)
endif()

project(ReliefForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(RELIEFFORGE_BUILD_DESKTOP "Build the Qt desktop application" ON)
option(RELIEFFORGE_BUILD_STEP "Build the OpenCascade STEP exporter" ON)
option(RELIEFFORGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(reliefforge_core
    src/core/Image.cpp
    src/core/HeightCurve.cpp
    src/core/HeightField.cpp
    src/core/Mesh.cpp
    src/core/ReliefPipeline.cpp
    src/core/Project.cpp
    src/export/StlExporter.cpp
    src/export/VectorExporter.cpp
)
target_include_directories(reliefforge_core PUBLIC src)
target_compile_features(reliefforge_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(reliefforge_core PRIVATE /W4 /permissive-)
    if(RELIEFFORGE_WARNINGS_AS_ERRORS)
        target_compile_options(reliefforge_core PRIVATE /WX)
    endif()
else()
    target_compile_options(reliefforge_core PRIVATE -Wall -Wextra -Wpedantic)
    if(RELIEFFORGE_WARNINGS_AS_ERRORS)
        target_compile_options(reliefforge_core PRIVATE -Werror)
    endif()
endif()

set(RELIEFFORGE_HAS_OCCT OFF)
if(RELIEFFORGE_BUILD_STEP)
    find_package(OpenCASCADE QUIET)
    if(OpenCASCADE_FOUND)
        set(RELIEFFORGE_HAS_OCCT ON)
        add_library(reliefforge_step src/cad/StepExporter.cpp)
        target_include_directories(reliefforge_step
            PUBLIC src
            PRIVATE ${OpenCASCADE_INCLUDE_DIR}
        )
        # Link only the STEP stack and the modeling modules used directly. The
        # imported OCCT targets carry their own transitive dependencies; using
        # OpenCASCADE_LIBRARIES would pull visualization, test, and DRAW modules
        # into a release bundle unnecessarily.
        target_link_libraries(reliefforge_step PUBLIC
            TKDESTEP
            TKGeomBase
            TKBRep
            TKTopAlgo
            TKPrim
            TKBool
            TKShHealing
        )
        target_compile_definitions(reliefforge_step PUBLIC RELIEFFORGE_HAS_OCCT=1)
        message(STATUS "ReliefForge: genuine OpenCascade STEP export enabled")
    else()
        message(STATUS "ReliefForge: OpenCascade not found; STEP target disabled")
    endif()
endif()

if(RELIEFFORGE_BUILD_DESKTOP)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 Quick3D Concurrent)
    if(Qt6_FOUND)
        qt_policy(SET QTP0004 NEW)
        qt_standard_project_setup(REQUIRES 6.5)

        set_source_files_properties(src/app/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
        set_source_files_properties(src/app/qml/InspectorSection.qml PROPERTIES QT_RESOURCE_ALIAS InspectorSection.qml)
        set_source_files_properties(src/app/qml/PropertySlider.qml PROPERTIES QT_RESOURCE_ALIAS PropertySlider.qml)
        set_source_files_properties(src/app/qml/EmptyState.qml PROPERTIES QT_RESOURCE_ALIAS EmptyState.qml)
        set(RELIEFFORGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/app/assets/ReliefForge.icns")
        set(RELIEFFORGE_BUNDLE_DOCUMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/COPYING"
            "${CMAKE_CURRENT_SOURCE_DIR}/CREDITS.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/CORRESPONDING_SOURCE.md"
        )
        set_source_files_properties(
            "${RELIEFFORGE_ICON}"
            ${RELIEFFORGE_BUNDLE_DOCUMENTS}
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )

        qt_add_executable(ReliefForge
            src/app/main.cpp
            src/app/AppController.cpp
            src/app/ReliefGeometry.cpp
            "${RELIEFFORGE_ICON}"
            ${RELIEFFORGE_BUNDLE_DOCUMENTS}
        )
        set_target_properties(ReliefForge PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "ReliefForge"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.reliefforge.desktop"
            MACOSX_BUNDLE_ICON_FILE "ReliefForge.icns"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 CHCOfficial"
        )
        qt_add_qml_module(ReliefForge
            URI ReliefForge
            VERSION 1.0
            QML_FILES
                src/app/qml/Main.qml
                src/app/qml/InspectorSection.qml
                src/app/qml/PropertySlider.qml
                src/app/qml/EmptyState.qml
        )
        qt_add_resources(ReliefForge "legal" PREFIX "/legal" FILES
            COPYING CREDITS.md THIRD_PARTY_NOTICES.md CORRESPONDING_SOURCE.md)
        # Checked-in procedural assets: available offline without a writable
        # installation directory or a runtime Python dependency.
        file(GLOB RELIEFFORGE_EXAMPLE_IMAGES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.png")
        set(RELIEFFORGE_EXAMPLES ${RELIEFFORGE_EXAMPLE_IMAGES} "${CMAKE_CURRENT_SOURCE_DIR}/examples/catalog.json")
        qt_add_resources(ReliefForge "examples" PREFIX "/examples"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/examples" FILES ${RELIEFFORGE_EXAMPLES})
        target_include_directories(ReliefForge PRIVATE src)
        target_link_libraries(ReliefForge PRIVATE
            reliefforge_core
            Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2 Qt6::Quick3D Qt6::Concurrent
        )
        if(RELIEFFORGE_HAS_OCCT)
            target_link_libraries(ReliefForge PRIVATE reliefforge_step)
        endif()
        target_compile_definitions(ReliefForge PRIVATE
            RELIEFFORGE_VERSION="${PROJECT_VERSION}"
            $<$<BOOL:${RELIEFFORGE_HAS_OCCT}>:RELIEFFORGE_HAS_OCCT=1>
        )

        install(TARGETS ReliefForge
            BUNDLE DESTINATION .
            RUNTIME DESTINATION bin
        )
        message(STATUS "ReliefForge: Qt desktop application enabled")
    else()
        message(STATUS "ReliefForge: Qt 6.5+ not found; desktop target disabled")
    endif()
endif()

add_executable(reliefforge-cli src/app/cli.cpp)
target_link_libraries(reliefforge-cli PRIVATE reliefforge_core)
if(RELIEFFORGE_HAS_OCCT)
    target_link_libraries(reliefforge-cli PRIVATE reliefforge_step)
    target_compile_definitions(reliefforge-cli PRIVATE RELIEFFORGE_HAS_OCCT=1)
endif()

if(BUILD_TESTING)
    add_executable(reliefforge_tests
        tests/TestMain.cpp
        tests/ImageTests.cpp
        tests/GeometryTests.cpp
        tests/VectorTests.cpp
        tests/ExportTests.cpp
        tests/ProjectTests.cpp
    )
    target_link_libraries(reliefforge_tests PRIVATE reliefforge_core)
    target_include_directories(reliefforge_tests PRIVATE tests)
    if(RELIEFFORGE_HAS_OCCT)
        target_sources(reliefforge_tests PRIVATE tests/StepTests.cpp)
        target_link_libraries(reliefforge_tests PRIVATE reliefforge_step)
        target_compile_definitions(reliefforge_tests PRIVATE RELIEFFORGE_HAS_OCCT=1)
    endif()
    add_test(NAME reliefforge_tests COMMAND reliefforge_tests)
    if(TARGET ReliefForge)
        target_sources(reliefforge_tests PRIVATE
            tests/PreviewTests.cpp tests/ControllerTests.cpp
            src/app/ReliefGeometry.cpp src/app/AppController.cpp)
        target_link_libraries(reliefforge_tests PRIVATE Qt6::Quick3D Qt6::Concurrent)
        target_compile_definitions(reliefforge_tests PRIVATE RELIEFFORGE_TEST_QT=1)
        qt_add_resources(reliefforge_tests "legal_test" PREFIX "/legal" FILES
            COPYING CREDITS.md THIRD_PARTY_NOTICES.md CORRESPONDING_SOURCE.md)
        qt_add_resources(reliefforge_tests "examples_test" PREFIX "/examples"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/examples" FILES ${RELIEFFORGE_EXAMPLES})
        set_tests_properties(reliefforge_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
