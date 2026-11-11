// do@Redlive

#include "dopch.h"

#include "InspectorViewModel.h"

#include "core/application/AssetController.h"
#include "core/application/DocumentController.h"
#include "core/application/SelectionController.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

namespace {

struct ComponentTemplate {
    const char* typeName;
    Json defaultValue;
};

const DynamicArray<ComponentTemplate>& ComponentTemplates()
{
    static const DynamicArray<ComponentTemplate> templates = {
        {"TagComponent", {{"tag", "default"}}},
        {"TransformComponent",
            {{"position", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"scale", {1.0, 1.0, 1.0}}}},
        {"CameraComponent",
            {{"type", 0}, {"zoom", 1.0}, {"fov", 60.0}, {"near_plane", 0.01}, {"far_plane", 1000.0},
             {"aspect_ratio", 16.0 / 9.0}, {"background", {1.0, 1.0, 1.0, 1.0}}}},
        {"SpriteRendererComponent",
            {{"sprite", {{"asset_id", 0}, {"sub_object_id", 0}}}, {"flip", false},
             {"pivot", {0.0, 0.0}}, {"depth", 0.0}, {"color", {1.0, 1.0, 1.0, 1.0}}}},
        {"RectRendererComponent",
            {{"size", {1.0, 1.0}}, {"color", {1.0, 1.0, 1.0, 1.0}}, {"thickness", 0.0}}},
        {"CircleRendererComponent",
            {{"radius", 1.0}, {"color", {1.0, 1.0, 1.0, 1.0}}, {"segments", 32}, {"thickness", 0.0}}},
        {"LineRendererComponent",
            {{"direction", {1.0, 0.0}}, {"length", 1.0}, {"thickness", 1.0}, {"color", {1.0, 1.0, 1.0, 1.0}}}},
        {"Rigidbody2dComponent", {{"type", 0}, {"gravity_scale", 1.0}, {"fixed_rotation", false}}},
        {"BoxCollider2dComponent",
            {{"offset", {0.0, 0.0}}, {"size", {10.0, 10.0}}, {"is_sensor", false}, {"layer", 1},
             {"mask", 4294967295u}, {"density", 1.0}, {"friction", 0.5}, {"restitution", 0.0}}},
        {"BoxColliderComponent",
            {{"offset", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"size", {1.0, 1.0, 1.0}},
             {"is_sensor", false}, {"layer", 1}, {"mask", 4294967295u}, {"density", 1.0},
             {"friction", 0.5}, {"restitution", 0.0}}},
        {"PointLightComponent",
            {{"color", {1.0, 1.0, 1.0, 1.0}}, {"intensity", 1.0}, {"radius", 0.0}, {"range", 10.0}}},
        {"HierarchyComponent", {{"parent_uuid", 0}, {"child_count", 0}}},
        {"AnimatorComponent",
            {{"controller", {{"asset_id", 0}, {"sub_object_id", 0}}}, {"speed", 1.0}, {"play_on_awake", true}}},
    };
    return templates;
}

const Json* DefaultComponentValue(const String& typeName)
{
    for (const ComponentTemplate& entry : ComponentTemplates()) {
        if (typeName == entry.typeName) {
            return &entry.defaultValue;
        }
    }
    return nullptr;
}

} // namespace

InspectorViewModel::InspectorViewModel(DocumentController& documents,
                                       SelectionController& selection,
                                       AssetController& assets, DomainEventBus& events)
    : m_documents(documents), m_selection(selection), m_assets(assets), m_events(events)
{
    m_documentSubscription = m_events.subscribe<DocumentChangedEvent>(
        [this](const DocumentChangedEvent&) { rebuild(); });
    m_selectionSubscription = m_events.subscribe<SelectionChangedEvent>(
        [this](const SelectionChangedEvent&) { endInteractiveEdit(); rebuild(); });
    m_assetSubscription = m_events.subscribe<AssetCatalogChangedEvent>(
        [this](const AssetCatalogChangedEvent&) {
            if (m_selection.target() == EditorSelection::Target::Asset) rebuild();
        });
    rebuild();
}

InspectorViewModel::~InspectorViewModel() { endInteractiveEdit(); }

void InspectorViewModel::beginInteractiveEdit()
{
    if (m_interactiveEdit) return;
    m_interactiveEdit = true;
    m_documents.history().beginMerge();
}

void InspectorViewModel::endInteractiveEdit()
{
    if (!m_interactiveEdit) return;
    m_interactiveEdit = false;
    m_documents.history().endMerge();
}

const DynamicArray<String>& InspectorViewModel::componentTypes() const
{
    static const DynamicArray<String> types = [] {
        DynamicArray<String> out;
        for (const ComponentTemplate& entry : ComponentTemplates()) {
            out.push_back(String(entry.typeName));
        }
        return out;
    }();
    return types;
}

bool InspectorViewModel::defaultComponentValue(const String& typeName, Json& out) const
{
    const Json* value = DefaultComponentValue(typeName);
    if (!value) {
        return false;
    }
    out = *value;
    return true;
}

void InspectorViewModel::renameEntity(const String& name)
{
    const UInt64 uuid = m_selection.selected();
    if (uuid != 0) {
        m_documents.renameEntity(uuid, name);
    }
}

void InspectorViewModel::deleteSelectedEntity()
{
    const UInt64 uuid = m_selection.selected();
    if (uuid != 0) {
        m_documents.deleteEntity(uuid);
    }
}

void InspectorViewModel::addComponent(const String& typeName)
{
    const UInt64 uuid = m_selection.selected();
    if (uuid == 0) {
        return;
    }
    const EditorEntity* entity = m_documents.documentModel().findEntity(uuid);
    if (!entity) {
        return;
    }
    for (const EditorComponent& component : entity->nativeComponents) {
        if (component.typeName == typeName) {
            return;
        }
    }
    const Json* defaultValue = DefaultComponentValue(typeName);
    if (defaultValue) {
        m_documents.addComponent(uuid, EditorComponent{typeName, *defaultValue});
    }
}

bool InspectorViewModel::moveComponent(Size_t index, int delta)
{
    const UInt64 uuid = m_selection.selected();
    if (uuid == 0) {
        return false;
    }
    return m_documents.moveComponent(uuid, index, delta);
}

void InspectorViewModel::removeComponent(Size_t index, bool managed)
{
    const UInt64 uuid = m_selection.selected();
    if (uuid == 0) {
        return;
    }
    if (managed) {
        m_documents.removeManagedComponent(uuid, index);
    } else {
        m_documents.removeComponent(uuid, index);
    }
}

void InspectorViewModel::commitComponentValue(Size_t index, bool managed, const Json& value,
                                              const String& fieldPath)
{
    const UInt64 uuid = m_selection.selected();
    if (uuid == 0) {
        return;
    }
    const EditorEntity* primary = m_documents.documentModel().findEntity(uuid);
    if (m_selection.selectedAll().size() > 1 && primary) {
        const DynamicArray<EditorComponent>& components =
            managed ? primary->managedComponents : primary->nativeComponents;
        const String typeName = index < components.size()
            ? components[index].typeName
            : String();
        if (!fieldPath.empty()) {
            m_documents.updateComponentFieldOnEntities(
                m_selection.selectedAll(), typeName, fieldPath, value, managed);
        } else {
            m_documents.updateComponentOnEntities(
                m_selection.selectedAll(), typeName, value, managed);
        }
    } else if (managed) {
        m_documents.updateManagedComponent(uuid, index, value);
    } else {
        m_documents.updateComponent(uuid, index, value);
    }
}

void InspectorViewModel::applyAssetSettings(const Json& settings)
{
    if (!m_asset.valid) {
        return;
    }
    if (m_assets.updateImportSettings(m_asset.asset.path, settings)) {
        rebuild();
    }
}

bool InspectorViewModel::getCustomInspectorUI(const String& typeName, Json& out) const
{
    return m_documents.getCustomInspectorUI(typeName, out);
}

void InspectorViewModel::dispatchInspectorEvent(const String& typeName, const String& controlId,
                                                const String& eventName, const Json& value)
{
    m_documents.dispatchEditorEvent("inspector", typeName, controlId, eventName, value);
}

void InspectorViewModel::rebuild()
{
    m_entity = InspectorEntityModel{};
    m_asset = InspectorAssetModel{};
    rebuildAsset();
    if (!m_asset.valid) {
        rebuildEntity();
    }
    notifyChanged();
}

void InspectorViewModel::rebuildAsset()
{
    const SelectionController& selection = m_selection;
    if (selection.target() != EditorSelection::Target::Asset) {
        return;
    }
    const UInt64 uuid = selection.selected();
    if (uuid == 0) {
        return;
    }
    DynamicArray<AssetBrowserEntry> entries;
    if (!m_assets.listAssets(entries)) {
        return;
    }
    for (const AssetBrowserEntry& entry : entries) {
        if (entry.uuid != uuid) {
            continue;
        }
        m_asset.valid = true;
        m_asset.asset = entry;
        AssetImportSettings settings;
        if (m_assets.getImportSettings(entry.path, settings)) {
            m_asset.hasImportSettings = true;
            m_asset.importSettings = settings;
        }
        return;
    }
}

void InspectorViewModel::rebuildEntity()
{
    const SelectionController& selection = m_selection;
    if (selection.target() != EditorSelection::Target::Entity) {
        return;
    }
    const UInt64 uuid = selection.selected();
    if (uuid == 0) {
        return;
    }
    const EditorDocumentModel& model = m_documents.documentModel();
    const EditorEntity* entity = model.findEntity(uuid);
    if (!entity) {
        return;
    }

    m_entity.valid = true;
    m_entity.uuid = uuid;
    m_entity.name = entity->name;

    m_assets.listAssets(m_entity.assets);

    for (UInt64 selectedUuid : selection.selectedAll()) {
        if (selectedUuid != uuid && model.findEntity(selectedUuid)) {
            m_entity.multiSelection.push_back(selectedUuid);
        }
    }

    if (!m_entity.multiSelection.empty()) {
        for (const EditorComponent& component : entity->nativeComponents) {
            bool common = true;
            for (UInt64 otherUuid : m_entity.multiSelection) {
                const EditorEntity* other = model.findEntity(otherUuid);
                if (!other) {
                    common = false;
                    break;
                }
                bool found = false;
                for (const EditorComponent& otherComponent : other->nativeComponents) {
                    if (otherComponent.typeName == component.typeName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    common = false;
                    break;
                }
            }
            if (common) {
                m_entity.commonNativeTypes.push_back(component.typeName);
            }
        }
    }

    const auto buildComponents = [this](const DynamicArray<EditorComponent>& source, bool managed,
                                        DynamicArray<InspectorComponentModel>& target) {
        target.reserve(source.size());
        for (const EditorComponent& component : source) {
            InspectorComponentModel model;
            model.typeName = component.typeName;
            model.value = component.value;
            model.managed = managed;
            if (!managed) {
                m_documents.inspectComponent(component.typeName, model.reflectedFields);
                Json customUI;
                if (m_documents.getCustomInspectorUI(component.typeName, customUI)) {
                    model.hasCustomUI = true;
                }
            }
            target.push_back(std::move(model));
        }
    };
    buildComponents(entity->nativeComponents, false, m_entity.nativeComponents);
    buildComponents(entity->managedComponents, true, m_entity.managedComponents);
}

} // namespace cakery
