#include "Glaze.h"
#include "Instance.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QTextStream>
#include <QUrl>

// Glaze собран статически — его QML-модуль подключаем явно
Q_IMPORT_QML_PLUGIN(GlazePlugin)

int main(int argc, char *argv[])
{
    // До QGuiApplication: ему нужен экран, а --version должен работать и по SSH, и в скриптах
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], "--version") == 0) {
            QTextStream(stdout) << "Jot " << JOT_VERSION << '\n';
            return 0;
        }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("jot"));
    app.setOrganizationName(QStringLiteral("jot"));
    app.setApplicationDisplayName(QStringLiteral("Jot"));
    app.setApplicationVersion(QStringLiteral(JOT_VERSION));

    // Становится app_id окна в Wayland — по нему Hyprland и .desktop узнают приложение
    app.setDesktopFileName(QStringLiteral("io.github.nook_apps.Jot"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.nook_apps.Jot"),
                                       QIcon(QStringLiteral(":/qt/qml/Jot/data/icons/io.github.nook_apps.Jot.svg"))));

    // Фиолетовый Jot из дизайн-системы (jot / jot-deep) и свой шрифт значков:
    // символы Nerd Font, которые рисуют Jot и контролы Glaze, вшиты в программу
    Glaze::setup({
        .brand = QColor(0xb4, 0x8c, 0xff),
        .brandDeep = QColor(0x8b, 0x5c, 0xf6),
        .iconFamily = QStringLiteral("Jot Icons"),
        .fonts = {QStringLiteral(":/qt/qml/Jot/fonts/JotIcons.ttf"), QStringLiteral(":/qt/qml/Jot/fonts/JotIconsPropo.ttf")},
    });

    // jot [файлы…] — нет такого файла — откроется пустой с этим именем, создастся при сохранении
    QStringList files;
    for (QString arg : app.arguments().mid(1)) {
        if (arg.startsWith(QLatin1String("file://")))
            arg = QUrl(arg).toLocalFile();
        if (!arg.isEmpty() && !arg.startsWith(QLatin1Char('-')))
            files << QFileInfo(arg).absoluteFilePath();
    }

    // Окно уже открыто — файлы уходят к нему вкладками
    if (Instance::forward(files))
        return 0;
    Instance instance(nullptr);
    instance.listen();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("startFiles"), files}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Jot", "Main");
    return app.exec();
}
