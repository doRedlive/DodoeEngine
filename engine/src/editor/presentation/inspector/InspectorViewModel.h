// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorTypes.h"
#include "core/application/DomainEventBus.h"
#include "presentation/ViewModel.h"

namespace cakery {

class AssetController;
class DocumentController;
class SelectionController;

struct InspectorComponentModel {
    String typeName;
    Json value;
    bool managed = false;
    DynamicArray<InspectorFieldMetadata> reflectedFields;
    bool hasCustomUI = false;
};

struct InspectorEntityModel {
    bool valid = false;
    UInt64 uuid = 0;
    String name;
    DynamicArray<InspectorComponentModel> nativeComponents;
    DynamicArray<InspectorComponentModel> managedComponents;
    DynamicArray<AssetBrowserEntry> assets;
    DynamicArray<UInt64> multiSelection;
    DynamicArray<String> commonNativeTypes;
};

struct InspectorAssetModel {
    bool valid = false;
    AssetBrowserEntry asset;
    bool hasImportSettings = false;
    AssetImportSettings importSettings;
};

class InspectorViewModel : public ViewModel {
public:
    InspectorViewModel(DocumentController& documents, SelectionController& selection,
                       AssetController& assets, DomainEventBus& events);
    ~InspectorViewModel() override;

    const InspectorEntityModel& entity() const { return m_entity; }
    const InspectorAssetModel& asset() const { return m_asset; }
    const DynamicArray<String>& componentTypes() const;

    bool defaultComponentValue(const String& typeName, Json& out) const;

    void renameEntity(const String& name);
    void deleteSelectedEntity();
    void addComponent(const String& typeName);
    bool moveComponent(Size_t index, int delta);
    void removeComponent(Size_t index, bool managed);
    void commitComponentValue(Size_t index, bool managed, const Json& value,
                              const String& fieldPath = {});
    void applyAssetSettings(const Json& settings);
    void beginInteractiveEdit();
    void endInteractiveEdit();
    bool getCustomInspectorUI(const String& typeName, Json& out) const;
    void dispatchInspectorEvent(const String& typeName, const String& controlId,
                                const String& eventName, const Json& value);

private:
    void rebuild();
    void rebuildAsset();
    void rebuildEntity();

    DocumentController& m_documents;
    SelectionController& m_selection;
    AssetController& m_assets;
    DomainEventBus& m_events;
    InspectorEntityModel m_entity;
    InspectorAssetModel m_asset;
    bool m_interactiveEdit = false;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_selectionSubscription;
    ScopedConnection m_assetSubscription;
};

} // namespace cakery
