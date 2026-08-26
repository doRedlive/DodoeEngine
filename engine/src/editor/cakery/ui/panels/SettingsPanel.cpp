// do@Redlive

#include "SettingsPanel.h"

#include "cakery/ui/inspector/EditorJsonWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <utility>
#include <vector>

namespace cakery {

namespace {

InspectorFieldMetadata MakeEnumField(
    const char* path, std::initializer_list<std::pair<const char*, int>> values)
{
    InspectorFieldMetadata metadata;
    metadata.name = path;
    metadata.kind = InspectorFieldKind::Enum;
    for (const auto& [name, value] : values) {
        metadata.enumValues.push_back({name, value});
    }
    return metadata;
}

std::vector<InspectorFieldMetadata> SettingsFieldMetadata()
{
    return {
        MakeEnumField("app_mode", {
            {"Game", 0}, {"Sandbox", 1}, {"Editor", 2},
        }),
        MakeEnumField("render_settings.api", {
            {"None", 0}, {"OpenGL", 1}, {"Vulkan", 2}, {"D3D12", 3},
        }),
        MakeEnumField("render_settings.pipeline", {
            {"None", 0}, {"Forward", 1}, {"Forward Plus", 2},
            {"Deferred", 3}, {"Deferred Plus", 4}, {"Only 2D", 5},
        }),
        MakeEnumField("render_settings.threading_mode", {
            {"Triple Thread", 0}, {"Dual Thread", 1}, {"Single Thread", 2},
        }),
        MakeEnumField("render_settings.present_mode", {
            {"VSync", 0}, {"Mailbox", 1}, {"Immediate", 2},
        }),
    };
}

} // namespace

SettingsPanel::SettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* container = new QWidget();
    auto* content = new QVBoxLayout(container);
    content->setContentsMargins(16, 16, 16, 16);
    content->setSpacing(8);
    m_editor = new EditorJsonWidget(nlohmann::json::object(), SettingsFieldMetadata(), {}, container);
    content->addWidget(m_editor);
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("settingsPanelFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 8, 12, 8);
    footerLayout->setSpacing(8);

    footerLayout->addStretch();

    auto* reloadButton = new QPushButton(tr("Reload"), footer);
    connect(reloadButton, &QPushButton::clicked, this, [this]() { reload(); });
    footerLayout->addWidget(reloadButton);

    auto* saveButton = new QPushButton(tr("Save"), footer);
    saveButton->setObjectName(QStringLiteral("projectPrimaryButton"));
    connect(saveButton, &QPushButton::clicked, this, [this]() { save(); });
    footerLayout->addWidget(saveButton);

    outer->addWidget(footer);
}

void SettingsPanel::setFilePath(const QString& path)
{
    m_filePath = path;
    reload();
}

void SettingsPanel::setFallback(std::function<nlohmann::json()> fallback)
{
    m_fallback = std::move(fallback);
}

void SettingsPanel::reload()
{
    nlohmann::json value;
    if (!m_filePath.isEmpty()) {
        std::ifstream file(m_filePath.toStdString());
        if (file.is_open()) {
            try {
                file >> value;
            } catch (const std::exception&) {
                value = nlohmann::json();
            }
        }
    }
    if (value.is_null() || (value.is_object() && value.empty())) {
        if (m_fallback) {
            nlohmann::json fallbackValue = m_fallback();
            if (fallbackValue.is_object() && !fallbackValue.empty()) {
                value = std::move(fallbackValue);
            }
        }
    }
    if (value.is_null()) {
        value = nlohmann::json::object();
    }
    m_editor->setValue(value);
}

void SettingsPanel::save()
{
    if (m_filePath.isEmpty()) {
        return;
    }
    std::filesystem::path target(m_filePath.toStdString());
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path());
    }
    std::ofstream file(target);
    if (!file.is_open()) {
        return;
    }
    file << m_editor->value().dump(4) << '\n';
    emit saved();
}

} // namespace cakery
