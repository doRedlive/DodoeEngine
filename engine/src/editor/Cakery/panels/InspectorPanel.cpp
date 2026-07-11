// do@Redlive

#include "InspectorPanel.h"
#include "framework/EditorContext.h"
#include "framework/selection/SelectionManager.h"
#include "framework/core/UuidResolve.h"
#include "property/PropertyDrawer.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
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
        } else {
            rebuildForEntity(sel.front());
        }
    });
    m_connections.emplace_back(m_ctx.selection().changed, h);
}

void InspectorPanel::refresh()
{
    auto& sel = m_ctx.selection();
    if (!sel.empty()) {
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
}

void InspectorPanel::rebuildForEntity(dodoe::Uuid uuid)
{
    clearEditors();

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, uuid);
    if (!entity.valid()) return;

    m_nameEdit->setEnabled(true);
    m_addBtn->setEnabled(true);

    if (entity.hasComponent<dodoe::IDComponent>()) {
        m_nameEdit->setText(QString::fromStdString(entity.name()));
    }

    auto& reg = PropertyDrawerRegistry::self();
    auto& db = dodoe::ComponentDB::self();

    for (auto& entry : db.entries()) {
        if (entry.name == "IDComponent" || entry.name == "TagComponent") continue;
        if (!db.hasComponent(entity, entry.name)) continue;

        void* compPtr = db.getComponentPtr(entity, entry.name);
        if (!compPtr) continue;

        dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(entry.name);
        if (!meta.isValid()) continue;

        dodoe::FieldAccessor* fields = nullptr;
        int count = meta.get_field_list(fields);
        if (count <= 0) { delete[] fields; continue; }

        auto* group = new QGroupBox(QString::fromStdString(entry.name), m_editorContainer);
        auto* groupLayout = new QVBoxLayout(group);

        for (int i = 0; i < count; ++i) {
            PropertyContext pc;
            pc.ctx           = &m_ctx;
            pc.entity        = uuid;
            pc.componentName = entry.name;
            pc.componentPtr  = compPtr;
            pc.field         = &fields[i];

            auto drawer = reg.create(fields[i]);
            if (drawer) {
                QWidget* w = drawer->build(pc);
                if (w) {
                    groupLayout->addWidget(w);
                    m_entries.push_back({entry.name, fields[i].getFieldName(), drawer.release(), w});
                }
            }
        }

        delete[] fields;
        m_editorLayout->insertWidget(m_editorLayout->count() - 1, group);
    }
}

} // namespace cakery
