cmake_minimum_required(VERSION 3.21)
project(omamusic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Widgets Network Quick QuickControls2 Svg Test)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_library(omacore STATIC
    src/paths.cpp src/paths.h
    src/theme.cpp src/theme.h
    src/config.cpp src/config.h
    src/track.cpp src/track.h
    src/rowsmodel.cpp src/rowsmodel.h
    src/library.cpp src/library.h
    src/mpvclient.cpp src/mpvclient.h
    src/player.cpp src/player.h
    src/instance.cpp src/instance.h
    src/settings.cpp src/settings.h
    src/lookscope.cpp src/lookscope.h
    src/autostart.cpp src/autostart.h
    src/equalizer.cpp src/equalizer.h
    src/playlists.cpp src/playlists.h
    src/trayicon.cpp src/trayicon.h
    src/mpris.cpp src/mpris.h
)
target_include_directories(omacore PUBLIC src)
target_link_libraries(omacore PUBLIC Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network Qt6::Svg)

qt_add_executable(omamusic src/main.cpp src/shell.cpp src/shell.h)
target_link_libraries(omamusic PRIVATE omacore Qt6::Widgets Qt6::Quick Qt6::QuickControls2)
# Scale.js is a plain (non-pragma) script on purpose; it is only imported by path.
set_source_files_properties(qml/Scale.js PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)


qt_add_qml_module(omamusic
    URI Omamusic
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/BrowseList.qml
        qml/DrillList.qml
        qml/EqualizerForm.qml
        qml/EqualizerPopup.qml
        qml/LibraryForm.qml
        qml/PlaylistPicker.qml
        qml/FastWheel.qml
        qml/Panel.qml
        qml/SeekBar.qml
        qml/TransportButton.qml
        qml/Fmt.js
        qml/Scale.js
        qml/UiText.qml
        qml/Segmented.qml
        qml/ToggleRow.qml
        qml/Stepper.qml
        qml/FontPicker.qml
        qml/SettingsForm.qml
        qml/LookForm.qml
)

qt_add_resources(omamusic icons
    PREFIX /icons
    BASE resources
    FILES resources/omamusic-symbolic.svg
)

configure_file(bin/omamusic-scan ${CMAKE_BINARY_DIR}/omamusic-scan COPYONLY)

# cmake --install build --prefix ~/.local
include(GNUInstallDirs)
install(TARGETS omamusic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS bin/omamusic-scan DESTINATION ${CMAKE_INSTALL_BINDIR})   # found beside the binary
install(FILES data/omamusic.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/omamusic-symbolic.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME omamusic.svg)

enable_testing()
add_subdirectory(tests)
