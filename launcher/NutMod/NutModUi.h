#pragma once

class QAction;
class QDialog;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeView;
class QVBoxLayout;
class QWidget;
class WideBar;

namespace NutMod {
void customizeAccountPage(QAction* microsoftAction,
                          QAction* offlineAction,
                          QAction* authlibAction,
                          WideBar* toolbar,
                          QTreeView* accountView,
                          int authServerColumn);

void customizeAuthlibLoginDialog(QDialog* dialog,
                                 QLineEdit* userField,
                                 QLineEdit* passwordField,
                                 QLineEdit* serverField,
                                 QLabel* loadingLabel,
                                 QDialogButtonBox* buttons);

void decorateAboutPage(QVBoxLayout* layout, QWidget* parent);

void customizeLauncherUpdateDialog(QDialog* dialog, QPushButton* skipButton, QPushButton* delayButton, bool mandatory);
}  // namespace NutMod
