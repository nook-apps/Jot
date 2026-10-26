#include "FolderModel.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace {
const QStringList kNoteFilters = {QStringLiteral("*.md"), QStringLiteral("*.markdown"), QStringLiteral("*.mdown"),
                                  QStringLiteral("*.txt")};

// Служебные папки сборок и зависимостей: заметок там нет, а файлов — тысячи
bool skippedDir(const QString &name)
{
    static const QSet<QString> names = {QStringLiteral("node_modules"), QStringLiteral("build"),
                                        QStringLiteral("target"), QStringLiteral("__pycache__"),
                                        QStringLiteral("venv")};
    return names.contains(name);
}

bool isInside(const QString &path, const QString &dir)
{
    return path == dir || path.startsWith(dir + QLatin1Char('/'));
}
}

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Сохранение файла — это несколько событий подряд; перестраиваем, когда утихнет
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(250);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &FolderModel::rebuild);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_rebuildTimer, qOverload<>(&QTimer::start));
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Node &n = m_rows.at(index.row());
    switch (role) {
    case NameRole:
        return n.name;
    case PathRole:
        return n.path;
    case IsDirRole:
        return n.dir;
    case DepthRole:
        return n.depth;
    case ExpandedRole:
        return n.dir && m_expanded.contains(n.path);
    }
    return {};
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {{NameRole, "name"}, {PathRole, "path"}, {IsDirRole, "isDir"}, {DepthRole, "depth"},
            {ExpandedRole, "expanded"}};
}

void FolderModel::setRoot(const QString &rawPath)
{
    const QString path = rawPath.isEmpty() ? QString() : QFileInfo(rawPath).absoluteFilePath();
    if (path == m_root)
        return;
    m_root = path;
    emit rootChanged();
    rebuild();
}

QString FolderModel::rootName() const
{
    if (m_root.isEmpty())
        return {};
    if (m_root == QDir::homePath())
        return QStringLiteral("Домашняя папка");
    return QFileInfo(m_root).fileName();
}

QStringList FolderModel::expanded() const
{
    QStringList list(m_expanded.cbegin(), m_expanded.cend());
    list.sort();
    return list;
}

void FolderModel::setExpanded(const QStringList &paths)
{
    const QSet<QString> set(paths.cbegin(), paths.cend());
    if (set == m_expanded)
        return;
    m_expanded = set;
    emit expandedChanged();
    rebuild();
}

QList<FolderModel::Node> FolderModel::children(const QString &dir, int depth) const
{
    QDir d(dir);
    const QFileInfoList dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QFileInfoList files = d.entryInfoList(kNoteFilters, QDir::Files);
    // Как в Shelf: сначала папки, внутри — по-человечески, «заметка 2» раньше «заметка 10»
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto byName = [&collator](const Node &a, const Node &b) { return collator.compare(a.name, b.name) < 0; };

    QList<Node> dirNodes, fileNodes;
    for (const QFileInfo &fi : dirs)
        if (!skippedDir(fi.fileName()))
            dirNodes.append({fi.absoluteFilePath(), fi.fileName(), true, depth});
    for (const QFileInfo &fi : files)
        fileNodes.append({fi.absoluteFilePath(), fi.fileName(), false, depth});
    std::sort(dirNodes.begin(), dirNodes.end(), byName);
    std::sort(fileNodes.begin(), fileNodes.end(), byName);
    return dirNodes + fileNodes;
}

QList<FolderModel::Node> FolderModel::subtree(const QString &dir, int depth) const
{
    QList<Node> out;
    for (const Node &n : children(dir, depth)) {
        out.append(n);
        if (n.dir && m_expanded.contains(n.path))
            out.append(subtree(n.path, depth + 1));
    }
    return out;
}

void FolderModel::rebuild()
{
    QList<Node> rows;
    if (!m_root.isEmpty() && QFileInfo(m_root).isDir())
        rows = subtree(m_root, 0);
    // Раскрытые папки, которых больше нет, забываем
    const qsizetype before = m_expanded.size();
    for (auto it = m_expanded.begin(); it != m_expanded.end();)
        it = QFileInfo(*it).isDir() ? std::next(it) : m_expanded.erase(it);

    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (m_expanded.size() != before)
        emit expandedChanged();
    emit countChanged();
    rewatch();
}

void FolderModel::rewatch()
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    if (m_root.isEmpty() || !QFileInfo(m_root).isDir())
        return;
    QStringList dirs{m_root};
    for (const QString &d : m_expanded)
        if (isInside(d, m_root))
            dirs.append(d);
    m_watcher.addPaths(dirs);
}

void FolderModel::toggle(int row)
{
    if (row < 0 || row >= m_rows.size() || !m_rows.at(row).dir)
        return;
    const Node node = m_rows.at(row);
    const QModelIndex idx = index(row);
    if (m_expanded.remove(node.path)) {
        // Свернуть: убрать строки поддерева, они идут подряд и глубже папки
        int last = row;
        while (last + 1 < m_rows.size() && m_rows.at(last + 1).depth > node.depth)
            ++last;
        if (last > row) {
            beginRemoveRows({}, row + 1, last);
            m_rows.remove(row + 1, last - row);
            endRemoveRows();
        }
    } else {
        m_expanded.insert(node.path);
        const QList<Node> kids = subtree(node.path, node.depth + 1);
        if (!kids.isEmpty()) {
            beginInsertRows({}, row + 1, row + int(kids.size()));
            for (qsizetype i = 0; i < kids.size(); ++i)
                m_rows.insert(row + 1 + i, kids.at(i));
            endInsertRows();
        }
    }
    emit dataChanged(idx, idx, {ExpandedRole});
    emit expandedChanged();
    emit countChanged();
    rewatch();
}

int FolderModel::indexOf(const QString &path) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).path == path)
            return i;
    return -1;
}

int FolderModel::reveal(const QString &path)
{
    if (m_root.isEmpty() || !isInside(path, m_root) || path == m_root)
        return -1;
    bool changed = false;
    for (QString dir = QFileInfo(path).absolutePath(); isInside(dir, m_root) && dir != m_root;
         dir = QFileInfo(dir).absolutePath()) {
        if (!m_expanded.contains(dir)) {
            m_expanded.insert(dir);
            changed = true;
        }
    }
    if (changed) {
        emit expandedChanged();
        rebuild();
    }
    return indexOf(path);
}

QString FolderModel::createNote(const QString &dir)
{
    const QString base = dir.isEmpty() ? m_root : dir;
    if (base.isEmpty() || !QFileInfo(base).isDir())
        return {};
    QString path = base + QStringLiteral("/Новая заметка.md");
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = base + QStringLiteral("/Новая заметка %1.md").arg(n);
    QFile f(path);
    // NewOnly: файл с этим именем мог появиться между проверкой и созданием
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return {};
    f.close();
    m_rebuildTimer.start();
    return path;
}
