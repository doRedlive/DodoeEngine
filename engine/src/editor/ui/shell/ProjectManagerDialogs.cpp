// do@Redlive

#include "dopch.h"

#include "ProjectManagerDialogs.h"

#include <QGridLayout>
#include <QMouseEvent>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStyle>
#include <QWindow>
#include <utility>
#include <algorithm>

namespace cakery {

QVector<PmTemplate> pmTemplates()
{
    return {
        {QStringLiteral("blank"), QStringLiteral("Blank Project"),
         QStringLiteral("Empty scene with the default pipeline, build from scratch."),
         QStringLiteral("Minimal"), 0.3},
        {QStringLiteral("tps"), QStringLiteral("Third Person"),
         QStringLiteral("Character controller, animation blends and camera follow."),
         QStringLiteral("3D"), 1.8},
        {QStringLiteral("fps"), QStringLiteral("First Person"),
         QStringLiteral("Weapon system, ballistics and hit feedback demo."),
         QStringLiteral("3D"), 2.4},
        {QStringLiteral("2d"), QStringLiteral("2D Platformer"),
         QStringLiteral("Tilemap, sprite animation and collider setup."),
         QStringLiteral("2D"), 0.9},
        {QStringLiteral("render"), QStringLiteral("Render Showroom"),
         QStringLiteral("PBR material ball array and post processing chain."),
         QStringLiteral("Render"), 3.1},
        {QStringLiteral("terrain"), QStringLiteral("Procedural Terrain"),
         QStringLiteral("Noise terrain generation, LOD and ambient occlusion."),
         QStringLiteral("Tech"), 1.5}
    };
}

namespace {

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

QString formatSize(qint64 bytes)
{
    const double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    if (gb < 1.0)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(gb, 0, 'f', 1);
}

QWidget* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("pmFieldLabel"));
    return label;
}

QPushButton* switchRow(const QString& name, const QString& desc, bool checked, QWidget* parent)
{
    auto* row = new QPushButton(parent);
    row->setObjectName(QStringLiteral("pmSwitchRow"));
    row->setCheckable(true);
    row->setChecked(checked);
    row->setCursor(Qt::PointingHandCursor);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(10);

    auto* textColumn = new QVBoxLayout();
    textColumn->setSpacing(3);
    auto* nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("pmSwitchName"));
    auto* descLabel = new QLabel(desc, row);
    descLabel->setObjectName(QStringLiteral("pmSwitchDesc"));
    descLabel->setWordWrap(true);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    descLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    textColumn->addWidget(nameLabel);
    textColumn->addWidget(descLabel);
    layout->addLayout(textColumn, 1);

    auto* pill = new QLabel(row);
    pill->setObjectName(QStringLiteral("pmSwitchPill"));
    pill->setFixedSize(34, 18);
    pill->setProperty("on", checked);
    pill->setAttribute(Qt::WA_TransparentForMouseEvents);
    QObject::connect(row, &QPushButton::toggled, pill, [pill](bool on) {
        pill->setProperty("on", on);
        pill->style()->unpolish(pill);
        pill->style()->polish(pill);
    });
    layout->addWidget(pill, 0, Qt::AlignVCenter);
    return row;
}

} // namespace

IslandDialog::IslandDialog(QWidget* parent, int preferredWidth)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setMinimumWidth(preferredWidth);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_shell = new QFrame(this);
    m_shell->setObjectName(QStringLiteral("pmDialogShell"));
    m_shell->setAttribute(Qt::WA_StyledBackground);
    root->addWidget(m_shell);

    m_shellLayout = new QVBoxLayout(m_shell);
    m_shellLayout->setContentsMargins(0, 0, 0, 0);
    m_shellLayout->setSpacing(0);

    m_head = new QWidget(m_shell);
    m_head->setObjectName(QStringLiteral("pmDlgHead"));
    m_head->installEventFilter(this);

    auto* scroll = new QScrollArea(m_shell);
    scroll->setObjectName(QStringLiteral("pmDlgScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(430);

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("pmDlgBody"));
    m_bodyLayout = new QVBoxLayout(body);
    m_bodyLayout->setContentsMargins(22, 8, 22, 18);
    m_bodyLayout->setSpacing(14);
    scroll->setWidget(body);
    m_shellLayout->addWidget(scroll, 1);
}

void IslandDialog::showEvent(QShowEvent* event)
{
    if (!m_positioned && parentWidget()) {
        m_positioned = true;
        adjustSize();
        const QRect avail = parentWidget()->geometry();
        const int targetWidth = qMin(width(), avail.width() - 64);
        const int targetHeight = qMin(height(), avail.height() - 64);
        resize(targetWidth, targetHeight);
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }
    QDialog::showEvent(event);
}

void IslandDialog::buildHead(const QString& title, const QString& desc)
{
    auto* headLayout = new QHBoxLayout(m_head);
    headLayout->setContentsMargins(22, 16, 14, 12);
    headLayout->setSpacing(12);

    auto* textColumn = new QVBoxLayout();
    textColumn->setSpacing(4);
    auto* titleLabel = new QLabel(title, m_head);
    titleLabel->setObjectName(QStringLiteral("pmDlgTitle"));
    auto* descLabel = new QLabel(desc, m_head);
    descLabel->setObjectName(QStringLiteral("pmDlgDesc"));
    descLabel->setWordWrap(true);
    textColumn->addWidget(titleLabel);
    textColumn->addWidget(descLabel);
    headLayout->addLayout(textColumn);
    headLayout->addStretch();

    auto* close = new QPushButton(m_head);
    close->setObjectName(QStringLiteral("pmDlgClose"));
    close->setFixedSize(28, 28);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    headLayout->addWidget(close, 0, Qt::AlignTop);

    m_shellLayout->insertWidget(0, m_head);
}

QHBoxLayout* IslandDialog::buildFoot(const QString& note)
{
    auto* foot = new QWidget(m_shell);
    foot->setObjectName(QStringLiteral("pmDlgFoot"));
    auto* footLayout = new QHBoxLayout(foot);
    footLayout->setContentsMargins(22, 12, 22, 14);
    footLayout->setSpacing(10);
    auto* noteLabel = new QLabel(note, foot);
    noteLabel->setObjectName(QStringLiteral("pmFootNote"));
    footLayout->addWidget(noteLabel);
    footLayout->addStretch();
    m_shellLayout->addWidget(foot);
    return footLayout;
}

bool IslandDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_head && event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            event->accept();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

DirectoryPickerDialog::DirectoryPickerDialog(QWidget* parent, const QString& startDir)
    : IslandDialog(parent, 560)
{
    buildHead(tr("Select Project Directory"),
              tr("Navigate into the target folder; the project is created inside it."));

    m_dir = startDir.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absolutePath()
        : startDir;
    if (!QDir(m_dir).exists())
        m_dir = QDir::homePath();

    auto* crumbsHost = new QWidget(this);
    crumbsHost->setObjectName(QStringLiteral("pmCrumbs"));
    m_crumbsLayout = new QHBoxLayout(crumbsHost);
    m_crumbsLayout->setContentsMargins(16, 10, 16, 10);
    m_crumbsLayout->setSpacing(4);
    bodyLayout()->addWidget(crumbsHost);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("pmFsList"));
    m_list->setMinimumHeight(260);
    m_list->setMaximumHeight(340);
    bodyLayout()->addWidget(m_list);

    auto* footLayout = buildFoot(QString());
    auto* upButton = new QPushButton(tr("Up One Level"), this);
    upButton->setObjectName(QStringLiteral("pmBtn"));
    auto* newFolderButton = new QPushButton(tr("New Folder"), this);
    newFolderButton->setObjectName(QStringLiteral("pmBtn"));
    m_currentLabel = new QLabel(this);
    m_currentLabel->setObjectName(QStringLiteral("pmFsCur"));
    m_pickButton = new QPushButton(tr("Select This Folder"), this);
    m_pickButton->setObjectName(QStringLiteral("pmPrimaryButton"));
    m_pickButton->setEnabled(false);

    footLayout->addWidget(upButton);
    footLayout->addWidget(newFolderButton);
    footLayout->addWidget(m_currentLabel, 1, Qt::AlignRight);
    footLayout->addWidget(m_pickButton);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item->data(Qt::UserRole + 1).toBool())
            enterPath(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item->data(Qt::UserRole + 1).toBool())
            return;
        m_picked = item->data(Qt::UserRole).toString();
        m_currentLabel->setText(m_picked);
        m_pickButton->setEnabled(true);
    });
    connect(upButton, &QPushButton::clicked, this, [this] {
        const QDir dir(m_dir);
        if (!dir.isRoot())
            enterPath(QFileInfo(m_dir).absolutePath());
    });
    connect(newFolderButton, &QPushButton::clicked, this, [this] {
        QDir dir(m_dir);
        QString name = tr("New Folder");
        int index = 1;
        while (dir.exists(name)) {
            ++index;
            name = tr("New Folder") + QStringLiteral(" %1").arg(index);
        }
        if (dir.mkpath(name))
            refreshList();
    });
    connect(m_pickButton, &QPushButton::clicked, this, [this] {
        if (!m_picked.isEmpty())
            accept();
    });

    refreshList();
}

void DirectoryPickerDialog::enterPath(const QString& path)
{
    m_dir = path;
    m_picked.clear();
    m_pickButton->setEnabled(false);
    m_currentLabel->clear();
    refreshList();
}

void DirectoryPickerDialog::refreshList()
{
    QDir dir(m_dir);
    m_list->clear();

    while (m_crumbsLayout->count() > 0) {
        if (auto* item = m_crumbsLayout->takeAt(0)) {
            if (auto* widget = item->widget())
                widget->deleteLater();
            delete item;
        }
    }
    const QStringList segments = m_dir.split(QDir::separator(), Qt::SkipEmptyParts);
    QString prefix;
    for (int i = 0; i < segments.size(); ++i) {
        prefix += (i == 0) ? segments[i] : (QDir::separator() + segments[i]);
        QString target = prefix;
        if (i == 0 && segments[0].endsWith(QLatin1Char(':')))
            target += QDir::separator();
        auto* crumb = new QPushButton(segments[i], this);
        crumb->setObjectName(QStringLiteral("pmCrumb"));
        crumb->setCursor(Qt::PointingHandCursor);
        connect(crumb, &QPushButton::clicked, this, [this, target] { enterPath(target); });
        m_crumbsLayout->addWidget(crumb);
        if (i < segments.size() - 1) {
            auto* slash = new QLabel(QDir::separator(), this);
            slash->setObjectName(QStringLiteral("pmCrumbSlash"));
            m_crumbsLayout->addWidget(slash);
        }
    }

    const QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    const QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : dirs) {
        auto* item = new QListWidgetItem(info.fileName(), m_list);
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setData(Qt::UserRole + 1, true);
        item->setForeground(QColor(0xBD, 0xC5, 0xD1));
    }
    for (const QFileInfo& info : files) {
        auto* item = new QListWidgetItem(info.fileName(), m_list);
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setData(Qt::UserRole + 1, false);
        item->setForeground(QColor(0x6E, 0x78, 0x86));
    }
    if (m_list->count() == 0) {
        auto* item = new QListWidgetItem(tr("This folder is empty"), m_list);
        item->setForeground(QColor(0x6E, 0x78, 0x86));
        Qt::ItemFlags flags = item->flags();
        flags &= ~Qt::ItemIsEnabled;
        item->setFlags(flags);
    }

    m_currentLabel->setText(m_picked.isEmpty() ? m_dir : m_picked);
}

NewProjectDialog::NewProjectDialog(QWidget* parent, const QStringList& existingNames,
                                   const QString& defaultParentDir)
    : IslandDialog(parent, 920),
      m_existingNames(existingNames),
      m_baseDir(defaultParentDir)
{
    buildHead(tr("New Project"),
              tr("Pick a template and location; a new entry appears in your library."));

    auto* nameField = new QVBoxLayout();
    nameField->setSpacing(6);
    nameField->addWidget(fieldLabel(tr("Project name"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("MyFirstProject"));
    nameField->addWidget(m_nameEdit);
    m_nameHint = new QLabel(tr("Used to create the project folder and project file."), this);
    m_nameHint->setObjectName(QStringLiteral("pmHint"));
    nameField->addWidget(m_nameHint);
    bodyLayout()->addLayout(nameField);

    auto* pathField = new QVBoxLayout();
    pathField->setSpacing(6);
    pathField->addWidget(fieldLabel(tr("Project directory"), this));
    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText(m_baseDir);
    pathRow->addWidget(m_pathEdit, 1);
    auto* browseButton = new QPushButton(tr("Browse"), this);
    browseButton->setObjectName(QStringLiteral("pmBtn"));
    pathRow->addWidget(browseButton);
    pathField->addLayout(pathRow);
    m_pathHint = new QLabel(tr("The directory is created on confirmation."), this);
    m_pathHint->setObjectName(QStringLiteral("pmHint"));
    pathField->addWidget(m_pathHint);
    bodyLayout()->addLayout(pathField);

    bodyLayout()->addWidget(fieldLabel(tr("Project template"), this));

    auto* templateGrid = new QGridLayout();
    templateGrid->setSpacing(8);
    m_templateGroup = new QButtonGroup(this);
    m_templateGroup->setExclusive(true);
    const QVector<PmTemplate> templates = pmTemplates();
    for (int i = 0; i < templates.size(); ++i) {
        const PmTemplate& tpl = templates[i];
        auto* button = new QPushButton(tpl.name + QStringLiteral("\n") + tpl.desc, this);
        button->setObjectName(QStringLiteral("pmTpl"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tpl.tags);
        m_templateGroup->addButton(button, i);
        templateGrid->addWidget(button, i / 2, i % 2);
        if (i == 0)
            button->setChecked(true);
    }
    bodyLayout()->addLayout(templateGrid);

    auto* summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("pmSummary"));
    auto* summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    summaryLayout->setSpacing(8);
    auto* sumKey = new QLabel(tr("Template"), summary);
    sumKey->setObjectName(QStringLiteral("pmSumKey"));
    m_sumTemplate = new QLabel(summary);
    m_sumTemplate->setObjectName(QStringLiteral("pmSumValue"));
    auto* sizeKey = new QLabel(tr("Estimated size"), summary);
    sizeKey->setObjectName(QStringLiteral("pmSumKey"));
    m_sumSize = new QLabel(summary);
    m_sumSize->setObjectName(QStringLiteral("pmSumValue"));
    summaryLayout->addWidget(sumKey);
    summaryLayout->addWidget(m_sumTemplate);
    summaryLayout->addStretch();
    summaryLayout->addWidget(sizeKey);
    summaryLayout->addWidget(m_sumSize);
    bodyLayout()->addWidget(summary);

    auto* footLayout = buildFoot(tr("The project is created immediately on confirm."));
    auto* cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName(QStringLiteral("pmBtn"));
    auto* create = new QPushButton(tr("Create Project"), this);
    create->setObjectName(QStringLiteral("pmPrimaryButton"));
    create->setCursor(Qt::PointingHandCursor);
    footLayout->addWidget(cancel);
    footLayout->addWidget(create);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this] { syncPathFromName(); });
    connect(m_pathEdit, &QLineEdit::textChanged, this, [this] { m_pathTouched = true; });
    connect(browseButton, &QPushButton::clicked, this, [this] {
        DirectoryPickerDialog picker(this, QFileInfo(m_pathEdit->text()).absolutePath());
        if (picker.exec() == QDialog::Accepted && !picker.selectedPath().isEmpty()) {
            m_baseDir = picker.selectedPath() + QDir::separator();
            m_pathEdit->setText(m_baseDir + projectName());
            m_pathTouched = false;
        }
    });
    connect(m_templateGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const QVector<PmTemplate> templates = pmTemplates();
        if (id >= 0 && id < templates.size()) {
            m_sumTemplate->setText(templates[id].name);
            m_sumSize->setText(QStringLiteral("%1 GB").arg(templates[id].sizeGb, 0, 'f', 1));
        }
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(create, &QPushButton::clicked, this, [this] { validate(); });

    const PmTemplate first = pmTemplates().first();
    m_sumTemplate->setText(first.name);
    m_sumSize->setText(QStringLiteral("%1 GB").arg(first.sizeGb, 0, 'f', 1));
}

QString NewProjectDialog::projectName() const
{
    return m_nameEdit->text().trimmed();
}

QString NewProjectDialog::projectPath() const
{
    return m_pathEdit->text().trimmed();
}

QString NewProjectDialog::templateId() const
{
    const int id = m_templateGroup->checkedId();
    const QVector<PmTemplate> templates = pmTemplates();
    return (id >= 0 && id < templates.size()) ? templates[id].id : QStringLiteral("blank");
}

void NewProjectDialog::syncPathFromName()
{
    if (m_pathTouched)
        return;
    QString name = projectName();
    if (name.isEmpty())
        name = tr("New Project");
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    m_pathEdit->setText(m_baseDir + name);
}

void NewProjectDialog::validate()
{
    const QString name = projectName();
    const QString path = projectPath();
    QString problem;
    if (name.isEmpty()) {
        problem = tr("Please enter a project name.");
    } else if (QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")).match(name).hasMatch()) {
        problem = tr("The name cannot contain / \\ : * ? \" < > |");
    } else {
        for (const QString& existing : m_existingNames) {
            if (existing.compare(name, Qt::CaseInsensitive) == 0) {
                problem = tr("A project with this name already exists.");
                break;
            }
        }
    }
    if (problem.isEmpty() && path.isEmpty())
        problem = tr("Please choose a project directory.");

    if (!problem.isEmpty()) {
        m_nameHint->setText(problem);
        m_nameHint->setProperty("bad", true);
        repolish(m_nameHint);
        return;
    }
    m_nameHint->setProperty("bad", false);
    repolish(m_nameHint);
    m_nameHint->setText(tr("Used to create the project folder and project file."));
    accept();
}

ProjectSettingsDialog::ProjectSettingsDialog(QWidget* parent, ProjectEntry& entry)
    : IslandDialog(parent, 660)
    , m_entry(entry)
{
    m_configPath = QDir(m_entry.projectPath).filePath(QStringLiteral("app_config.json"));

    QJsonObject root;
    QFile file(m_configPath);
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    const QJsonObject renderSettings = root[QStringLiteral("render_settings")].toObject();
    const QString name = root[QStringLiteral("name")].toString(QStringLiteral("Dodoe Engine"));
    const int appMode = root[QStringLiteral("app_mode")].toInt(0);
    const int engineMode = root[QStringLiteral("engine_mode")].toInt(0);
    const int width = root[QStringLiteral("width")].toInt(1920);
    const int height = root[QStringLiteral("height")].toInt(1080);
    const bool resizeable = root[QStringLiteral("window_resizeable")].toBool(true);
    const int api = renderSettings[QStringLiteral("api")].toInt(3);
    const int pipeline = renderSettings[QStringLiteral("pipeline")].toInt(3);
    const int presentMode = renderSettings[QStringLiteral("present_mode")].toInt(1);

    buildHead(tr("Project Settings"), m_entry.name);

    auto makeSegment = [this](const QString& label, const QStringList& items, int checkedId) {
        auto* column = new QVBoxLayout();
        column->setSpacing(6);
        column->addWidget(fieldLabel(label, this));
        auto* host = new QFrame(this);
        host->setObjectName(QStringLiteral("pmSeg"));
        auto* layout = new QHBoxLayout(host);
        layout->setContentsMargins(3, 3, 3, 3);
        layout->setSpacing(3);
        auto* group = new QButtonGroup(host);
        group->setExclusive(true);
        for (int i = 0; i < items.size(); ++i) {
            auto* button = new QPushButton(items[i], host);
            button->setObjectName(QStringLiteral("pmSegBtn"));
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            group->addButton(button, i);
            layout->addWidget(button, 1);
            button->setChecked(i == checkedId);
        }
        if (group->checkedId() < 0 && layout->count() > 0) {
            if (auto* button = qobject_cast<QPushButton*>(layout->itemAt(0)->widget()))
                button->setChecked(true);
        }
        column->addWidget(host);
        return std::make_pair(column, group);
    };

    bodyLayout()->addWidget(fieldLabel(tr("Window title"), this));
    m_nameEdit = new QLineEdit(name, this);
    bodyLayout()->addWidget(m_nameEdit);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(320, 7680);
    m_widthSpin->setValue(width);
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(240, 4320);
    m_heightSpin->setValue(height);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(14);
    auto* widthColumn = new QVBoxLayout();
    widthColumn->setSpacing(6);
    widthColumn->addWidget(fieldLabel(tr("Width"), this));
    widthColumn->addWidget(m_widthSpin);
    auto* heightColumn = new QVBoxLayout();
    heightColumn->setSpacing(6);
    heightColumn->addWidget(fieldLabel(tr("Height"), this));
    heightColumn->addWidget(m_heightSpin);
    sizeRow->addLayout(widthColumn, 1);
    sizeRow->addLayout(heightColumn, 1);
    sizeRow->addStretch(2);
    bodyLayout()->addLayout(sizeRow);

    m_resizeableRow = switchRow(tr("Resizable window"),
                                tr("Allow resizing the application window."),
                                resizeable, this);
    bodyLayout()->addWidget(m_resizeableRow);
    bodyLayout()->addSpacing(6);

    auto appModePair = makeSegment(tr("App mode"),
        {tr("Game"), tr("Sandbox"), tr("Editor"), tr("Server")}, appMode);
    bodyLayout()->addLayout(appModePair.first);
    bodyLayout()->addSpacing(2);

    auto engineModePair = makeSegment(tr("Engine mode"),
        {tr("Full"), tr("TwoD"), tr("GUI")}, engineMode);
    bodyLayout()->addLayout(engineModePair.first);
    bodyLayout()->addSpacing(2);

    auto apiPair = makeSegment(tr("Render backend"),
        {tr("None"), tr("OpenGL"), tr("Vulkan"), tr("D3D12")}, api);
    auto presentPair = makeSegment(tr("Present mode"),
        {tr("VSync"), tr("Mailbox"), tr("Immediate")}, presentMode);
    auto* renderRow = new QHBoxLayout();
    renderRow->setSpacing(14);
    renderRow->addLayout(apiPair.first, 4);
    renderRow->addLayout(presentPair.first, 3);
    bodyLayout()->addLayout(renderRow);
    m_appModeGroup = appModePair.second;
    m_engineModeGroup = engineModePair.second;
    m_apiGroup = apiPair.second;
    m_presentGroup = presentPair.second;
    bodyLayout()->addSpacing(2);

    auto* pipelineColumn = new QVBoxLayout();
    pipelineColumn->setSpacing(6);
    pipelineColumn->addWidget(fieldLabel(tr("Render pipeline"), this));
    m_pipelineCombo = new QComboBox(this);
    m_pipelineCombo->addItems({tr("None"), tr("Forward"), tr("Forward+"), tr("Deferred"),
                               tr("Deferred+"), tr("Only 2D"), tr("Only GUI")});
    m_pipelineCombo->setCurrentIndex(qBound(0, pipeline, 6));
    pipelineColumn->addWidget(m_pipelineCombo);
    bodyLayout()->addLayout(pipelineColumn);
    bodyLayout()->addSpacing(6);

    bodyLayout()->addWidget(fieldLabel(tr("Render options"), this));
    m_singleThreadRow = switchRow(tr("Single-threaded rendering"),
                                  tr("Drive the renderer from a single thread."),
                                  renderSettings[QStringLiteral("enable_single_thread")].toBool(false), this);
    m_baselineRow = switchRow(tr("Baseline renderer layer"),
                              tr("Enable the reference renderer path for debugging."),
                              renderSettings[QStringLiteral("enable_baseline_renderer")].toBool(false), this);
    m_windowlessRow = switchRow(tr("Windowless rendering"),
                                tr("Render without creating an application window."),
                                renderSettings[QStringLiteral("windowless")].toBool(false), this);
    m_archiveRow = switchRow(tr("Archived"),
                             tr("Hide from Recent and list it under Archived."),
                             m_entry.archived, this);
    bodyLayout()->addWidget(m_singleThreadRow);
    bodyLayout()->addWidget(m_baselineRow);
    bodyLayout()->addWidget(m_windowlessRow);
    bodyLayout()->addWidget(m_archiveRow);

    auto* meta = new QLabel(this);
    meta->setObjectName(QStringLiteral("pmMeta"));
    const QFileInfo configInfo(m_configPath);
    meta->setText(configInfo.exists()
        ? tr("Config file %1  ·  Size %2  ·  Modified %3")
            .arg(configInfo.fileName(),
                 formatSize(configInfo.size()),
                 configInfo.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")))
        : tr("app_config.json will be created on save."));
    bodyLayout()->addWidget(meta);

    auto* footLayout = buildFoot(tr("Saved into app_config.json; applies on next launch."));
    auto* cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName(QStringLiteral("pmBtn"));
    auto* save = new QPushButton(tr("Save Settings"), this);
    save->setObjectName(QStringLiteral("pmPrimaryButton"));
    save->setCursor(Qt::PointingHandCursor);
    footLayout->addWidget(cancel);
    footLayout->addWidget(save);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this] {
        m_archived = m_archiveRow->isChecked();
        writeAppConfig();
        accept();
    });
}

void ProjectSettingsDialog::writeAppConfig()
{
    QFile file(m_configPath);
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    root[QStringLiteral("name")] = m_nameEdit->text().trimmed();
    root[QStringLiteral("app_mode")] = qMax(0, m_appModeGroup->checkedId());
    root[QStringLiteral("engine_mode")] = qMax(0, m_engineModeGroup->checkedId());
    root[QStringLiteral("width")] = m_widthSpin->value();
    root[QStringLiteral("height")] = m_heightSpin->value();
    root[QStringLiteral("window_resizeable")] = m_resizeableRow->isChecked();

    QJsonObject renderSettings = root[QStringLiteral("render_settings")].toObject();
    renderSettings[QStringLiteral("api")] = qMax(0, m_apiGroup->checkedId());
    renderSettings[QStringLiteral("pipeline")] = qBound(0, m_pipelineCombo->currentIndex(), 6);
    renderSettings[QStringLiteral("present_mode")] = qMax(0, m_presentGroup->checkedId());
    renderSettings[QStringLiteral("enable_single_thread")] = m_singleThreadRow->isChecked();
    renderSettings[QStringLiteral("enable_baseline_renderer")] = m_baselineRow->isChecked();
    renderSettings[QStringLiteral("windowless")] = m_windowlessRow->isChecked();
    root[QStringLiteral("render_settings")] = renderSettings;

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}

} // namespace cakery
