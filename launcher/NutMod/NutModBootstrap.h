#pragma once

#include <QString>

class SettingsObject;

namespace NutMod {
void applyDefaultSettings(SettingsObject* settings);

bool showMicrosoftLoginWizard();
bool fetchCurseForgeKeyOnStartup();
bool allowLegacyDataMigration();
bool requireOfficialAccountForThirdPartyAccounts();
bool confirmThirdPartyAuthenticationServer();

bool useBundledChineseTranslation();
bool allowRemoteTranslationUpdates();
QString bundledTranslationPath(const QString& languageCode);

QString authServerUrl();
QString accountListEmptyText();
QString accountLoginMessage();
QString accountTypeDisplayName(bool isAuthlibInjector, const QString& originalName);
}  // namespace NutMod
