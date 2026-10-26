#include "Instance.h"

#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QUrl>

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

    // Шрифты вшиты в программу: значки интерфейса (символы Nerd Font) и JetBrains Mono
    // для текста — на Windows и в чистом дистрибутиве их нет, а вид должен быть один
    for (const char *file : {"JotIcons.ttf", "JotIconsPropo.ttf", "JetBrainsMono-Regular.otf", "JetBrainsMono-Italic.otf",
                             "JetBrainsMono-SemiBold.otf", "JetBrainsMono-Bold.otf", "JetBrainsMono-BoldItalic.otf"})
        QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Jot/fonts/") + QLatin1String(file));

    // qt6ct кладёт в шрифт приложения имя начертания («Regular»), его наследуют
    // все шрифты, и оно перебивает жирность: font.bold и жирное в тексте
    // рисовались бы обычным. Начертание выбираем по весу сами
    QFont font = QGuiApplication::font();
    if (!font.styleName().isEmpty()) {
        font.setStyleName(QString());
        QGuiApplication::setFont(font);
    }

    // Свои контролы рисуем сами; Basic не тянет вид qt6ct/kvantum
    QQuickStyle::setStyle(QStringLiteral("Basic"));
#ifdef Q_OS_WIN
    // Без альфа-канала окно на Windows непрозрачное, и фон Acrylic под ним не виден
    QQuickWindow::setDefaultAlphaBuffer(true);
#endif

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
