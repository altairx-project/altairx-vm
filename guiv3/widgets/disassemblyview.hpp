#ifndef DISASSEMBLYVIEW_HPP_INCLUDED
#define DISASSEMBLYVIEW_HPP_INCLUDED

#include <QWidget>

#include "vm/runner.hpp"

class DisassemblyView : public QWidget
{
    Q_OBJECT

public:
    explicit DisassemblyView(QWidget* parent = nullptr);
    ~DisassemblyView() override;

    void setRunner(VMRunner& runner);

private:
    void disassemble();
    void onStatusChanged(VMRunner::Status status);

    struct Internals;
    std::unique_ptr<Internals> impl{};
};

#endif // DISASSEMBLYVIEW_HPP
