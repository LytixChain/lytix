// Copyright (c) 2018 The Phore developers
// Copyright (c) 2018 The Curium developers
// Copyright (c) 2018 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "privkeypage.h"
#include "ui_privkeypage.h"

#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"

#include <univalue.h>
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QMessageBox>
#include <QSortFilterProxyModel>

MNAddressPage::MNAddressPage(QWidget* parent) : QDialog(parent),
                                            ui(new Ui::MNAddressPage)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->copyAddress->setIcon(QIcon());
#endif
	key = MNAddressPage::createmasternodekey();
	ui->payTo->setText(key);

    // Context menu actions
    QAction* copyAddressAction = new QAction(tr("&Copy Address"), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addSeparator();

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyAddress_clicked()));

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(accept()));
}



MNAddressPage::~MNAddressPage()
{
    delete ui;
}


void MNAddressPage::on_copyAddress_clicked()
{
     GUIUtil::setClipboard(ui->payTo->text());

}

QString MNAddressPage::createmasternodekey()
{
    CKey secret;
    secret.MakeNewKey(false);

    return QString::fromStdString(CBitcoinSecret(secret).ToString());
}
