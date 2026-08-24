// do@Redlive

#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <string>

class QFormLayout;

namespace cakery {

class EditorJsonWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorJsonWidget(const nlohmann::json& value, QWidget* parent = nullptr);

    void setValue(const nlohmann::json& value);
    const nlohmann::json& value() const { return m_value; }

signals:
    void valueChanged();

private:
    void rebuild();
    void buildField(QFormLayout* form, const std::string& key, const std::string& path, const nlohmann::json& value);
    QWidget* buildVectorField(const std::string& path, const nlohmann::json& value);
    QWidget* buildColorField(const std::string& path, const nlohmann::json& value);
    QWidget* buildBoolField(const std::string& path, const nlohmann::json& value);
    QWidget* buildLayerField(const std::string& path, const nlohmann::json& value);
    nlohmann::json& valueAt(const std::string& path);

    nlohmann::json m_value;
};

} // namespace cakery
