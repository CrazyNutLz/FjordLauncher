#pragma once

#include "ClientUpdateManifest.h"
#include <QJsonObject>
#include <atomic>
#include <functional>

namespace NutMod {
// Disk work runs off the UI thread. Network/UI callbacks are supplied by ClientUpdateTask.
class ClientUpdateEngine {
public:
    using Download = std::function<void(const QString&, const QString&, const QString&, qint64)>;
    using Confirm = std::function<bool(const QString&)>;
    using Status = std::function<void(const QString&)>;
    ClientUpdateEngine(QString gameRoot, Download download, Confirm confirm, Status status, std::atomic_bool& canceled);
    void recover();
    int apply(const ClientManifest& manifest, const QString& manifestHash);
    static QString hashFile(const QString& path);
    static void checkNoLinks(const QString& absolutePath);
    static QString safePath(const QString& root, const QString& relative);
    // Exposed for deterministic fault injection in isolated tests, never configured by the API.
    std::function<void(int)> afterMutation;
    std::function<void()> beforeCommit;
    // Called after comparing actual changes, before confirmation and installation.
    std::function<void(bool)> planReady;
private:
    struct Target { ClientFile file; QString staged; };
    QString m_root, m_work;
    Download m_download;
    Confirm m_confirm;
    Status m_status;
    std::atomic_bool& m_canceled;
    qint64 m_expandedBytes = 0;
    void checkCancel() const;
    QString obtain(const ClientFile& file, qint64 limit);
    QList<Target> unpack(const ClientArchive& entry);
    void save(const QString& path, const QJsonObject& object);
};
}  // namespace NutMod
