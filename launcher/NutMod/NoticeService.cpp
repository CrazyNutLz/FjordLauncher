#include "NoticeService.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QMessageBox>
#include <QUrl>
#include <QWidget>

#include "Application.h"
#include "DesktopServices.h"
#include "NoticeDialog.h"
#include "NoticeManifest.h"
#include "NutMod/NutModConfig.h"
#include "settings/SettingsObject.h"

namespace NutMod {
NoticeService::NoticeService(QWidget* parentWidget, SettingsObject* settings, QObject* parent)
    : QObject(parent), m_parentWidget(parentWidget), m_settings(settings)
{}

void NoticeService::start(bool showAll)
{
    if (m_job) {
        return;
    }

    auto [download, response] = Net::Download::makeByteArray(QUrl(Config::NoticeManifestUrl));
    m_job = makeShared<NetJob>(QStringLiteral("NutMod::ServerNotices"), APPLICATION->network());
    m_job->setAskRetry(false);
    m_job->setAutoRetryLimit(2);
    m_job->addNetAction(download);

    connect(m_job.get(), &Task::succeeded, this, [this, response, showAll]() {
        processResponse(*response, showAll);
        m_job.reset();
        deleteLater();
    });
    connect(m_job.get(), &Task::failed, this, [this, showAll](const QString& reason) {
        qWarning() << "Failed to fetch server notices:" << reason;
        if (showAll && m_parentWidget) {
            QMessageBox::warning(m_parentWidget, QStringLiteral("服务器公告"), QStringLiteral("获取服务器公告失败：\n%1").arg(reason));
        }
        m_job.reset();
        deleteLater();
    });
    m_job->start();
}

void NoticeService::processResponse(const QByteArray& response, bool showAll)
{
    QString error;
    const auto notices = parseNoticeManifest(response, error);
    if (!error.isEmpty()) {
        qWarning() << "Failed to parse server notices:" << error;
        if (showAll && m_parentWidget) {
            QMessageBox::warning(m_parentWidget, QStringLiteral("服务器公告"), QStringLiteral("服务器公告格式错误：\n%1").arg(error));
        }
        return;
    }

    auto seenVersions = m_settings->get("NutModSeenNoticeVersions").toStringList();
    bool displayedAny = false;
    for (const auto& notice : notices) {
        const bool shouldShow = showAll || notice.showTime == QStringLiteral("always") || !seenVersions.contains(notice.version);
        if (!shouldShow || !m_parentWidget) {
            continue;
        }

        displayedAny = true;
        NoticeDialog dialog(notice, m_parentWidget);
        const bool accepted = dialog.exec() == QDialog::Accepted;
        if (notice.showTime == QStringLiteral("once")) {
            markSeen(notice.version);
            seenVersions.append(notice.version);
        }
        if (accepted) {
            executeAction(notice.action, notice.actionText);
            if (notice.action == QStringLiteral("exit")) {
                break;
            }
        }
    }

    if (showAll && !displayedAny && m_parentWidget) {
        QMessageBox::information(m_parentWidget, QStringLiteral("服务器公告"), QStringLiteral("当前没有可显示的服务器公告。"));
    }
}

void NoticeService::executeAction(const QString& action, const QString& actionText)
{
    if (action.isEmpty()) {
        return;
    }
    if (action == QStringLiteral("openurl")) {
        const QUrl url(actionText);
        if (url.isValid() && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))) {
            DesktopServices::openUrl(url);
        } else {
            qWarning() << "Ignored invalid notice URL:" << actionText;
        }
    } else if (action == QStringLiteral("clipboard") || action == QStringLiteral("clipbord")) {
        QApplication::clipboard()->setText(actionText);
    } else if (action == QStringLiteral("exit")) {
        QCoreApplication::quit();
    } else {
        qWarning() << "Ignored unknown notice action:" << action;
    }
}

void NoticeService::markSeen(const QString& version)
{
    auto seenVersions = m_settings->get("NutModSeenNoticeVersions").toStringList();
    seenVersions.removeAll(version);
    seenVersions.append(version);
    while (seenVersions.size() > 100) {
        seenVersions.removeFirst();
    }
    m_settings->set("NutModSeenNoticeVersions", seenVersions);
}
}  // namespace NutMod
