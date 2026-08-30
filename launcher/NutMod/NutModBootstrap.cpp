#include "NutModBootstrap.h"

#include "NutMod/NutModConfig.h"
#include "settings/SettingsObject.h"

namespace NutMod {
void applyDefaultSettings(SettingsObject* settings)
{
    settings->registerSetting("FjordDefaultAppearanceApplied", false);
    if (!settings->get("FjordDefaultAppearanceApplied").toBool()) {
        settings->set("IconTheme", Config::DefaultIconTheme);
        settings->set("ApplicationTheme", Config::DefaultApplicationTheme);
        settings->set("FjordDefaultAppearanceApplied", true);
    }

    settings->registerSetting("FjordDefaultLanguageApplied", false);
    if (!settings->get("FjordDefaultLanguageApplied").toBool()) {
        settings->set("Language", Config::DefaultLanguage);
        settings->set("FjordDefaultLanguageApplied", true);
    }
}

bool showMicrosoftLoginWizard()
{
    return false;
}

bool fetchCurseForgeKeyOnStartup()
{
    return false;
}

bool allowLegacyDataMigration()
{
    return false;
}

bool requireOfficialAccountForThirdPartyAccounts()
{
    return false;
}

bool confirmThirdPartyAuthenticationServer()
{
    return false;
}

bool alwaysCheckLauncherUpdatesOnStartup()
{
    return true;
}

bool allowSkippingLauncherUpdates()
{
    return false;
}

bool useBundledChineseTranslation()
{
    return true;
}

bool allowRemoteTranslationUpdates()
{
    return false;
}

QString bundledTranslationPath(const QString& languageCode)
{
    return languageCode == Config::DefaultLanguage ? QStringLiteral(":/nutmod/translations/zh.po") : QString();
}

QString authServerUrl()
{
    return Config::AuthServerUrl;
}

QString accountListEmptyText()
{
    return QStringLiteral("欢迎！\n请选择“添加雕版账号”来登录。");
}

QString accountLoginMessage()
{
    return QStringLiteral("请输入您的雕版账号和密码。<br><br>如果您是第一次加入服务器，请先<a href=\"%1\">点击这里注册账号</a>。")
        .arg(Config::AccountRegisterUrl);
}

QString accountTypeDisplayName(bool isAuthlibInjector, const QString& originalName)
{
    return isAuthlibInjector ? QStringLiteral("雕版账号") : originalName;
}
}  // namespace NutMod
