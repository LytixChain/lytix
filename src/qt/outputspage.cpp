// Copyright (c) 2018 The Phore developers
// Copyright (c) 2018 The Curium developers
// Copyright (c) 2018-2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "outputspage.h"
#include "ui_outputspage.h"

#include "master/activenode.h"
#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "master/node-budget.h"
#include "master/node-payments.h"
#include "master/nodeconfig.h"
#include "master/nodeman.h"

#include <univalue.h>
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <boost/tokenizer.hpp>
#include <fstream>

OutPutsPage::OutPutsPage(QWidget* parent) : QDialog(parent),
                                            ui(new Ui::OutPutsPage)
{
    ui->setupUi(this);

	key = OutPutsPage::getmasternodeoutputs();
	ui->outPutsWidget->append(key);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addSeparator();


    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(accept()));
}



OutPutsPage::~OutPutsPage()
{
    delete ui;
}




QString OutPutsPage::getmasternodeoutputs ()
{
    // Find possible candidates
    vector<COutput> possibleCoins = activeMasternode.SelectCoinsMasternode();

    UniValue result(UniValue::VARR);
    BOOST_FOREACH (COutput& out, possibleCoins) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("txhash", out.tx->GetHash().ToString()));
        obj.push_back(Pair("outputidx", out.i));
        result.push_back(obj);
    }
	
        std::string strPrint;

        // Format result reply
        if (result.isNull())
            strPrint = "";
        else if (result.isStr())
            strPrint = result.get_str();
        else
            strPrint = result.write(2);

    return QString::fromStdString(strPrint);
}
