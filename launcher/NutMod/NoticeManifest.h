#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace NutMod {
struct NoticeEntry {
    QString version;
    QString title;
    QString text;
    QString showTime;
    QString primaryButtonText;
    QString secondaryButtonText;
    QString action;
    QString actionText;
};

QList<NoticeEntry> parseNoticeManifest(const QByteArray& data, QString& error);
}  // namespace NutMod
