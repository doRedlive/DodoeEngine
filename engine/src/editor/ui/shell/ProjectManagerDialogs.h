// do@Redlive
#pragma once
#include "dopch.h"

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "services/ProjectConfig.h"

namespace cakery {

struct PmTemplate {
    QString id;
    QString name;
    QString desc;
    QString tags;
    double sizeGb = 0.0;
};

QVector<PmTemplate> pmTemplates();

class IslandDialog : public QDialog {
    Q_OBJECT
public:
    IslandDialog(QWidget* parent, int preferredWidth);

    QVBoxLayout* bodyLayout() const { return m_bodyLayout; }
    void buildHead(const QString& title, const QString& desc);
    QHBoxLayout* buildFoot(const QString& note);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QFrame* m_shell = nullptr;
    QVBoxLayout* m_shellLayout = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
    QWidget* m_head = nullptr;
    bool m_positioned = false;
};

class DirectoryPickerDialog : public IslandDialog {
    Q_OBJECT
public:
    DirectoryPickerDialog(QWidget* parent, const QString& startDir);

    QString selectedPath() const { return m_picked; }

private:
    void enterPath(const QString& path);
    void refreshList();

    QString m_dir;
    QString m_picked;
    QHBoxLayout* m_crumbsLayout = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_currentLabel = nullptr;
    QPushButton* m_pickButton = nullptr;
};

class NewProjectDialog : public IslandDialog {
    Q_OBJECT
public:
    NewProjectDialog(QWidget* parent, const QStringList& existingNames,
                     const QString& defaultParentDir);

    QString projectName() const;
    QString projectPath() const;
    QString templateId() const;

private:
    void syncPathFromName();
    void validate();

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_nameHint = nullptr;
    QLabel* m_pathHint = nullptr;
    QLabel* m_sumTemplate = nullptr;
    QLabel* m_sumSize = nullptr;
    QButtonGroup* m_templateGroup = nullptr;
    QStringList m_existingNames;
    QString m_baseDir;
    bool m_pathTouched = false;
};

class ProjectSettingsDialog : public IslandDialog {
    Q_OBJECT
public:
    ProjectSettingsDialog(QWidget* parent, ProjectEntry& entry);

    bool archived() const { return m_archived; }

private:
    void writeAppConfig();

    ProjectEntry& m_entry;
    QString m_configPath;
    QLineEdit* m_nameEdit = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QPushButton* m_resizeableRow = nullptr;
    QButtonGroup* m_appModeGroup = nullptr;
    QButtonGroup* m_engineModeGroup = nullptr;
    QButtonGroup* m_apiGroup = nullptr;
    QComboBox* m_pipelineCombo = nullptr;
    QButtonGroup* m_presentGroup = nullptr;
    QPushButton* m_singleThreadRow = nullptr;
    QPushButton* m_baselineRow = nullptr;
    QPushButton* m_windowlessRow = nullptr;
    QPushButton* m_archiveRow = nullptr;
    bool m_archived = false;
};

} // namespace cakery
