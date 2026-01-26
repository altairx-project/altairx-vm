#include "memoryview.hpp"
#include "ui_memoryview.h"

MemoryView::MemoryView(QWidget* parent)
    : QWidget{parent}
    , ui{new Ui::MemoryView}
{
    ui->setupUi(this);
}

MemoryView::~MemoryView()
{
    delete ui;
}
