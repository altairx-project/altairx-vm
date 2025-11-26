#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include <utilities.hpp>

#include <QFileDialog>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow{parent}
    , ui{new Ui::MainWindow}
    , m_runner{new VMRunner{this}}
{
    ui->setupUi(this);

    ui->menuView->addAction(ui->registerViewDock->toggleViewAction());

    auto* pauseRunAction = new QAction{QIcon{":/icons/play.png"}, "Play"};
    pauseRunAction->setEnabled(false);
    connect(pauseRunAction, &QAction::triggered, this, []{});
    ui->toolBar->addAction(pauseRunAction);

    connect(ui->actionLoad_ELF_program, &QAction::triggered, this, &MainWindow::loadHostedProgram);

    // Runner handle the communication and thread coherency with the core, nothing to worry about here.
    connect(m_runner, &VMRunner::statusChanged, this, &MainWindow::statusChanged);
    connect(m_runner, &VMRunner::loadingError, this, &MainWindow::loadingError);
    connect(m_runner, &VMRunner::corePanic, this, &MainWindow::corePanic);
    connect(m_runner, &VMRunner::coreError, this, &MainWindow::coreError);
    connect(m_runner, &VMRunner::syscall, this, &MainWindow::syscall);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadHostedProgram()
{
    const auto fileName = QFileDialog::getOpenFileName(this,
        tr("Open program"), {}, tr("ELF Files (*)"));

    m_runner->loadHostedProgram(fileName, {});
    m_runner->start(true);
}

void MainWindow::loadingError(QString error)
{
    statusBar()->showMessage(error);
}

void MainWindow::corePanic(QString error)
{
    statusBar()->showMessage(error);
}

void MainWindow::coreError(int code)
{
    statusBar()->showMessage(QString{"Core stopped with error code #%1"}.arg(code));
}

void MainWindow::statusChanged(VMRunner::Status status)
{
    ui->label->setText(toString(status));
}

void MainWindow::syscall()
{
    statusBar()->showMessage(QString{"Syscall!"});
}
