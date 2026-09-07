#include "ClientUpdateService.h"

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "ClientUpdateEngine.h"
#include "NutMod/config/NutModConfig.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QSaveFile>
#include <QSet>
#include <limits>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace NutMod {
namespace {
QString processIdentity(qint64 pid)
{
#ifdef Q_OS_WIN
    if (pid <= 0 || pid > std::numeric_limits<DWORD>::max())
        throw QStringLiteral("无效游戏进程记录");
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        if (GetLastError() == ERROR_INVALID_PARAMETER)
            return {};
        throw QStringLiteral("无法确认原游戏进程是否已退出，请先关闭游戏后重试。");
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    const bool readable = GetProcessTimes(process, &created, &exited, &kernel, &user);
    CloseHandle(process);
    if (!alive)
        return {};
    if (!readable)
        throw QStringLiteral("无法读取游戏进程启动时间");
    return QString::number((static_cast<quint64>(created.dwHighDateTime) << 32) | created.dwLowDateTime);
#else
    Q_UNUSED(pid);
    return {};
#endif
}
void checkRecordedGame(const QString& root)
{
    const auto path = ClientUpdateEngine::safePath(root, ".nutmod-game.json");
    if (!QFileInfo::exists(path))
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4096)
        throw QStringLiteral("无法读取游戏进程记录");
    const auto record = QJsonDocument::fromJson(file.readAll()).object();
    const auto pid = record.value("pid").toInteger();
    const auto created = record.value("created").toString();
    if (created.isEmpty())
        throw QStringLiteral("游戏进程记录损坏，请先关闭游戏再移除 .nutmod-game.json。");
    if (processIdentity(pid) == created)
        throw QStringLiteral("该实例的游戏仍在运行（可能来自上次启动器进程），请先关闭游戏再更新或启动。");
    file.close();
    if (!QFile::remove(path))
        throw QStringLiteral("无法清理已退出游戏的进程记录");
}
}  // namespace
ClientUpdateService& ClientUpdateService::instance()
{
    static ClientUpdateService service;
    return service;
}
QString ClientUpdateService::waitingMessage()
{
    return QStringLiteral("客户端正在更新，请在更新完成后再启动游戏。");
}
bool ClientUpdateService::reserved(BaseInstance* instance) const
{
    for (const auto& lease : m_leases)
        if (lease->instance == instance)
            return true;
    return false;
}
QString ClientUpdateService::launchBlockReason(BaseInstance* instance) const
{
    if (busy())
        return waitingMessage();
    if (m_startupRequested && !m_updateReady)
        return QStringLiteral("客户端更新尚未完成，请通过帮助菜单检查客户端更新/修复，完成后再启动游戏。");
    if (m_preparing)
        return QStringLiteral("正在准备启动游戏，请稍候。");
    if (reserved(instance))
        return QStringLiteral("该实例正在启动或运行，请勿重复启动。");
    return {};
}
bool ClientUpdateService::reserve(BaseInstance* instance, Task* owner, QString& error)
{
    if (!(error = launchBlockReason(instance)).isEmpty())
        return false;
    auto* mc = dynamic_cast<MinecraftInstance*>(instance);
    if (!mc) {
        error = QStringLiteral("请选择 Minecraft 实例");
        return false;
    }
    try {
        ClientUpdateEngine::checkNoLinks(mc->instanceRoot());
        ClientUpdateEngine::checkNoLinks(mc->gameRoot());
        if (!QDir().mkpath(mc->gameRoot()))
            throw QStringLiteral("无法创建游戏目录");
        auto lease = std::make_shared<Lease>();
        lease->instance = instance;
        lease->rootLock = rootLock();
        lease->instanceLock = std::make_unique<QLockFile>(ClientUpdateEngine::safePath(mc->instanceRoot(), ".nutmod-session.lock"));
        lease->gameLock = std::make_unique<QLockFile>(ClientUpdateEngine::safePath(mc->gameRoot(), ".nutmod-session.lock"));
        lease->instanceLock->setStaleLockTime(0);
        lease->gameLock->setStaleLockTime(0);
        if (!lease->instanceLock->tryLock(0) || !lease->gameLock->tryLock(0))
            throw QStringLiteral("其他启动器正在使用或更新该实例，请在更新完成并关闭游戏后重试。");
        checkRecordedGame(mc->gameRoot());
        m_leases.insert(owner, lease);
        connect(owner, &Task::finished, this, [this, owner] { release(owner); });
        connect(owner, &QObject::destroyed, this, [this, owner] { release(owner); });
        return true;
    } catch (const QString& problem) {
        error = problem;
        return false;
    }
}
bool ClientUpdateService::reserveLaunch(BaseInstance* instance, Task* owner, QString& error)
{
    if (!reserve(instance, owner, error))
        return false;
    m_preparing = owner;
    return true;
}
void ClientUpdateService::releasePreparation(QObject* owner)
{
    if (m_preparing == owner) {
        m_preparing.clear();
        emit changed();
    }
}
bool ClientUpdateService::recordGame(QObject* owner, qint64 pid, QString& error)
{
    try {
        if (!m_leases.contains(owner))
            throw QStringLiteral("游戏启动操作锁已失效");
        auto* mc = dynamic_cast<MinecraftInstance*>(m_leases.value(owner)->instance);
        const auto identity = processIdentity(pid);
        if (identity.isEmpty())
            throw QStringLiteral("游戏进程尚未准备好");
        QSaveFile file(ClientUpdateEngine::safePath(mc->gameRoot(), ".nutmod-game.json"));
        const auto data = QJsonDocument(QJsonObject{ { "pid", pid }, { "created", identity } }).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            throw QStringLiteral("无法保存游戏进程记录，已取消启动以保护客户端文件。");
        return true;
    } catch (const QString& problem) {
        error = problem;
        return false;
    }
}
void ClientUpdateService::release(QObject* owner)
{
    releasePreparation(owner);
    m_leases.remove(owner);
}
shared_qobject_ptr<ClientUpdateTask> ClientUpdateService::createTask(const QString& root, Task* owner, QWidget* widget, bool repair)
{
    if (!m_leases.contains(owner) || busy())
        return shared_qobject_ptr<ClientUpdateTask>();
    auto task = makeShared<ClientUpdateTask>(root, Config::ClientUpdateManifestUrl, widget, repair);
    m_task = task.get();
    m_status = waitingMessage();
    connect(task.get(), &Task::abortStatusChanged, this, [this] { emit changed(); });
    auto phase = std::make_shared<QString>(QStringLiteral("检查客户端更新"));
    connect(task.get(), &Task::status, this, [this, phase](const QString& status) { *phase = status; m_status = status; emit changed(); });
    connect(task.get(), &Task::progress, this, [this, phase](qint64 current, qint64 total) {
        m_status = total > 0 ? QStringLiteral("下载 %1%（%2 / %3 KiB） · %4").arg(current * 100 / total).arg(current / 1024).arg(total / 1024).arg(*phase)
                            : QStringLiteral("已下载 %1 KiB · %2").arg(current / 1024).arg(*phase);
        emit changed();
    });
    connect(task.get(), &Task::finished, this, [this, result = task.get()] {
        m_task.clear();
        m_status = result->wasSuccessful()
                       ? (result->changedFiles() > 0 ? QStringLiteral("更新完成 · %1 个文件 · 可启动游戏").arg(result->changedFiles())
                                                     : QStringLiteral("客户端已是最新 · 可启动游戏"))
                       : QStringLiteral("客户端更新未完成，请检查更新/修复后再启动游戏。") + result->failReason();
        emit changed();
    });
    emit changed();
    return task;
}
std::shared_ptr<QLockFile> ClientUpdateService::rootLock()
{
    // Tests without Application still exercise the per-instance locks.
    if (!APPLICATION_DYN)
        return {};
    if (auto lock = m_rootLock.lock())
        return lock;
    auto lock = std::make_shared<QLockFile>(ClientUpdateEngine::safePath(APPLICATION->dataRoot(), ".nutmod-session.lock"));
    lock->setStaleLockTime(0);
    if (!lock->tryLock(0))
        throw QStringLiteral("其他启动器正在使用此客户端目录，请关闭游戏并等待更新完成后重试。");
    m_rootLock = lock;
    return lock;
}
bool ClientUpdateService::reserveUpdate(Task* owner, QString& error)
{
    if (busy() || m_preparing || !m_leases.isEmpty() || !APPLICATION->updatesAreAllowed() || APPLICATION->launcherUpdateRunning()) {
        error = QStringLiteral("请关闭游戏并等待当前操作完成后再更新客户端。");
        return false;
    }
    try {
        auto lease = std::make_shared<Lease>();
        lease->rootLock = rootLock();
        QSet<QString> locked;
        for (int i = 0; i < APPLICATION->instances()->count(); ++i) {
            auto* mc = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->at(i));
            if (!mc)
                continue;
            if (mc->isRunning())
                throw QStringLiteral("请先关闭所有游戏再更新客户端。");
            for (const auto& directory : { mc->instanceRoot(), mc->gameRoot() }) {
                if (!QFileInfo::exists(directory))
                    continue;
                const auto path = ClientUpdateEngine::safePath(directory, ".nutmod-session.lock");
                if (locked.contains(path.toCaseFolded()))
                    continue;
                auto lock = std::make_unique<QLockFile>(path);
                lock->setStaleLockTime(0);
                if (!lock->tryLock(0))
                    throw QStringLiteral("其他启动器正在使用实例 %1，请先关闭游戏。").arg(mc->name());
                locked.insert(path.toCaseFolded());
                lease->updateLocks.push_back(std::move(lock));
            }
            checkRecordedGame(mc->gameRoot());
        }
        m_leases.insert(owner, lease);
        connect(owner, &QObject::destroyed, this, [this, owner] { release(owner); });
        return true;
    } catch (const QString& problem) {
        error = problem;
        return false;
    }
}
void ClientUpdateService::manual(QWidget* widget)
{
    automatic(widget, [](bool) {}, true);
}
void ClientUpdateService::automatic(QWidget* widget, std::function<void(bool)> finished, bool repair)
{
    auto owner = makeShared<ClientUpdateTask>(QString(), QString(), widget);
    QString error;
    if (!reserveUpdate(owner.get(), error)) {
        m_startupRequested = true;
        if (!busy()) {
            m_status = QStringLiteral("客户端更新未完成：") + error;
            emit changed();
        }
        QMessageBox::warning(widget, QStringLiteral("客户端更新失败"), error);
        finished(false);
        return;
    }
    m_startupRequested = true;
    m_updateReady = false;
    APPLICATION->clientUpdateStarted();
    m_automaticTask = createTask(APPLICATION->dataRoot(), owner.get(), widget, repair);
    m_automaticOwner = owner;
    connect(m_automaticTask.get(), &Task::finished, this, [this, widget = QPointer<QWidget>(widget), finished] {
        const bool success = m_automaticTask->wasSuccessful();
        const auto error = m_automaticTask->failReason();
        const bool showCompletion = m_automaticTask->completionPromptRequired();
        const int changedFiles = m_automaticTask->changedFiles();
        m_updateReady = success;
        release(m_automaticOwner.get());
        m_automaticTask.reset();
        m_automaticOwner.reset();
        APPLICATION->clientUpdateFinished();
        emit changed();
        if (!success)
            QMessageBox::warning(widget, QStringLiteral("客户端更新失败"), error + QStringLiteral("\n请通过“检查客户端更新/修复”重试，完成后再启动游戏。"));
        else if (showCompletion)
            QMessageBox::information(widget, QStringLiteral("客户端更新完成"),
                                     QStringLiteral("本次已更新 %1 个文件，现在可以启动游戏。").arg(changedFiles));
        finished(success);
    }, Qt::QueuedConnection);
    m_automaticTask->start();
}
void ClientUpdateService::addActions(QMenu* menu, QAction* before, QWidget* widget, std::function<BaseInstance*()>)
{
    auto* updateAction = new QAction(QStringLiteral("检查客户端更新/修复"), menu);
    auto* launcherUpdateAction = new QAction(QStringLiteral("检查启动器更新"), menu);
    auto* cancelAction = new QAction(QStringLiteral("取消客户端更新"), menu);
    cancelAction->setEnabled(m_task && m_task->canAbort());
    connect(this, &ClientUpdateService::changed, cancelAction, [this, cancelAction] {
        cancelAction->setEnabled(m_task && m_task->canAbort());
    });
    menu->insertAction(before, updateAction);
    menu->insertAction(before, launcherUpdateAction);
    menu->insertAction(before, cancelAction);
    connect(cancelAction, &QAction::triggered, widget, [this, widget] {
        if (m_task && !m_task->abort())
            QMessageBox::information(widget, QStringLiteral("客户端更新"), QStringLiteral("正在安装或恢复文件，请等待完成。"));
    });
    connect(updateAction, &QAction::triggered, widget, [this, widget] { manual(widget); });
    connect(launcherUpdateAction, &QAction::triggered, widget, [widget] {
        if (!APPLICATION->updatesAreAllowed() || APPLICATION->launcherUpdateRunning()) {
            QMessageBox::information(widget, QStringLiteral("启动器更新"), QStringLiteral("请关闭游戏并等待当前更新完成后再检查启动器更新。"));
            return;
        }
        if (APPLICATION->updaterEnabled())
            APPLICATION->triggerUpdateCheck();
        else
            QMessageBox::information(widget, QStringLiteral("启动器更新"), QStringLiteral("当前启动器不支持自动更新。"));
    });
}
}  // namespace NutMod
