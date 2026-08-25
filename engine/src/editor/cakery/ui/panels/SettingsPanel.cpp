// do@Redlive

#include "SettingsPanel.h"

#include "cakery/ui/inspector/EditorJsonWidget.h"

#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <filesystem>
#include <fstream>

namespace cakery {

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
    m_editor = new EditorJsonWidget(nlohmann::json::object(), container);
    content->addWidget(m_editor);
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("settingsPanelFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 8, 12, 8);
    footerLayout->setSpacing(8);

    m_pathLabel = new QLabel(footer);
    m_pathLabel->setObjectName(QStringLiteral("settingsPanelPath"));
    m_pathLabel->setText(tr("No file"));
    m_pathLabel->setStyleSheet(QStringLiteral("color: #909090;"));
    footerLayout->addWidget(m_pathLabel, 1);

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
    if (m_pathLabel) {
        m_pathLabel->setText(m_filePath.isEmpty() ? tr("No file") : QFileInfo(m_filePath).fileName());
        m_pathLabel->setToolTip(m_filePath);
    }
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
