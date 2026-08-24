// do@Redlive

#include "ProjectManagerWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>

namespace cakery {

ProjectManagerWindow::ProjectManagerWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Cakery Engine - Project Manager");
    setObjectName("ProjectManagerWindow");
    resize(960, 640);
    setMinimumSize(720, 480);

    auto* mainLayout = new QVBoxLayout(this);

    // Header
    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Cakery Engine", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setObjectName("projectManagerTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({"Last Edited", "Name", "Path"});
    headerLayout->addWidget(m_sortCombo);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Filter projects...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(180);
    headerLayout->addWidget(m_searchEdit);

    auto* newBtn = new QPushButton("New", this);
    newBtn->setObjectName("projectPrimaryButton");
    headerLayout->addWidget(newBtn);

    auto* importBtn = new QPushButton("Import", this);
    headerLayout->addWidget(importBtn);

    auto* scanBtn = new QPushButton("Scan", this);
    headerLayout->addWidget(scanBtn);

    mainLayout->addLayout(headerLayout);

    // Main content
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_projectList = new QListWidget(this);
    m_projectList->setObjectName("projectList");
    m_projectList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    splitter->addWidget(m_projectList);

    m_detailPanel = new QWidget(this);
    m_detailPanel->setObjectName("projectDetailPanel");
    auto* detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(24, 24, 24, 24);

    m_detailName = new QLabel("No Project Selected", m_detailPanel);
    QFont nameFont = m_detailName->font();
    nameFont.setPointSize(18);
    nameFont.setBold(true);
    m_detailName->setFont(nameFont);
    m_detailName->setObjectName("projectDetailName");
    m_detailName->setWordWrap(true);
    detailLayout->addWidget(m_detailName);

    m_detailDesc = new QLabel("", m_detailPanel);
    m_detailDesc->setObjectName("projectDetailDescription");
    m_detailDesc->setWordWrap(true);
    detailLayout->addWidget(m_detailDesc);

    detailLayout->addSpacing(16);

    m_detailPath = new QLabel("", m_detailPanel);
    m_detailPath->setObjectName("projectDetailPath");
    m_detailPath->setWordWrap(true);
    detailLayout->addWidget(m_detailPath);

    m_detailDate = new QLabel("", m_detailPanel);
    m_detailDate->setObjectName("projectDetailDate");
    detailLayout->addWidget(m_detailDate);

    detailLayout->addStretch();

    m_openBtn = new QPushButton("Open", m_detailPanel);
    m_openBtn->setObjectName("projectPrimaryButton");
    m_openBtn->setFixedHeight(36);
    m_openBtn->setEnabled(false);
    detailLayout->addWidget(m_openBtn);

    auto* showInExplorerBtn = new QPushButton("Show in Explorer", m_detailPanel);
    showInExplorerBtn->setFlat(true);
    showInExplorerBtn->setObjectName("projectFlatButton");
    detailLayout->addWidget(showInExplorerBtn);

    splitter->addWidget(m_detailPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter, 1);

    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProjectManagerWindow::onSearchChanged);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectManagerWindow::onSortChanged);
    connect(m_projectList, &QListWidget::currentRowChanged, this, [this](int) { onProjectSelected(); });
    connect(m_openBtn, &QPushButton::clicked, this, &ProjectManagerWindow::onOpenProject);
    connect(newBtn, &QPushButton::clicked, this, &ProjectManagerWindow::onNewProject);
    connect(importBtn, &QPushButton::clicked, this, &ProjectManagerWindow::onImportProject);
    connect(scanBtn, &QPushButton::clicked, this, &ProjectManagerWindow::onScanFolder);

    // Load projects
    ProjectConfig::getInstance().load();
    rebuildList();
}

void ProjectManagerWindow::rebuildList()
{
    m_filtered.clear();
    auto& cfg = ProjectConfig::getInstance();
    QString search = m_searchEdit->text().trimmed();

    for (const auto& proj : cfg.projects()) {
        if (!search.isEmpty() &&
            !proj.name.contains(search, Qt::CaseInsensitive) &&
            !proj.projectFile.contains(search, Qt::CaseInsensitive))
            continue;
        m_filtered.append(proj);
    }

    // Sort
    int sort = m_sortCombo->currentIndex();
    std::sort(m_filtered.begin(), m_filtered.end(), [sort](const ProjectEntry& a, const ProjectEntry& b) {
        if (sort == 1) return a.name < b.name;
        if (sort == 2) return a.projectFile < b.projectFile;
        return a.lastModifiedUnix > b.lastModifiedUnix;
    });

    m_projectList->clear();
    for (const auto& proj : m_filtered) {
        auto* item = new QListWidgetItem(proj.name + "\n" + proj.projectFile);
        item->setData(Qt::UserRole, m_filtered.indexOf(proj)); // Index into m_filtered
        if (proj.isMissing)
            item->setForeground(QColor(150, 80, 80));
        m_projectList->addItem(item);
    }

    m_selectedIndex = -1;
    updateDetailPanel();
}

void ProjectManagerWindow::updateDetailPanel()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_filtered.size()) {
        m_detailName->setText("No Project Selected");
        m_detailDesc->setText("Select a project from the list or create a new one.");
        m_detailPath->setText("");
        m_detailDate->setText("");
        m_openBtn->setEnabled(false);
        return;
    }

    const auto& proj = m_filtered[m_selectedIndex];
    m_detailName->setText(proj.name);
    m_detailDesc->setText(proj.description);
    m_detailPath->setText("Location: " + proj.projectPath);
    m_detailDate->setText("Modified: " + proj.displayDate());
    m_openBtn->setEnabled(!proj.isMissing);
}

void ProjectManagerWindow::onProjectSelected()
{
    auto* item = m_projectList->currentItem();
    if (!item) {
        m_selectedIndex = -1;
    } else {
        m_selectedIndex = item->data(Qt::UserRole).toInt();
    }
    updateDetailPanel();
}

void ProjectManagerWindow::onSearchChanged(const QString&)
{
    rebuildList();
}

void ProjectManagerWindow::onSortChanged(int)
{
    rebuildList();
}

void ProjectManagerWindow::onOpenProject()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_filtered.size()) return;
    const auto& proj = m_filtered[m_selectedIndex];
    if (proj.isMissing) return;
    if (!QFileInfo::exists(proj.projectFile)) return;

    ProjectConfig::getInstance().markOpened(proj.projectFile);
    ProjectConfig::getInstance().save();

    emit onProjectOpened(proj.projectFile);
    accept();
}

void ProjectManagerWindow::onNewProject()
{
    QString name = "NewProject";
    QString location = QFileDialog::getExistingDirectory(this, "Select Project Location",
                                                          QDir::homePath());
    if (location.isEmpty()) return;

    QString fullDir = QDir(location).filePath(name);
    QDir().mkpath(QDir(fullDir).filePath("Assets/Scenes"));
    QDir().mkpath(QDir(fullDir).filePath("Binaries"));
    QDir().mkpath(QDir(fullDir).filePath("Configs"));

    QString projFile = QDir(fullDir).filePath(name + ".doproj");
    QJsonObject projObj;
    projObj["Name"] = name;
    projObj["Version"] = "1.0";
    projObj["ProjectPath"] = name + ".doproj";
    projObj["AssetDirectory"] = "Assets";
    projObj["StartSceneName"] = "Main";

    QJsonObject root;
    root["Project"] = projObj;

    QFile file(projFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }

    // Create the start scene file
    QJsonObject sceneRoot;
    sceneRoot["m_name"] = "Main";
    QJsonArray entities;
    QJsonObject cameraEntity;
    cameraEntity["m_uuid"] = 1;
    cameraEntity["m_name"] = "Primary Camera";
    QJsonArray camComponents;
    QJsonObject tagComp;
    tagComp["m_type_name"] = "TagComponent";
    tagComp["m_component"] = "{\"tag\":\"PrimaryCamera\"}";
    camComponents.append(tagComp);
    QJsonObject transComp;
    transComp["m_type_name"] = "TransformComponent";
    transComp["m_component"] = "{\"position\":[0.0,0.0,0.0],\"rotation\":[0.0,0.0,0.0],\"scale\":[1.0,1.0,1.0]}";
    camComponents.append(transComp);
    QJsonObject cam2dComp;
    cam2dComp["m_type_name"] = "CameraComponent";
    cam2dComp["m_component"] = "{\"type\":0,\"zoom\":1.0,\"background\":[1.0,1.0,1.0,1.0]}";
    camComponents.append(cam2dComp);
    cameraEntity["m_native_components"] = camComponents;
    cameraEntity["m_managed_components"] = QJsonArray();
    entities.append(cameraEntity);
    sceneRoot["m_entities"] = entities;

    QFile sceneFile(QDir(fullDir).filePath("Assets/Scenes/Main.doscn"));
    if (sceneFile.open(QIODevice::WriteOnly)) {
        sceneFile.write(QJsonDocument(sceneRoot).toJson(QJsonDocument::Indented));
        sceneFile.close();
    }

    ProjectEntry entry;
    entry.name = name;
    entry.projectFile = projFile;
    entry.lastOpenedUnix = QDateTime::currentSecsSinceEpoch();
    entry.lastModifiedUnix = QDateTime::currentSecsSinceEpoch();
    entry.projectPath = fullDir;
    entry.isMissing = false;

    ProjectConfig::getInstance().addProject(entry);
    ProjectConfig::getInstance().markOpened(projFile);
    ProjectConfig::getInstance().save();

    emit onProjectOpened(projFile);
    accept();
}

void ProjectManagerWindow::onImportProject()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Import .doproj File",
                                                     QDir::homePath(),
                                                     "Dodoe Project (*.doproj)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    ProjectEntry entry;
    entry.name = fi.completeBaseName();
    entry.projectFile = fi.absoluteFilePath();
    entry.lastModifiedUnix = fi.lastModified().toSecsSinceEpoch();
    entry.projectPath = fi.absolutePath();
    entry.isMissing = false;

    ProjectConfig::getInstance().addProject(entry);
    ProjectConfig::getInstance().save();
    rebuildList();
}

void ProjectManagerWindow::onScanFolder()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Scan Folder for Projects");
    if (folder.isEmpty()) return;

    QDirIterator it(folder, {"*.doproj"}, QDir::Files, QDirIterator::Subdirectories);
    int added = 0;
    while (it.hasNext()) {
        it.next();
        QFileInfo fi(it.filePath());
        ProjectEntry entry;
        entry.name = fi.completeBaseName();
        entry.projectFile = fi.absoluteFilePath();
        entry.lastModifiedUnix = fi.lastModified().toSecsSinceEpoch();
        entry.projectPath = fi.absolutePath();
        entry.isMissing = false;
        ProjectConfig::getInstance().addProject(entry);
        added++;
    }

    ProjectConfig::getInstance().save();
    rebuildList();
}

void ProjectManagerWindow::onRefresh()
{
    ProjectConfig::getInstance().load();
    rebuildList();
}

} // namespace cakery
