#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include <core.hpp>
#include <opcode.hpp>
#include <memory.hpp>

#include <QFileDialog>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow{parent}
    , ui{new Ui::MainWindow}
    , m_runner{new VMRunner{this}}
{
    ui->setupUi(this);

    ui->registerViewDock->hide();
    ui->memoryViewDock->hide();
    ui->callStackViewDock->hide();

    ui->menuView->addAction(ui->consoleDock->toggleViewAction());
    ui->menuView->addAction(ui->registerViewDock->toggleViewAction());
    ui->menuView->addAction(ui->memoryViewDock->toggleViewAction());
    ui->menuView->addAction(ui->callStackViewDock->toggleViewAction());

    // Play/pause button
    QIcon playPauseIcon{};
    playPauseIcon.addFile(":/icons/play.png", QSize{}, QIcon::Mode::Normal, QIcon::State::Off);
    playPauseIcon.addFile(":/icons/pause.png", QSize{}, QIcon::Mode::Normal, QIcon::State::On);
    m_playPauseAction = new QAction{playPauseIcon, "Play"};
    m_playPauseAction->setEnabled(false);
    m_playPauseAction->setCheckable(true);
    ui->toolBar->addAction(m_playPauseAction);

    // Debugger commands
    m_stepOutPauseAction = new QAction{"Step out"};
    m_stepOutPauseAction->setEnabled(false);
    ui->toolBar->addAction(m_stepOutPauseAction);

    m_stepOverPauseAction = new QAction{"Step over"};
    m_stepOverPauseAction->setEnabled(false);
    ui->toolBar->addAction(m_stepOverPauseAction);

    m_stepInPauseAction = new QAction{"Step in"};
    m_stepInPauseAction->setEnabled(false);
    ui->toolBar->addAction(m_stepInPauseAction);

    connect(ui->actionLoad_ELF_program, &QAction::triggered, this, &MainWindow::loadHostedProgram);
    connect(m_playPauseAction, &QAction::toggled, this, &MainWindow::playPauseToggled);
    connect(m_stepOutPauseAction, &QAction::triggered, m_runner, &VMRunner::stepOut);
    connect(m_stepOverPauseAction, &QAction::triggered, m_runner, &VMRunner::stepOver);
    connect(m_stepInPauseAction, &QAction::triggered, m_runner, &VMRunner::stepIn);

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
    if(const auto* core{m_runner->core()}; core)
    {
        ui->disassemblyView->setRunner(*m_runner);
    }
}

void MainWindow::playPauseToggled(bool checked)
{
    if(checked) // when checked it means user wants to resume program
    {
        m_runner->start(std::numeric_limits<std::uint64_t>::max());
    }
    else
    {
        m_runner->pause();
    }
}

void MainWindow::loadingError(QString error)
{
    ui->console->outputArgText("-- Could not load program:\n  #%1", error);
}

void MainWindow::corePanic(QString error)
{
    ui->console->outputArgText("-- Core paniced:\n  #%1", error);
}

void MainWindow::coreError(int code)
{
    ui->console->outputArgText("-- Core stopped with error code #%1", code);
}

void MainWindow::statusChanged(VMRunner::Status status)
{
    m_playPauseAction->blockSignals(true);
    m_playPauseAction->setChecked(status == VMRunner::Status::Running);
    m_playPauseAction->setEnabled(status != VMRunner::Status::Stopped);
    m_playPauseAction->blockSignals(false);

    m_stepOutPauseAction->setEnabled(status == VMRunner::Status::Paused);
    m_stepOverPauseAction->setEnabled(status == VMRunner::Status::Paused);
    m_stepInPauseAction->setEnabled(status == VMRunner::Status::Paused);

    if(status == VMRunner::Status::Paused)
    {

    }

    ui->console->outputArgText("-- Core status changed to %1", toString(status));
}

void MainWindow::syscall(AxCore& core) // non-const as runner thread is not running, it is safe to modify everything we want!
{
    uint64_t* const args = &core.registers().gpi[1];

    const auto intrinsic_id = static_cast<SyscallId>(args[0]);
    switch(intrinsic_id)
    {
    case SyscallId::exit:
    {
        ui->console->outputArgText("-- Program exited with code %1", static_cast<int>(args[1]));
        m_runner->stop();
        break;
    }
    case SyscallId::stdio_read:
    {
        void* addr = core.memory().map(core, args[2]);
        // args[0] = std::fread(addr, 1, args[3], id_to_file(args[1]));
        // block until the user type something in the console?
        break;
    }
    case SyscallId::stdio_write:
    {
        const char* addr = reinterpret_cast<const char*>(core.memory().map(core, args[2]));
        const uint64_t size = args[3];
        ui->console->outputText(QString::fromUtf8(addr, size));
        break;
    }
    default:
    {
        ui->console->outputArgText("-- Unknown syscall #%1. Aborting.", static_cast<uint64_t>(intrinsic_id));
        m_runner->stop();
        break;
    }
    }
}
