#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QSet>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Дерево заметок одной папки для боковой панели, развёрнутое в плоский список:
// у каждой строки своя глубина, раскрытая папка просто вставляет детей после себя.
// Видны папки и текстовые файлы (.md, .markdown, .txt); скрытое и служебное — нет.
// За раскрытыми папками следит QFileSystemWatcher: новые и удалённые файлы
// появляются в панели сами
class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(QString rootName READ rootName NOTIFY rootChanged)
    // Раскрытые папки — для сессии
    Q_PROPERTY(QStringList expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, PathRole, IsDirRole, DepthRole, ExpandedRole };

    explicit FolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString root() const { return m_root; }
    void setRoot(const QString &path);
    QString rootName() const;
    QStringList expanded() const;
    void setExpanded(const QStringList &paths);

    // Раскрыть или свернуть папку в строке row
    Q_INVOKABLE void toggle(int row);
    // Раскрыть папки на пути к файлу, чтобы его строка стала видна; номер строки или -1
    Q_INVOKABLE int reveal(const QString &path);
    Q_INVOKABLE int indexOf(const QString &path) const;
    // Новая пустая заметка в папке dir: «Новая заметка.md», «Новая заметка 2.md»…
    // Возвращает путь или пустую строку, если создать не удалось
    Q_INVOKABLE QString createNote(const QString &dir);
    // QML сам не умеет спросить файловую систему: jot ~/notes — это папка, а не файл
    Q_INVOKABLE bool isDir(const QString &path) const;

signals:
    void rootChanged();
    void expandedChanged();
    void countChanged();

private:
    struct Node {
        QString path;
        QString name;
        bool dir = false;
        int depth = 0;
    };

    QList<Node> children(const QString &dir, int depth) const;
    // Строки поддерева папки dir — для вставки после неё
    QList<Node> subtree(const QString &dir, int depth) const;
    void rebuild();
    void rewatch();

    QString m_root;
    QList<Node> m_rows;
    QSet<QString> m_expanded;
    QFileSystemWatcher m_watcher;
    QTimer m_rebuildTimer;
};
