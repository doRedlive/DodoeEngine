// do@Redlive

#pragma once

#include <filesystem>
#include <string>

namespace cakery {

class EditorResourceLocator {
public:
    explicit EditorResourceLocator(std::filesystem::path packagedRoot);

    void setProjectRoot(std::filesystem::path projectRoot);
    std::filesystem::path resolve(const std::string& resourceId) const;
    const std::filesystem::path& packagedRoot() const;

private:
    std::filesystem::path m_packagedRoot;
    std::filesystem::path m_projectOverrideRoot;
};

} // namespace cakery
