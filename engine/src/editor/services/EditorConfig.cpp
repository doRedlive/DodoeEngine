// do@Redlive

#include "EditorConfig.h"

#include <fstream>
#include <filesystem>

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

    m_editor      = loadJsonFile(builtinDir + "/editor.json");
    m_menus       = loadJsonFile(builtinDir + "/menus.json");
    m_panels      = loadJsonFile(builtinDir + "/panels.json");
    m_inspectors  = loadJsonFile(builtinDir + "/inspectors.json");

    if (!projectDir.empty()) {
        mergeOverride(m_editor,     projectDir, "editor.json");
        mergeOverride(m_menus,      projectDir, "menus.json");
        mergeOverride(m_panels,     projectDir, "panels.json");
        mergeOverride(m_inspectors, projectDir, "inspectors.json");
    }

    if (!userDir.empty()) {
        mergeOverride(m_editor,     userDir, "editor.json");
        mergeOverride(m_menus,      userDir, "menus.json");
        mergeOverride(m_panels,     userDir, "panels.json");
        mergeOverride(m_inspectors, userDir, "inspectors.json");
    }

    if (m_editor.is_null()) {
        m_editor = dodoe::Json::object();
    }
    if (m_menus.is_null()) {
        m_menus = dodoe::Json::object();
    }
    if (m_panels.is_null()) {
        m_panels = dodoe::Json::object();
    }
    if (m_inspectors.is_null()) {
        m_inspectors = dodoe::Json::object();
    }

    return true;
}

dodoe::Json EditorConfig::layoutJson(const std::string& name) const
{
    std::string path = m_builtinDir + "/layouts/" + name + ".layout.json";
    dodoe::Json j = loadJsonFile(path);
    if (!j.is_null()) return j;

    if (!m_projectDir.empty()) {
        path = m_projectDir + "/layouts/" + name + ".layout.json";
        j = loadJsonFile(path);
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
    const std::string path = m_userDir + "/editor.json";
    dodoe::Json override = loadJsonFile(path);
    if (override.is_null() || !override.is_object()) {
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

dodoe::Json EditorConfig::loadJsonFile(const std::string& path) const
{
    if (!std::filesystem::exists(path)) {
        return dodoe::Json();
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return dodoe::Json();
    }

    try {
        dodoe::Json j;
        file >> j;
        return j;
    } catch (const std::exception&) {
        return dodoe::Json();
    }
}

void EditorConfig::mergeOverride(dodoe::Json& base, const std::string& overrideDir, const std::string& filename)
{
    std::string path = overrideDir + "/" + filename;
    if (!std::filesystem::exists(path)) return;

    dodoe::Json override = loadJsonFile(path);
    if (override.is_null() || !override.is_object()) return;

    if (base.is_null() || !base.is_object()) {
        base = std::move(override);
        return;
    }

    base.merge_patch(override);
}

} // namespace cakery
