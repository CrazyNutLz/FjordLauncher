#include <QTest>
#include <QApplication>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLibraryInfo>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QTimer>
#include <QSignalSpy>
#include <QProcess>
#include <QLockFile>
#include <QThread>
#include <QElapsedTimer>
#include "NutMod/client_update/ClientUpdateEngine.h"
#include "NutMod/client_update/ClientUpdateTask.h"
#include "NutMod/client_update/ClientUpdateService.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/INISettingsObject.h"
#include <archive.h>
#include <archive_entry.h>

using namespace NutMod;
namespace {
QString digest(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
void put(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
        throw QStringLiteral("fixture write failed");
}
QByteArray get(const QString& path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
QByteArray zip(const QList<QPair<QString, QByteArray>>& entries)
{
    QTemporaryDir dir;
    const auto path = dir.filePath("test.zip");
    auto* writer = archive_write_new();
    archive_write_set_format_zip(writer);
    if (archive_write_open_filename(writer, path.toUtf8().constData()) != ARCHIVE_OK)
        throw QStringLiteral("zip fixture open failed");
    for (const auto& entry : entries) {
        auto* item = archive_entry_new();
        archive_entry_set_pathname(item, entry.first.toUtf8().constData());
        archive_entry_set_size(item, entry.second.size());
        archive_entry_set_filetype(item, AE_IFREG);
        archive_entry_set_perm(item, 0644);
        archive_write_header(writer, item);
        archive_write_data(writer, entry.second.constData(), entry.second.size());
        archive_entry_free(item);
    }
    archive_write_close(writer);
    archive_write_free(writer);
    return get(path);
}
struct Fixture {
    QTemporaryDir dir;
    std::atomic_bool canceled{ false };
    QMap<QString, QByteArray> remote;
    int downloads = 0, confirmations = 0;
    bool accept = true;
    QString summary;
    ClientUpdateEngine engine;
    Fixture() : engine(dir.path(), [this](const QString& url, const QString& path, const QString&, qint64) {
        downloads++;
        if (!remote.contains(url)) throw QStringLiteral("network failed");
        put(path, remote.value(url));
    }, [this](const QString& text) { summary = text; confirmations++; return accept; }, [](const QString&) {}, canceled) {}
    ClientFile file(QString path, QByteArray content, bool seed = false) {
        const auto url = "https://example.test/" + path;
        remote[url] = content;
        return { path, digest(content), url, seed };
    }
};
class MemoryReply : public QNetworkReply {
public:
    MemoryReply(QNetworkRequest request, QByteArray body, int delay, QObject* parent, QString redirect = {}) : QNetworkReply(parent), m_body(std::move(body))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        if (!redirect.isEmpty()) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 302);
            setHeader(QNetworkRequest::LocationHeader, QUrl(redirect));
        }
        setHeader(QNetworkRequest::ContentLengthHeader, m_body.size());
        open(QIODevice::ReadOnly);
        QTimer::singleShot(delay, this, [this] {
            if (isFinished()) return;
            emit readyRead();
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(OperationCanceledError, "Canceled");
        setFinished(true);
        emit errorOccurred(OperationCanceledError);
        emit finished();
    }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* destination, qint64 max) override {
        const auto n = qMin(max, static_cast<qint64>(m_body.size()) - m_offset);
        if (n <= 0) return -1;
        memcpy(destination, m_body.constData() + m_offset, static_cast<size_t>(n));
        m_offset += n;
        return n;
    }
private:
    QByteArray m_body;
    qint64 m_offset = 0;
};
class MemoryNetwork : public QNetworkAccessManager {
public:
    QMap<QString, QByteArray> replies;
    QMap<QString, QString> redirects;
    int requests = 0, delay = 1;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        ++requests;
        return new MemoryReply(request, replies.value(request.url().toString()), delay, this, redirects.value(request.url().toString()));
    }
};
class LeaseOwner : public Task {
public:
    void complete() { emitSucceeded(); }
protected:
    void executeTask() override {}
};
}  // namespace

class ClientUpdateTest : public QObject {
    Q_OBJECT
private slots:
    void downloadProtocols_data()
    {
        QTest::addColumn<QString>("initial");
        QTest::addColumn<QString>("destination");
        QTest::addColumn<bool>("wrongHash");
        QTest::addColumn<bool>("success");
        QTest::newRow("http-direct") << "http://alist.test/file" << "" << false << true;
        QTest::newRow("http-to-https") << "http://alist.test/file" << "https://storage.test/file" << false << true;
        QTest::newRow("https-to-http") << "https://alist.test/file" << "http://storage.test/file" << false << true;
        QTest::newRow("http-to-http") << "http://alist.test/file" << "http://storage.test/file" << false << true;
        QTest::newRow("http-still-checks-hash") << "http://alist.test/file" << "" << true << false;
        QTest::newRow("reject-ftp-redirect") << "http://alist.test/file" << "ftp://storage.test/file" << false << false;
    }
    void downloadProtocols()
    {
        QFETCH(QString, initial);
        QFETCH(QString, destination);
        QFETCH(bool, wrongHash);
        QFETCH(bool, success);
        QTemporaryDir dir;
        MemoryNetwork net;
        QJsonObject file{ { "path", "config/a" }, { "sha256", digest("new") }, { "url", initial } };
        net.replies["https://example.test/manifest"] = QJsonDocument(QJsonObject{ { "files", QJsonArray{ file } } }).toJson();
        net.replies[destination.isEmpty() ? initial : destination] = wrongHash ? "wrong" : "new";
        if (!destination.isEmpty())
            net.redirects[initial] = destination;
        ClientUpdateTask task(dir.path(), "https://example.test/manifest", nullptr, false, &net);
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QCOMPARE(task.wasSuccessful(), success);
        if (success)
            QCOMPARE(get(dir.filePath("config/a")), QByteArray("new"));
        else
            QVERIFY(!QFileInfo::exists(dir.filePath("config/a")));
    }
    void completionPrompt_data()
    {
        QTest::addColumn<bool>("silent");
        QTest::addColumn<bool>("unchanged");
        QTest::addColumn<bool>("accept");
        QTest::addColumn<bool>("badDownload");
        QTest::addColumn<bool>("completion");
        QTest::newRow("confirmed-success") << false << false << true << false << true;
        QTest::newRow("silent-success") << true << false << true << false << false;
        QTest::newRow("no-change") << false << true << true << false << false;
        QTest::newRow("canceled") << false << false << false << false << false;
        QTest::newRow("confirmed-failure") << false << false << true << true << false;
    }
    void completionPrompt()
    {
        QFETCH(bool, silent);
        QFETCH(bool, unchanged);
        QFETCH(bool, accept);
        QFETCH(bool, badDownload);
        QFETCH(bool, completion);
        QTemporaryDir dir;
        MemoryNetwork net;
        QWidget parent;
        const QJsonObject file{ { "path", "config/a" }, { "sha256", digest("new") }, { "url", "http://alist.test/file" }, { "silent", silent } };
        net.replies["https://example.test/manifest"] = QJsonDocument(QJsonObject{ { "files", QJsonArray{file} } }).toJson();
        net.replies["http://alist.test/file"] = badDownload ? "bad" : "new";
        if (unchanged)
            put(dir.filePath("config/a"), "new");
        int prompts = 0;
        QTimer click;
        connect(&click, &QTimer::timeout, this, [&] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                auto* box = qobject_cast<QMessageBox*>(widget);
                if (box && box->isVisible()) {
                    ++prompts;
                    box->button(accept ? QMessageBox::Ok : QMessageBox::Cancel)->click();
                }
            }
        });
        click.start(10);
        ClientUpdateTask task(dir.path(), "https://example.test/manifest", &parent, false, &net);
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QCOMPARE(task.completionPromptRequired(), completion);
        QCOMPARE(prompts, silent || unchanged ? 0 : 1);
    }
    void taskNetworkAndCancel()
    {
        QTemporaryDir dir;
        MemoryNetwork net;
        const auto hash = digest("new");
        const QJsonObject file{ { "path", "config/a" }, { "sha256", hash }, { "url", "https://example.test/file?sign=secret" } };
        net.replies["https://example.test/manifest"] = QJsonDocument(QJsonObject{ { "files", QJsonArray{ file } } }).toJson();
        net.replies["https://example.test/file?sign=secret"] = "new";
        ClientUpdateTask task(dir.path(), "https://example.test/manifest", nullptr, false, &net);
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QVERIFY2(task.wasSuccessful(), qPrintable(task.failReason()));
        QCOMPARE(get(dir.filePath("config/a")), QByteArray("new"));
        QCOMPARE(net.requests, 2);
        net.delay = 1000;
        ClientUpdateTask canceled(dir.path(), "https://example.test/manifest", nullptr, false, &net);
        QSignalSpy stopped(&canceled, &Task::finished);
        canceled.start();
        QTRY_VERIFY_WITH_TIMEOUT(net.requests >= 3, 5000);
        QVERIFY(canceled.abort());
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 10000);
        QVERIFY(!canceled.wasSuccessful());
        QCOMPARE(get(dir.filePath("config/a")), QByteArray("new"));
    }
    void launchGateAndCrossProcessLock()
    {
        QTemporaryDir dir;
        INISettingsObject global(dir.filePath("global.cfg"));
        for (const auto& key : QStringList{ "ShowGameTime", "RecordGameTime", "PreLaunchCommand", "WrapperCommand", "PostExitCommand", "ShowConsole", "AutoCloseConsole",
                                           "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop" })
            global.registerSetting(key, false);
        MinecraftInstance mc(&global, std::make_unique<INISettingsObject>(dir.filePath("instance.cfg")), dir.path());
        LeaseOwner owner;
        owner.start();
        auto& service = ClientUpdateService::instance();
        QString error;
        QVERIFY2(service.reserveLaunch(&mc, &owner, error), qPrintable(error));
        auto task = service.createTask(mc.gameRoot(), &owner, nullptr);
        QVERIFY(task);
        for (const auto& status : QStringList{ "download", "extract", "replace", "delete", "rollback" }) {
            task->setStatus(status);
            QVERIFY(service.busy());
            QCOMPARE(service.launchBlockReason(&mc), ClientUpdateService::waitingMessage());
        }
        // A second process uses the same on-disk lease, not a separate UI flag.
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), { "--probe-lock", QDir(mc.gameRoot()).filePath(".nutmod-session.lock") });
        QVERIFY(child.waitForFinished(5000));
        QCOMPARE(child.exitCode(), 42);
        task.reset();
        QTRY_VERIFY(!service.busy());
        QVERIFY2(service.recordGame(&owner, QCoreApplication::applicationPid(), error), qPrintable(error));
        owner.complete();
        QVERIFY(service.launchBlockReason(&mc).isEmpty());
        child.start(QCoreApplication::applicationFilePath(), { "--probe-lock", QDir(mc.gameRoot()).filePath(".nutmod-session.lock") });
        QVERIFY(child.waitForFinished(5000));
        QCOMPARE(child.exitCode(), 0);
        LeaseOwner secondOwner;
        secondOwner.start();
        QVERIFY(!service.reserveLaunch(&mc, &secondOwner, error));
        QVERIFY(error.contains(QStringLiteral("仍在运行")));
        QVERIFY(QFile::remove(QDir(mc.gameRoot()).filePath(".nutmod-game.json")));
        QVERIFY2(service.reserveLaunch(&mc, &secondOwner, error), qPrintable(error));
        secondOwner.complete();
    }
    void protocol()
    {
        auto manifest = parseClientManifest(R"({"silent":true,"files":[],"remove":[],"archives":[]})");
        QVERIFY(manifest.silent);
        const QList<QByteArray> bad{
            R"({"updatefile":[]})", R"({"silent":"true","files":[]})", "{}", R"({"files":{}})",
            R"({"remove":[{"dir":"mods","match":"contains","name":""}]})",
            R"({"archives":[{"path":".","sha256":"bad","url":"https://example.test/a"}]})"
        };
        for (const auto& data : bad)
            QVERIFY_THROWS_EXCEPTION(QString, parseClientManifest(data));
        const QStringList paths{ "../config/a", "C:/config/a", "/mods/a", "mods/../a", "mods/a:stream", "mods/CON.jar", "mods/a.", "mods/a ", "mods\\a", "mods//a", ".nutmod-update/journal.json" };
        for (const auto& path : paths)
            QVERIFY_THROWS_EXCEPTION(QString, clientManagedPath(path));
        QCOMPARE(clientManagedPath("config/OpenBlocks.cfg"), QString("config/OpenBlocks.cfg"));
    }
    void filesAndRepair()
    {
        Fixture f;
        ClientManifest m;
        m.files = { f.file("config/a.cfg", "new"), f.file("config/user.cfg", "default", true) };
        put(f.dir.filePath("config/a.cfg"), "old");
        put(f.dir.filePath("config/user.cfg"), "custom");
        QCOMPARE(f.engine.apply(m, "manifest"), 1);
        QCOMPARE(get(f.dir.filePath("config/a.cfg")), QByteArray("new"));
        QCOMPARE(get(f.dir.filePath("config/user.cfg")), QByteArray("custom"));
        QCOMPARE(f.confirmations, 0);
        QCOMPARE(f.engine.apply(m, "manifest"), 0);
        QCOMPARE(f.downloads, 1);
        put(f.dir.filePath("config/a.cfg"), "damaged");
        QCOMPARE(f.engine.apply(m, "manifest"), 1);
        QCOMPARE(f.downloads, 1); // verified content cache repairs without network
        QFile::remove(f.dir.filePath("config/user.cfg"));
        QCOMPARE(f.engine.apply(m, "manifest"), 1);
        QCOMPARE(get(f.dir.filePath("config/user.cfg")), QByteArray("default"));
    }
    void launcherRootPaths()
    {
        Fixture f;
        const QString target = "instances/GT_New_Horizons_2.8.0_Java_17-25/.minecraft/config/defaultserverlist.cfg";
        const QString other = "instances/Other/.minecraft/config/defaultserverlist.cfg";
        put(f.dir.filePath(other), "keep");
        put(f.dir.filePath("launcher-assets/old-banner.png"), "old");
        ClientManifest m;
        m.files = { f.file(target, "server"), f.file("server-info.txt", "info") };
        ClientArchive archive;
        static_cast<ClientFile&>(archive) = f.file(".", zip({ { "welcome.txt", "welcome" }, { "launcher-assets/banner.png", "banner" } }));
        m.archives = { archive };
        m.removals = { { "launcher-assets", "old-banner", {}, true, false } };
        QCOMPARE(f.engine.apply(m, "root-manifest"), 5);
        QCOMPARE(get(f.dir.filePath(target)), QByteArray("server"));
        QCOMPARE(get(f.dir.filePath(other)), QByteArray("keep"));
        QCOMPARE(get(f.dir.filePath("server-info.txt")), QByteArray("info"));
        QCOMPARE(get(f.dir.filePath("welcome.txt")), QByteArray("welcome"));
        QVERIFY(!QFileInfo::exists(f.dir.filePath("launcher-assets/old-banner.png")));
        QCOMPARE(f.engine.apply(m, "root-manifest"), 0);
        m.removals = { { ".", "server-info.txt", {}, false, false } };
        m.files.clear();
        QCOMPARE(f.engine.apply(m, "root-manifest"), 1);
        QVERIFY(!QFileInfo::exists(f.dir.filePath("server-info.txt")));
        const QJsonObject file{ { "path", target }, { "sha256", digest("server") }, { "url", "http://alist.test/file" } };
        QCOMPARE(parseClientManifest(QJsonDocument(QJsonObject{ { "files", QJsonArray{file} } }).toJson()).files[0].path, target);
        for (const auto& path : QStringList{ ".nutmod-update/journal.json", "instances/Test/.minecraft/.nutmod-game.json", ".nutmod-session.lock", "../outside" })
            QVERIFY_THROWS_EXCEPTION(QString, clientManagedPath(path));
    }
    void deletionRules()
    {
        Fixture f;
        put(f.dir.filePath("mods/Example-1.jar"), "old");
        put(f.dir.filePath("mods/Example-2.jar"), "current");
        put(f.dir.filePath("mods/sub/Example-0.jar"), "older");
        put(f.dir.filePath("mods/keep.jar"), "custom");
        ClientManifest m;
        m.files = { f.file("mods/Example-2.jar", "current") };
        m.removals = { { "mods", "example", {}, true, false }, { "mods", "KEEP.jar", digest("other"), false, false } };
        QCOMPARE(f.engine.apply(m, "m"), 1);
        QVERIFY(!QFileInfo::exists(f.dir.filePath("mods/Example-1.jar")));
        QVERIFY(QFileInfo::exists(f.dir.filePath("mods/Example-2.jar")));
        QVERIFY(QFileInfo::exists(f.dir.filePath("mods/sub/Example-0.jar")));
        QVERIFY(QFileInfo::exists(f.dir.filePath("mods/keep.jar")));
        m.removals[0].recursive = true;
        m.removals[1].sha256.clear();
        QCOMPARE(f.engine.apply(m, "m2"), 2);
        QCOMPARE(f.engine.apply(m, "m2"), 0);
    }
    void failureDoesNotDelete()
    {
        Fixture f;
        put(f.dir.filePath("mods/old.jar"), "old");
        ClientManifest m;
        m.files = { f.file("mods/new.jar", "new") };
        m.removals = { { "mods", "old.jar", {}, false, false } };
        f.remote.clear();
        QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        QCOMPARE(get(f.dir.filePath("mods/old.jar")), QByteArray("old"));
        f.remote[m.files[0].url] = "wrong hash";
        QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        QCOMPARE(get(f.dir.filePath("mods/old.jar")), QByteArray("old"));
    }
    void confirmationAndCancellation()
    {
        Fixture f;
        ClientManifest m;
        m.silent = false;
        m.files = { f.file("config/a", "new") };
        f.accept = false;
        QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        QCOMPARE(f.confirmations, 1);
        QCOMPARE(f.downloads, 0);
        f.accept = true;
        f.canceled = true;
        QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        QVERIFY(!QFileInfo::exists(f.dir.filePath("config/a")));
    }
    void perEntrySilent()
    {
        Fixture f;
        ClientManifest m;
        m.silent = true;
        auto quiet = f.file("config/quiet", "q");
        auto visible = f.file("config/visible", "v");
        visible.silent = false;
        m.files = { quiet, visible };
        QCOMPARE(f.engine.apply(m, "mixed"), 2);
        QCOMPARE(f.confirmations, 1);
        QVERIFY(f.summary.contains("config/visible"));
        QVERIFY(f.summary.contains("config/quiet"));
        // An unchanged non-silent target must not make a silent repair prompt.
        put(f.dir.filePath("config/quiet"), "broken");
        QCOMPARE(f.engine.apply(m, "mixed"), 1);
        QCOMPARE(f.confirmations, 1);
        m.silent = false;
        m.files[0].silent = true;
        put(f.dir.filePath("config/quiet"), "broken");
        QCOMPARE(f.engine.apply(m, "mixed"), 1);
        QCOMPARE(f.confirmations, 1);
        ClientArchive a;
        static_cast<ClientFile&>(a) = f.file(".", zip({ { "config/zip-visible", "z" } }));
        a.silent = false;
        m.silent = true;
        m.archives = { a };
        QCOMPARE(f.engine.apply(m, "zip"), 1);
        QCOMPARE(f.confirmations, 2);
        put(f.dir.filePath("config/zip-visible"), "broken");
        QCOMPARE(f.engine.apply(m, "zip"), 1);
        QCOMPARE(f.confirmations, 3);
        QVERIFY(f.summary.contains("config/zip-visible"));
        m.archives[0].silent = true;
        put(f.dir.filePath("config/zip-visible"), "broken");
        QCOMPARE(f.engine.apply(m, "zip"), 1);
        QCOMPARE(f.confirmations, 3);
        put(f.dir.filePath("config/delete-visible"), "old");
        ClientRemoval r{ "config", "delete-visible", {}, false, false };
        r.silent = false;
        m.removals = { r };
        QCOMPARE(f.engine.apply(m, "delete"), 1);
        QCOMPARE(f.confirmations, 4);
        QVERIFY(f.summary.contains("config/delete-visible"));
        auto parsed = parseClientManifest(R"({"force":true,"silent":false,"files":[],"remove":[{"dir":"config","match":"exact","name":"old","silent":true}]})");
        QVERIFY(parsed.force);
        QVERIFY(parsed.removals[0].silent.value());
        QVERIFY_THROWS_EXCEPTION(QString, parseClientManifest(R"({"force":"true","files":[]})"));
        QVERIFY_THROWS_EXCEPTION(QString, parseClientManifest(R"({"files":[],"remove":[{"dir":"config","match":"exact","name":"old","silent":1}]})"));
    }
    void forcedDownloadCannotCancel()
    {
        QTemporaryDir dir;
        MemoryNetwork net;
        net.delay = 250;
        const QJsonObject file{ { "path", "config/a" }, { "sha256", digest("new") }, { "url", "http://alist.test/file" }, { "silent", true } };
        net.replies["https://example.test/manifest"] = QJsonDocument(QJsonObject{ { "force", true }, { "silent", false }, { "files", QJsonArray{file} } }).toJson();
        net.replies["http://alist.test/file"] = "new";
        ClientUpdateTask task(dir.path(), "https://example.test/manifest", nullptr, false, &net);
        QSignalSpy done(&task, &Task::finished);
        task.start();
        QTRY_COMPARE_WITH_TIMEOUT(net.requests, 2, 5000);
        QVERIFY(!task.canAbort());
        QVERIFY(!task.abort());
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
        QVERIFY2(task.wasSuccessful(), qPrintable(task.failReason()));
        QCOMPARE(get(dir.filePath("config/a")), QByteArray("new"));
    }
    void cumulativeUpdates()
    {
        Fixture f;
        ClientManifest m;
        m.silent = true;
        bool forced = false;
        f.engine.planReady = [&](bool value) { forced = value; };
        for (int i = 0; i < 10; ++i) {
            auto file = f.file(QString("config/update-%1.cfg").arg(i), QByteArray::number(i));
            file.notice = QString("Update %1 completed").arg(i);
            file.silent = false;
            file.force = i == 0;
            m.files.append(file);
        }
        QCOMPARE(f.engine.apply(m, "cumulative"), 10);
        QVERIFY(forced);
        QCOMPARE(f.downloads, 10);
        QCOMPARE(f.confirmations, 1);
        for (int i = 0; i < 10; ++i)
            QVERIFY(f.summary.contains(QString("Update %1 completed").arg(i)));
        QCOMPARE(f.engine.apply(m, "cumulative"), 0);
        QVERIFY(!forced);
        QCOMPARE(f.downloads, 10);
        QCOMPARE(f.confirmations, 1);
        // Already-applied forced history must not force unrelated later repairs.
        put(f.dir.filePath("config/update-8.cfg"), "outdated");
        put(f.dir.filePath("config/update-9.cfg"), "outdated");
        m.files[8].notice = QString("Shared update notice");
        m.files[9].notice = QString("Shared update notice");
        QCOMPARE(f.engine.apply(m, "cumulative"), 2);
        QVERIFY(!forced);
        QVERIFY(!f.summary.contains("Update 0 completed"));
        QVERIFY(!f.summary.contains("config/update-0.cfg"));
        QCOMPARE(f.summary.count("Shared update notice"), 1);
        const auto state = QJsonDocument::fromJson(get(f.dir.filePath(".nutmod-update/state.json"))).object();
        QCOMPARE(state.value("notice").toString(), f.summary);
        // Per-entry false overrides global force, and an unmatched deletion stays out.
        m.force = true;
        put(f.dir.filePath("config/update-9.cfg"), "outdated");
        ClientRemoval absent{ "config", "absent", {}, false, false };
        absent.force = true;
        absent.notice = QString("Do not show absent removal");
        m.removals = { absent };
        QCOMPARE(f.engine.apply(m, "cumulative"), 1);
        QVERIFY(!forced);
        QVERIFY(!f.summary.contains("Do not show absent removal"));
    }
    void cumulativeZipAndRemovalMetadata()
    {
        Fixture f;
        ClientManifest m;
        bool forced = false;
        f.engine.planReady = [&](bool value) { forced = value; };
        ClientArchive a;
        static_cast<ClientFile&>(a) = f.file(".", zip({ { "config/fromzip", "z" } }));
        a.silent = false;
        a.force = true;
        a.notice = QString("Zip notice");
        m.archives = { a };
        QCOMPARE(f.engine.apply(m, "zip"), 1);
        QVERIFY(forced);
        QVERIFY(f.summary.contains("Zip notice"));
        m.archives[0].force = false;
        m.archives[0].notice = QString("New zip explanation");
        put(f.dir.filePath("config/fromzip"), "bad");
        QCOMPARE(f.engine.apply(m, "zip"), 1);
        QVERIFY(!forced);
        QVERIFY(f.summary.contains("New zip explanation"));
        QVERIFY(!f.summary.contains("Zip notice"));
        ClientRemoval r{ "config", "old", {}, false, false };
        r.force = true;
        r.silent = false;
        r.notice = QString("Delete old file");
        m.removals = { r };
        put(f.dir.filePath("config/old"), "old");
        QCOMPARE(f.engine.apply(m, "delete"), 1);
        QVERIFY(forced);
        QVERIFY(f.summary.contains("Delete old file"));
        QVERIFY(!f.summary.contains("New zip explanation"));
        auto parsed = parseClientManifest(R"({"files":[],"remove":[{"dir":"config","match":"exact","name":"old","force":false,"notice":"Cleanup"}]})");
        QVERIFY(!parsed.removals[0].force.value());
        QCOMPARE(parsed.removals[0].notice.value(), QString("Cleanup"));
        QVERIFY_THROWS_EXCEPTION(QString, parseClientManifest(R"({"files":[],"remove":[{"dir":"config","match":"exact","name":"old","notice":false}]})"));
    }
    void zipRepairAndProtectedTargets()
    {
        Fixture f;
        const auto bytes = zip({ { "client/config/a.cfg", "a" }, { "client/mods/Example-2.jar", "v2" } });
        ClientArchive a;
        static_cast<ClientFile&>(a) = f.file(".", bytes);
        a.source = "client/";
        ClientManifest m;
        m.archives = { a };
        m.removals = { { "mods", "Example", {}, true, false } };
        put(f.dir.filePath("mods/Example-1.jar"), "v1");
        QCOMPARE(f.engine.apply(m, "m"), 3);
        QCOMPARE(get(f.dir.filePath("config/a.cfg")), QByteArray("a"));
        QCOMPARE(get(f.dir.filePath("mods/Example-2.jar")), QByteArray("v2"));
        QCOMPARE(f.engine.apply(m, "m"), 0);
        QCOMPARE(f.downloads, 1);
        put(f.dir.filePath("config/a.cfg"), "broken");
        QCOMPARE(f.engine.apply(m, "m"), 1);
        QCOMPARE(f.downloads, 1);
        // Index loss rebuilds from the authenticated archive, not game files.
        QDir(f.dir.filePath(".nutmod-update/indexes")).removeRecursively();
        QCOMPARE(f.engine.apply(m, "m"), 0);
        QCOMPARE(f.downloads, 1);
    }
    void zipInvalidPathsAndConflicts()
    {
        for (const auto& name : QStringList{ "../outside", "client/../../outside", ".nutmod-update/journal.json" }) {
            Fixture f;
            ClientArchive a;
            static_cast<ClientFile&>(a) = f.file(".", zip({ { name, "bad" } }));
            ClientManifest m;
            m.archives = { a };
            QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        }
        Fixture f;
        ClientArchive a;
        static_cast<ClientFile&>(a) = f.file(".", zip({ { "config/A", "one" } }));
        ClientManifest m;
        m.archives = { a };
        m.files = { f.file("config/a", "two") };
        QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
        QVERIFY(!QFileInfo::exists(f.dir.filePath("config/a")));
    }
    void rollbackEveryMutation()
    {
        for (int fault = 1; fault <= 4; ++fault) {
            Fixture f;
            put(f.dir.filePath("config/a"), "old");
            put(f.dir.filePath("mods/old"), "remove");
            ClientManifest m;
            m.files = { f.file("config/a", "new"), f.file("config/b", "added") };
            m.removals = { { "mods", "old", {}, false, false } };
            f.engine.afterMutation = [fault](int n) { if (n == fault) throw QStringLiteral("injected write failure"); };
            QVERIFY_THROWS_EXCEPTION(QString, f.engine.apply(m, "m"));
            QCOMPARE(get(f.dir.filePath("config/a")), QByteArray("old"));
            QCOMPARE(get(f.dir.filePath("mods/old")), QByteArray("remove"));
            QVERIFY(!QFileInfo::exists(f.dir.filePath("config/b")));
            f.engine.recover();
        }
    }
    void recoverInterruptedTransaction()
    {
        for (int fault = 1; fault <= 4; ++fault) {
            Fixture f;
            put(f.dir.filePath("config/a"), "old");
            put(f.dir.filePath("mods/old"), "remove");
            ClientManifest m;
            m.files = { f.file("config/a", "new"), f.file("config/b", "added") };
            m.removals = { { "mods", "old", {}, false, false } };
            f.engine.afterMutation = [fault](int n) { if (n == fault) throw 42; };
            QVERIFY_THROWS_EXCEPTION(int, f.engine.apply(m, "m"));
            ClientUpdateEngine recovery(f.dir.path(), {}, {}, [](const QString&) {}, f.canceled);
            recovery.recover();
            QCOMPARE(get(f.dir.filePath("config/a")), QByteArray("old"));
            QCOMPARE(get(f.dir.filePath("mods/old")), QByteArray("remove"));
            QVERIFY(!QFileInfo::exists(f.dir.filePath("config/b")));
            recovery.recover();
        }
    }
};
int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication::addLibraryPath(QLibraryInfo::path(QLibraryInfo::PluginsPath));
    QApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments()[1] == "--probe-lock") {
        QLockFile lock(app.arguments()[2]);
        lock.setStaleLockTime(0);
        return lock.tryLock(0) ? 0 : 42;
    }
    ClientUpdateTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ClientUpdate_test.moc"
