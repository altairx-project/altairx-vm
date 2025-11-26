#include <QObject>
#include <QTest>
#include <QTemporaryFile>
#include <QCoreApplication>

#include <thread>
#include <chrono>
using namespace std::chrono_literals;

#include <vm/runner.hpp>
#include <core.hpp>
#include <memory.hpp>

#include <make_opcode.hpp>

class TestRunner : public QObject
{
    Q_OBJECT

private slots:
    void testRunnerBlockedInSyscall()
    {
        VMRunner runner;

        QObject::connect(&runner, &VMRunner::loadingError, this, [this, &runner]
        {
            QVERIFY(false);
        });

        QObject::connect(&runner, &VMRunner::syscall, this, [this, &runner]
        {
            std::this_thread::sleep_for(50ms); // let time pass to check that core is truely blocked
            runner.stop();
        });

        QTemporaryFile file;
        QVERIFY(file.open());
        createSyscallProgram(file);

        runner.loadRawProgram(file.fileName(), 0);
        runner.start(false);

        // main loop
        while(runner.status() != VMRunner::Status::Stopped)
        {
            QApplication::processEvents();
        }

        QVERIFY(runner.core());
        // Ensure that the core has been staled until the syscall has been resolved
        QCOMPARE(runner.core()->registers().pc, 2);
    }

private:
    void createSyscallProgram(QFile& file)
    {
        const auto program =
            make_bundle(make_noop_opcode() | 1,
                make_simple_opcode(AX_EXE_CU_SYSCALL));

        file.write(reinterpret_cast<const char*>(program.data()), program.size() * 4ull);
        QVERIFY(file.flush());
    }
};

QTEST_MAIN(TestRunner)
#include "test_runner.moc"
