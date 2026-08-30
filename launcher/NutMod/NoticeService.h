#pragma once

#include <QObject>
#include <QPointer>

#include "net/NetJob.h"

class SettingsObject;
class QWidget;

namespace NutMod {
class NoticeService final : public QObject {
   public:
    NoticeService(QWidget* parentWidget, SettingsObject* settings, QObject* parent = nullptr);

    void start(bool showAll);

   private:
    void processResponse(const QByteArray& response, bool showAll);
    void executeAction(const QString& action, const QString& actionText);
    void markSeen(const QString& version);

    QPointer<QWidget> m_parentWidget;
    SettingsObject* m_settings = nullptr;
    NetJob::Ptr m_job;
};
}  // namespace NutMod
