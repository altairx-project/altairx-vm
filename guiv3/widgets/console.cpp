#include "console.hpp"
#include "ui_console.h"

Console::Console(QWidget* parent)
    : QWidget{parent}
    , ui{new Ui::Console}
{
    ui->setupUi(this);
}

Console::~Console()
{
    delete ui;
}

void Console::outputText(const QString& str)
{
    ui->output->append(str);
}