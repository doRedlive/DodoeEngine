// do@Redlive

#include "dopch.h"

#include "EditorResourceLocator.h"

#include <utility>

namespace cakery {

EditorResourceLocator::EditorResourceLocator(FsPath packagedRoot)
    : m_packagedRoot(std::move(packagedRoot))
{
}

void EditorResourceLocator::setProjectRoot(FsPath projectRoot)
{
    m_projectOverrideRoot = std::move(projectRoot) / "ProjectSettings" / "Editor";
}

FsPath EditorResourceLocator::resolve(const String& resourceId) const
{
    constexpr const char* prefix = "editor://";
    if (resourceId.rfind(prefix, 0) != 0) {
        return {};
    }

    const FsPath relative = resourceId.substr(std::char_traits<char>::length(prefix));
    const FsPath projectPath = m_projectOverrideRoot / relative;
    if (!m_projectOverrideRoot.empty() && std::filesystem::is_regular_file(projectPath)) {
        return projectPath;
    }

    const FsPath packagedPath = m_packagedRoot / relative;
    return std::filesystem::is_regular_file(packagedPath) ? packagedPath : FsPath{};
}

const FsPath& EditorResourceLocator::packagedRoot() const
{
    return m_packagedRoot;
}

} // namespace cakery
