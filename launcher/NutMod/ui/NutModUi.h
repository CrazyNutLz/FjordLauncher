#pragma once

class QAction;
class QDialog;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QTreeView;
class QVBoxLayout;
class QWidget;
class WideBar;
class SettingsObject;

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

void addServerNoticeAction(QMenu* menu, QAction* beforeAction, QWidget* parent, SettingsObject* settings);

void customizeLauncherUpdateDialog(QDialog* dialog, QPushButton* skipButton, QPushButton* delayButton, bool mandatory);
}  // namespace NutMod
