#pragma once

class QAction;
class QDialog;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QToolBar;
class QTreeView;
class QVBoxLayout;
class QWidget;

namespace NutMod {
void customizeAccountPage(QAction* microsoftAction,
                          QAction* offlineAction,
                          QAction* authlibAction,
                          QToolBar* toolbar,
                          QTreeView* accountView,
                          int authServerColumn);

void customizeAuthlibLoginDialog(QDialog* dialog,
                                 QLineEdit* userField,
                                 QLineEdit* passwordField,
                                 QLineEdit* serverField,
                                 QLabel* loadingLabel,
                                 QDialogButtonBox* buttons);

void decorateAboutPage(QVBoxLayout* layout, QWidget* parent);
}  // namespace NutMod
