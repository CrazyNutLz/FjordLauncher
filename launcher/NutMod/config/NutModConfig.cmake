# Central configuration for CrazyNut's custom launcher features.
# Change the base URL here when the API server is migrated.
set(NUTMOD_API_BASE_URL "https://nutnode.top/software/fjrod-java25")

set(NUTMOD_LAUNCHER_UPDATE_MANIFEST_URL "${NUTMOD_API_BASE_URL}/launcher.json")

# Reserved for the client updater and custom announcements.
set(NUTMOD_CLIENT_UPDATE_MANIFEST_URL "${NUTMOD_API_BASE_URL}/client.json")
set(NUTMOD_NOTICE_MANIFEST_URL "${NUTMOD_API_BASE_URL}/notice.json")

# Server and branding configuration.
set(NUTMOD_AUTH_SERVER_URL "https://auth.mc-user.com:233/a000d3f85bc311ea908800163e095b49")
set(NUTMOD_ACCOUNT_REGISTER_URL "https://login.mc-user.com:233/a000d3f85bc311ea908800163e095b49/register")
set(NUTMOD_WEBSITE_URL "https://nutnode.top/")
set(NUTMOD_DONATION_LIST_URL "https://docs.qq.com/sheet/DSmZ0VXVxVUZaSE9u")

# Launcher defaults and visible version channel.
set(NUTMOD_DEFAULT_LANGUAGE "zh")
set(NUTMOD_DEFAULT_APPLICATION_THEME "dark")
set(NUTMOD_DEFAULT_ICON_THEME "breeze_dark")
set(NUTMOD_VERSION_CHANNEL "DaDiaoVersion")
