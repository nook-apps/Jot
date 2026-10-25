#include "FilePortal.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QUrl>

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kChooser = QStringLiteral("org.freedesktop.portal.FileChooser");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");

// Фильтр портала: a(sa(us)) — имя и список правил (0 — маска, 1 — MIME-тип)
struct FilterRule {
    uint type = 0;
    QString pattern;
};
struct Filter {
    QString name;
    QList<FilterRule> rules;
};
}

Q_DECLARE_METATYPE(FilterRule)
Q_DECLARE_METATYPE(Filter)

QDBusArgument &operator<<(QDBusArgument &arg, const FilterRule &r)
{
    arg.beginStructure();
    arg << r.type << r.pattern;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, FilterRule &r)
{
    arg.beginStructure();
    arg >> r.type >> r.pattern;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const Filter &f)
{
    arg.beginStructure();
    arg << f.name << f.rules;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Filter &f)
{
    arg.beginStructure();
    arg >> f.name >> f.rules;
    arg.endStructure();
    return arg;
}

namespace {
QList<Filter> markdownFilters()
{
    return {
        {QStringLiteral("Markdown"),
         {{0, QStringLiteral("*.md")}, {0, QStringLiteral("*.markdown")}, {0, QStringLiteral("*.mdown")},
          {1, QStringLiteral("text/markdown")}}},
        {QStringLiteral("Текст"), {{0, QStringLiteral("*.txt")}, {1, QStringLiteral("text/plain")}}},
        {QStringLiteral("Все файлы"), {{0, QStringLiteral("*")}}},
    };
}

// Портал ждёт путь массивом байт с нулём на конце
QByteArray folderBytes(const QString &folder)
{
    const QString f = folder.isEmpty() ? QDir::homePath() : folder;
    return QFile::encodeName(f) + '\0';
}
}

FilePortal::FilePortal(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<QList<FilterRule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();
}

void FilePortal::open(const QString &purpose, const QString &folder)
{
    request(QStringLiteral("OpenFile"), purpose, QStringLiteral("Открыть"),
            {{QStringLiteral("multiple"), true},
             {QStringLiteral("current_folder"), folderBytes(folder)},
             {QStringLiteral("filters"), QVariant::fromValue(markdownFilters())}});
}

void FilePortal::save(const QString &purpose, const QString &folder, const QString &name)
{
    request(QStringLiteral("SaveFile"), purpose, QStringLiteral("Сохранить"),
            {{QStringLiteral("current_name"), name},
             {QStringLiteral("current_folder"), folderBytes(folder)},
             {QStringLiteral("filters"), QVariant::fromValue(markdownFilters())}});
}

void FilePortal::request(const QString &method, const QString &purpose, const QString &title, QVariantMap options)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Путь будущего Request известен заранее: подписываемся до вызова,
    // иначе быстрый ответ может прийти раньше подписки
    const QString token = QStringLiteral("jot%1").arg(++m_counter);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    options.insert(QStringLiteral("handle_token"), token);
    m_pending.insert(handle, purpose);
    bus.connect(kService, handle, kRequest, QStringLiteral("Response"), this, SLOT(response(uint, QVariantMap)));

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kChooser, method);
    msg << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, handle, purpose](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            m_pending.remove(handle);
            QDBusConnection::sessionBus().disconnect(kService, handle, kRequest, QStringLiteral("Response"), this,
                                                     SLOT(response(uint, QVariantMap)));
            emit failed(purpose, reply.error().message());
            return;
        }
        // Старые порталы не знают handle_token и выдают свой путь
        const QString actual = reply.value().path();
        if (actual != handle && !m_pending.contains(actual)) {
            m_pending.insert(actual, purpose);
            QDBusConnection::sessionBus().connect(kService, actual, kRequest, QStringLiteral("Response"), this,
                                                  SLOT(response(uint, QVariantMap)));
        }
    });
}

void FilePortal::response(uint code, const QVariantMap &results)
{
    const QString handle = message().path();
    const QString purpose = m_pending.take(handle);
    QDBusConnection::sessionBus().disconnect(kService, handle, kRequest, QStringLiteral("Response"), this,
                                             SLOT(response(uint, QVariantMap)));
    if (purpose.isEmpty())
        return;
    // 0 — выбрали, 1 — отмена, 2 — окно закрылось иначе
    if (code != 0) {
        emit cancelled(purpose);
        return;
    }
    QStringList paths;
    for (const QString &uri : results.value(QStringLiteral("uris")).toStringList()) {
        const QUrl url(uri);
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (paths.isEmpty())
        emit cancelled(purpose);
    else
        emit chosen(purpose, paths);
}
