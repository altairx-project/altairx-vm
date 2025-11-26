#include "registersview.hpp"
#include "ui_registersview.h"

RegistersView::RegistersView(QWidget* parent)
    : QWidget{parent}
    , ui{new Ui::RegistersView}
{
    ui->setupUi(this);
}

RegistersView::~RegistersView()
{
    delete ui;
}
