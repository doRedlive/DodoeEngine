// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/bridge/EditorTypes.h"


#include <string>
#include <vector>

class QFormLayout;

namespace cakery {

class EditorJsonWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorJsonWidget(const Json& value, QWidget* parent = nullptr);
    EditorJsonWidget(const Json& value,
                     DynamicArray<InspectorFieldMetadata> fields,
                     DynamicArray<AssetBrowserEntry> assets,
                     QWidget* parent = nullptr);

    void setValue(const Json& value);
    const Json& value() const { return m_value; }

signals:
    void valueChanged();
    void fieldChanged(const QString& path);

private:
    void rebuild();
    void buildField(QFormLayout* form, const String& key, const String& path,
                    const Json& value, const InspectorFieldMetadata* metadata = nullptr);
    const InspectorFieldMetadata* metadataForPath(const String& path) const;
    QWidget* buildVectorField(const String& path, const Json& value);
    QWidget* buildColorField(const String& path, const Json& value);
    QWidget* buildBoolField(const String& path, const Json& value);
    QWidget* buildLayerField(const String& key, const String& path,
                             const Json& value);
    QWidget* buildAssetReferenceField(const String& path, const Json& value,
                                      const InspectorFieldMetadata& metadata);
    Json& valueAt(const String& path);

    Json m_value;
    DynamicArray<InspectorFieldMetadata> m_fields;
    DynamicArray<AssetBrowserEntry> m_assets;
};

} // namespace cakery
