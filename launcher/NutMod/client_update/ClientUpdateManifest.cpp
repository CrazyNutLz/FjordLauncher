#include "ClientUpdateManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace NutMod {
QString clientRelativePath(QString path, bool rootAllowed)
{
    if (rootAllowed && path == QStringLiteral("."))
        return path;
    static const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9¹²³]|lpt[0-9¹²³])(?:\\.|$)"),
                                           QRegularExpression::CaseInsensitiveOption);
    if (path.isEmpty() || path.contains(QRegularExpression(QStringLiteral("[\\\\:*?\"<>|\\x00-\\x1f]"))))
        throw QStringLiteral("无效更新路径：%1").arg(path);
    for (const auto& part : path.split('/')) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') || part.endsWith(' ') || device.match(part).hasMatch())
            throw QStringLiteral("无效更新路径：%1").arg(path);
    }
    return path;
}

QString clientManagedPath(QString path, bool rootAllowed)
{
    path = clientRelativePath(path, rootAllowed);
    if (rootAllowed && path == ".")
        return path;
    for (const auto& part : path.split('/')) {
        if (part.startsWith(".nutmod-", Qt::CaseInsensitive))
            throw QStringLiteral("更新路径不能操作更新器的状态或锁文件：%1").arg(path);
    }
    return path;
}

bool clientValidHash(const QString& hash)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-fA-F0-9]{64}$"));
    return pattern.match(hash).hasMatch();
}

namespace {
void keys(const QJsonObject& obj, const QStringList& allowed)
{
    for (auto it = obj.begin(); it != obj.end(); ++it)
        if (!allowed.contains(it.key()))
            throw QStringLiteral("未知客户端更新字段：%1（仅支持新协议）").arg(it.key());
}
QString string(const QJsonObject& obj, const QString& key, const QString& fallback = {}, bool required = false)
{
    if (!obj.contains(key) && !required)
        return fallback;
    if (!obj.value(key).isString() || (required && obj.value(key).toString().isEmpty()))
        throw QStringLiteral("字段 %1 必须是%2字符串").arg(key, required ? QStringLiteral("非空") : QString());
    return obj.value(key).toString();
}
bool boolean(const QJsonObject& obj, const QString& key, bool fallback)
{
    if (!obj.contains(key))
        return fallback;
    if (!obj.value(key).isBool())
        throw QStringLiteral("字段 %1 必须是 JSON 布尔值").arg(key);
    return obj.value(key).toBool();
}
QJsonArray array(const QJsonObject& obj, const QString& key)
{
    if (!obj.contains(key))
        return {};
    if (!obj.value(key).isArray())
        throw QStringLiteral("字段 %1 必须是数组").arg(key);
    return obj.value(key).toArray();
}
QJsonObject object(const QJsonValue& value)
{
    if (!value.isObject())
        throw QStringLiteral("更新规则必须是对象");
    return value.toObject();
}
ClientFile file(const QJsonObject& obj, bool archive)
{
    ClientFile result;
    result.path = clientManagedPath(string(obj, "path", {}, true), archive);
    if (!archive && !result.path.contains('/'))
        throw QStringLiteral("文件路径必须包含目录和文件名");
    result.sha256 = string(obj, "sha256", {}, true).toLower();
    if (!clientValidHash(result.sha256))
        throw QStringLiteral("sha256 必须是 64 位十六进制值");
    result.url = string(obj, "url", {}, true);
    QUrl url(result.url, QUrl::StrictMode);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment())
        throw QStringLiteral("更新下载地址必须是有效 HTTP 或 HTTPS URL");
    const auto policy = string(obj, "policy", "managed");
    if (policy != "managed" && policy != "seed")
        throw QStringLiteral("policy 仅支持 managed 或 seed");
    result.seed = policy == "seed";
    if (obj.contains("silent"))
        result.silent = boolean(obj, "silent", true);
    if (obj.contains("force"))
        result.force = boolean(obj, "force", false);
    if (obj.contains("notice"))
        result.notice = string(obj, "notice");
    return result;
}
}  // namespace

ClientManifest parseClientManifest(const QByteArray& data)
{
    if (data.size() > 4 * 1024 * 1024)
        throw QStringLiteral("客户端更新清单超过 4 MiB");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        throw QStringLiteral("客户端更新清单不是有效 JSON 对象");
    const auto obj = doc.object();
    keys(obj, { "silent", "force", "notice", "files", "remove", "archives" });
    if (!obj.contains("files") && !obj.contains("remove") && !obj.contains("archives"))
        throw QStringLiteral("客户端更新清单缺少操作数组；请发布新协议清单");
    ClientManifest result;
    result.silent = boolean(obj, "silent", true);
    result.force = boolean(obj, "force", false);
    result.notice = string(obj, "notice");
    for (const auto& value : array(obj, "files")) {
        auto item = object(value);
        keys(item, { "path", "sha256", "url", "policy", "silent", "force", "notice" });
        result.files.append(file(item, false));
    }
    for (const auto& value : array(obj, "archives")) {
        auto item = object(value);
        keys(item, { "path", "sha256", "url", "policy", "source", "silent", "force", "notice" });
        ClientArchive entry;
        static_cast<ClientFile&>(entry) = file(item, true);
        entry.source = string(item, "source");
        if (entry.source == "/")
            throw QStringLiteral("source 必须是包内相对目录，包根请使用空字符串");
        if (entry.source.endsWith('/'))
            entry.source.chop(1);
        if (!entry.source.isEmpty())
            entry.source = clientRelativePath(entry.source) + '/';
        result.archives.append(entry);
    }
    for (const auto& value : array(obj, "remove")) {
        auto item = object(value);
        keys(item, { "dir", "match", "name", "recursive", "sha256", "silent", "force", "notice" });
        ClientRemoval entry;
        entry.dir = clientManagedPath(string(item, "dir", {}, true), true);
        entry.name = string(item, "name", {}, true);
        if (entry.name.trimmed().isEmpty() || entry.name.contains('/') || entry.name.contains('\\') ||
            entry.name.contains(QRegularExpression(QStringLiteral("[\\x00-\\x1f]"))))
            throw QStringLiteral("删除规则 name 必须是非空文件名或名称片段");
        const auto match = string(item, "match", {}, true);
        if (match != "exact" && match != "contains")
            throw QStringLiteral("删除规则 match 仅支持 exact 或 contains");
        entry.contains = match == "contains";
        entry.recursive = boolean(item, "recursive", false);
        if (item.contains("silent"))
            entry.silent = boolean(item, "silent", true);
        if (item.contains("force"))
            entry.force = boolean(item, "force", false);
        if (item.contains("notice"))
            entry.notice = string(item, "notice");
        if (item.contains("sha256")) {
            entry.sha256 = string(item, "sha256", {}, true).toLower();
            if (!clientValidHash(entry.sha256))
                throw QStringLiteral("删除规则 sha256 无效");
        }
        result.removals.append(entry);
    }
    if (result.files.size() + result.archives.size() + result.removals.size() > 10000)
        throw QStringLiteral("客户端更新规则超过 10000 条");
    return result;
}
}  // namespace NutMod
