#pragma once

#include <QString>

class SettingsObject;
class QWidget;

namespace NutMod {
void applyDefaultSettings(SettingsObject* settings);
void checkServerNotices(QWidget* parent, SettingsObject* settings, bool showAll = false);

bool showMicrosoftLoginWizard();
bool fetchCurseForgeKeyOnStartup();
bool allowLegacyDataMigration();
bool requireOfficialAccountForThirdPartyAccounts();
bool confirmThirdPartyAuthenticationServer();

bool alwaysCheckLauncherUpdatesOnStartup();
bool allowSkippingLauncherUpdates();

bool useBundledChineseTranslation();
bool allowRemoteTranslationUpdates();
QString bundledTranslationPath(const QString& languageCode);

QString authServerUrl();
QString accountListEmptyText();
QString accountLoginMessage();
QString accountTypeDisplayName(bool isAuthlibInjector, const QString& originalName);
}  // namespace NutMod
