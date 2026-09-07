#include "NoticeDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "NoticeManifest.h"

namespace NutMod {
NoticeDialog::NoticeDialog(const NoticeEntry& notice, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(notice.title);
    setModal(true);
    resize(680, 460);

    auto* layout = new QVBoxLayout(this);
    auto* body = new QTextBrowser(this);
    body->setPlainText(notice.text);
    body->setReadOnly(true);
    layout->addWidget(body, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* primaryButton = buttons->addButton(notice.primaryButtonText, QDialogButtonBox::AcceptRole);
    primaryButton->setDefault(true);
    connect(primaryButton, &QPushButton::clicked, this, &QDialog::accept);

    if (!notice.secondaryButtonText.isEmpty()) {
        auto* secondaryButton = buttons->addButton(notice.secondaryButtonText, QDialogButtonBox::RejectRole);
        connect(secondaryButton, &QPushButton::clicked, this, &QDialog::reject);
    }
    layout->addWidget(buttons);
}
}  // namespace NutMod
