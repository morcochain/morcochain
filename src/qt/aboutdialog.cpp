#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "clientmodel.h"
#include "clientversion.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Set current copyright year
    ui->PPCoinCopyrightLabel->setText(tr("Copyright") + QString(" &copy; 2011-%1 ").arg(PEERCOIN_COPYRIGHT_YEAR) + tr("Peercoin (PPCoin) Developers"));
    ui->NuCopyrightLabel->setText(tr("Copyright") + QString(" &copy; 2014-%1 ").arg(COPYRIGHT_YEAR) + tr("Nu Developers"));
}

void AboutDialog::setModel(ClientModel *model)
{
    if(model)
    {
        ui->versionLabel->setText(model->formatFullVersion());
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_buttonBox_accepted()
{
    close();
}
