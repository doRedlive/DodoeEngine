// do@Redlive

#include "dopch.h"

#include "EditorConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "config/layered_json.h"

namespace cakery {

bool EditorConfig::load(const String& builtinDir, const String& projectDir, const String& userDir)
{
    m_builtinDir  = builtinDir;
    m_projectDir  = projectDir;
    m_userDir     = userDir;

    m_editor     = dodoe::config::LoadLayered(builtinDir, projectDir, userDir, "editor.json");
    m_menus      = dodoe::config::LoadLayered(builtinDir, projectDir, userDir, "menus.json");
    m_panels     = dodoe::config::LoadLayered(builtinDir, projectDir, userDir, "panels.json");
    m_inspectors = dodoe::config::LoadLayered(builtinDir, projectDir, userDir, "inspectors.json");

    return true;
}

dodoe::Json EditorConfig::layoutJson(const String& name) const
{
    dodoe::Json j = dodoe::config::LoadJsonFile(
        FsPath(m_builtinDir) / "layouts" / (name + ".layout.json"));
    if (!j.is_null()) return j;

    if (!m_projectDir.empty()) {
        j = dodoe::config::LoadJsonFile(
            FsPath(m_projectDir) / "layouts" / (name + ".layout.json"));
        if (!j.is_null()) return j;
    }

    return dodoe::Json::object();
}

String EditorConfig::themeName() const
{
    if (m_editor.contains("theme") && m_editor["theme"].is_string()) {
        return m_editor["theme"].get<String>();
    }
    return "cakery-dark";
}

int EditorConfig::scriptFontPointSize() const
{
    if (m_editor.contains("scriptFontSize") && m_editor["scriptFontSize"].is_number_integer()) {
        return std::clamp(m_editor["scriptFontSize"].get<int>(), 8, 32);
    }
    return 11;
}

String EditorConfig::defaultLayoutName() const
{
    if (m_editor.contains("defaultLayout") && m_editor["defaultLayout"].is_string()) {
        return m_editor["defaultLayout"].get<String>();
    }
    return "default";
}

String EditorConfig::scriptFontFamily() const
{
    if (m_editor.contains("scriptFontFamily") && m_editor["scriptFontFamily"].is_string()) {
        const auto family = m_editor["scriptFontFamily"].get<String>();
        if (!family.empty()) return family;
    }
    return "Consolas";
}

int EditorConfig::scriptFontWeight() const
{
    if (m_editor.contains("scriptFontWeight") && m_editor["scriptFontWeight"].is_number_integer()) {
        return std::clamp(m_editor["scriptFontWeight"].get<int>(), 100, 900);
    }
    return 400;
}

String EditorConfig::themePath() const
{
    return m_builtinDir + "/themes/" + themeName() + ".qss";
}

String EditorConfig::shortcut(const String& action) const
{
    if (m_editor.contains("shortcuts") && m_editor["shortcuts"].contains(action)) {
        return m_editor["shortcuts"][action].get<String>();
    }
    return "";
}

void EditorConfig::setThemeName(const String& themeName)
{
    if (themeName.empty()) {
        return;
    }
    m_editor["theme"] = themeName;
    if (m_userDir.empty()) {
        return;
    }

    std::filesystem::create_directories(m_userDir);
    const FsPath path = FsPath(m_userDir) / "editor.json";
    dodoe::Json override = dodoe::config::LoadJsonFile(path);
    if (!override.is_object()) {
        override = dodoe::Json::object();
    }
    override["theme"] = themeName;
    std::ofstream file(path);
    if (file.is_open()) {
        file << override.dump(2) << '\n';
    }
}

void EditorConfig::setScriptFontPointSize(int pointSize)
{
    setScriptFont(scriptFontFamily(), pointSize, scriptFontWeight());
}

void EditorConfig::setScriptFont(const String& family, int pointSize, int weight)
{
    const int normalized = std::clamp(pointSize, 8, 32);
    const int normalizedWeight = std::clamp(weight, 100, 900);
    const String normalizedFamily = family.empty() ? String("Consolas") : family;
    m_editor["scriptFontSize"] = normalized;
    m_editor["scriptFontFamily"] = normalizedFamily;
    m_editor["scriptFontWeight"] = normalizedWeight;
    scriptFontChanged.fire();
    if (m_userDir.empty()) {
        return;
    }

    std::filesystem::create_directories(m_userDir);
    const FsPath path = FsPath(m_userDir) / "editor.json";
    dodoe::Json override = dodoe::config::LoadJsonFile(path);
    if (!override.is_object()) {
        override = dodoe::Json::object();
    }
    override["scriptFontSize"] = normalized;
    override["scriptFontFamily"] = normalizedFamily;
    override["scriptFontWeight"] = normalizedWeight;
    std::ofstream file(path);
    if (file.is_open()) {
        file << override.dump(2) << '\n';
    }
}

void EditorConfig::reload()
{
    load(m_builtinDir, m_projectDir, m_userDir);
    scriptFontChanged.fire();
}

} // namespace cakery
