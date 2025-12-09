#include "runner.hpp"

#include <exception>
#include <memory>
#include <variant>

#include <QCoreApplication>
#include <QFile>
#include <QSettings>

#include <core.hpp>
#include <elf_loader.hpp>
#include <memory.hpp>
#include <message_stack.hpp>
#include <utilities.hpp>

namespace
{

struct AddBreakpoint
{
    uint64_t address{};
    bool enabled{};
    bool single_shot{};
};

struct RemoveBreakpoint
{
    uint64_t address{};
};

using AsyncCommand = std::variant<AddBreakpoint, RemoveBreakpoint>;

}

class VMRunner::Worker : public QObject
{
    Q_OBJECT

public:
    Worker(VMRunner* parent)
        : m_parent{parent}
    {
    }

    void loadRawProgram(const QString& path, uint64_t entry_point)
    {
        stop(true); // first stop the running program, if any
        cleanup();  // destroy current program, if any

        const auto content = readFile(path);
        if(!content)
        {
            return;
        }

        auto [memory, core] = makeCore();
        if(content->size() > memory->wram_bytesize())
        {
            loadingError(QString{"Program is too big to fit into ROM memory. ROM size is %1. Program size is %2. (in bytes)"}
                    .arg(memory->wram_bytesize())
                    .arg(content->size()));
            return;
        }

        std::memcpy(memory->map(*core, AxMemory::WRAM_BEGIN), content->data(), content->size());
        core->registers().pc = entry_point / 4ull;

        makeReady(std::move(memory), std::move(core));
    }

    void loadProgram(const QString& path, std::string_view entry_point_name)
    {
        stop(true); // first stop the running program, if any
        cleanup();  // destroy current program, if any

        const auto content = readFile(path);
        if(!content)
        {
            return;
        }

        auto [memory, core] = makeCore();
        try
        {
            ax_load_elf_program(*core, content->data(), content->size(), entry_point_name);
        }
        catch(const std::exception& e)
        {
            loadingError("Error while parsing ELF file \"" + path + "\": " + QString{e.what()});
            return;
        }

        makeReady(std::move(memory), std::move(core));
    }

    void loadHostedProgram(const QString& path, const std::vector<std::string_view>& argv)
    {
        stop(true); // first stop the running program, if any
        cleanup();  // destroy current program, if any

        const auto content = readFile(path);
        if(!content)
        {
            return;
        }

        auto [memory, core] = makeCore();
        try
        {
            ax_load_elf_hosted_program(*core, content->data(), content->size(), path.toUtf8().toStdString(), argv);
        }
        catch(const std::exception& e)
        {
            loadingError("Error while parsing ELF file \"" + path + "\": " + QString{e.what()});
            return;
        }

        makeReady(std::move(memory), std::move(core));
    }

    // Run a good amount of cycles at once, this enable more steady performances
    static constexpr uint64_t cycleBundleSize = 1024;

    // Core thread entry point, called as a slot by another thread
    void start(uint64_t cycleCount)
    {
        if(cycleCount == 0)
        {
            return;
        }

        if(!compareExchangeStatus(Status::Paused, Status::Running))
        {
            return; // do not have a program or was stopped once for all
        }

        // This mutex indicates if this function is currently being run.
        // This is used to "join the thread" when stopping but not terminating the QThread itself
        std::lock_guard lock{m_threadRunningMutex};

        try
        {
            // it will be impossible to reach uint64 maximum in practice
            for(uint64_t i{}; i < cycleCount; ++i)
            {
                if(status() != Status::Running)
                {
                    return; // leaving this functions if another thread changed runner status
                }

                processMessages();

                const auto limit = std::min(cycleBundleSize, cycleCount - i);
                for(uint64_t cycle{}; cycle < limit; ++cycle)
                {
                    if(i != 0 || cycle != 0) [[likely]] // we do not check them on the very first cycle after a request
                    {
                        if(auto* bp = m_core->hit_breakpoint(); bp && bp->enabled) [[unlikely]]
                        {
                            if(bp->single_shot)
                            {
                                m_core->remove_breakpoint(bp);
                            }

                            setStatus(Status::Paused);
                            return;
                        }
                    }

                    m_core->cycle();

                    if(m_core->error()) [[unlikely]]
                    {
                        coreError(m_core->error());
                        setStatus(Status::Stopped);
                        return;
                    }

                    // let connected slots handle syscalls
                    if(m_core->syscall(&Worker::syscall, this, *m_core)) [[unlikely]]
                    {
                        // stop this pass of execution if syscall changed the state of the runner.
                        if(status() != Status::Running)
                        {
                            return;
                        }
                    }
                }
            }

            // We are leaving "normally" so we simply pause the runner.
            setStatus(Status::Paused);
        }
        catch(const std::exception& e)
        {
            corePanic(e.what());
            setStatus(Status::Stopped);
            return;
        }
    }

    // can only pause if running
    bool pause()
    {
        return compareExchangeStatus(Status::Running, Status::Paused);
    }

    void addBreakpoint(uint64_t address, bool enabled)
    {
        m_core->add_breakpoint(AxCore::Breakpoint{address, enabled});
    }

    // add a single shot breakpoint on next instruction.
    // single shot breakpoints do not override the non single shot ones.
    void setStepOutBreakpoint()
    {
        const auto where = AxCore::pc_to_wram(m_core->registers().lr + 1);
        m_core->add_breakpoint(AxCore::Breakpoint{where, true, true});
    }

    void setStepOverBreakpoint()
    {
        const auto where = AxCore::pc_to_wram(m_core->registers().pc + 1);
        m_core->add_breakpoint(AxCore::Breakpoint{where, true, true});
    }

    // always force to stop regardless of current state.
    void stop(bool sync)
    {
        setStatus(Status::Stopped);
        if(sync)
        {
            // wait until the thread no long holds the lock
            std::lock_guard lock{m_threadRunningMutex};
        }
    }

    void addBreakpoint(uint64_t address, bool enabled, bool single_shot)
    {
        m_messages.push(AddBreakpoint{address, enabled, single_shot});
    }

    void removeBreakpoint(uint64_t address)
    {
        m_messages.push(RemoveBreakpoint{address});
    }

    VMRunner::Status status() const noexcept
    {
        return m_status.load(std::memory_order_acquire);
    }

    const AxCore* core() const noexcept
    {
        return m_core.get();
    }

    const AxMemory* memory() const noexcept
    {
        return m_memory.get();
    }

signals:
    void statusChanged(Status status);
    void loadingError(QString error);
    void corePanic(QString error);
    void coreError(int code);
    void syscall(AxCore& core);

private:
    // user must use high level API stop, resume, pause, etc...
    void setStatus(Status status) noexcept
    {
        const auto old = m_status.exchange(status);
        if(old != status)
        {
            statusChanged(status);
        }
    }

    bool compareExchangeStatus(Status expected, Status desired) noexcept
    {
        if(m_status.compare_exchange_strong(expected, desired))
        {
            statusChanged(desired);
            return true;
        }

        return false;
    }

    std::pair<std::unique_ptr<AxMemory>, std::unique_ptr<AxCore>> makeCore()
    {
        // Reconstruct Context to get a fully cleaned context.
        QSettings settings;
        const auto wram_size = settings.value("coreconfig/wram", 16).toULongLong();
        const auto spmt_size = settings.value("coreconfig/spmt", 256).toULongLong();
        const auto spm2_size = settings.value("coreconfig/spm2", 512).toULongLong();

        auto memory = std::make_unique<AxMemory>(wram_size, spmt_size, spm2_size);
        auto core = std::make_unique<AxCore>(*memory);

        return std::make_pair(std::move(memory), std::move(core));
    }

    std::optional<QByteArray> readFile(const QString& path)
    {
        QFile file{path};
        if(!file.open(QFile::ReadOnly))
        {
            loadingError("Failed to open file \"" + path + "\"");
            return std::nullopt;
        }

        return std::make_optional(file.readAll());
    }

    void makeReady(std::unique_ptr<AxMemory>&& memory, std::unique_ptr<AxCore>&& core)
    {
        m_memory = std::move(memory);
        m_core = std::move(core);
        setStatus(Status::Paused);
    }

    void processMessages()
    {
        while(auto message = m_messages.try_pop())
        {
            // clang-format off
            auto visitors = ax_overloads {
                [this](const AddBreakpoint& breakpoint)
                {
                    m_core->add_breakpoint(AxCore::Breakpoint{breakpoint.address, breakpoint.enabled, breakpoint.single_shot});
                },
                [this](const RemoveBreakpoint& breakpoint)
                {
                    m_core->remove_breakpoint(breakpoint.address);
                }
            };
            // clang-format on

            std::visit(visitors, *message);
        }
    }

    void cleanup()
    {
        m_core.reset();
        m_memory.reset();
    }

    VMRunner* m_parent{};
    std::unique_ptr<AxMemory> m_memory;
    std::unique_ptr<AxCore> m_core;
    MessageStack<AsyncCommand> m_messages;
    std::atomic<VMRunner::Status> m_status{VMRunner::Status::Stopped};
    std::mutex m_threadRunningMutex{};
};

VMRunner::VMRunner(QObject* parent)
    : QObject{parent}
    , m_worker{new Worker{this}}
{
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &VMRunner::start, m_worker, &Worker::start);

    // Information connections
    connect(m_worker, &Worker::statusChanged, this, &VMRunner::statusChanged, Qt::QueuedConnection);
    // happens within main thread, not runner thread, so direct connection is coherent
    connect(m_worker, &Worker::loadingError, this, &VMRunner::loadingError, Qt::DirectConnection);
    // this causes the core to stop, no need to synchronize anything
    connect(m_worker, &Worker::corePanic, this, &VMRunner::corePanic, Qt::QueuedConnection);
    connect(m_worker, &Worker::coreError, this, &VMRunner::coreError, Qt::QueuedConnection);
    // Slots must handle these before the runner thread can run again
    connect(m_worker, &Worker::syscall, this, &VMRunner::syscall, Qt::ConnectionType::BlockingQueuedConnection);

    m_thread.start();
}

VMRunner::~VMRunner()
{
    m_worker->stop(false); // do not sync here, this leaves our infinite loop
    m_thread.quit();       // this leave the event loop of the QThread
    // this blocks this thread until we can destroy everything
    // the worker itself is destroyed using the QThread::finished signal.
    m_thread.wait();
}

void VMRunner::loadRawProgram(const QString& path, std::uint64_t entry_point)
{
    m_worker->loadRawProgram(path, entry_point);
}

void VMRunner::loadProgram(const QString& path, std::string_view entry_point_name)
{
    m_worker->loadProgram(path, entry_point_name);
}

void VMRunner::loadHostedProgram(const QString& path, const std::vector<std::string_view>& argv)
{
    m_worker->loadHostedProgram(path, argv);
}

bool VMRunner::pause()
{
    return m_worker->pause();
}

void VMRunner::stepOut()
{
    m_worker->setStepOutBreakpoint();
    start(std::numeric_limits<uint64_t>::max());
}

void VMRunner::stepOver()
{
    m_worker->setStepOverBreakpoint();
    start(std::numeric_limits<uint64_t>::max());
}

void VMRunner::stepIn()
{
    // step in is just running a single instruction
    start(1);
}

void VMRunner::stop()
{
    m_worker->stop(false); // do not sync here, this may be called by a slot blocking our thread!
}

void VMRunner::addBreakpoint(uint64_t address, bool enabled, bool single_shot)
{
    m_worker->addBreakpoint(address, enabled, single_shot);
}

void VMRunner::removeBreakpoint(uint64_t address)
{
    m_worker->removeBreakpoint(address);
}

VMRunner::Status VMRunner::status() const noexcept
{
    return m_worker->status();
}

const AxCore* VMRunner::core() const noexcept
{
    return m_worker->core();
}

const AxMemory* VMRunner::memory() const noexcept
{
    return m_worker->memory();
}

QString toString(VMRunner::Status status)
{
    switch(status)
    {
    case VMRunner::Status::Stopped:
        return QObject::tr("Stopped");
    case VMRunner::Status::Paused:
        return QObject::tr("Paused");
    case VMRunner::Status::Running:
        return QObject::tr("Running");
    default:
        return QObject::tr("Unknown status");
    }
}

#include "runner.moc" // this is required for MOC in source files
