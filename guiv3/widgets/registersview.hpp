#ifndef REGISTERSVIEW_HPP_INCLUDED
#define REGISTERSVIEW_HPP_INCLUDED

#include <QWidget>

namespace Ui
{
class RegistersView;
}

class RegistersView : public QWidget
{
    Q_OBJECT

public:
    explicit RegistersView(QWidget* parent = nullptr);
    ~RegistersView();

private:
    Ui::RegistersView* ui;
};

#endif // REGISTERSVIEW_HPP
