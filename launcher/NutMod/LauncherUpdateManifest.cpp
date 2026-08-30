#include "LauncherUpdateManifest.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

#include "Json.h"

namespace NutMod {
bool parseLauncherUpdateManifest(const QByteArray& response, LauncherUpdateManifest& manifest, QString& error)
{
    try {
        const auto document = Json::requireDocument(response);
        const auto root = Json::requireObject(document);

        const auto schemaVersion = Json::requireInteger(root, "schemaVersion");
        if (schemaVersion != 1) {
            error = QString("Unsupported launcher update manifest schema version: %1").arg(schemaVersion);
            return false;
        }

        manifest.version = Json::requireString(root, "version");
        manifest.title = root["title"].toString();
        manifest.notes = root["notes"].toString();
        manifest.publishedAt = QDateTime::fromString(root["publishedAt"].toString(), Qt::ISODate);
        manifest.mandatory = root["mandatory"].toBool(false);

        const auto artifact = Json::requireObject(root, "artifact");
        manifest.artifact.fileName = Json::requireString(artifact, "fileName");
        manifest.artifact.url = Json::requireString(artifact, "url");
        manifest.artifact.sha256 = Json::requireString(artifact, "sha256").trimmed();

        const QUrl artifactUrl(manifest.artifact.url);
        if (manifest.artifact.url.startsWith('[') || !artifactUrl.isValid() || artifactUrl.isRelative()
            || (artifactUrl.scheme() != "https" && artifactUrl.scheme() != "http")) {
            error = "The launcher update artifact URL must be a plain HTTP or HTTPS URL (Markdown links are not supported).";
            return false;
        }

        static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9A-Fa-f]{64}$"));
        if (!sha256Pattern.match(manifest.artifact.sha256).hasMatch()) {
            error = "The launcher update artifact SHA-256 must contain exactly 64 hexadecimal characters.";
            return false;
        }

        return true;
    } catch (Json::JsonException& exception) {
        error = QString("Failed to parse launcher update manifest: %1\n%2").arg(exception.what(), QString::fromUtf8(response));
        return false;
    }
}
}  // namespace NutMod
