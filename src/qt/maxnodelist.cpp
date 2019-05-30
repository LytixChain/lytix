// Copyright (c) 2014-2016 The Dash Developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxnodelist.h"
#include "ui_maxnodelist.h"


#include "activemaxnode.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "init.h"
#include "maxnode-sync.h"
#include "maxnodeconfig.h"
#include "maxnodeman.h"
#include "sync.h"
#include "wallet.h"
#include "walletmodel.h"
#include "askpassphrasedialog.h"

#include <QMessageBox>
#include <QTimer>

CCriticalSection cs_maxnodes;

MaxnodeList::MaxnodeList(QWidget* parent) : QWidget(parent),
                                                  ui(new Ui::MaxnodeList),
                                                  clientModel(0),
                                                  walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyMaxnodes->setAlternatingRowColors(true);
    ui->tableWidgetMyMaxnodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyMaxnodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyMaxnodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyMaxnodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyMaxnodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyMaxnodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetMyMaxnodes->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction* startMaxAliasAction = new QAction(tr("Start alias"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(startMaxAliasAction);
    connect(ui->tableWidgetMyMaxnodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startMaxAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    // Fill MAX list
    fFilterUpdated = true;
    nTimeFilterUpdated = GetTime();
}

MaxnodeList::~MaxnodeList()
{
    delete ui;
}

void MaxnodeList::setClientModel(ClientModel* model)
{
    this->clientModel = model;
}

void MaxnodeList::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
}

void MaxnodeList::showContextMenu(const QPoint& point)
{
    QTableWidgetItem* item = ui->tableWidgetMyMaxnodes->itemAt(point);
    if (item) contextMenu->exec(QCursor::pos());
}


void MaxnodeList::StartAlias(std::string strMaxAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strMaxAlias;

    BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
        if (maxe.getAlias() == strMaxAlias) {
            std::string strError;
            CMaxnodeBroadcast maxb;

            bool fSuccess = CMaxnodeBroadcast::Create(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), strError, maxb);

            if (fSuccess) {
                strStatusHtml += "<br>Successfully started maxnode.";
                maxnodeman.UpdateMaxnodeList(maxb);
                maxb.Relay();
            } else {
                strStatusHtml += "<br>Failed to start maxnode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void MaxnodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
        std::string strError;
        CMaxnodeBroadcast maxb;

        int nIndex;
        if(!maxe.castOutputIndex(nIndex))
            continue;

        CTxIn txin = CTxIn(uint256S(maxe.getTxHash()), uint32_t(nIndex));
        CMaxnode* pmax = maxnodeman.Find(txin);

        if (strCommand == "start-missing" && pmax) continue;

        bool fSuccess = CMaxnodeBroadcast::Create(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), strError, maxb);

        if (fSuccess) {
            nCountSuccessful++;
            maxnodeman.UpdateMaxnodeList(maxb);
            maxb.Relay();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + maxe.getAlias() + ". Error: " + strError;
        }
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d maxnodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void MaxnodeList::updateMyMaxnodeInfo(QString strMaxAlias, QString strAddr, CMaxnode* pmax)
{
    LOCK(cs_maxlistupdate);
    bool fOldRowFound = false;
    int nNewRow = 0;

    for (int i = 0; i < ui->tableWidgetMyMaxnodes->rowCount(); i++) {
        if (ui->tableWidgetMyMaxnodes->item(i, 0)->text() == strMaxAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if (nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyMaxnodes->rowCount();
        ui->tableWidgetMyMaxnodes->insertRow(nNewRow);
    }

    QTableWidgetItem* aliasItem = new QTableWidgetItem(strMaxAlias);
    QTableWidgetItem* addrItem = new QTableWidgetItem(pmax ? QString::fromStdString(pmax->addr.ToString()) : strAddr);
    QTableWidgetItem* protocolItem = new QTableWidgetItem(QString::number(pmax ? pmax->protocolVersion : -1));
    QTableWidgetItem* statusItem = new QTableWidgetItem(QString::fromStdString(pmax ? pmax->GetStatus() : "MISSING"));
    GUIUtil::DHMSTableWidgetItem* activeSecondsItem = new GUIUtil::DHMSTableWidgetItem(pmax ? (pmax->lastPing.sigTime - pmax->sigTime) : 0);
    QTableWidgetItem* lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", pmax ? pmax->lastPing.sigTime : 0)));
    QTableWidgetItem* pubkeyItem = new QTableWidgetItem(QString::fromStdString(pmax ? CBitcoinAddress(pmax->pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyMaxnodes->setItem(nNewRow, 6, pubkeyItem);
}

void MaxnodeList::updateMyNodeList(bool fForce)
{
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my maxnode list only once in MY_MAXNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_MAXNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if (nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetMyMaxnodes->setSortingEnabled(false);
    BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
        int nIndex;
        if(!maxe.castOutputIndex(nIndex))
            continue;

        CTxIn txin = CTxIn(uint256S(maxe.getTxHash()), uint32_t(nIndex));
        CMaxnode* pmax = maxnodeman.Find(txin);
        updateMyMaxnodeInfo(QString::fromStdString(maxe.getAlias()), QString::fromStdString(maxe.getIp()), pmax);
    }
    ui->tableWidgetMyMaxnodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void MaxnodeList::on_startButton_clicked()
{
    // Find selected node alias
    QItemSelectionModel* selectionModel = ui->tableWidgetMyMaxnodes->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();

    if (selected.count() == 0) return;

    QModelIndex index = selected.at(0);
    int nSelectedRow = index.row();
    std::string strMaxAlias = ui->tableWidgetMyMaxnodes->item(nSelectedRow, 0)->text().toStdString();

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm maxnode start"),
        tr("Are you sure you want to start maxnode %1?").arg(QString::fromStdString(strMaxAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full));

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strMaxAlias);
        return;
    }

    StartAlias(strMaxAlias);
}


void MaxnodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all maxnodes start"),
        tr("Are you sure you want to start ALL maxnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full));

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void MaxnodeList::on_startMissingButton_clicked()
{
    if (!maxnodeSync.IsMaxnodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until maxnode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing maxnodes start"),
        tr("Are you sure you want to start MISSING maxnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::Unlock_Full));

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void MaxnodeList::on_tableWidgetMyMaxnodes_itemSelectionChanged()
{
    if (ui->tableWidgetMyMaxnodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void MaxnodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
