// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

namespace cakery {

class UpdateTilesetCommand final : public EditorCommand {
public:
    struct Params {
        dodoe::UInt32 tile_width{16};
        dodoe::UInt32 tile_height{16};
        dodoe::UInt32 margin{0};
        dodoe::UInt32 spacing{0};
    };

    UpdateTilesetCommand(dodoe::UUID tilemap, dodoe::UUID tilesetAssetId,
                         dodoe::UInt32 tileWidth, dodoe::UInt32 tileHeight,
                         dodoe::UInt32 margin, dodoe::UInt32 spacing);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    struct State {
        dodoe::UInt32 tile_width{0};
        dodoe::UInt32 tile_height{0};
        dodoe::UInt32 columns{0};
        dodoe::UInt32 tile_count{0};
        dodoe::UInt32 margin{0};
        dodoe::UInt32 spacing{0};
    };

    bool apply(const State& state);

    dodoe::UUID m_tilemap;
    dodoe::UUID m_tilesetAssetId;
    Params m_params;
    State m_previous;
    bool m_captured = false;
};

} // namespace cakery
