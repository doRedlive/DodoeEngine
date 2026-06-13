#pragma once

#include <QWidget>

namespace cakery {

class CameraController;

class SceneWidget : public QWidget {
    Q_OBJECT
public:
    explicit SceneWidget(QWidget* parent = nullptr);
    ~SceneWidget() override;

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void setupCamera();

    CameraController* m_camera = nullptr;
    bool m_firstShow = true;
};

} // namespace cakery
