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
    Core Concurrent Gui Quick QuickControls2 Quick3D Multimedia Xml Sql Test)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

qt_add_executable(marchcraft
    src/main.cpp
    src/DrillProject.cpp
    src/DrillProject.h
    src/DrillTypes.h
    src/MusicDocument.cpp
    src/MusicDocument.h
    src/MidiSynthEngine.cpp
    src/MidiSynthEngine.h
    src/TransportController.cpp
    src/TransportController.h
    src/SceneTypes.h
    src/AssetCatalog.cpp
    src/AssetCatalog.h
)

qt_add_qml_module(marchcraft
    URI MarchCraft
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/FieldView.qml
        qml/ThreeDView.qml
        qml/Mannequin3D.qml
        qml/Venue3D.qml
        qml/Prop3D.qml
        qml/FormationDialog.qml
        qml/MusicPanel.qml
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
    set_target_properties(marchcraft PROPERTIES WIN32_EXECUTABLE TRUE)
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
        src/DrillProject.h
        src/DrillTypes.h
        src/MusicDocument.cpp
        src/MusicDocument.h
        src/MidiSynthEngine.cpp
        src/MidiSynthEngine.h
        src/TransportController.cpp
        src/TransportController.h
        src/SceneTypes.h
        src/AssetCatalog.cpp
        src/AssetCatalog.h
    )
    target_include_directories(marchcraft_tests PRIVATE src)
    target_link_libraries(marchcraft_tests PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Gui Qt6::Multimedia Qt6::Xml Qt6::Sql Qt6::Test)
    qt_add_resources(marchcraft_tests "marchcraft_test_catalog"
        PREFIX "/catalog"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/catalog.json")
    add_test(NAME marchcraft_tests COMMAND marchcraft_tests)
    get_filename_component(MARCHCRAFT_QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
    get_filename_component(MARCHCRAFT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(marchcraft_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_property(TEST marchcraft_tests PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${MARCHCRAFT_QT_BIN}"
        "PATH=path_list_prepend:${MARCHCRAFT_COMPILER_BIN}")
endif()

install(TARGETS marchcraft
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin)
install(DIRECTORY third_party/fluidsynth DESTINATION bin)
install(DIRECTORY third_party/soundfonts DESTINATION bin)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION bin)
