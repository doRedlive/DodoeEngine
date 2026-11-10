// do@Redlive
#pragma once
#include "dopch.h"

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"


#include <filesystem>
#include <string>

namespace cakery {

class InstantiatePrefabCommand final : public EditorCommand {
public:
    InstantiatePrefabCommand(String name, FsPath prefabPath,
                             Json position);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

    dodoe::UUID created() const { return m_createdUuid; }
    void setCreatedUuid(dodoe::UUID uuid) { m_createdUuid = uuid; }

private:
    String m_name;
    FsPath m_prefabPath;
    Json m_position;
    dodoe::UUID m_createdUuid;
    bool m_created = false;
};

} // namespace cakery
