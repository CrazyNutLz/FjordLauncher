#include "NutModUi.h"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "NutMod/NutModConfig.h"
#include "ui/widgets/WideBar.h"

namespace NutMod {
void customizeAccountPage(QAction* microsoftAction,
                          QAction* offlineAction,
                          QAction* authlibAction,
                          WideBar* toolbar,
                          QTreeView* accountView,
                          int authServerColumn)
{
    microsoftAction->setVisible(false);
    offlineAction->setVisible(false);
    toolbar->removeAction(microsoftAction);
    toolbar->removeAction(offlineAction);
    authlibAction->setText(QStringLiteral("添加雕版账号"));
    accountView->setColumnHidden(authServerColumn, true);
}

void customizeAuthlibLoginDialog(QDialog* dialog,
                                 QLineEdit* userField,
                                 QLineEdit* passwordField,
                                 QLineEdit* serverField,
                                 QLabel* loadingLabel,
                                 QDialogButtonBox* buttons)
{
    dialog->setWindowTitle(QStringLiteral("登录雕版账号"));
    dialog->setAcceptDrops(false);
    userField->setPlaceholderText(QStringLiteral("通行证账号或邮箱（不是游戏ID）"));
    passwordField->setPlaceholderText(QStringLiteral("密码"));
    serverField->setText(Config::AuthServerUrl);
    serverField->hide();
    loadingLabel->setText(QStringLiteral("正在登录，请稍候……"));
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
}

void decorateAboutPage(QVBoxLayout* layout, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setTextFormat(Qt::RichText);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    label->setText(QStringLiteral(
                       "<p align=\"center\"><b>飞翔的大雕 GTNH 公益服务器专用启动器</b></p>"
                       "<p align=\"center\">本启动器由飞翔的大雕服主（@CrazyNut）基于 Fjord Launcher 魔改，专为大雕 GTNH 公益服务器定制。</p>"
                       "<p align=\"center\">公益服的长期运营离不开大家的支持。欢迎自愿投喂服主；无论金额多少，都是对服务器的鼓励。"
                       "收到的所有赞助都将用于服务器升级与日常维护。有你们的支持，公益服才能走得更稳、更远。</p>"
                       "<p align=\"center\">服主 QQ:736190890　　服务器群:629995845<br/>"
                       "服务器官网：<a href=\"%1\">访问官网</a>　　赞助名单：<a href=\"%2\">点击查看</a></p>")
                       .arg(Config::WebsiteUrl, Config::DonationListUrl));

    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    layout->insertWidget(0, separator);
    layout->insertWidget(0, label);
}

void customizeLauncherUpdateDialog(QDialog* dialog, QPushButton* skipButton, QPushButton* delayButton, bool mandatory)
{
    skipButton->hide();
    if (mandatory) {
        delayButton->hide();
        dialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
    }
}
}  // namespace NutMod
