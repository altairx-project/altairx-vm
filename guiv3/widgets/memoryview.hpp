#ifndef MEMORYVIEW_HPP_INCLUDED
#define MEMORYVIEW_HPP_INCLUDED

#include <QWidget>

namespace Ui
{
class MemoryView;
}

class MemoryView : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryView(QWidget* parent = nullptr);
    ~MemoryView();

private:
    Ui::MemoryView* ui;
};

#endif // MEMORYVIEW_HPP
