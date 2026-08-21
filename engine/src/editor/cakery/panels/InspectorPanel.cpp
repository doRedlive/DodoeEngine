// do@Redlive

#include "InspectorPanel.h"
#include "framework/EditorContext.h"
#include "framework/selection/SelectionManager.h"
#include "framework/config/EditorConfig.h"
#include "framework/core/UuidResolve.h"
#include "property/PropertyDrawer.h"
#include "property/CustomEditorRegistry.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/id_component.h"

#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMenu>
#include <QLabel>

namespace cakery {

InspectorPanel::InspectorPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);

    auto* headerRow = new QHBoxLayout();
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Entity Name");
    m_nameEdit->setEnabled(false);
    headerRow->addWidget(m_nameEdit, 1);
    mainLayout->addLayout(headerRow);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_editorContainer = new QWidget();
    m_editorLayout = new QVBoxLayout(m_editorContainer);
    m_editorLayout->setContentsMargins(0, 0, 0, 0);
    m_editorLayout->addStretch();

    m_scrollArea->setWidget(m_editorContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    m_addBtn = new QPushButton("Add Component", this);
    m_addBtn->setEnabled(false);
    mainLayout->addWidget(m_addBtn);

    auto h = m_ctx.selection().changed.connect([this](const auto& sel) {
        if (sel.empty()) {
            m_nameEdit->setEnabled(false);
            m_nameEdit->clear();
            m_addBtn->setEnabled(false);
            clearEditors();
        } else if (m_ctx.selection().target() == SelectionTarget::Asset) {
            rebuildForAsset(sel.front());
        } else {
            rebuildForEntity(sel.front());
        }
    });
    m_connections.emplace_back(m_ctx.selection().changed, h);
}

void InspectorPanel::refresh()
{
    auto& sel = m_ctx.selection();
    if (sel.empty()) return;
    if (m_ctx.selection().target() == SelectionTarget::Asset) {
        rebuildForAsset(sel.primary());
    } else {
        rebuildForEntity(sel.primary());
    }
}

void InspectorPanel::clearEditors()
{
    for (auto& entry : m_entries) {
        if (entry.widget) {
            m_editorLayout->removeWidget(entry.widget);
            entry.widget->deleteLater();
        }
    }
    m_entries.clear();
    m_currentEntity = dodoe::UUID();
    m_currentAsset = dodoe::UUID();
}

void InspectorPanel::applyFieldAttributes(dodoe::FieldAccessor* fields, int count, const std::string& typeName)
{
    auto& inspectors = EditorConfig::self().inspectorsJson();
    if (!inspectors.contains("fieldAttributes")) return;

    auto& attrs = inspectors["fieldAttributes"];
    for (int i = 0; i < count; ++i) {
        std::string key = typeName + "." + std::string(fields[i].getFieldName());
        if (attrs.contains(key)) {
            auto& fa = attrs[key];
            if (fa.contains("Hidden") && fa["Hidden"].get<bool>()) {
                fields[i].setAttribute("Hidden", "true");
            }
            if (fa.contains("Tooltip")) {
                fields[i].setAttribute("Tooltip", fa["Tooltip"].get<std::string>().c_str());
            }
            if (fa.contains("Range") && fa["Range"].is_array() && fa["Range"].size() == 2) {
                std::string rangeStr = std::to_string(fa["Range"][0].get<float>()) + "," +
                                       std::to_string(fa["Range"][1].get<float>());
                fields[i].setAttribute("Range", rangeStr.c_str());
            }
            if (fa.contains("ReadOnly") && fa["ReadOnly"].get<bool>()) {
                fields[i].setAttribute("ReadOnly", "true");
            }
        }
    }
}

void InspectorPanel::renderFields(const std::string& typeName, void* objPtr, dodoe::UUID ownerUuid, bool isAsset)
{
    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(typeName.c_str()));
    if (!meta.isValid()) return;

    dodoe::FieldAccessor* fields = nullptr;
    int count = meta.get_field_list(fields);
    if (count <= 0) { delete[] fields; return; }

    applyFieldAttributes(fields, count, typeName.c_str());

    auto* group = new QGroupBox(QString::fromStdString(typeName.c_str()), m_editorContainer);
    auto* groupLayout = new QVBoxLayout(group);

    InspectorContext ic;
    ic.ctx           = &m_ctx;
    ic.entity        = ownerUuid;
    ic.componentName = typeName;
    ic.componentPtr  = objPtr;
    ic.parent        = group;

    auto customEditor = CustomEditorRegistry::self().create(typeName.c_str());
    if (customEditor) {
        QWidget* w = customEditor->build(ic);
        if (w) {
            groupLayout->addWidget(w);
            m_entries.push_back({typeName.c_str(), "", nullptr, customEditor.release(), w});
        }
    } else {
        auto& reg = PropertyDrawerRegistry::self();
        for (int i = 0; i < count; ++i) {
            if (fields[i].isHidden()) continue;

            const char* headerText = fields[i].attribute("Header");
            if (headerText && headerText[0]) {
                auto* headerLabel = new QLabel(QString::fromUtf8(headerText));
                headerLabel->setStyleSheet("color:#4EC9B0; font-size:11px; font-weight:bold;");
                groupLayout->addWidget(headerLabel);
            }

            PropertyContext pc;
            pc.ctx           = &m_ctx;
            pc.entity        = ownerUuid;
            pc.componentName = typeName;
            pc.componentPtr  = objPtr;
            pc.field         = &fields[i];
            pc.isAsset       = isAsset;

            auto drawer = reg.create(fields[i]);
            if (drawer) {
                QWidget* w = drawer->build(pc);
                if (w) {
                    const char* tooltip = fields[i].attribute("Tooltip");
                    if (tooltip && tooltip[0]) {
                        w->setToolTip(QString::fromUtf8(tooltip));
                    }

                    groupLayout->addWidget(w);
                    m_entries.push_back({typeName.c_str(), fields[i].getFieldName(), drawer.release(), nullptr, w});
                }
            }
        }
    }

    delete[] fields;
    m_editorLayout->insertWidget(m_editorLayout->count() - 1, group);
}

void InspectorPanel::addPlaceholder(const QString& text)
{
    auto* label = new QLabel(text, m_editorContainer);
    label->setStyleSheet("color:#8C8C8C; font-size:12px;");
    label->setAlignment(Qt::AlignCenter);
    m_editorLayout->insertWidget(m_editorLayout->count() - 1, label);
}

void InspectorPanel::showAssetMeta(dodoe::Asset* asset)
{
    auto* group = new QGroupBox("Asset", m_editorContainer);
    auto* gl = new QVBoxLayout(group);
    auto addRow = [gl](const QString& key, const QString& val) {
        auto* row = new QHBoxLayout();
        auto* k = new QLabel(key);
        k->setStyleSheet("color:#8C8C8C;font-size:11px;min-width:70px;");
        auto* v = new QLabel(val);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(k);
        row->addWidget(v, 1);
        gl->addLayout(row);
    };
    addRow("Name", QString::fromUtf8(asset->getName().c_str()));
    addRow("Type", QString::fromUtf8(dodoe::Asset::assetTypeToString(asset->getType())));
    addRow("Path", QString::fromUtf8(asset->getSourcePath().c_str()));
    addRow("Builtin", asset->getMetaData().is_builtin ? "Yes" : "No");
    m_editorLayout->insertWidget(m_editorLayout->count() - 1, group);
}

void InspectorPanel::rebuildForAsset(dodoe::UUID uuid)
{
    m_currentEntity = dodoe::UUID();
    m_currentAsset = uuid;

    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return;

    dodoe::Asset* asset = am->findAsset(uuid);
    if (!asset) {
        m_nameEdit->setEnabled(false);
        m_nameEdit->clear();
        m_addBtn->setEnabled(false);
        clearEditors();
        addPlaceholder("Asset not loaded");
        return;
    }

    m_nameEdit->setEnabled(true);
    m_nameEdit->setText(QString::fromUtf8(asset->getName().c_str()));
    m_addBtn->setEnabled(false);

    std::string typeName = std::string(dodoe::Asset::assetTypeToString(asset->getType())) + "Asset";
    bool reflectable = dodoe::TypeMeta::newMetaFromName(dodoe::String(typeName.c_str())).isValid();

    clearEditors();

    if (asset->isReadOnly() || !reflectable) {
        showAssetMeta(asset);
        if (!asset->isReadOnly()) {
            addPlaceholder("Editor not implemented yet");
        }
        return;
    }

    renderFields(typeName, asset, uuid, true);
}

void InspectorPanel::rebuildForEntity(dodoe::UUID uuid)
{
    bool sameEntity = (uuid == m_currentEntity);
    m_currentEntity = uuid;
    m_currentAsset = dodoe::UUID();

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, uuid);
    if (!entity.valid()) return;

    m_nameEdit->setEnabled(true);
    m_addBtn->setEnabled(true);

    if (entity.hasComponent<dodoe::IDComponent>()) {
        m_nameEdit->setText(QString::fromStdString(entity.name().c_str()));
    }

    auto& db = dodoe::ComponentDB::self();

    if (sameEntity) {
        for (auto& entry : m_entries) {
            if (entry.drawer) {
                PropertyContext pc;
                pc.ctx           = &m_ctx;
                pc.entity        = uuid;
                pc.componentName = entry.componentName;
                pc.componentPtr  = db.getComponentPtr(entity, dodoe::String(entry.componentName.c_str()));
                entry.drawer->updateValue(pc);
            } else if (entry.customEditor) {
                InspectorContext ic;
                ic.ctx           = &m_ctx;
                ic.entity        = uuid;
                ic.componentName = entry.componentName;
                ic.componentPtr  = db.getComponentPtr(entity, dodoe::String(entry.componentName.c_str()));
                ic.parent        = entry.widget ? entry.widget->parentWidget() : nullptr;
                entry.customEditor->refresh(ic);
            }
        }
        return;
    }

    clearEditors();

    for (auto& entry : db.entries()) {
        if (entry.name == "IDComponent" || entry.name == "TagComponent") continue;
        if (!db.hasComponent(entity, entry.name)) continue;

        void* compPtr = db.getComponentPtr(entity, entry.name);
        if (!compPtr) continue;

        renderFields(entry.name.c_str(), compPtr, uuid);
    }
}

} // namespace cakery
