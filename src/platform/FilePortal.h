#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PortalReceiver;

// Окна «Открыть» и «Сохранить» через xdg-desktop-portal: у кого бэкендом
// стоит Shelf, тот получит окно Shelf, у остальных — окно их окружения.
// Ответ приходит позже сигналом; purpose — метка запроса, чтобы QML понял,
// на что ответ (open, saveAs, folder).
// Портал есть только на Linux. Где его нет (Windows, Linux без сессионной шины)
// available ложно, а failed приходит сразу — QML тогда показывает окно Qt
class FilePortal : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit FilePortal(QObject *parent = nullptr);

    bool available() const { return m_available; }

    Q_INVOKABLE void open(const QString &purpose, const QString &folder);
    Q_INVOKABLE void save(const QString &purpose, const QString &folder, const QString &name);
    // Выбрать папку (для панели заметок)
    Q_INVOKABLE void openFolder(const QString &purpose, const QString &folder);

signals:
    void chosen(const QString &purpose, const QStringList &paths);
    void cancelled(const QString &purpose);
    // Портал недоступен или ответил ошибкой
    void failed(const QString &purpose, const QString &message);

private:
    friend class PortalReceiver;
    void request(const QString &method, const QString &purpose, const QString &title, QVariantMap options);
    void response(const QString &handle, uint code, const QVariantMap &results);

    bool m_available = false;
    PortalReceiver *m_receiver = nullptr;
    // Путь объекта Request → purpose
    QHash<QString, QString> m_pending;
    int m_counter = 0;
};
