#include "runner.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include <QCoreApplication>
#include <QFile>
#include <QSettings>

#include <core.hpp>
#include <elf_loader.hpp>
#include <memory.hpp>
#include <panic.hpp>

class VMRunner::Worker : public QObject
{
    Q_OBJECT

public:
    Worker(VMRunner* parent)
        : m_parent{parent}
    {
    }

    void loadRawProgram(const QString& path, std::uint64_t entry_point)
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

        ready(std::move(memory), std::move(core));
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

        ready(std::move(memory), std::move(core));
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

        ready(std::move(memory), std::move(core));
    }

    // Run a good amount of cycles at once, this enable more steady performances
    static constexpr std::size_t cycleBundleSize = 8 * 1024;

    // Core thread entry point
    void start(bool paused)
    {
        if(!compareExchangeStatus(Status::Ready, paused ? Status::Paused : Status::Running))
        {
            return; // wasn't ready
        }

        // This mutex indicates if this function is currently being run.
        // This is used to "join the thread" when stopping but not terminating the QThread itself
        std::lock_guard lock{m_threadRunningMutex};

        try
        {
            uint16_t i = 0;
            while(true)
            {
                coreError(i++);

                switch(m_status.load(std::memory_order_acquire))
                {
                case VMRunner::Status::Stopped:
                    return; // leave this thread
                case VMRunner::Status::Paused:
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                    continue; // try again later
                case VMRunner::Status::Running:
                    break;
                default:
                    ax_panic("VMRunner status unknown. Aborting.");
                }

                std::size_t cycle = 0;
                while(m_core->error() == 0 && cycle < cycleBundleSize)
                {
                    if(auto* bp = m_core->hit_breakpoint(); bp && bp->enabled)
                    {
                        setStatus(Status::Paused);
                        break;
                    }

                    m_core->cycle();

                    // let connected slots handle syscalls
                    if(m_core->syscall(&Worker::syscall, this)) [[unlikely]]
                    {
                        // stop this pass of execution if syscall changed the state of the runner.
                        if(m_status.load(std::memory_order_acquire) != Status::Running)
                        {
                            break;
                        }
                    }

                    cycle += 1;
                }

                if(m_core->error())
                {
                    coreError(m_core->error());
                    setStatus(Status::Stopped);
                }
            }
        }
        catch(const std::exception& e)
        {
            corePanic(QString{e.what()});
        }

        // Whatever is the reason that made us leave, mark status as stopped
        setStatus(Status::Stopped);
    }

    bool pause()
    {
        // can only pause if running
        return compareExchangeStatus(Status::Running, Status::Paused);
    }

    bool resume()
    {
        // can only resume if purposely paused
        return compareExchangeStatus(Status::Paused, Status::Running);
    }

    void stop(bool sync)
    {
        // always force stopped regardless of current state.
        setStatus(Status::Stopped);
        if(sync)
        {
            // wait until the thread no long holds the lock
            std::lock_guard lock{m_threadRunningMutex};
        }
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
    void syscall();

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

    void ready(std::unique_ptr<AxMemory>&& memory, std::unique_ptr<AxCore>&& core)
    {
        m_memory = std::move(memory);
        m_core = std::move(core);
        setStatus(Status::Ready);
    }

    void cleanup()
    {
        m_core.reset();
        m_memory.reset();
    }

    VMRunner* m_parent{};
    std::unique_ptr<AxMemory> m_memory;
    std::unique_ptr<AxCore> m_core;
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

bool VMRunner::resume()
{
    return m_worker->resume();
}

void VMRunner::stop()
{
    m_worker->stop(false); // do not sync here, this may be called by a slot blocking our thread!
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
    case VMRunner::Status::Ready:
        return QObject::tr("Ready");
    case VMRunner::Status::Paused:
        return QObject::tr("Paused");
    case VMRunner::Status::Running:
        return QObject::tr("Running");
    default:
        return QObject::tr("Unknown status");
    }
}

#include "runner.moc" // this is required for MOC in source files
