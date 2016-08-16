/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2013
 */
#include "walletview.h"
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "askpassphrasedialog.h"
#include "ui_interface.h"

#include "parkpage.h"
#include "votepage.h"
#include "bitcoinunits.h"
#include "rpcconsole.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QDesktopServices>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QMenu>

WalletView::WalletView(QWidget *parent, BitcoinGUI *_gui):
    QStackedWidget(parent),
    gui(_gui),
    clientModel(0),
    walletModel(0)
{
    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
#ifndef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    exportButton->setIcon(QIcon(":/icons/export"));
#endif
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(gui);

    signVerifyMessageDialog = new SignVerifyMessageDialog(gui);

    // nubit:
    votePage = new VotePage(gui);
    parkPage = new ParkPage(gui);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(addressBookPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);

    //nubit:
    addWidget(votePage);
    addWidget(parkPage);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Clicking on "Send Coins" in the address book sends you to the send coins tab
    connect(addressBookPage, SIGNAL(sendCoins(QString)), this, SLOT(gotoSendCoinsPage(QString)));
    // Clicking on "Verify Message" in the address book opens the verify message tab in the Sign/Verify Message dialog
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page opens the sign message tab in the Sign/Verify Message dialog
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));
    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    gotoOverviewPage();
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI *gui)
{
    this->gui = gui;
}

void WalletView::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if (clientModel)
    {
        overviewPage->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
        votePage->setClientModel(clientModel);
    }
}

void WalletView::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if (walletModel)
    {
        BitcoinUnits::baseUnit = walletModel->getUnit();

        // Receive and report messages from wallet thread
        connect(walletModel, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);
        overviewPage->setWalletModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus();
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // nubit:
        parkPage->setModel(walletModel);
        votePage->setModel(walletModel);

        gui->getParkAction()->setVisible(walletModel->getUnit() != 'S');

        gui->getVoteAction()->setVisible(walletModel->getUnit() == 'S');

        gui->getRPCConsole()->setModel(walletModel);

        if (walletModel->getUnit() != 'S' && currentWidget() == votePage)
            gotoOverviewPage();

        if (walletModel->getUnit() == 'S' && currentWidget() == parkPage)
            gotoOverviewPage();

        gui->getSharesMenu()->setEnabled(walletModel->getUnit() == 'S');

        // nubit: change the client stylesheet when the unit context changes
        if (walletModel->getUnit() == 'S')
        {
            QFile stylesheet(":/styles/nushares.qss");
            stylesheet.open(QFile::ReadOnly);
            QString setSheet = QLatin1String(stylesheet.readAll());
            gui->setStyleSheet(setSheet);
        }
        else
        {
            QFile stylesheet(":/styles/nubits.qss");
            stylesheet.open(QFile::ReadOnly);
            QString setSheet = QLatin1String(stylesheet.readAll());
            gui->setStyleSheet(setSheet);
        }

        // Embed application fonts
        QFont newFont(":/fonts/Roboto-Regular.ttf", 14, true);
        setFont(newFont);

        QVector<QAction *>& changeUnitActions = *gui->getChangeUnitActions();
        changeUnitActions.clear();
        QSignalMapper *mapper = new QSignalMapper();

        BOOST_FOREACH(CWallet *wallet, setpwalletRegistered)
        {
            QString unitString(wallet->Unit());
            QAction *action = new QAction(BitcoinUnits::baseName(wallet->Unit()), this);
            action->setCheckable(true);
            if (unitString == QString(walletModel->getWallet()->Unit()))
                action->setChecked(true);

            changeUnitActions.append(action);

            connect(action, SIGNAL(triggered()), mapper, SLOT(map()));
            mapper->setMapping(action, QString(wallet->Unit()));
        }
        connect(mapper, SIGNAL(mapped(const QString&)), this, SLOT(changeUnit(const QString&)));
        gui->getUnitMenu()->clear();
        for (int i=0; i < changeUnitActions.size(); ++i)
            gui->getUnitMenu()->addAction(changeUnitActions[i]);

        if (walletModel->getUnit() == 'S')
        {
            gui->setSwitchUnitTarget("B");
            gui->getSwitchUnitAction()->setText(tr("NuBits"));
        }
        else
        {
            gui->setSwitchUnitTarget("S");
            gui->getSwitchUnitAction()->setText(tr("NuShares"));
        }
    }
}

void WalletView::incomingTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if(!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();

    gui->incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address);
}

void WalletView::gotoOverviewPage()
{
    gui->getOverviewAction()->setChecked(true);
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    gui->getHistoryAction()->setChecked(true);
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoAddressBookPage()
{
    gui->getAddressBookAction()->setChecked(true);
    setCurrentWidget(addressBookPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    gui->getReceiveCoinsAction()->setChecked(true);
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    gui->getSendCoinsAction()->setChecked(true);
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handleURI(const QString& strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        gotoSendCoinsPage();
        emit showNormalIfMinimized();
        return true;
    }
    else
        return false;
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::setEncryptionStatus()
{
    gui->setEncryptionStatus(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if (!filename.isEmpty()) {
        if (!walletModel->backupWallet(filename)) {
            gui->message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."),
                      CClientUIInterface::MSG_ERROR);
        }
        else
            gui->message(tr("Backup Successful"), tr("The wallet data was successfully saved to the new location."),
                      CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::unlockForMinting(bool status)
{
    if(!walletModel)
        return;

    if (status)
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Locked)
            return;

        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        walletModel->setUnlockedForMintingOnly(true);
    }
    else
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        if (!walletModel->isUnlockedForMintingOnly())
            return;

        walletModel->setWalletLocked(true);
        walletModel->setUnlockedForMintingOnly(false);
    }
}

void WalletView::changeUnit(const QString &unit)
{
    WalletModel *oldWalletModel = this->walletModel;
    OptionsModel *optionsModel = oldWalletModel->getOptionsModel();
    WalletModel *newWalletModel = NULL;

    BOOST_FOREACH(CWallet *wallet, setpwalletRegistered)
    {
        if (QString(wallet->Unit()) == unit)
        {
            newWalletModel = new WalletModel(wallet, optionsModel);
            break;
        }
    }
    setUpdatesEnabled(false);
    setWalletModel(newWalletModel);
    setUpdatesEnabled(true);
}

void WalletView::exportPeercoinKeys()
{
    QMessageBox::StandardButton reply;

    QString sQuestion = tr("All your NuShares private keys will be converted to Peercoin private keys and imported into your Peercoin wallet.\n\nThe Peercoin wallet must be running, unlocked (if it was encrypted) and accept RPC commands.\n\nThis process may take several minutes because Peercoin will scan the blockchain for transactions on all the imported keys.\n\nDo you want to proceed?");
    reply = QMessageBox::warning(this, tr("Peercoin keys export confirmation"), sQuestion, QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    bool fMustLock = false;
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        fMustLock = true;
    }

    try {
        int iExportedCount, iErrorCount;
        walletModel->ExportPeercoinKeys(iExportedCount, iErrorCount);
        if (fMustLock)
            walletModel->setWalletLocked(true);
        QMessageBox::information(this,
                tr("Peercoin keys export"),
                tr("%1 key(s) were exported to Peercoin.\n%2 key(s) were either already known or invalid.")
                  .arg(iExportedCount)
                  .arg(iErrorCount)
                );
    }
    catch (std::runtime_error &e) {
        if (fMustLock)
            walletModel->setWalletLocked(true);
        QMessageBox::critical(this,
                tr("Peercoin keys export"),
                tr("Error: %1").arg(e.what()));
    }
}

unsigned char WalletView::getUnit() const
{
    return walletModel->getUnit();
}

bool WalletView::isUnlockedForMintingOnly() const
{
    return walletModel->isUnlockedForMintingOnly();
}

void WalletView::gotoVotePage()
{
    gui->getVoteAction()->setChecked(true);
    setCurrentWidget(votePage);
}

void WalletView::gotoParkPage()
{
    gui->getParkAction()->setChecked(true);
    setCurrentWidget(parkPage);
}
