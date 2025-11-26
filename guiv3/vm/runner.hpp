#ifndef VMRUNNER_HPP_INCLUDED
#define VMRUNNER_HPP_INCLUDED

#include <QObject>
#include <QString>
#include <QThread>

#include <variant>
#include <message_stack.hpp>

class AxCore;
class AxMemory;

class VMRunner : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Stopped, // Runner is stopped. A program must be loaded before doing something else.
        Ready, // Runner has a program and is ready to be started
        Paused, // Runner is running but paused.
        Running, // Runner is running.
    };

    VMRunner(QObject* parent = nullptr);
    ~VMRunner();

    // load a file and put PC at specified entry point location
    void loadRawProgram(const QString& path, std::uint64_t entry_point);

    // load an ELF file and put PC at specified entry point location
    void loadProgram(const QString& path, std::string_view entry_point_name);

    // load an ELF file
    // See ax_load_elf_hosted_program
    void loadHostedProgram(const QString& path, const std::vector<std::string_view>& argv);

    // Pause and resume the core.
    bool pause();
    bool resume();

    // Stop core if running, then resets context.
    // Program must be loaded again from file after stop.
    void stop();

    Status status() const noexcept; // return current status.
    const AxCore* core() const noexcept;
    const AxMemory* memory() const noexcept;

signals:
    /// Input signals
    // Once a program has been loaded, this enable the runner thread to work
    void start(bool paused);

    /// Output signals
    void statusChanged(Status newStatus);
    // Received when loading failed. load function returns right after slots are done.
    void loadingError(QString error);
    void corePanic(QString error);
    void coreError(int code);
    // These signals are queued, meaning the runner thread is blocked until the slots return
    void syscall();

private:
    class Worker;
    Worker* m_worker;
    QThread m_thread;
};

QString toString(VMRunner::Status status);

#endif
