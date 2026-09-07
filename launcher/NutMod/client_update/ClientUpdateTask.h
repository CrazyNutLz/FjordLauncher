#pragma once

#include "net/NetJob.h"
#include "tasks/Task.h"
#include <QFutureWatcher>
#include <QPointer>
#include <atomic>

class QWidget;
namespace NutMod {
class ClientUpdateTask : public Task {
    Q_OBJECT
public:
    ClientUpdateTask(QString root, QString manifestUrl, QWidget* parentWidget, bool repair = false, QNetworkAccessManager* network = nullptr);
    bool abort() override;
    int changedFiles() const { return m_changed; }
    bool completionPromptRequired() const { return m_confirmed && wasSuccessful() && m_changed > 0; }
protected:
    void executeTask() override;
private:
    QString m_root, m_url;
    QPointer<QWidget> m_widget;
    bool m_repair;
    bool m_confirmed = false;
    std::atomic_bool m_canceled{ false };
    std::atomic_bool m_committing{ false };
    std::atomic_bool m_forced{ false };
    int m_changed = 0;
    qint64 m_downloadedBytes = 0;
    QFutureWatcher<QString> m_worker;
    NetJob::Ptr m_job;
    QNetworkAccessManager* m_network;
    void download(const QString& url, const QString& path, const QString& hash, qint64 limit);
    void report(const QString& status);
};
}  // namespace NutMod
