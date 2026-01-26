#include "registersview.hpp"
#include "ui_registersview.h"

#include <QLabel>

#include <opcode.hpp>
#include <core.hpp>

namespace
{

QString getRegName(uint32_t i)
{
    return QString::fromUtf8(format_as(AxOpcodeArg::Reg(i)));
}

QString getFRegName(uint32_t i)
{
    return QString::fromUtf8(format_as(AxOpcodeArg::FReg(i)));
}

}

struct RegistersView::Internals
{
    VMRunner* runner{};
    std::array<QLabel*, 64> regLabels{};
    std::array<QLabel*, 64> fregLabels{};
};

RegistersView::RegistersView(QWidget* parent)
    : QWidget{parent}
    , ui{new Ui::RegistersView}
    , impl{new Internals}
{
    ui->setupUi(this);

    QFont font{"Consolas", 9};

    for(uint32_t i{}; i < 64; ++i)
    {
        impl->regLabels[i] = new QLabel(QString{"%1: %2"}.arg(getRegName(i), 4).arg("???"));
        impl->regLabels[i]->setFont(font);
        // make selectable;
        ui->regLayout->addWidget(impl->regLabels[i], i);

        impl->fregLabels[i] = new QLabel(QString{"%1: %2"}.arg(getFRegName(i), 4).arg("???"));
        impl->fregLabels[i]->setFont(font);
        ui->fregLayout->addWidget(impl->fregLabels[i], i);
    }
}

RegistersView::~RegistersView() = default;

void RegistersView::setRunner(VMRunner& runner)
{
    if(impl->runner) // disconned old runner in case it is still used elsewhere
    {
        impl->runner->disconnect(this);
    }

    impl->runner = &runner;
    connect(impl->runner, &VMRunner::statusChanged, this, &RegistersView::onStatusChanged);
}

void RegistersView::onStatusChanged(VMRunner::Status status)
{
    if(status == VMRunner::Status::Paused)
    {
        const AxCore& core = *impl->runner->core();
        const auto& gpi = core.registers().gpi;
        const auto& gpf = core.registers().gpf;

        for(size_t i{}; i < 64; ++i)
        {
            impl->regLabels[i]->setText(QString{"%1: %2"}.arg(getRegName(i), 4).arg(gpi[i]));
            impl->fregLabels[i]->setText(QString{"%1: %2"}.arg(getFRegName(i), 4).arg(gpf[i], 'g', 16));
        }
    }
    else
    {
        for(size_t i{}; i < 64; ++i)
        {
            impl->regLabels[i]->setText(QStringLiteral("???"));
            impl->fregLabels[i]->setText(QStringLiteral("???"));
        }
    }
}
