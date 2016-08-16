#include <QTimer>

#include "votepage.h"
#include "ui_votepage.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "custodianvotedialog.h"
#include "parkratevotedialog.h"
#include "motionvotedialog.h"
#include "feevotedialog.h"
#include "datafeeddialog.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "liquidityinfo.h"

VotePage::VotePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VotePage),
    model(NULL),
    lastBestBlock(NULL)
{
    ui->setupUi(this);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(MODEL_UPDATE_DELAY);
}

VotePage::~VotePage()
{
    delete ui;
}

void VotePage::setModel(WalletModel *model)
{
    this->model = model;
}

void VotePage::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;

    connect(clientModel, SIGNAL(liquidityChanged()), this, SLOT(updateLiquidity()));
}

void VotePage::on_custodianVote_clicked()
{
    CustodianVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_parkRateVote_clicked()
{
    ParkRateVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_motionVote_clicked()
{
    MotionVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_feeVote_clicked()
{
    FeeVoteDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::on_dataFeedButton_clicked()
{
    DataFeedDialog dlg(this);
    dlg.setModel(model);
    dlg.exec();
}

void VotePage::update()
{
    if (!model)
        return;

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain)
            return;

        if (pindexBest == lastBestBlock)
            return;

        lastBestBlock = pindexBest;

        fillCustodianTable();
        fillParkRateTable();
    }
}

void VotePage::fillCustodianTable()
{
    QTableWidget* table = ui->custodianTable;
    table->setRowCount(0);
    int row = 0;
    for (CBlockIndex* pindex = lastBestBlock; pindex; pindex = pindex->pprevElected)
    {
        BOOST_FOREACH(const CCustodianVote& custodianVote, pindex->vElectedCustodian)
        {
            table->setRowCount(row + 1);

            QTableWidgetItem *addressItem = new QTableWidgetItem();
            addressItem->setData(Qt::DisplayRole, QString::fromStdString(custodianVote.GetAddress().ToString()));
            table->setItem(row, 0, addressItem);

            QTableWidgetItem *amountItem = new QTableWidgetItem();
            amountItem->setData(Qt::DisplayRole, BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), custodianVote.nAmount));
            amountItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
            table->setItem(row, 1, amountItem);

            QTableWidgetItem *dateItem = new QTableWidgetItem();
            dateItem->setData(Qt::DisplayRole, GUIUtil::dateTimeStr(pindex->nTime));
            table->setItem(row, 2, dateItem);

            row++;
        }
    }
    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

void VotePage::fillParkRateTable()
{
    QTableWidget* table = ui->parkRateTable;
    std::vector<CParkRate> parkRates;

    BOOST_FOREACH(const CParkRateVote& parkRateResult, lastBestBlock->vParkRateResult)
    {
        if (parkRateResult.cUnit == 'B')
        {
            parkRates = parkRateResult.vParkRate;
            break;
        }
    }

    table->setRowCount(parkRates.size());
    for (size_t i = 0; i < parkRates.size(); i++)
    {
        const CParkRate& parkRate = parkRates[i];

        QString durationString = GUIUtil::blocksToTime(parkRate.GetDuration());
        QTableWidgetItem *durationItem = new QTableWidgetItem(durationString);
        durationItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(i, 0, durationItem);

        double interestRate = GUIUtil::annualInterestRatePercentage(parkRate.nRate, parkRate.GetDuration());
        QString rateString = QString("%L1%").arg(interestRate, 0, 'f', 3);
        QTableWidgetItem *rateItem = new QTableWidgetItem(rateString);
        rateItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(i, 1, rateItem);
    }
    table->setVisible(false);
    table->resizeColumnsToContents();
    table->setVisible(true);
}

struct LiquidityTotal
{
    qint64 buy;
    qint64 sell;

    LiquidityTotal() :
        buy(0),
        sell(0)
    {
    }

    LiquidityTotal operator+=(const LiquidityTotal& other)
    {
        buy += other.buy;
        sell += other.sell;
        return *this;
    }

    LiquidityTotal operator+=(const CLiquidityInfo& other)
    {
        buy += other.nBuyAmount;
        sell += other.nSellAmount;
        return *this;
    }

    void AddToTable(QTableWidget* table, const QString& label) const
    {
        int row = table->rowCount();

        table->setRowCount(row + 1);

        QTableWidgetItem *currencyItem = new QTableWidgetItem();
        currencyItem->setData(Qt::DisplayRole, label);
        table->setItem(row, 0, currencyItem);

        QTableWidgetItem *buyItem = new QTableWidgetItem();
        buyItem->setData(Qt::DisplayRole, BitcoinUnits::format(BitcoinUnits::BTC, buy));
        buyItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(row, 1, buyItem);

        QTableWidgetItem *sellItem = new QTableWidgetItem();
        sellItem->setData(Qt::DisplayRole, BitcoinUnits::format(BitcoinUnits::BTC, sell));
        sellItem->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(row, 2, sellItem);
    }
};

void VotePage::updateLiquidity()
{
    unsigned char cUnit = 'B';
    LiquidityTotal total;
    QMap<QString, LiquidityTotal> tierMap;
    {
        LOCK(cs_mapLiquidityInfo);

        BOOST_FOREACH(const PAIRTYPE(const CLiquiditySource, CLiquidityInfo)& item, mapLiquidityInfo)
        {
            const CLiquidityInfo& info = item.second;
            if (info.cUnit == cUnit)
            {
                total += info;
                QString tier = QString::fromStdString(info.GetTier());
                tierMap[tier] += info;
            }
        }
    }

    QTableWidget* table = ui->liquidity;
    table->setRowCount(0);

    QMap<QString, LiquidityTotal>::const_iterator it = tierMap.constBegin();
    while (it != tierMap.constEnd())
    {
        const QString& tier = it.key();
        const LiquidityTotal& liquidity = it.value();

        QString label;
        if (tier == "")
            label = tr("Unknown tier");
        else
            label = tr("Tier %1").arg(tier);
        liquidity.AddToTable(table, label);

        it++;
    }

    total.AddToTable(table, tr("Total"));

    table->setVisible(false);
    table->resizeColumnToContents(0);
    table->setVisible(true);
}
