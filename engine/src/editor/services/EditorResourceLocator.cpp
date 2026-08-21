// do@Redlive

#include "EditorResourceLocator.h"

#include <utility>

namespace cakery {

EditorResourceLocator::EditorResourceLocator(std::filesystem::path packagedRoot)
    : m_packagedRoot(std::move(packagedRoot))
{
}

void EditorResourceLocator::setProjectRoot(std::filesystem::path projectRoot)
{
    m_projectOverrideRoot = std::move(projectRoot) / "ProjectSettings" / "Editor";
}

std::filesystem::path EditorResourceLocator::resolve(const std::string& resourceId) const
{
    constexpr const char* prefix = "editor://";
    if (resourceId.rfind(prefix, 0) != 0) {
        return {};
    }

    const std::filesystem::path relative = resourceId.substr(std::char_traits<char>::length(prefix));
    const std::filesystem::path projectPath = m_projectOverrideRoot / relative;
    if (!m_projectOverrideRoot.empty() && std::filesystem::is_regular_file(projectPath)) {
        return projectPath;
    }

    const std::filesystem::path packagedPath = m_packagedRoot / relative;
    return std::filesystem::is_regular_file(packagedPath) ? packagedPath : std::filesystem::path{};
}

const std::filesystem::path& EditorResourceLocator::packagedRoot() const
{
    return m_packagedRoot;
}

} // namespace cakery
