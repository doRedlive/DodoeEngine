// do@Redlive

#pragma once

#include <QApplication>
#include <memory>

namespace cakery {

class MainWindow;

class CakeryApplication : public QApplication {
    Q_OBJECT
public:
    CakeryApplication(int& argc, char** argv);
    ~CakeryApplication();

    int run();

private:
    MainWindow* m_mainWindow = nullptr;
};

} // namespace cakery
