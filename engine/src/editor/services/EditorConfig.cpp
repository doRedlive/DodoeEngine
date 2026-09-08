// do@Redlive

#include "EditorConfig.h"

#include <filesystem>
#include <fstream>

#include "config/layered_json.h"

namespace cakery {

EditorConfig& EditorConfig::self()
{
    static EditorConfig instance;
    return instance;
}

bool EditorConfig::load(const std::string& builtinDir, const std::string& projectDir, const std::string& userDir)
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

dodoe::Json EditorConfig::layoutJson(const std::string& name) const
{
    dodoe::Json j = dodoe::config::LoadJsonFile(
        std::filesystem::path(m_builtinDir) / "layouts" / (name + ".layout.json"));
    if (!j.is_null()) return j;

    if (!m_projectDir.empty()) {
        j = dodoe::config::LoadJsonFile(
            std::filesystem::path(m_projectDir) / "layouts" / (name + ".layout.json"));
        if (!j.is_null()) return j;
    }

    return dodoe::Json::object();
}

std::string EditorConfig::themeName() const
{
    if (m_editor.contains("theme") && m_editor["theme"].is_string()) {
        return m_editor["theme"].get<std::string>();
    }
    return "cakery-dark";
}

std::string EditorConfig::defaultLayoutName() const
{
    if (m_editor.contains("defaultLayout") && m_editor["defaultLayout"].is_string()) {
        return m_editor["defaultLayout"].get<std::string>();
    }
    return "default";
}

std::string EditorConfig::themePath() const
{
    return m_builtinDir + "/themes/" + themeName() + ".qss";
}

std::string EditorConfig::shortcut(const std::string& action) const
{
    if (m_editor.contains("shortcuts") && m_editor["shortcuts"].contains(action)) {
        return m_editor["shortcuts"][action].get<std::string>();
    }
    return "";
}

void EditorConfig::setThemeName(const std::string& themeName)
{
    if (themeName.empty()) {
        return;
    }
    m_editor["theme"] = themeName;
    if (m_userDir.empty()) {
        return;
    }

    std::filesystem::create_directories(m_userDir);
    const std::filesystem::path path = std::filesystem::path(m_userDir) / "editor.json";
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

void EditorConfig::reload()
{
    load(m_builtinDir, m_projectDir, m_userDir);
}

} // namespace cakery
