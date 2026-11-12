// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorCommands.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

#include <functional>

namespace dodoe {
    class Entity;
    class Scene;
}

namespace cakery {

class EditorCamera;
class EditorSession;
class TilePaintService;
struct BackendEventMessage;

// The tilemap edit controller has no dependency on RuntimeBackend or the
// runtime application. The host injects a scene provider, the editor camera
// and an event publisher.
class TilemapEditController final {
public:
    explicit TilemapEditController(EditorSession& session);
    ~TilemapEditController();

    TilemapEditController(const TilemapEditController&) = delete;
    TilemapEditController& operator=(const TilemapEditController&) = delete;

    void setSceneProvider(std::function<dodoe::Scene*()> provider);
    void setCamera(EditorCamera* camera);
    void setPublisher(std::function<void(const BackendEventMessage&)> publisher);

    TilePaintService& paint() { return *m_paint; }
    const TilePaintService& paint() const { return *m_paint; }

    bool handleCommand(const TilemapCreateCommand& command);
    bool handleCommand(const TilemapActivateCommand& command);
    bool handleCommand(const TilemapToolCommand& command);
    bool handleCommand(const TilemapBrushFlipCommand& command);
    bool handleCommand(const TilemapBrushRotateCommand& command);
    bool handleCommand(const TilemapRandomBrushCommand& command);
    bool handleCommand(const TilemapSelectionDeleteCommand& command);
    bool handleCommand(const TilemapSelectionCopyCommand& command);
    bool handleCommand(const TilemapSelectionPasteCommand& command);
    bool handleCommand(const TilemapBrushCommand& command);
    bool handleCommand(const TilemapEditTilesetCommand& command);
    bool handleCommand(const TilemapAddTilesetCommand& command);
    bool handleCommand(const TilemapRemoveTilesetCommand& command);
    bool handleCommand(const TilemapResizeCommand& command);
    bool handleCommand(const TilemapLayerAddCommand& command);
    bool handleCommand(const TilemapLayerRemoveCommand& command);
    bool handleCommand(const TilemapLayerMoveCommand& command);
    bool handleCommand(const TilemapLayerActiveCommand& command);
    bool handleCommand(const TilemapLayerVisibleCommand& command);
    bool handleCommand(const TilemapLayerOpacityCommand& command);
    bool handleCommand(const TilemapLayerRenameCommand& command);

    bool queryState(const String& tilemapUuid, Json& out) const;
    void updateFromSelection();
    void activateEdit(const dodoe::UUID& tilemapUuid, const dodoe::UUID& layerUuid);
    bool screenToCell(float screenX, float screenY, int& outX, int& outY) const;
    void updateHover(float screenX, float screenY);
    dodoe::Entity activeTilemapEntity() const;
    void invalidateTarget();
    void emitEditMode(bool active);

private:
    dodoe::Scene* activeScene() const;
    void executeLayerField(dodoe::UUID layer, const String& field, const Json& value);
    void publish(const BackendEventMessage& message) const;

    EditorSession& m_session;
    Scope<TilePaintService> m_paint;
    std::function<dodoe::Scene*()> m_sceneProvider;
    std::function<void(const BackendEventMessage&)> m_publisher;
    EditorCamera* m_camera = nullptr;
};

} // namespace cakery
