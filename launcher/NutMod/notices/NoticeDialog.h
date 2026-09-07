#pragma once

#include <QDialog>

namespace NutMod {
struct NoticeEntry;

class NoticeDialog final : public QDialog {
   public:
    explicit NoticeDialog(const NoticeEntry& notice, QWidget* parent = nullptr);
};
}  // namespace NutMod
