#include "ClientUpdateTask.h"

#include "Application.h"
#include "ClientUpdateEngine.h"
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QSemaphore>
#include <QTimer>
#include <QtConcurrent>

namespace NutMod {
namespace {
class UpdateConfirmation final : public QMessageBox {
public:
    UpdateConfirmation(const QString& summary, bool forced, QWidget* parent)
        : QMessageBox(QMessageBox::Question, QStringLiteral("客户端更新"), summary.left(6000),
                      forced ? QMessageBox::Ok : QMessageBox::Ok | QMessageBox::Cancel, parent), m_forced(forced)
    {
        setTextFormat(Qt::PlainText);
        if (summary.size() > 6000) {
            setText(summary.left(6000) + QStringLiteral("\n……请展开详细信息查看完整列表。"));
            setDetailedText(summary);
        }
        if (forced)
            setWindowFlag(Qt::WindowCloseButtonHint, false);
    }
    void reject() override { if (!m_forced) QMessageBox::reject(); }
protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (m_forced && event->key() == Qt::Key_Escape) event->ignore(); else QMessageBox::keyPressEvent(event);
    }
    void closeEvent(QCloseEvent* event) override
    {
        if (m_forced) event->ignore(); else QMessageBox::closeEvent(event);
    }
private:
    bool m_forced;
};
class DownloadGuard final : public Net::Validator {
public:
    DownloadGuard(QString expected, qint64 limit, QObject* context, std::function<void()> stop)
        : m_expected(std::move(expected)), m_limit(limit), m_context(context), m_stop(std::move(stop)) {}
    bool init(QNetworkRequest& request) override
    {
        m_size = 0;
        m_hash.reset();
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setRawHeader("Cache-Control", "no-cache");
        // File/ZIP URLs may use either protocol, including AList redirects. Keep the
        // manifest on HTTPS; it supplies the trusted SHA-256 for these downloads.
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             m_expected.isEmpty() ? QNetworkRequest::NoLessSafeRedirectPolicy : QNetworkRequest::ManualRedirectPolicy);
        return allowedScheme(request.url());
    }
    bool write(QByteArray& data) override
    {
        m_size += data.size();
        if (m_size > m_limit) {
            QMetaObject::invokeMethod(m_context, m_stop, Qt::QueuedConnection);
            return false;
        }
        m_hash.addData(data);
        return true;
    }
    bool abort() override { return true; }
    bool validate(QNetworkReply& reply) override
    {
        return allowedScheme(reply.url()) && reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 &&
               (m_expected.isEmpty() || m_hash.result().toHex() == m_expected.toLatin1());
    }
private:
    bool allowedScheme(const QUrl& url) const
    {
        return url.scheme() == "https" || (!m_expected.isEmpty() && url.scheme() == "http");
    }
    QString m_expected;
    qint64 m_limit, m_size = 0;
    QObject* m_context;
    std::function<void()> m_stop;
    QCryptographicHash m_hash{ QCryptographicHash::Sha256 };
};
}  // namespace

ClientUpdateTask::ClientUpdateTask(QString root, QString url, QWidget* widget, bool repair, QNetworkAccessManager* network)
    : m_root(std::move(root)), m_url(std::move(url)), m_widget(widget), m_repair(repair), m_network(network)
{
    setAbortable(true);
    connect(&m_worker, &QFutureWatcher<QString>::finished, this, [this] {
        m_job.reset();
        const auto error = m_worker.result();
        if (error.isEmpty())
            emitSucceeded();
        else
            emitFailed(error);
    });
}

void ClientUpdateTask::report(const QString& status)
{
    QMetaObject::invokeMethod(this, [this, status] {
        setStatus(status);
        qInfo().noquote() << "[ClientUpdate]" << status;
    }, Qt::QueuedConnection);
}

void ClientUpdateTask::download(const QString& url, const QString& path, const QString& hash, qint64 limit)
{
    struct Result { QSemaphore ready; QString error; bool completed = false; };
    auto result = std::make_shared<Result>();
    QMetaObject::invokeMethod(this, [this, result, url, path, hash, limit] {
        if (m_canceled.load()) {
            result->error = QStringLiteral("已取消客户端更新");
            result->ready.release();
            return;
        }
        auto request = Net::Download::makeFile(QUrl(url), path, Net::NetRequest::Option::RedactUrl);
        request->addValidator(new DownloadGuard(hash, limit, this, [this] {
            if (m_job)
                m_job->abort();
        }));
        m_job = makeShared<NetJob>(QStringLiteral("客户端更新下载"), m_network ? m_network : APPLICATION->network(), 1);
        m_job->setAskRetry(false);
        m_job->setAutoRetryLimit(2);
        m_job->addNetAction(request);
        connect(request.get(), &Task::progress, this, &Task::setProgress);
        connect(request.get(), &Task::details, this, &Task::setDetails);
        auto* timeout = new QTimer(m_job.get());
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, m_job.get(), [this] { if (m_job) m_job->abort(); });
        connect(m_job.get(), &Task::finished, this, [this, result, timeout] {
            if (result->completed)
                return;
            result->completed = true;
            timeout->stop();
            if (!m_job->wasSuccessful())
                result->error = QStringLiteral("客户端文件下载失败、超时或校验不通过，请重试。");
            result->ready.release();
        });
        timeout->start(hash.isEmpty() ? 30000 : 300000);
        m_job->start();
    }, Qt::QueuedConnection);
    result->ready.acquire();
    if (!result->error.isEmpty())
        throw result->error;
    m_downloadedBytes += QFileInfo(path).size();
    if (m_downloadedBytes > 20LL * 1024 * 1024 * 1024)
        throw QStringLiteral("本次客户端下载总量超过 20 GiB");
}

void ClientUpdateTask::executeTask()
{
    setStatus(QStringLiteral("检查客户端更新…"));
    m_worker.setFuture(QtConcurrent::run([this]() -> QString {
        try {
            ClientUpdateEngine engine(m_root,
                [this](const QString& url, const QString& path, const QString& hash, qint64 limit) { download(url, path, hash, limit); },
                [this](const QString& summary) {
                    bool accepted = false;
                    QMetaObject::invokeMethod(this, [this, &accepted, summary] {
                        if (!m_widget || m_canceled.load())
                            return;
                        UpdateConfirmation box(summary, m_forced.load(), m_widget);
                        accepted = box.exec() == QMessageBox::Ok;
                        m_confirmed = accepted;
                    }, Qt::BlockingQueuedConnection);
                    return accepted;
                }, [this](const QString& status) { report(status); }, m_canceled);
            engine.beforeCommit = [this] {
                m_committing.store(true);
                QMetaObject::invokeMethod(this, [this] { setAbortable(false); }, Qt::QueuedConnection);
            };
            engine.planReady = [this](bool forced) {
                QMetaObject::invokeMethod(this, [this, forced] {
                    m_forced.store(forced);
                    setAbortable(!forced);
                }, Qt::BlockingQueuedConnection);
            };
            m_committing.store(true);
            engine.recover();
            m_committing.store(false);
            if (m_canceled.load())
                throw QStringLiteral("已取消客户端更新");
            const auto manifestPath = ClientUpdateEngine::safePath(m_root, ".nutmod-update/manifest.json");
            if (!QDir().mkpath(QFileInfo(manifestPath).absolutePath()))
                throw QStringLiteral("无法创建更新工作目录");
            if (m_repair) {
                const auto indexes = ClientUpdateEngine::safePath(m_root, ".nutmod-update/indexes");
                for (const auto& file : QDir(indexes).entryList({ "*.json" }, QDir::Files)) {
                    const auto index = ClientUpdateEngine::safePath(indexes, file);
                    if (!QFile::remove(index))
                        throw QStringLiteral("无法重建 ZIP 索引");
                }
            }
            download(m_url, manifestPath, {}, 4 * 1024 * 1024);
            QFile file(manifestPath);
            if (!file.open(QIODevice::ReadOnly))
                throw QStringLiteral("无法读取客户端清单");
            const auto bytes = file.readAll();
            const auto manifest = parseClientManifest(bytes);
            if (m_canceled.load())
                throw QStringLiteral("已取消客户端更新");
            m_changed = engine.apply(manifest, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
            report(m_changed ? QStringLiteral("客户端更新完成") : QStringLiteral("客户端文件已是最新"));
            return {};
        } catch (const QString& error) {
            return error;
        } catch (const std::exception& error) {
            return QStringLiteral("客户端更新失败：%1").arg(QString::fromUtf8(error.what()));
        }
    }));
}

bool ClientUpdateTask::abort()
{
    if (m_committing.load() || m_forced.load())
        return false;
    m_canceled.store(true);
    if (m_job && m_job->isRunning())
        m_job->abort();
    return true;  // Completion is emitted only after the worker and network stop.
}
}  // namespace NutMod
