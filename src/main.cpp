#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
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
    app.setDesktopFileName(QStringLiteral("org.stasyao.jot"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.stasyao.jot"),
                                       QIcon(QStringLiteral(":/qt/qml/Jot/data/icons/org.stasyao.jot.svg"))));

    // Значки интерфейса — вшитые символы Nerd Font: не зависим от установленных шрифтов
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Jot/fonts/JotIcons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Jot/fonts/JotIconsPropo.ttf"));

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

    // jot [файл] — нет такого файла — откроется пустой с этим именем, создастся при сохранении
    QString start = app.arguments().value(1);
    if (start.startsWith(QLatin1String("file://")))
        start = QUrl(start).toLocalFile();
    if (!start.isEmpty())
        start = QFileInfo(start).absoluteFilePath();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("startFile"), start}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Jot", "Main");
    return app.exec();
}
