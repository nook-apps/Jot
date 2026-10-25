#include "Session.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

Session::Session(QObject *parent)
    : QObject(parent)
{
}

QString Session::file()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/jot/session.json");
}

void Session::save(const QVariantList &tabs, int current)
{
    if (!QDir().mkpath(QFileInfo(file()).absolutePath()))
        return;
    const QJsonObject o{{QStringLiteral("current"), current},
                        {QStringLiteral("tabs"), QJsonArray::fromVariantList(tabs)}};
    // QSaveFile: выключение посреди записи не оставит обрезанный файл
    QSaveFile f(file());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

QVariantMap Session::load() const
{
    QFile f(file());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (!o.value(QStringLiteral("tabs")).isArray())
        return {};
    return o.toVariantMap();
}
