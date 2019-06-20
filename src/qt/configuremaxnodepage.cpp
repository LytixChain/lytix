// Copyright (c) 2018 The Phore developers
// Copyright (c) 2018 The Curium developers
// Copyright (c) 2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "configuremaxnodepage.h"
#include "ui_configuremaxnodepage.h"

#include "activemaxnode.h"
#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "maxnode-budget.h"
#include "maxnode-payments.h"
#include "maxnodeconfig.h"
#include "maxnodeman.h"
#include "maxnodelist.h"
#include "wallet.h"

#include <univalue.h>
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <iostream>
#include <string>

ConfigureMaxnodePage::ConfigureMaxnodePage(Mode mode, QWidget* parent) : QDialog(parent),
                                                                   ui(new Ui::ConfigureMaxnodePage),
                                                                   mapper(0),
                                                                   mode(mode)
{
    ui->setupUi(this);
	
	GUIUtil::setupAliasWidget(ui->aliasEdit, this);
	GUIUtil::setupIPWidget(ui->vpsIpEdit, this);
	GUIUtil::setupPrivKeyWidget(ui->privKeyEdit, this);
	GUIUtil::setupTXIDWidget(ui->outputEdit, this);
	GUIUtil::setupTXIDIndexWidget(ui->outputIdEdit, this);

    switch (mode) {
    case NewConfigureMaxnode:
        setWindowTitle(tr("New Maxnode Alias"));
        break;
    case EditConfigureMaxnode:
        setWindowTitle(tr("Edit Maxnode Alias"));
        break;
    }

}

ConfigureMaxnodePage::~ConfigureMaxnodePage()
{
    delete ui;
}


void ConfigureMaxnodePage::loadAlias(QString strMaxAlias)
{
   ui->aliasEdit->setText(strMaxAlias);
}

void ConfigureMaxnodePage::counter(int counter)
{
   setCounters(counter);
}


void ConfigureMaxnodePage::MNAliasCache(QString MnAliasCache)
{
   setMnAliasCache(MnAliasCache);
}

void ConfigureMaxnodePage::loadIP(QString strIP)
{
   ui->vpsIpEdit->setText(strIP);
}

void ConfigureMaxnodePage::loadPrivKey(QString strPrivKey)
{
   ui->privKeyEdit->setText(strPrivKey);
}

void ConfigureMaxnodePage::loadTxHash(QString strTxHash)
{
   ui->outputEdit->setText(strTxHash);
}

void ConfigureMaxnodePage::loadOutputIndex(QString strOutputIndex)
{
   ui->outputIdEdit->setText(strOutputIndex);
}


void ConfigureMaxnodePage::saveCurrentRow()
{

    switch (mode) {
    case NewConfigureMaxnode:
		if(ui->aliasEdit->text().toStdString().empty() || ui->vpsIpEdit->text().toStdString().empty() || ui->privKeyEdit->text().toStdString().empty() || ui->outputEdit->text().toStdString().empty() || ui->outputIdEdit->text().toStdString().empty()) {
			break;
		}	
		maxnodeConfig.add(ui->aliasEdit->text().toStdString(), ui->vpsIpEdit->text().toStdString(), ui->privKeyEdit->text().toStdString(), ui->outputEdit->text().toStdString(), ui->outputIdEdit->text().toStdString());
		maxnodeConfig.writeToMaxnodeConf();
        break;
    case EditConfigureMaxnode:
		if(ui->aliasEdit->text().toStdString().empty() || ui->vpsIpEdit->text().toStdString().empty() || ui->privKeyEdit->text().toStdString().empty() || ui->outputEdit->text().toStdString().empty() || ui->outputIdEdit->text().toStdString().empty()) {
			break;
		}
	    
	    QString MnAlias = getMnAliasCache();
		ConfigureMaxnodePage::updateAlias(ui->aliasEdit->text().toStdString(), ui->vpsIpEdit->text().toStdString(), ui->privKeyEdit->text().toStdString(), ui->outputEdit->text().toStdString(), ui->outputIdEdit->text().toStdString(), MnAlias.toStdString());
		break;
    }
}

void ConfigureMaxnodePage::accept()
{
	saveCurrentRow();
	emit accepted();
    QDialog::accept();
}


void ConfigureMaxnodePage::updateAlias(std::string Alias, std::string IP, std::string PrivKey, std::string TxHash, std::string OutputIndex, std::string mnAlias)
{
	BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry mne, maxnodeConfig.getEntries()) {
		if(mnAlias == mne.getAlias()) {
			int count = 0;
			count = getCounters();
			vector<COutPoint> confLockedCoins;
			uint256 mnTxHash;
			mnTxHash.SetHex(mne.getTxHash());
			int nIndex;
			if(!mne.castOutputIndex(nIndex))
				continue;
			COutPoint outpoint = COutPoint(mnTxHash, nIndex);
			confLockedCoins.push_back(outpoint);
			pwalletMain->UnlockCoin(outpoint);

			maxnodeConfig.deleteAlias(count);
			maxnodeConfig.add(Alias, IP, PrivKey, TxHash, OutputIndex);
			// write to maxnode.conf
			maxnodeConfig.writeToMaxnodeConf();
		}
	}	

}

void ConfigureMaxnodePage::on_AutoFillPrivKey_clicked()
{
    CKey secret;
    secret.MakeNewKey(false);

	ui->privKeyEdit->setText(QString::fromStdString(CBitcoinSecret(secret).ToString()));
}


void ConfigureMaxnodePage::on_AutoFillOutputs_clicked()
{
    // Find possible candidates
    vector<COutput> possibleCoins = activeMaxnode.SelectCoinsMaxnode();
        int test = 0;
    BOOST_FOREACH (COutput& out, possibleCoins) {
        std::string TXHash = out.tx->GetHash().ToString();
        std::string OutputID = std::to_string(out.i);
                BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry mne, maxnodeConfig.getEntries()) {
                        if(OutputID == mne.getOutputIndex() && TXHash == mne.getTxHash()) {
                                test = 1;

                        }
                }

                if(test == 0) {
                        ui->outputEdit->setText(QString::fromStdString(out.tx->GetHash().ToString()));
                        ui->outputIdEdit->setText(QString::fromStdString(std::to_string(out.i)));

                        break;
                }
                test = 0;
    }
}

