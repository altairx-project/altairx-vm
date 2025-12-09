#ifndef CONSOLE_HPP_INCLUDED
#define CONSOLE_HPP_INCLUDED

#include <QWidget>

#include <cstdint>

namespace Ui
{
class Console;
}

class Console : public QWidget
{
    Q_OBJECT

public:
    explicit Console(QWidget* parent = nullptr);
    ~Console();

    void outputText(const QString& str);

    template<typename... Args>
    void outputArgText(const QString& format, Args&&... args)
    {
        outputText((format.arg(std::forward<Args>(args)), ...));
    }

private:
    Ui::Console* ui;
};

#endif // Console_HPP
