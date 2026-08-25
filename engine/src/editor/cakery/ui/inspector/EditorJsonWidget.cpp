// do@Redlive

#include "EditorJsonWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include <QIcon>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QPointer>
#include <QPixmap>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cakery {

namespace {

const char* kThumbnailExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".webp"};

bool IsAssetImage(const QString& path)
{
    const int dot = path.lastIndexOf(QLatin1Char('.'));
    if (dot < 0) {
        return false;
    }
    const QString suffix = path.mid(dot).toLower();
    for (const char* extension : kThumbnailExtensions) {
        if (suffix == QLatin1String(extension)) {
            return true;
        }
    }
    return false;
}

std::mutex gThumbnailMutex;
std::unordered_map<QString, QImage> gThumbnailCache;

QString AssetReferenceTargetType(const std::string& typeName)
{
    for (const std::string_view prefix : {std::string_view("PPtr<"), std::string_view("AssetHandle<")}) {
        if (typeName.starts_with(prefix) && typeName.back() == '>') {
            return QString::fromStdString(typeName.substr(prefix.size(), typeName.size() - prefix.size() - 1));
        }
    }
    return {};
}

bool IsAssetCompatible(const AssetBrowserEntry& asset, const QString& targetType)
{
    const QString assetType = QString::fromStdString(asset.type);
    if (targetType == QLatin1String("Sprite") || targetType == QLatin1String("Texture2D")) {
        return assetType == QLatin1String("Texture") || assetType == QLatin1String("Sprite");
    }
    if (targetType == QLatin1String("Mesh") || targetType == QLatin1String("Skeleton") ||
        targetType == QLatin1String("AnimClip")) {
        return assetType == QLatin1String("Mesh");
    }
    if (targetType == QLatin1String("Material")) return assetType == QLatin1String("Material");
    if (targetType == QLatin1String("Anim2DClip")) return assetType == QLatin1String("Anim2DClip");
    if (targetType == QLatin1String("AnimatorController")) return assetType == QLatin1String("AnimatorController");
    if (targetType == QLatin1String("Tileset")) return assetType == QLatin1String("Tileset");
    return targetType.isEmpty() || assetType == targetType;
}

class AssetReferenceField final : public QToolButton {
public:
    explicit AssetReferenceField(QWidget* parent = nullptr) : QToolButton(parent) {
        setObjectName(QStringLiteral("inspectorAssetReference"));
        setAcceptDrops(true);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setIconSize(QSize(24, 24));
        setMinimumHeight(30);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    std::function<void(std::uint64_t, const QString&)> assigned;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasFormat("application/x-cakery-asset")) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override {
        const QList<QByteArray> parts = event->mimeData()
            ->data("application/x-cakery-asset").split('\n');
        if (parts.size() == 2 && assigned) {
            bool ok = false;
            const std::uint64_t guid = parts[0].toULongLong(&ok);
            if (ok) {
                assigned(guid, QString::fromUtf8(parts[1]));
                event->acceptProposedAction();
            }
        }
    }
};

class AssetPickerPopup final : public QFrame {
public:
    AssetPickerPopup(const std::vector<AssetBrowserEntry>& assets,
                     const QString& targetType,
                     std::function<void(std::uint64_t, const QString&)> selected)
        : QFrame(nullptr), m_selected(std::move(selected))
    {
        setObjectName(QStringLiteral("inspectorAssetPicker"));
        setWindowFlags(Qt::Popup);
        setAttribute(Qt::WA_DeleteOnClose);
        setFixedSize(560, 390);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText(targetType.isEmpty()
            ? tr("Search assets")
            : tr("Search %1 assets").arg(targetType));
        m_filter->setClearButtonEnabled(true);
        layout->addWidget(m_filter);
        m_grid = new QListWidget(this);
        m_grid->setViewMode(QListView::IconMode);
        m_grid->setResizeMode(QListView::Adjust);
        m_grid->setMovement(QListView::Static);
        m_grid->setWrapping(true);
        m_grid->setWordWrap(true);
        m_grid->setIconSize(QSize(56, 56));
        m_grid->setGridSize(QSize(96, 92));
        m_grid->setSpacing(4);
        layout->addWidget(m_grid, 1);

        addItem(tr("None"), 0, QString());
        for (const auto& asset : assets) {
            if (!IsAssetCompatible(asset, targetType)) {
                continue;
            }
            addItem(QString::fromStdString(asset.name), asset.uuid, QString::fromStdString(asset.path));
        }
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& filter) {
            for (int i = 0; i < m_grid->count(); ++i) {
                auto* item = m_grid->item(i);
                item->setHidden(!filter.isEmpty() && !item->text().contains(filter, Qt::CaseInsensitive));
            }
        });
        connect(m_grid, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            if (!item) return;
            const std::uint64_t assetId = item->data(Qt::UserRole).toULongLong();
            const QString assetPath = item->data(Qt::UserRole + 1).toString();
            const auto selected = m_selected;
            close();
            QTimer::singleShot(0, [selected, assetId, assetPath]() {
                selected(assetId, assetPath);
            });
        });
        startThumbnailLoading();
    }

private:
    void addItem(const QString& name, std::uint64_t id, const QString& path) {
        auto* item = new QListWidgetItem(name, m_grid);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(id));
        item->setData(Qt::UserRole + 1, path);
    }

    void startThumbnailLoading() {
        std::vector<QString> paths;
        paths.reserve(static_cast<std::size_t>(m_grid->count()));
        for (int i = 1; i < m_grid->count(); ++i) {
            const QString path = m_grid->item(i)->data(Qt::UserRole + 1).toString();
            if (!path.isEmpty() && IsAssetImage(path)) {
                paths.push_back(path);
            }
        }
        if (paths.empty()) {
            return;
        }
        QPointer<AssetPickerPopup> owner = this;
        std::thread([owner, paths = std::move(paths)]() {
            for (const QString& path : paths) {
                QImage thumb;
                {
                    std::lock_guard<std::mutex> lock(gThumbnailMutex);
                    const auto cached = gThumbnailCache.find(path);
                    if (cached != gThumbnailCache.end()) {
                        thumb = cached->second;
                    }
                }
                if (thumb.isNull()) {
                    QImage image(path);
                    if (image.isNull()) {
                        continue;
                    }
                    thumb = image.scaled(QSize(56, 56), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    std::lock_guard<std::mutex> lock(gThumbnailMutex);
                    gThumbnailCache[path] = thumb;
                }
                if (!owner) {
                    break;
                }
                const QImage copy = thumb;
                QMetaObject::invokeMethod(QCoreApplication::instance(), [owner, path, copy]() {
                    AssetPickerPopup* popup = owner.data();
                    if (!popup) {
                        return;
                    }
                    for (int i = 0; i < popup->m_grid->count(); ++i) {
                        auto* item = popup->m_grid->item(i);
                        if (item->data(Qt::UserRole + 1).toString() == path) {
                            item->setIcon(QIcon(QPixmap::fromImage(copy)));
                            break;
                        }
                    }
                }, Qt::QueuedConnection);
            }
        }).detach();
    }

    QLineEdit* m_filter = nullptr;
    QListWidget* m_grid = nullptr;
    std::function<void(std::uint64_t, const QString&)> m_selected;
};

class ScrubDoubleSpinBox : public QDoubleSpinBox {
public:
    ScrubDoubleSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setAlignment(Qt::AlignRight);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        lineEdit()->installEventFilter(this);
    }

protected:
    QSize minimumSizeHint() const override
    {
        return QSize(48, QAbstractSpinBox::minimumSizeHint().height());
    }

    QSize sizeHint() const override
    {
        return QSize(88, QAbstractSpinBox::sizeHint().height());
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == lineEdit() && event->type() == QEvent::Wheel) {
            event->accept();
            return true;
        }
        return QDoubleSpinBox::eventFilter(watched, event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        event->accept();
    }
};

class ScrubIntSpinBox : public QSpinBox {
public:
    ScrubIntSpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent)
    {
        setAlignment(Qt::AlignRight);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        lineEdit()->installEventFilter(this);
    }

protected:
    QSize minimumSizeHint() const override
    {
        return QSize(48, QAbstractSpinBox::minimumSizeHint().height());
    }

    QSize sizeHint() const override
    {
        return QSize(88, QAbstractSpinBox::sizeHint().height());
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == lineEdit() && event->type() == QEvent::Wheel) {
            event->accept();
            return true;
        }
        return QSpinBox::eventFilter(watched, event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        event->accept();
    }
};

class NumericScrubFilter final : public QObject {
public:
    NumericScrubFilter(std::function<double()> readValue, std::function<void(double)> setValue,
                       std::function<void()> commit, QObject* parent)
        : QObject(parent), m_readValue(std::move(readValue)), m_setValue(std::move(setValue)),
          m_commit(std::move(commit)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() != Qt::LeftButton) return false;
            m_pressed = true;
            m_dragging = false;
            m_startPosition = mouse->globalPosition();
            m_startValue = m_readValue();
            return true;
        }
        case QEvent::MouseMove: {
            if (!m_pressed) return false;
            auto* mouse = static_cast<QMouseEvent*>(event);
            const double offset = mouse->globalPosition().x() - m_startPosition.x();
            if (!m_dragging && qAbs(offset) < 3.0) return true;
            m_dragging = true;
            const double step = qMax(qAbs(m_startValue) * 0.01, 0.05);
            m_setValue(m_startValue + offset * step);
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (!m_pressed || mouse->button() != Qt::LeftButton) return false;
            m_pressed = false;
            if (m_dragging) m_commit();
            return true;
        }
        default:
            return false;
        }
    }

private:
    std::function<double()> m_readValue;
    std::function<void(double)> m_setValue;
    std::function<void()> m_commit;
    QPointF m_startPosition;
    double m_startValue = 0.0;
    bool m_pressed = false;
    bool m_dragging = false;
};

void InstallNumericScrub(QLabel* label, QDoubleSpinBox* spin) {
    label->setCursor(Qt::SizeHorCursor);
    label->installEventFilter(new NumericScrubFilter(
        [spin]() { return spin->value(); }, [spin](double value) { spin->setValue(value); },
        [spin]() { emit spin->editingFinished(); }, label));
}

void InstallNumericScrub(QLabel* label, QSpinBox* spin) {
    label->setCursor(Qt::SizeHorCursor);
    label->installEventFilter(new NumericScrubFilter(
        [spin]() { return static_cast<double>(spin->value()); },
        [spin](double value) { spin->setValue(qRound(value)); },
        [spin]() { emit spin->editingFinished(); }, label));
}

int DecimalsFor(double value);

QDoubleSpinBox* MakeDoubleSpinBox(double value) {
    auto* spin = new ScrubDoubleSpinBox();
    spin->setRange(-1000000.0, 1000000.0);
    spin->setDecimals(DecimalsFor(value));
    spin->setValue(value);
    return spin;
}

QLabel* MakeNonEditableLabel(const std::string& text) {
    auto* label = new QLabel(QString::fromStdString(text));
    label->setStyleSheet(QStringLiteral("color: #A0A0A0;"));
    return label;
}

int ChannelByte(double v) {
    int value = static_cast<int>(v * 255.0 + 0.5);
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

QColor ColorFromArray(const nlohmann::json& value) {
    if (value.is_array() && value.size() >= 3) {
        return QColor(ChannelByte(value[0].get<double>()), ChannelByte(value[1].get<double>()),
                      ChannelByte(value[2].get<double>()),
                      value.size() >= 4 ? ChannelByte(value[3].get<double>()) : 255);
    }
    return QColor(255, 255, 255);
}

QString SwatchStyle(const QColor& color) {
    return QStringLiteral("background: %1; border: 1px solid #000000; border-radius: 2px;")
        .arg(color.name());
}

int DecimalsFor(double value) {
    QString text = QString::number(value, 'g', 12);
    const int dot = text.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        return 1;
    }
    int decimals = text.size() - dot - 1;
    while (decimals > 0 && text.at(dot + decimals) == QLatin1Char('0')) {
        --decimals;
    }
    if (decimals == 0) {
        return 1;
    }
    return qMin(decimals, 6);
}

QString TitleCaseKey(const std::string& key) {
    QString result;
    bool capitalize = true;
    for (char c : key) {
        if (c == '_') {
            result += QLatin1Char(' ');
            capitalize = true;
        } else if (capitalize) {
            result += QChar::fromLatin1(c).toUpper();
            capitalize = false;
        } else {
            result += QChar::fromLatin1(c);
        }
    }
    return result;
}

} // namespace

EditorJsonWidget::EditorJsonWidget(const nlohmann::json& value, QWidget* parent)
    : QWidget(parent), m_value(value)
{
    if (!m_value.is_object()) {
        m_value = nlohmann::json::object();
    }
    rebuild();
}

EditorJsonWidget::EditorJsonWidget(const nlohmann::json& value,
                                   std::vector<InspectorFieldMetadata> fields,
                                   std::vector<AssetBrowserEntry> assets,
                                   QWidget* parent)
    : QWidget(parent), m_value(value), m_fields(std::move(fields)), m_assets(std::move(assets))
{
    if (!m_value.is_object()) {
        m_value = nlohmann::json::object();
    }
    rebuild();
}

void EditorJsonWidget::setValue(const nlohmann::json& value) {
    if (value == m_value) {
        return;
    }
    m_value = value;
    rebuild();
}

void EditorJsonWidget::rebuild() {
    QLayout* oldLayout = layout();
    if (oldLayout) {
        delete oldLayout;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(7);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    std::unordered_set<std::string> rendered;
    for (const auto& metadata : m_fields) {
        if (metadata.hidden || !m_value.contains(metadata.name)) {
            continue;
        }
        rendered.insert(metadata.name);
        buildField(form, metadata.name, metadata.name, m_value.at(metadata.name), &metadata);
    }
    for (auto it = m_value.begin(); it != m_value.end(); ++it) {
        if (rendered.contains(it.key())) {
            continue;
        }
        buildField(form, it.key(), it.key(), it.value());
    }
    layout->addLayout(form);
}

void EditorJsonWidget::buildField(QFormLayout* form, const std::string& key, const std::string& path,
                                  const nlohmann::json& value,
                                  const InspectorFieldMetadata* metadata) {
    if (metadata && metadata->kind == InspectorFieldKind::AssetHandle) {
        QLabel* label = new QLabel(TitleCaseKey(key));
        label->setObjectName(QStringLiteral("inspectorFieldLabel"));
        label->setMinimumWidth(76);
        if (!metadata->tooltip.empty()) {
            label->setToolTip(QString::fromStdString(metadata->tooltip));
        }
        auto* field = buildAssetReferenceField(path, value, *metadata);
        field->setEnabled(!metadata->readOnly);
        form->addRow(label, field);
        return;
    }
    if (value.is_object()) {
        auto* group = new QGroupBox(TitleCaseKey(key));
        group->setObjectName(QStringLiteral("inspectorNestedGroup"));
        auto* subForm = new QFormLayout(group);
        subForm->setContentsMargins(6, 6, 6, 6);
        subForm->setHorizontalSpacing(8);
        subForm->setVerticalSpacing(7);
        subForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        subForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        for (auto it = value.begin(); it != value.end(); ++it) {
            const std::string subPath = path + "." + it.key();
            buildField(subForm, it.key(), subPath, it.value());
        }
        form->addRow(group);
        return;
    }

    QLabel* label = new QLabel(TitleCaseKey(key));
    label->setObjectName(QStringLiteral("inspectorFieldLabel"));
    label->setMinimumWidth(76);
    if (metadata && !metadata->tooltip.empty()) {
        label->setToolTip(QString::fromStdString(metadata->tooltip));
    }

    if (metadata && metadata->kind == InspectorFieldKind::Enum && value.is_number_integer()) {
        auto* combo = new QComboBox();
        int current = value.get<int>();
        int currentIndex = -1;
        for (const auto& option : metadata->enumValues) {
            combo->addItem(QString::fromStdString(option.name), option.value);
            if (option.value == current) {
                currentIndex = combo->count() - 1;
            }
        }
        if (currentIndex >= 0) {
            combo->setCurrentIndex(currentIndex);
        }
        combo->setEnabled(!metadata->readOnly);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, path, combo](int index) {
            if (index < 0) return;
            valueAt(path) = combo->itemData(index).toInt();
            emit valueChanged();
        });
        form->addRow(label, combo);
        return;
    }

    if (value.is_number_integer() || value.is_number_unsigned()) {
        if (key == "layer" || key == "mask") {
            auto* layer = buildLayerField(key, path, value);
            if (metadata && metadata->readOnly) layer->setEnabled(false);
            form->addRow(label, layer);
            return;
        }
        if (key == "id") {
            std::string text;
            if (value.is_number_unsigned()) {
                text = std::to_string(value.get<unsigned long long>());
            } else {
                text = std::to_string(value.get<long long>());
            }
            form->addRow(label, MakeNonEditableLabel(text));
            return;
        }
        if (value.is_number_unsigned()) {
            const unsigned long long number = value.get<unsigned long long>();
            if (number > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
                form->addRow(label, MakeNonEditableLabel(std::to_string(number)));
                return;
            }
            auto* spin = new ScrubIntSpinBox();
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(number));
            if (metadata && metadata->hasRange) {
                spin->setRange(static_cast<int>(metadata->rangeMin), static_cast<int>(metadata->rangeMax));
            }
            if (metadata && metadata->readOnly) spin->setReadOnly(true);
            if (!metadata || !metadata->readOnly) InstallNumericScrub(label, spin);
            connect(spin, &QSpinBox::editingFinished, this, [this, path, spin]() {
                valueAt(path) = spin->value();
                emit valueChanged();
            });
            form->addRow(label, spin);
            return;
        }
        const long long number = value.get<long long>();
        if (number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max()) {
            auto* spin = new ScrubIntSpinBox();
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(number));
            if (metadata && metadata->hasRange) {
                spin->setRange(static_cast<int>(metadata->rangeMin), static_cast<int>(metadata->rangeMax));
            }
            if (metadata && metadata->readOnly) spin->setReadOnly(true);
            if (!metadata || !metadata->readOnly) InstallNumericScrub(label, spin);
            connect(spin, &QSpinBox::editingFinished, this, [this, path, spin]() {
                valueAt(path) = spin->value();
                emit valueChanged();
            });
            form->addRow(label, spin);
        } else {
            form->addRow(label, MakeNonEditableLabel(std::to_string(number)));
        }
        return;
    }

    if (value.is_number_float()) {
        auto* spin = MakeDoubleSpinBox(value.get<double>());
        if (metadata && metadata->hasRange) {
            spin->setRange(metadata->rangeMin, metadata->rangeMax);
        }
        if (metadata && metadata->readOnly) spin->setReadOnly(true);
        if (!metadata || !metadata->readOnly) InstallNumericScrub(label, spin);
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, path, spin]() {
            valueAt(path) = spin->value();
            emit valueChanged();
        });
        form->addRow(label, spin);
        return;
    }

    if (value.is_boolean()) {
        auto* check = buildBoolField(path, value);
        if (metadata && metadata->readOnly) check->setEnabled(false);
        form->addRow(label, check);
        return;
    }

    if (value.is_string()) {
        if (key == "tag") {
            auto* combo = new QComboBox();
            combo->setObjectName(QStringLiteral("inspectorTagField"));
            combo->setEditable(true);
            const QString current = QString::fromStdString(value.get<std::string>());
            const QStringList standardTags = {QStringLiteral("Untagged"), QStringLiteral("Player"),
                                              QStringLiteral("MainCamera"), QStringLiteral("Enemy"),
                                              QStringLiteral("GameController")};
            combo->addItems(standardTags);
            if (combo->findText(current) < 0) combo->addItem(current);
            combo->setCurrentText(current);
            if (metadata && metadata->readOnly) combo->setEnabled(false);
            connect(combo, &QComboBox::textActivated, this, [this, path](const QString& text) {
                valueAt(path) = text.toStdString();
                emit valueChanged();
            });
            connect(combo->lineEdit(), &QLineEdit::editingFinished, this, [this, path, combo]() {
                valueAt(path) = combo->currentText().toStdString();
                emit valueChanged();
            });
            form->addRow(label, combo);
            return;
        }
        auto* edit = new QLineEdit(QString::fromStdString(value.get<std::string>()));
        if (metadata && metadata->readOnly) edit->setReadOnly(true);
        connect(edit, &QLineEdit::editingFinished, this, [this, path, edit]() {
            valueAt(path) = edit->text().toStdString();
            emit valueChanged();
        });
        form->addRow(label, edit);
        return;
    }

    if (value.is_array()) {
        bool allNumbers = true;
        for (const auto& element : value) {
            if (!element.is_number()) {
                allNumbers = false;
                break;
            }
        }
        if (allNumbers && !value.empty()) {
            const std::size_t size = value.size();
            const bool isColor = (size == 3 || size == 4) &&
                ((metadata && metadata->kind == InspectorFieldKind::Color) ||
                 key.find("color") != std::string::npos || key == "background");
            if (isColor) {
                auto* color = buildColorField(path, value);
                if (metadata && metadata->readOnly) color->setEnabled(false);
                form->addRow(label, color);
            } else {
                auto* vector = buildVectorField(path, value);
                if (metadata && metadata->hasRange) {
                    for (auto* spin : vector->findChildren<QDoubleSpinBox*>()) {
                        spin->setRange(metadata->rangeMin, metadata->rangeMax);
                    }
                }
                if (metadata && metadata->readOnly) vector->setEnabled(false);
                form->addRow(label, vector);
            }
            return;
        }
        form->addRow(label, MakeNonEditableLabel(value.dump()));
        return;
    }

    form->addRow(label, MakeNonEditableLabel("null"));
}

QWidget* EditorJsonWidget::buildAssetReferenceField(const std::string& path,
                                                     const nlohmann::json& value,
                                                     const InspectorFieldMetadata& metadata) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorAssetReferenceRow"));
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* field = new AssetReferenceField(container);
    auto* clear = new QToolButton(container);
    clear->setObjectName(QStringLiteral("inspectorAssetReferenceClear"));
    clear->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    clear->setToolTip(tr("Clear reference"));
    clear->setFixedSize(30, 30);
    layout->addWidget(field, 1);
    layout->addWidget(clear);
    std::uint64_t guid = 0;
    QString storedPath;
    if (value.is_object()) {
        if (value.contains("asset_id") && value["asset_id"].is_number_unsigned()) {
            guid = value["asset_id"].get<std::uint64_t>();
        } else if (value.contains("file_id") && value["file_id"].is_object()) {
            const auto& fileId = value["file_id"];
            if (fileId.contains("file_uuid") && fileId["file_uuid"].is_number_unsigned()) {
                guid = fileId["file_uuid"].get<std::uint64_t>();
            }
            if (fileId.contains("path") && fileId["path"].is_string()) {
                storedPath = QString::fromStdString(fileId["path"].get<std::string>());
            }
        }
        if (storedPath.isEmpty() && value.contains("legacy_path") && value["legacy_path"].is_string()) {
            storedPath = QString::fromStdString(value["legacy_path"].get<std::string>());
        }
    }

    const AssetBrowserEntry* selected = nullptr;
    for (const auto& asset : m_assets) {
        if ((guid != 0 && asset.uuid == guid) ||
            (!storedPath.isEmpty() && QString::fromStdString(asset.path).endsWith(storedPath))) {
            selected = &asset;
            break;
        }
    }
    const auto setPresentation = [field, clear](const AssetBrowserEntry* asset) {
        clear->setVisible(asset != nullptr);
        if (!asset) {
            field->setText(QObject::tr("None"));
            field->setIcon(QIcon());
            field->setToolTip(QString());
            return;
        }
        field->setText(QString::fromStdString(asset->name));
        field->setToolTip(QStringLiteral("%1\n%2").arg(QString::fromStdString(asset->path),
                                                          QString::fromStdString(asset->type)));
        QImage image(QString::fromStdString(asset->path));
        if (!image.isNull()) {
            field->setIcon(QIcon(QPixmap::fromImage(image.scaled(
                QSize(24, 24), Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        } else {
            field->setIcon(QIcon());
        }
    };
    setPresentation(selected);

    const QString targetType = AssetReferenceTargetType(metadata.typeName);
    QPointer<EditorJsonWidget> owner(this);
    const auto assign = [owner, path, field, targetType, setPresentation](std::uint64_t assetId,
                                                                            const QString& assetPath) {
        if (!owner) {
            return;
        }
        const AssetBrowserEntry* asset = nullptr;
        for (const auto& candidate : owner->m_assets) {
            if (candidate.uuid == assetId) {
                asset = &candidate;
                break;
            }
        }
        if (assetId == 0 || !asset) {
            owner->valueAt(path) = nlohmann::json{{"asset_id", 0}, {"sub_object_id", 0}};
            setPresentation(nullptr);
        } else if (IsAssetCompatible(*asset, targetType)) {
            owner->valueAt(path) = nlohmann::json{{"asset_id", assetId}, {"sub_object_id", 0},
                                                   {"legacy_path", assetPath.toStdString()}};
            setPresentation(asset);
        } else {
            return;
        }
        emit owner->valueChanged();
    };
    connect(clear, &QToolButton::clicked, this, [assign]() { assign(0, QString()); });
    connect(field, &QToolButton::clicked, this, [this, field, targetType, assign]() {
        auto* picker = new AssetPickerPopup(m_assets, targetType, assign);
        picker->move(field->mapToGlobal(QPoint(0, field->height())));
        picker->show();
    });
    field->assigned = [assign](std::uint64_t assetId, const QString& assetPath) {
        assign(assetId, assetPath);
    };
    return container;
}
QWidget* EditorJsonWidget::buildVectorField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorVector"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(6);
    std::vector<QDoubleSpinBox*> spins;
    static constexpr const char* axisNames[] = {"X", "Y", "Z", "W"};
    for (std::size_t index = 0; index < value.size(); ++index) {
        auto* axis = new QLabel(QString::fromLatin1(axisNames[index < 4 ? index : 3]), container);
        axis->setObjectName(QStringLiteral("inspectorAxisLabel"));
        axis->setProperty("axis", QString::fromLatin1(axisNames[index < 4 ? index : 3]).toLower());
        hbox->addWidget(axis);
        auto* spin = MakeDoubleSpinBox(value[index].get<double>());
        InstallNumericScrub(axis, spin);
        spins.push_back(spin);
        hbox->addWidget(spin);
    }
    hbox->addStretch();
    for (auto* spin : spins) {
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, path, spins]() {
            nlohmann::json array = nlohmann::json::array();
            for (auto* s : spins) {
                array.push_back(s->value());
            }
            valueAt(path) = array;
            emit valueChanged();
        });
    }
    return container;
}

QWidget* EditorJsonWidget::buildColorField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorColorField"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(4, 2, 4, 2);
    auto color = std::make_shared<QColor>(ColorFromArray(value));
    auto* swatch = new QPushButton(container);
    swatch->setObjectName(QStringLiteral("inspectorColorSwatch"));
    swatch->setFixedSize(28, 16);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setStyleSheet(SwatchStyle(*color));
    hbox->addWidget(swatch);
    hbox->addStretch();
    const bool hasAlpha = value.size() >= 4;
    connect(swatch, &QPushButton::clicked, this, [this, path, swatch, color, hasAlpha](bool) {
        const QColor picked = QColorDialog::getColor(*color, swatch, tr("Select Color"));
        if (!picked.isValid()) {
            return;
        }
        *color = picked;
        swatch->setStyleSheet(SwatchStyle(*color));
        nlohmann::json array = nlohmann::json::array();
        array.push_back(picked.redF());
        array.push_back(picked.greenF());
        array.push_back(picked.blueF());
        if (hasAlpha) {
            array.push_back(picked.alphaF());
        }
        valueAt(path) = array;
        emit valueChanged();
    });
    return container;
}

QWidget* EditorJsonWidget::buildBoolField(const std::string& path, const nlohmann::json& value) {
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorBoolField"));
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(6, 1, 6, 1);
    hbox->setSpacing(6);
    auto* check = new QCheckBox(container);
    check->setChecked(value.get<bool>());
    hbox->addWidget(check);
    hbox->addStretch();
    connect(check, &QCheckBox::toggled, this, [this, path, check](bool on) {
        valueAt(path) = on;
        emit valueChanged();
    });
    return container;
}

QWidget* EditorJsonWidget::buildLayerField(const std::string& key, const std::string& path,
                                           const nlohmann::json& value) {
    const std::uint32_t current = value.get<std::uint32_t>();
    if (key == "layer") {
        auto* combo = new QComboBox();
        combo->setObjectName(QStringLiteral("inspectorLayerField"));
        for (int i = 0; i < 32; ++i) {
            combo->addItem(QStringLiteral("Layer %1").arg(i), i);
        }
        combo->setCurrentIndex(qBound(0, static_cast<int>(current), 31));
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, path, combo](int index) {
                    if (index >= 0) {
                        valueAt(path) = combo->itemData(index).toUInt();
                        emit valueChanged();
                    }
                });
        return combo;
    }

    auto* button = new QToolButton();
    button->setObjectName(QStringLiteral("inspectorMaskField"));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCursor(Qt::PointingHandCursor);
    auto* menu = new QMenu(button);
    auto bits = std::make_shared<std::uint32_t>(current);
    auto actions = std::make_shared<std::vector<QAction*>>();
    const auto updateText = [button, bits, actions]() {
        if (*bits == 0u) {
            button->setText(QObject::tr("Nothing"));
        } else if (*bits == std::numeric_limits<std::uint32_t>::max()) {
            button->setText(QObject::tr("Everything"));
        } else {
            int selected = 0;
            int selectedLayer = 0;
            for (int i = 0; i < static_cast<int>(actions->size()); ++i) {
                if ((*actions)[i]->isChecked()) {
                    ++selected;
                    selectedLayer = i;
                }
            }
            button->setText(selected == 1 ? QObject::tr("Layer %1").arg(selectedLayer)
                                          : QObject::tr("Mixed..."));
        }
    };
    for (int i = 0; i < 32; ++i) {
        QAction* action = menu->addAction(QObject::tr("Layer %1").arg(i));
        action->setCheckable(true);
        action->setChecked((current & (1u << i)) != 0);
        actions->push_back(action);
        connect(action, &QAction::toggled, this, [this, path, bits, actions, updateText, i](bool checked) {
            if (checked) *bits |= (1u << i);
            else *bits &= ~(1u << i);
            updateText();
            valueAt(path) = *bits;
            emit valueChanged();
        });
    }
    button->setMenu(menu);
    updateText();
    return button;
}

nlohmann::json& EditorJsonWidget::valueAt(const std::string& path) {
    nlohmann::json* node = &m_value;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t end = dot == std::string::npos ? path.size() : dot;
        const std::string segment = path.substr(start, end - start);
        node = &(*node)[segment];
        start = end + 1;
    }
    return *node;
}

} // namespace cakery
