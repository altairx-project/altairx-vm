#ifndef MAINWINDOW_HPP_INCLUDED
#define MAINWINDOW_HPP_INCLUDED

#include <QMainWindow>

#include "vm/runner.hpp"

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void loadHostedProgram();

    void statusChanged(VMRunner::Status status);
    void loadingError(QString error);
    void corePanic(QString error);
    void coreError(int code);
    void syscall();

private:
    Ui::MainWindow* ui;
    VMRunner* m_runner{};
};
#endif // MAINWINDOW_HPP
