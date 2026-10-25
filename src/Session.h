#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Вкладки окна между запусками: ~/.local/state/jot/session.json.
// Пишется и при обычном закрытии, и по ходу работы — после сбоя или выключения
// вкладки вернутся так же. Безымянные и несохранённые вкладки хранят текст черновиком
class Session : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Session(QObject *parent = nullptr);

    // tabs — [{ path, cursor, draft? }], current — номер текущей,
    // extra — остальное состояние окна (панель папки: folder, pinned, expanded, sidebar)
    Q_INVOKABLE void save(const QVariantList &tabs, int current, const QVariantMap &extra = {});
    // { tabs, current, …extra } или пусто, если сессии нет
    Q_INVOKABLE QVariantMap load() const;

private:
    static QString file();
};
