#pragma once

#include "ClientUpdateTask.h"
#include <QHash>
#include <QLockFile>
#include <QPointer>
#include <memory>
#include <vector>

class BaseInstance;
class QMenu;
class QAction;
namespace NutMod {
class ClientUpdateService : public QObject {
    Q_OBJECT
public:
    static ClientUpdateService& instance();
    static QString waitingMessage();
    void automatic(QWidget* widget, std::function<void(bool)> finished, bool repair = false);
    bool busy() const { return !m_task.isNull() || bool(m_automaticOwner); }
    bool reserved(BaseInstance* instance) const;
    QString launchBlockReason(BaseInstance* instance) const;
    bool reserveLaunch(BaseInstance* instance, Task* owner, QString& error);
    void releasePreparation(QObject* owner);
    bool recordGame(QObject* owner, qint64 pid, QString& error);
    shared_qobject_ptr<ClientUpdateTask> createTask(const QString& root, Task* owner, QWidget* widget, bool repair = false);
    void addActions(QMenu* menu, QAction* before, QWidget* widget, std::function<BaseInstance*()> selected);
    QString currentStatus() const { return m_status; }
signals:
    void changed();
private:
    struct Lease {
        BaseInstance* instance = nullptr;
        std::unique_ptr<QLockFile> instanceLock, gameLock;
        std::shared_ptr<QLockFile> rootLock;
        std::vector<std::unique_ptr<QLockFile>> updateLocks;
    };
    QHash<QObject*, std::shared_ptr<Lease>> m_leases;
    QPointer<QObject> m_preparing;
    QPointer<ClientUpdateTask> m_task;
    shared_qobject_ptr<ClientUpdateTask> m_automaticOwner, m_automaticTask;
    QString m_status;
    bool m_startupRequested = false, m_updateReady = false;
    std::weak_ptr<QLockFile> m_rootLock;
    std::shared_ptr<QLockFile> rootLock();
    bool reserveUpdate(Task* owner, QString& error);
    bool reserve(BaseInstance* instance, Task* owner, QString& error);
    void release(QObject* owner);
    void manual(QWidget* widget);
};
}  // namespace NutMod
