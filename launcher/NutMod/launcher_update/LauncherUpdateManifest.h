#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace NutMod {
struct LauncherUpdateArtifact {
    QString fileName;
    QString url;
    QString sha256;
};

struct LauncherUpdateManifest {
    QString version;
    QString title;
    QString notes;
    QDateTime publishedAt;
    bool mandatory = false;
    LauncherUpdateArtifact artifact;
};

bool parseLauncherUpdateManifest(const QByteArray& response, LauncherUpdateManifest& manifest, QString& error);
}  // namespace NutMod
