#ifndef REGISTERSVIEW_HPP_INCLUDED
#define REGISTERSVIEW_HPP_INCLUDED

#include <QWidget>
#include <QLabel>

#include "vm/runner.hpp"

namespace Ui
{
class RegistersView;
}

class RegistersView : public QWidget
{
    Q_OBJECT

public:
    explicit RegistersView(QWidget* parent = nullptr);
    ~RegistersView() override;

    void setRunner(VMRunner& runner);

private:
    void onStatusChanged(VMRunner::Status status);

    std::unique_ptr<Ui::RegistersView> ui;
    struct Internals;
    std::unique_ptr<Internals> impl{};
};

#endif // REGISTERSVIEW_HPP
