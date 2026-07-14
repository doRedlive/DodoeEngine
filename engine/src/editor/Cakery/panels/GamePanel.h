// do@Redlive

#pragma once

#include "Panel.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

namespace cakery {

struct EditorViewport;

class GamePanel : public Panel {
    Q_OBJECT
public:
    explicit GamePanel(EditorContext& ctx, QWidget* parent = nullptr);
    ~GamePanel() override;

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void buildToolbar();
    void onAspectChanged(int index);

    EditorViewport* m_vp = nullptr;
    QComboBox* m_aspectCombo = nullptr;
    QCheckBox* m_maximizeCheck = nullptr;
    QLabel* m_noCameraLabel = nullptr;
    bool m_firstShow = true;
};

} // namespace cakery
