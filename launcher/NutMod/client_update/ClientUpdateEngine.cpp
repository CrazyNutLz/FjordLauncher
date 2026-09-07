#include "ClientUpdateEngine.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStorageInfo>
#include <QUuid>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace NutMod {
namespace {
constexpr qint64 FileLimit = 2LL * 1024 * 1024 * 1024;
constexpr qint64 ExpandedLimit = 20LL * 1024 * 1024 * 1024;
QJsonObject readObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        throw QStringLiteral("无法读取更新状态：%1").arg(path);
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        throw QStringLiteral("更新状态损坏：%1").arg(path);
    return doc.object();
}
void mkdirFor(const QString& path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        throw QStringLiteral("无法创建更新目录：%1").arg(path);
}
void removeTree(const QString& path)
{
    ClientUpdateEngine::checkNoLinks(path);
    if (!QFileInfo::exists(path))
        return;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext())
        ClientUpdateEngine::checkNoLinks(it.next());
    if (!QDir(path).removeRecursively())
        throw QStringLiteral("无法清理更新暂存目录：%1").arg(path);
}
void renameFile(const QString& from, const QString& to)
{
    mkdirFor(to);
    if (!QFile::rename(from, to))
        throw QStringLiteral("无法移动文件（可能被占用）：%1").arg(from);
}
}  // namespace

ClientUpdateEngine::ClientUpdateEngine(QString root, Download download, Confirm confirm, Status status, std::atomic_bool& canceled)
    : m_root(QDir(root).absolutePath()), m_work(QDir(m_root).filePath(".nutmod-update")),
      m_download(std::move(download)), m_confirm(std::move(confirm)), m_status(std::move(status)), m_canceled(canceled)
{
    checkNoLinks(m_root);
    checkNoLinks(m_work);
}

void ClientUpdateEngine::checkNoLinks(const QString& path)
{
    QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    while (true) {
        if (info.isSymLink() || info.isJunction())
            throw QStringLiteral("更新路径包含链接：%1").arg(info.filePath());
#ifdef Q_OS_WIN
        const auto native = QDir::toNativeSeparators(info.filePath());
        const DWORD attr = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
            throw QStringLiteral("更新路径包含重解析点：%1").arg(info.filePath());
#endif
        const auto parent = info.dir().absolutePath();
        if (parent == info.absoluteFilePath())
            break;
        info.setFile(parent);
    }
}

QString ClientUpdateEngine::safePath(const QString& root, const QString& relative)
{
    clientRelativePath(relative);
    const auto path = QDir(root).absoluteFilePath(relative);
    checkNoLinks(path);
    return path;
}

QString ClientUpdateEngine::hashFile(const QString& path)
{
    checkNoLinks(path);
    QFileInfo info(path);
    if (!info.exists())
        return {};
    if (!info.isFile())
        throw QStringLiteral("目标不是普通文件：%1").arg(path);
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file))
        throw QStringLiteral("无法校验文件：%1").arg(path);
    return QString::fromLatin1(hash.result().toHex());
}

void ClientUpdateEngine::save(const QString& path, const QJsonObject& object)
{
    checkNoLinks(path);
    mkdirFor(path);
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw QStringLiteral("无法保存更新状态：%1").arg(path);
}

void ClientUpdateEngine::checkCancel() const
{
    if (m_canceled.load())
        throw QStringLiteral("已取消客户端更新");
}

QString ClientUpdateEngine::obtain(const ClientFile& file, qint64 limit)
{
    checkCancel();
    const auto path = safePath(m_work, "cache/" + file.sha256);
    if (hashFile(path) != file.sha256) {
        mkdirFor(path);
        m_status(QStringLiteral("下载客户端文件：%1").arg(file.path));
        m_download(file.url, path, file.sha256, limit);
        checkCancel();
        if (hashFile(path) != file.sha256)
            throw QStringLiteral("下载文件 SHA-256 校验失败：%1").arg(file.path);
    }
    return path;
}

QList<ClientUpdateEngine::Target> ClientUpdateEngine::unpack(const ClientArchive& entry)
{
    const auto identity = entry.sha256 + '\n' + entry.path + '\n' + entry.source + (entry.seed ? "\nseed" : "\nmanaged");
    const auto key = QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
    const auto indexPath = safePath(m_work, "indexes/" + key + ".json");
    if (QFileInfo::exists(indexPath)) {
        try {
            const auto index = readObject(indexPath);
            QList<Target> targets;
            bool repair = false;
            if (index.value("identity").toString() != identity || !index.value("files").isArray() || index.value("files").toArray().isEmpty())
                throw QStringLiteral("索引无效");
            for (const auto& value : index.value("files").toArray()) {
                auto obj = value.toObject();
                ClientFile file{ clientManagedPath(obj.value("path").toString()), obj.value("sha256").toString(), {}, entry.seed };
                file.silent = entry.silent;
                file.force = entry.force;
                file.notice = entry.notice;
                if (!clientValidHash(file.sha256) || targets.size() >= 10000)
                    throw QStringLiteral("索引无效");
                const auto local = hashFile(safePath(m_root, file.path));
                repair |= local.isEmpty() || (!file.seed && local != file.sha256);
                targets.append({ file, {} });
            }
            if (!repair)
                return targets;
        } catch (const QString&) {
            // Rebuild malformed/missing indexes from the authenticated ZIP, never from local game files.
        }
    }
    const auto zip = obtain(entry, FileLimit);
    const auto expanded = safePath(m_work, "expanded/" + key);
    removeTree(expanded);
    std::unique_ptr<archive, decltype(&archive_read_free)> reader(archive_read_new(), archive_read_free);
    archive_read_support_format_zip(reader.get());
#ifdef Q_OS_WIN
    const int opened = archive_read_open_filename_w(reader.get(), reinterpret_cast<const wchar_t*>(zip.utf16()), 65536);
#else
    const int opened = archive_read_open_filename(reader.get(), zip.toUtf8().constData(), 65536);
#endif
    if (opened != ARCHIVE_OK)
        throw QStringLiteral("无法打开 ZIP 更新包");
    QList<Target> targets;
    QJsonArray index;
    QSet<QString> seen;
    int count = 0;
    archive_entry* item = nullptr;
    int result = ARCHIVE_OK;
    while ((result = archive_read_next_header(reader.get(), &item)) == ARCHIVE_OK) {
        checkCancel();
        if (++count > 10000 || archive_entry_is_encrypted(item) || archive_entry_symlink(item) || archive_entry_hardlink(item))
            throw QStringLiteral("ZIP 包含不支持的条目、链接或过多文件");
        const auto* rawName = archive_entry_pathname_utf8(item);
        if (!rawName)
            throw QStringLiteral("ZIP 文件名不是有效 UTF-8");
        QString name = QString::fromUtf8(rawName);
        const bool directory = archive_entry_filetype(item) == AE_IFDIR;
        if (directory && name.endsWith('/'))
            name.chop(1);
        clientRelativePath(name);
        if (!directory && archive_entry_filetype(item) != AE_IFREG)
            throw QStringLiteral("ZIP 只允许普通文件和目录");
        const auto folded = name.toCaseFolded();
        if (seen.contains(folded))
            throw QStringLiteral("ZIP 中存在重复路径：%1").arg(name);
        seen.insert(folded);
        if (directory || (!entry.source.isEmpty() && !name.startsWith(entry.source))) {
            if (archive_read_data_skip(reader.get()) != ARCHIVE_OK)
                throw QStringLiteral("ZIP 内容损坏");
            continue;
        }
        name = name.mid(entry.source.size());
        const auto relative = clientManagedPath(entry.path == "." ? name : entry.path + '/' + name);
        m_status(QStringLiteral("解压 ZIP：") + relative);
        const auto destination = safePath(expanded, relative);
        mkdirFor(destination);
        QFile out(destination);
        if (!out.open(QIODevice::WriteOnly))
            throw QStringLiteral("无法暂存 ZIP 文件：%1").arg(relative);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 size = 0;
        char buffer[65536];
        la_ssize_t n;
        while ((n = archive_read_data(reader.get(), buffer, sizeof(buffer))) > 0) {
            checkCancel();
            size += n;
            m_expandedBytes += n;
            if (size > FileLimit || m_expandedBytes > ExpandedLimit || out.write(buffer, n) != n)
                throw QStringLiteral("ZIP 超过大小限制或磁盘空间不足");
            hash.addData(QByteArrayView(buffer, n));
        }
        if (n < 0 || !out.flush())
            throw QStringLiteral("ZIP 内容损坏或写入失败");
        out.close();
        ClientFile file{ relative, QString::fromLatin1(hash.result().toHex()), {}, entry.seed };
        file.silent = entry.silent;
        file.force = entry.force;
        file.notice = entry.notice;
        targets.append({ file, destination });
        index.append(QJsonObject{ { "path", relative }, { "sha256", file.sha256 } });
    }
    if (result != ARCHIVE_EOF || targets.isEmpty())
        throw QStringLiteral("ZIP 损坏，或 source 目录不存在/没有文件");
    save(indexPath, { { "identity", identity }, { "files", index } });
    return targets;
}

void ClientUpdateEngine::recover()
{
    const auto journalPath = safePath(m_work, "journal.json");
    if (!QFileInfo::exists(journalPath))
        return;
    auto journal = readObject(journalPath);
    if (journal.value("status").toString() == "committed")
        return;
    if (journal.value("status").toString() != "pending" || !journal.value("operations").isArray())
        throw QStringLiteral("客户端更新日志损坏，请保留备份并联系维护者");
    const auto id = clientRelativePath(journal.value("id").toString());
    if (id.contains('/'))
        throw QStringLiteral("无效事务标识");
    m_status(QStringLiteral("恢复上次未完成的客户端更新…"));
    const auto operations = journal.value("operations").toArray();
    for (auto it = operations.end(); it != operations.begin();) {
        const auto op = (*--it).toObject();
        const auto relative = clientManagedPath(op.value("path").toString());
        const auto target = safePath(m_root, relative);
        const auto backup = safePath(m_work, "transactions/" + id + "/backup/" + relative);
        const auto oldHash = op.value("old").toString();
        const auto newHash = op.value("new").toString();
        if ((!oldHash.isEmpty() && !clientValidHash(oldHash)) || (!newHash.isEmpty() && !clientValidHash(newHash)))
            throw QStringLiteral("恢复日志中的哈希无效");
        const auto current = hashFile(target);
        const auto saved = hashFile(backup);
        if (!saved.isEmpty()) {
            if (saved != oldHash || (!current.isEmpty() && current != newHash && current != oldHash))
                throw QStringLiteral("恢复时发现外部修改，请保留备份：%1").arg(relative);
            if (!current.isEmpty() && !QFile::remove(target))
                throw QStringLiteral("无法恢复被占用文件：%1").arg(relative);
            renameFile(backup, target);
        } else if (oldHash.isEmpty()) {
            if (!current.isEmpty() && (current != newHash || !QFile::remove(target)))
                throw QStringLiteral("无法撤销新增文件：%1").arg(relative);
        } else if (current != oldHash) {
            throw QStringLiteral("恢复备份缺失：%1").arg(relative);
        }
    }
    if (!QFile::remove(journalPath))
        throw QStringLiteral("无法清除已恢复事务日志");
}

int ClientUpdateEngine::apply(const ClientManifest& manifest, const QString& manifestHash)
{
    recover();
    checkCancel();
    QList<Target> targets;
    for (const auto& file : manifest.files)
        targets.append({ file, {} });
    for (const auto& entry : manifest.archives) {
        m_status(QStringLiteral("检查 ZIP：%1").arg(entry.path));
        targets.append(unpack(entry));
    }
    QSet<QString> keep;
    for (const auto& target : targets) {
        const auto folded = target.file.path.toCaseFolded();
        if (keep.contains(folded))
            throw QStringLiteral("多个更新条目写入同一路径：%1").arg(target.file.path);
        keep.insert(folded);
    }
    for (const auto& path : keep) {
        auto parent = path;
        while (parent.contains('/')) {
            parent = parent.left(parent.lastIndexOf('/'));
            if (keep.contains(parent))
                throw QStringLiteral("更新目标存在文件/目录冲突：%1").arg(path);
        }
    }
    QJsonArray operations;
    QSet<QString> visible;
    bool forced = false;
    QStringList notices;
    const auto includeRule = [&](const auto& rule) {
        forced |= rule.force.value_or(manifest.force);
        const auto notice = rule.notice.value_or(manifest.notice).trimmed();
        if (!notice.isEmpty() && !notices.contains(notice))
            notices.append(notice);
    };
    QMap<QString, QString> sources;
    for (const auto& target : targets) {
        checkCancel();
        m_status(QStringLiteral("校验客户端：%1").arg(target.file.path));
        const auto old = hashFile(safePath(m_root, target.file.path));
        if (old == target.file.sha256 || (target.file.seed && !old.isEmpty()))
            continue;
        operations.append(QJsonObject{ { "path", target.file.path }, { "old", old }, { "new", target.file.sha256 } });
        sources.insert(target.file.path, target.staged);
        includeRule(target.file);
        if (!target.file.silent.value_or(manifest.silent))
            visible.insert(target.file.path.toCaseFolded());
    }
    QSet<QString> removed;
    for (const auto& rule : manifest.removals) {
        const auto directory = rule.dir == "." ? m_root : safePath(m_root, rule.dir);
        if (!QFileInfo::exists(directory))
            continue;
        if (!QFileInfo(directory).isDir())
            throw QStringLiteral("删除规则的 dir 不是目录：%1").arg(rule.dir);
        QDirIterator it(directory, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        rule.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            checkCancel();
            const auto absolute = it.next();
            checkNoLinks(absolute);
            if (!it.fileInfo().isFile())
                continue;
            const auto name = it.fileName();
            if (!(rule.contains ? name.contains(rule.name, Qt::CaseInsensitive) : name.compare(rule.name, Qt::CaseInsensitive) == 0))
                continue;
            const auto relative = clientManagedPath(QDir(m_root).relativeFilePath(absolute));
            const auto folded = relative.toCaseFolded();
            if (keep.contains(folded)) {
                m_status(QStringLiteral("保留清单目标：%1").arg(relative));
                continue;
            }
            const auto old = hashFile(absolute);
            if (!rule.sha256.isEmpty() && old != rule.sha256) {
                m_status(QStringLiteral("跳过哈希不符的删除目标：%1").arg(relative));
                continue;
            }
            if (!rule.silent.value_or(manifest.silent))
                visible.insert(folded);
            includeRule(rule);
            if (removed.contains(folded))
                continue;
            removed.insert(folded);
            operations.append(QJsonObject{ { "path", relative }, { "old", old }, { "new", "" } });
        }
    }
    if (planReady)
        planReady(forced);
    checkCancel();
    if (operations.isEmpty())
        return 0;
    if (operations.size() > 10000)
        throw QStringLiteral("更新操作超过 10000 个文件");
    QString summary = notices.join('\n') + QStringLiteral("\n\n本次安装/替换 %1 个文件，删除 %2 个文件。\n覆盖和删除前会自动备份。\n")
                                             .arg(sources.size()).arg(removed.size());
    if (forced)
        summary += QStringLiteral("本次包含必需更新，请确认后完成更新再启动游戏。\n");
    for (const auto& value : operations) {
        const auto op = value.toObject();
        summary += (op.value("new").toString().isEmpty() ? QStringLiteral("删除 ") : QStringLiteral("安装 ")) + op.value("path").toString() + '\n';
    }
    if (!visible.isEmpty() && !m_confirm(summary))
        throw QStringLiteral("已取消客户端更新");
    for (const auto& target : targets) {
        if (sources.contains(target.file.path) && sources.value(target.file.path).isEmpty())
            sources[target.file.path] = obtain(target.file, FileLimit);
    }
    checkCancel();
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto transaction = safePath(m_work, "transactions/" + id);
    qint64 bytes = 0;
    for (auto it = sources.begin(); it != sources.end(); ++it)
        bytes += QFileInfo(it.value()).size();
    if (bytes > ExpandedLimit || (QStorageInfo(m_root).bytesAvailable() >= 0 && QStorageInfo(m_root).bytesAvailable() < bytes + 16 * 1024 * 1024))
        throw QStringLiteral("更新空间不足或总文件大小超过限制");
    // Copy all new content onto the same volume before touching any installed file.
    for (const auto& value : operations) {
        const auto op = value.toObject();
        const auto relative = op.value("path").toString();
        if (hashFile(safePath(m_root, relative)) != op.value("old").toString())
            throw QStringLiteral("文件在更新检查后发生变化，请重试：%1").arg(relative);
        if (sources.contains(relative)) {
            const auto staged = safePath(transaction, "new/" + relative);
            mkdirFor(staged);
            if (!QFile::copy(sources.value(relative), staged) || hashFile(staged) != op.value("new").toString())
                throw QStringLiteral("无法准备更新文件：%1").arg(relative);
        }
    }
    checkCancel();
    const auto journalPath = safePath(m_work, "journal.json");
    QString previous;
    if (QFileInfo::exists(journalPath))
        previous = readObject(journalPath).value("id").toString();
    QJsonObject journal{ { "id", id }, { "status", "pending" }, { "operations", operations } };
    save(journalPath, journal);
    if (beforeCommit)
        beforeCommit();
    checkCancel();
    m_status(QStringLiteral("正在安装客户端更新，请勿关闭启动器…"));
    try {
        int mutations = 0;
        int completed = 0;
        for (const auto& value : operations) {
            const auto op = value.toObject();
            const auto relative = op.value("path").toString();
            const auto target = safePath(m_root, relative);
            if (hashFile(target) != op.value("old").toString())
                throw QStringLiteral("文件在提交前发生变化：%1").arg(relative);
            m_status(QStringLiteral("安装进度 %1/%2 · ").arg(completed).arg(operations.size()) +
                     (op.value("new").toString().isEmpty() ? QStringLiteral("删除：") : QStringLiteral("替换：")) + relative);
            if (!op.value("old").toString().isEmpty()) {
                renameFile(target, safePath(transaction, "backup/" + relative));
                if (afterMutation)
                    afterMutation(++mutations);
            }
            if (!op.value("new").toString().isEmpty()) {
                renameFile(safePath(transaction, "new/" + relative), target);
                if (afterMutation)
                    afterMutation(++mutations);
                if (hashFile(target) != op.value("new").toString())
                    throw QStringLiteral("安装后校验失败：%1").arg(relative);
            }
            ++completed;
            m_status(QStringLiteral("已完成 %1/%2 个文件").arg(completed).arg(operations.size()));
        }
        save(safePath(m_work, "state.json"), { { "manifest", manifestHash }, { "notice", summary }, { "transaction", id } });
        journal["status"] = "committed";
        save(journalPath, journal);
    } catch (const QString& error) {
        try {
            recover();
        } catch (const QString& recovery) {
            throw error + QStringLiteral("\n客户端更新未完成，请先修复后再启动游戏。\n") + recovery;
        }
        throw error + QStringLiteral("\n已恢复更新前的文件。");
    }
    // Cleanup is best effort after commit. Never turn a successful commit into rollback.
    try {
        if (!previous.isEmpty() && previous != id && !previous.contains('/'))
            removeTree(safePath(m_work, "transactions/" + clientRelativePath(previous)));
        removeTree(safePath(m_work, "expanded"));
    } catch (const QString& error) {
        m_status(error);
    }
    return static_cast<int>(operations.size());
}
}  // namespace NutMod
