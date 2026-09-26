cmake_minimum_required(VERSION 3.24)

project(MarchCraft VERSION 0.6.0 LANGUAGES CXX)

option(MARCHCRAFT_SYNC_PRODUCTION_PREVIEW
       "Copy successful Windows builds into the production preview directory" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Concurrent Gui Widgets PrintSupport Quick QuickControls2 Quick3D Multimedia Xml Sql Test)

set_source_files_properties(qml/MarchCraftTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

qt_add_executable(marchcraft
    src/main.cpp
    src/DrillProject.cpp
    src/ProjectAlgorithms.cpp
    src/ProjectStorage.cpp
    src/ProjectPersistence.cpp
    src/ProjectFormations.cpp
    src/ProjectClinic.cpp
    src/ProjectTransitions.cpp
    src/TransitionPath.cpp
    src/ProjectMusic.cpp
    src/ProjectTimeline.cpp
    src/DrillProject.h
    src/DrillTypes.h
    src/MusicDocument.cpp
    src/MusicDocument.h
    src/TimelinePlanner.cpp
    src/TimelinePlanner.h
    src/MidiSynthEngine.cpp
    src/MidiSynthEngine.h
    src/TransportController.cpp
    src/TransportController.h
    src/ExportController.cpp
    src/ExportController.h
    src/WorkspaceController.cpp
    src/WorkspaceController.h
    src/SceneTypes.h
    src/AssetCatalog.cpp
    src/AssetCatalog.h
)

qt_add_qml_module(marchcraft
    URI MarchCraft
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/EditorCommandBars.qml
        qml/WelcomeWorkspace.qml
        qml/ExportWorkspace.qml
        qml/ExportVideoWindow.qml
        qml/EditorPreferences.qml
        qml/PerformerDialog.qml
        qml/ProjectSettingsDialog.qml
        qml/RosterPanel.qml
        qml/InspectorPanel.qml
        qml/MovementTabs.qml
        qml/TimelineViewport.qml
        qml/SetTimeline.qml
        qml/FieldView.qml
        qml/ThreeDView.qml
        qml/CameraMotion.js
        qml/PerformerMarker3D.qml
        qml/HumanPerformer3D.qml
        qml/HumanGait.js
        qml/Venue3D.qml
        qml/Prop3D.qml
        qml/FormationDialog.qml
        qml/MovableDialog.qml
        qml/MusicPanel.qml
        qml/MarchCraftTheme.qml
        qml/AppButton.qml
        qml/AppToolButton.qml
        qml/AppMenuItem.qml
        qml/WorkspaceLogic.qml
        qml/AppIcon.qml
        qml/ShapeIcon.qml
        qml/ShapeDrawLogic.qml
        qml/ProjectSetupPage.qml
    SOURCES
        src/HumanGeometry.cpp
        src/HumanGeometry.h
)

# Headers shared by the application and generated QML registration.
target_include_directories(marchcraft PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(MARCHCRAFT_SAMPLE_JSON "${CMAKE_CURRENT_SOURCE_DIR}/../data/coordinates.json")
set_source_files_properties("${MARCHCRAFT_SAMPLE_JSON}" PROPERTIES
    QT_RESOURCE_ALIAS "coordinates.json")
qt_add_resources(marchcraft "marchcraft_samples"
    PREFIX "/samples"
    FILES "${MARCHCRAFT_SAMPLE_JSON}")

qt_add_resources(marchcraft "marchcraft_catalog"
    PREFIX "/catalog"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/catalog.json")

set(MARCHCRAFT_HUMAN_GLB "${CMAKE_CURRENT_SOURCE_DIR}/assets/performer/human_performer.glb")
set(MARCHCRAFT_HUMAN_GLB_LOD1 "${CMAKE_CURRENT_SOURCE_DIR}/assets/performer/human_performer_lod1.glb")
set(MARCHCRAFT_HUMAN_GLB_LOD2 "${CMAKE_CURRENT_SOURCE_DIR}/assets/performer/human_performer_lod2.glb")
set_source_files_properties("${MARCHCRAFT_HUMAN_GLB}" PROPERTIES QT_RESOURCE_ALIAS "human_performer.glb")
set_source_files_properties("${MARCHCRAFT_HUMAN_GLB_LOD1}" PROPERTIES QT_RESOURCE_ALIAS "human_performer_lod1.glb")
set_source_files_properties("${MARCHCRAFT_HUMAN_GLB_LOD2}" PROPERTIES QT_RESOURCE_ALIAS "human_performer_lod2.glb")
qt_add_resources(marchcraft "marchcraft_performer"
    PREFIX "/performer"
    FILES "${MARCHCRAFT_HUMAN_GLB}" "${MARCHCRAFT_HUMAN_GLB_LOD1}" "${MARCHCRAFT_HUMAN_GLB_LOD2}")

set(MARCHCRAFT_LOGO_PNG "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/marchcraft-logo.png")
set_source_files_properties("${MARCHCRAFT_LOGO_PNG}" PROPERTIES QT_RESOURCE_ALIAS "marchcraft-logo.png")
qt_add_resources(marchcraft "marchcraft_branding"
    PREFIX "/branding"
    FILES "${MARCHCRAFT_LOGO_PNG}")

add_custom_command(TARGET marchcraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth"
            "$<TARGET_FILE_DIR:marchcraft>/fluidsynth"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/soundfonts"
            "$<TARGET_FILE_DIR:marchcraft>/soundfonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:marchcraft>/THIRD_PARTY_NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/libfluidsynth-3.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/SDL3.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/sndfile.dll"
            "$<TARGET_FILE_DIR:marchcraft>"
    VERBATIM)

target_link_libraries(marchcraft PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Quick3D
    Qt6::Multimedia
    Qt6::Xml
    Qt6::Sql
)

target_compile_definitions(marchcraft PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

if(WIN32)
    target_sources(marchcraft PRIVATE marchcraft.rc)
    set_target_properties(marchcraft PROPERTIES WIN32_EXECUTABLE TRUE)
    # A build-tree executable must be runnable on its own, not only from a Qt
    # developer shell. Bundle the Qt DLLs, plugins, and QML imports beside it.
    get_filename_component(MARCHCRAFT_QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
    add_custom_command(TARGET marchcraft POST_BUILD
        COMMAND "${MARCHCRAFT_QT_BIN}/windeployqt.exe"
                --release
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:marchcraft>"
        VERBATIM)
endif()

if(WIN32 AND MARCHCRAFT_SYNC_PRODUCTION_PREVIEW)
    # Keep the user-facing production preview executable synchronized after every build.
    set(MARCHCRAFT_PRODUCTION_PREVIEW_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/MarchCraft-Production-Preview")
    add_custom_command(TARGET marchcraft POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:marchcraft>"
                "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}/MarchCraft.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth"
                "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}/fluidsynth"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/soundfonts"
                "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}/soundfonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
                "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}/THIRD_PARTY_NOTICES.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/libfluidsynth-3.dll"
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/SDL3.dll"
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth/sndfile.dll"
                "${MARCHCRAFT_PRODUCTION_PREVIEW_DIR}"
        VERBATIM)
endif()

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(marchcraft_tests
        tests/tst_drillproject.cpp
        src/DrillProject.cpp
        src/ProjectAlgorithms.cpp
        src/ProjectStorage.cpp
        src/ProjectPersistence.cpp
        src/ProjectFormations.cpp
        src/ProjectClinic.cpp
        src/ProjectTransitions.cpp
        src/TransitionPath.cpp
        src/ProjectMusic.cpp
        src/ProjectTimeline.cpp
        src/DrillProject.h
        src/DrillTypes.h
        src/MusicDocument.cpp
        src/MusicDocument.h
        src/TimelinePlanner.cpp
        src/TimelinePlanner.h
        src/MidiSynthEngine.cpp
        src/MidiSynthEngine.h
        src/TransportController.cpp
        src/TransportController.h
        src/ExportController.cpp
        src/ExportController.h
        src/WorkspaceController.cpp
        src/WorkspaceController.h
        src/SceneTypes.h
        src/AssetCatalog.cpp
        src/AssetCatalog.h
    )
    target_include_directories(marchcraft_tests PRIVATE src)
    target_link_libraries(marchcraft_tests PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Xml Qt6::Sql Qt6::Test)
    qt_add_resources(marchcraft_tests "marchcraft_test_catalog"
        PREFIX "/catalog"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/catalog.json")
    add_test(NAME marchcraft_tests COMMAND marchcraft_tests -o -,txt)
    get_filename_component(MARCHCRAFT_QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
    get_filename_component(MARCHCRAFT_QT_PREFIX "${MARCHCRAFT_QT_BIN}/.." ABSOLUTE)
    get_filename_component(MARCHCRAFT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_test(NAME ui_qml
        COMMAND "${MARCHCRAFT_QT_BIN}/qmltestrunner.exe"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                -import "${CMAKE_CURRENT_SOURCE_DIR}/qml")
    set_tests_properties(marchcraft_tests PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=minimal;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${MARCHCRAFT_QT_PREFIX}/plugins;QML2_IMPORT_PATH=${MARCHCRAFT_QT_PREFIX}/qml")
    set_tests_properties(ui_qml PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=minimal;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${MARCHCRAFT_QT_PREFIX}/plugins;QML2_IMPORT_PATH=${MARCHCRAFT_QT_PREFIX}/qml")
    set_property(TEST marchcraft_tests PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${MARCHCRAFT_QT_BIN}"
        "PATH=path_list_prepend:${MARCHCRAFT_COMPILER_BIN}")
    set_property(TEST ui_qml PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${MARCHCRAFT_QT_BIN}"
        "PATH=path_list_prepend:${MARCHCRAFT_COMPILER_BIN}")
endif()

install(TARGETS marchcraft
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin)
install(DIRECTORY third_party/fluidsynth DESTINATION bin)
install(DIRECTORY third_party/soundfonts DESTINATION bin)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION bin)
