#include "Platform.h"

#include <QDir>
#include <QFileInfo>

Platform::Platform(QObject *parent)
    : QObject(parent)
{
}

QString Platform::os()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString Platform::prettyPath(const QString &path)
{
    if (path.isEmpty())
        return path;
    const QString home = QDir::homePath();
    QString p = path;
    if (p == home || p.startsWith(home + QLatin1Char('/')))
        p = QLatin1Char('~') + p.mid(home.size());
    return QDir::toNativeSeparators(p);
}

QString Platform::localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : QString();
}

QUrl Platform::fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(path.isEmpty() ? QDir::homePath() : path);
}

bool Platform::isDir(const QString &path)
{
    return QFileInfo(path).isDir();
}
