// do@Redlive
#pragma once
#include "dopch.h"
#include "core/Signal.h"

#include <string>
#include <functional>


namespace cakery {

class EditorConfig {
public:
    bool load(const String& builtinDir, const String& projectDir = "", const String& userDir = "");

    const dodoe::Json& editorJson() const { return m_editor; }
    const dodoe::Json& menusJson() const { return m_menus; }
    const dodoe::Json& panelsJson() const { return m_panels; }
    const dodoe::Json& inspectorsJson() const { return m_inspectors; }

    dodoe::Json layoutJson(const String& name) const;

    String themeName() const;
    int scriptFontPointSize() const;
    String scriptFontFamily() const;
    int scriptFontWeight() const;
    String defaultLayoutName() const;
    String themePath() const;
    String builtinDir() const { return m_builtinDir; }

    String shortcut(const String& action) const;
    void setThemeName(const String& themeName);
    void setScriptFontPointSize(int pointSize);
    void setScriptFont(const String& family, int pointSize, int weight);
    Signal<> scriptFontChanged;

    void reload();

private:
    String m_builtinDir;
    String m_projectDir;
    String m_userDir;

    dodoe::Json m_editor;
    dodoe::Json m_menus;
    dodoe::Json m_panels;
    dodoe::Json m_inspectors;
};

} // namespace cakery
