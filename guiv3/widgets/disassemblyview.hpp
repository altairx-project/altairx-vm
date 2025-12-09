#ifndef DISASSEMBLYVIEW_HPP_INCLUDED
#define DISASSEMBLYVIEW_HPP_INCLUDED

#include <QWidget>

#include "vm/runner.hpp"

class DisassemblyView : public QWidget
{
    Q_OBJECT

public:
    explicit DisassemblyView(QWidget* parent = nullptr);
    ~DisassemblyView() override = default;

    void setRunner(VMRunner& runner);

private:
    void disassemble();
    void onStatusChanged(VMRunner::Status status);

    struct Internals;
    Internals* m_impl{};
};

#endif // DISASSEMBLYVIEW_HPP
