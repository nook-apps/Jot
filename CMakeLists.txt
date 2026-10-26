cmake_minimum_required(VERSION 3.21)
project(jot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ninja по умолчанию запускает nproc+2 компиляторов, каждый cc1plus с Qt ест ~350 МБ —
# держим не больше 4 сразу, как в Shelf
set_property(GLOBAL PROPERTY JOB_POOLS compile=4)
set(CMAKE_JOB_POOL_COMPILE compile)

# 6.7 — из-за QStandardPaths::GenericStateLocation (~/.local/state).
# Network — для QLocalServer (одно окно на сеанс)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

# D-Bus — только на Linux: портал выбора файлов и светлая/тёмная тема из портала.
# На Windows и macOS Jot собирается без него и берёт окна выбора и тему у Qt
# -DJOT_WITH_DBUS=OFF собирает Linux-версию так, как её видит Windows: проверка
# запасных путей без второй машины
option(JOT_WITH_DBUS "Портал и тема через D-Bus (только Linux)" ON)
if(UNIX AND NOT APPLE AND JOT_WITH_DBUS)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    set(JOT_DBUS ON)
endif()
qt_standard_project_setup(REQUIRES 6.7)

qt_add_executable(jot src/main.cpp)

set(QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/Editor.qml
    qml/TabStrip.qml
    qml/Sidebar.qml
    qml/GlassButton.qml
    qml/GlassDialog.qml
    qml/TextButton.qml
    qml/ConfirmDialog.qml
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Файлы лежат в qml/, но в модуле — в корне: иначе синглтон Theme
# не виден из соседних файлов (неявный импорт идёт по каталогу файла)
foreach(f ${QML_FILES})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(jot
    URI Jot
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        fonts/JotIcons.ttf fonts/JotIconsPropo.ttf
        fonts/JetBrainsMono-Regular.otf fonts/JetBrainsMono-Italic.otf fonts/JetBrainsMono-SemiBold.otf
        fonts/JetBrainsMono-Bold.otf fonts/JetBrainsMono-BoldItalic.otf
        data/icons/io.github.nook_apps.Jot.svg
    SOURCES
        src/Document.h src/Document.cpp
        src/MarkdownHighlighter.h src/MarkdownHighlighter.cpp
        src/Session.h src/Session.cpp
        src/FolderModel.h src/FolderModel.cpp
        # Всё, что на разных системах устроено по-разному. Уйдёт в общий модуль Glaze
        src/platform/Platform.h src/platform/Platform.cpp
        src/platform/Instance.h src/platform/Instance.cpp
        src/platform/FilePortal.h src/platform/FilePortal.cpp
        src/platform/ThemeSource.h src/platform/ThemeSource.cpp
)

# QML компилируется только в байт-код, без C++ для привязок (AOT): сгенерированный C++
# пользуется закрытым API Qt и привязал бы сборку к точной версии Qt (см. Shelf)
set_target_properties(jot PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
# На Windows — окно без консоли рядом
set_target_properties(jot PROPERTIES WIN32_EXECUTABLE TRUE)
target_include_directories(jot PRIVATE src src/platform)
target_compile_definitions(jot PRIVATE JOT_VERSION="${PROJECT_VERSION}")
target_link_libraries(jot PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network)
if(JOT_DBUS)
    target_link_libraries(jot PRIVATE Qt6::DBus)
    target_compile_definitions(jot PRIVATE JOT_HAVE_DBUS)
endif()
if(MSVC)
    target_compile_options(jot PRIVATE /W4 /utf-8)
else()
    target_compile_options(jot PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS jot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Ярлык в меню и иконка — по стандарту freedesktop, только на Linux
if(UNIX AND NOT APPLE)
    configure_file(data/io.github.nook_apps.Jot.desktop.in io.github.nook_apps.Jot.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.nook_apps.Jot.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES data/icons/io.github.nook_apps.Jot.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
