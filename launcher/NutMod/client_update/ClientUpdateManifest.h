#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <optional>

namespace NutMod {
struct ClientFile {
    QString path, sha256, url;
    bool seed = false;
    std::optional<bool> silent;
    std::optional<bool> force;
    std::optional<QString> notice;
};
struct ClientArchive : ClientFile {
    QString source;
};
struct ClientRemoval {
    QString dir, name, sha256;
    bool contains = false;
    bool recursive = false;
    std::optional<bool> silent;
    std::optional<bool> force;
    std::optional<QString> notice;
};
struct ClientManifest {
    bool silent = true;
    QString notice;
    QList<ClientFile> files;
    QList<ClientArchive> archives;
    bool force = false;
    QList<ClientRemoval> removals;
};

// Throw QString on invalid input. Paths use Windows semantics on all platforms.
QString clientRelativePath(QString path, bool rootAllowed = false);
QString clientManagedPath(QString path, bool rootAllowed = false);
bool clientValidHash(const QString& hash);
ClientManifest parseClientManifest(const QByteArray& data);
}  // namespace NutMod
