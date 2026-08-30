#include "NoticeManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace NutMod {
QList<NoticeEntry> parseNoticeManifest(const QByteArray& data, QString& error)
{
    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QStringLiteral("JSON parse error at offset %1: %2").arg(parseError.offset).arg(parseError.errorString());
        return {};
    }
    if (!document.isArray()) {
        error = QStringLiteral("The notice manifest root must be an array.");
        return {};
    }

    QList<NoticeEntry> notices;
    for (const auto& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        const auto object = value.toObject();
        NoticeEntry notice;
        notice.version = object.value(QStringLiteral("version")).toString().trimmed();
        notice.title = object.value(QStringLiteral("title")).toString().trimmed();
        notice.text = object.value(QStringLiteral("notice")).toString();
        notice.showTime = object.value(QStringLiteral("showtime")).toString(QStringLiteral("once")).trimmed().toLower();
        notice.primaryButtonText = object.value(QStringLiteral("btn1")).toString(QStringLiteral("确定")).trimmed();
        notice.secondaryButtonText = object.value(QStringLiteral("btn2")).toString(QStringLiteral("取消")).trimmed();
        notice.action = object.value(QStringLiteral("opt")).toString().trimmed().toLower();
        notice.actionText = object.value(QStringLiteral("opttxt")).toString();

        if (notice.version.isEmpty()) {
            continue;
        }
        if (notice.title.isEmpty()) {
            notice.title = QStringLiteral("服务器公告");
        }
        if (notice.showTime != QStringLiteral("always") && notice.showTime != QStringLiteral("once")) {
            notice.showTime = QStringLiteral("once");
        }
        if (notice.primaryButtonText.isEmpty()) {
            notice.primaryButtonText = QStringLiteral("确定");
        }
        notices.append(notice);
    }

    return notices;
}
}  // namespace NutMod
