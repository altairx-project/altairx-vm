#ifndef CALLSTACKVIEW_HPP_INCLUDED
#define CALLSTACKVIEW_HPP_INCLUDED

#include <QWidget>

namespace Ui
{
class CallStackView;
}

class CallStackView : public QWidget
{
    Q_OBJECT

public:
    explicit CallStackView(QWidget* parent = nullptr);
    ~CallStackView();

private:
    Ui::CallStackView* ui;
};

#endif // CALLSTACKVIEW_HPP
