#include "lookscope.h"

#include "settings.h"

LookScope::LookScope(Settings *settings, bool panel, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_panel(panel)
{
    connect(m_settings, &Settings::changed, this, &LookScope::changed);
}

QString LookScope::density() const { return m_panel ? m_settings->panelDensity() : m_settings->density(); }
QString LookScope::fontSize() const { return m_panel ? m_settings->panelFontSize() : m_settings->fontSize(); }
QString LookScope::fontFamily() const { return m_panel ? m_settings->panelFontFamily() : m_settings->fontFamily(); }
QString LookScope::accent() const { return m_panel ? m_settings->panelAccent() : m_settings->accent(); }
bool LookScope::showAccent() const { return m_panel ? m_settings->panelShowAccent() : m_settings->showAccent(); }
double LookScope::densityScale() const { return m_panel ? m_settings->panelDensityScale() : m_settings->densityScale(); }
double LookScope::fontScale() const { return m_panel ? m_settings->panelFontScale() : m_settings->fontScale(); }
bool LookScope::panelCover() const { return m_settings->panelCover(); }
bool LookScope::panelSeek() const { return m_settings->panelSeek(); }
bool LookScope::panelVolume() const { return m_settings->panelVolume(); }
bool LookScope::panelUpNext() const { return m_settings->panelUpNext(); }
int LookScope::panelUpNextCount() const { return m_settings->panelUpNextCount(); }

void LookScope::set(const QString &prop, const QVariant &value)
{
    static const QStringList lookProps{"density", "fontSize", "fontFamily", "accent", "showAccent"};
    if (m_panel && lookProps.contains(prop)) {
        QString key = prop;
        key[0] = key.at(0).toUpper();
        m_settings->set(QStringLiteral("panel") + key, value);
    } else {
        m_settings->set(prop, value);
    }
}

QStringList LookScope::fontFamilies() const
{
    return m_settings->fontFamilies();
}
