// do@Redlive

#pragma once

#include <QWidget>

#include "bridge/EditorBackend.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

class QFormLayout;

namespace cakery {

class EditorJsonWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorJsonWidget(const nlohmann::json& value, QWidget* parent = nullptr);
    EditorJsonWidget(const nlohmann::json& value,
                     std::vector<InspectorFieldMetadata> fields,
                     std::vector<AssetBrowserEntry> assets,
                     QWidget* parent = nullptr);

    void setValue(const nlohmann::json& value);
    const nlohmann::json& value() const { return m_value; }

signals:
    void valueChanged();

private:
    void rebuild();
    void buildField(QFormLayout* form, const std::string& key, const std::string& path,
                    const nlohmann::json& value, const InspectorFieldMetadata* metadata = nullptr);
    QWidget* buildVectorField(const std::string& path, const nlohmann::json& value);
    QWidget* buildColorField(const std::string& path, const nlohmann::json& value);
    QWidget* buildBoolField(const std::string& path, const nlohmann::json& value);
    QWidget* buildLayerField(const std::string& key, const std::string& path,
                             const nlohmann::json& value);
    QWidget* buildAssetReferenceField(const std::string& path, const nlohmann::json& value,
                                      const InspectorFieldMetadata& metadata);
    nlohmann::json& valueAt(const std::string& path);

    nlohmann::json m_value;
    std::vector<InspectorFieldMetadata> m_fields;
    std::vector<AssetBrowserEntry> m_assets;
};

} // namespace cakery
