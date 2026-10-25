#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Окна «Открыть» и «Сохранить» через xdg-desktop-portal: у кого бэкендом
// стоит Shelf, тот получит окно Shelf, у остальных — окно их окружения.
// Ответ приходит позже сигналом; purpose — метка запроса, чтобы QML понял,
// на что ответ (open, saveAs, saveAsThenClose…)
// QDBusContext — чтобы в слоте ответа узнать по пути сообщения, на какой запрос ответ
class FilePortal : public QObject, protected QDBusContext
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit FilePortal(QObject *parent = nullptr);

    Q_INVOKABLE void open(const QString &purpose, const QString &folder);
    Q_INVOKABLE void save(const QString &purpose, const QString &folder, const QString &name);
    // Выбрать папку (для панели заметок)
    Q_INVOKABLE void openFolder(const QString &purpose, const QString &folder);

signals:
    void chosen(const QString &purpose, const QStringList &paths);
    void cancelled(const QString &purpose);
    // Портал недоступен или ответил ошибкой
    void failed(const QString &purpose, const QString &message);

private Q_SLOTS:
    void response(uint code, const QVariantMap &results);

private:
    void request(const QString &method, const QString &purpose, const QString &title, QVariantMap options);

    // Путь объекта Request → purpose
    QHash<QString, QString> m_pending;
    int m_counter = 0;
};
