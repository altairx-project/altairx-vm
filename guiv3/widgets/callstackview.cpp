#include "callstackview.hpp"
#include "ui_callstackview.h"

CallStackView::CallStackView(QWidget* parent)
    : QWidget{parent}
    , ui{new Ui::CallStackView}
{
    ui->setupUi(this);
}

CallStackView::~CallStackView()
{
    delete ui;
}
