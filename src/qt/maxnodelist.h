// Copyright (c) 2014-2016 The Dash Developers
// Copyright (c) 2016-2017 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODELIST_H
#define MAXNODELIST_H

#include "maxnode.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QMenu>
#include <QTimer>
#include <QWidget>

#define MY_MAXNODELIST_UPDATE_SECONDS 60
#define MAXNODELIST_UPDATE_SECONDS 15
#define MAXNODELIST_FILTER_COOLDOWN_SECONDS 3

namespace Ui
{
class MaxnodeList;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Maxnode Manager page widget */
class MaxnodeList : public QWidget
{
    Q_OBJECT

public:
    explicit MaxnodeList(QWidget* parent = 0);
    ~MaxnodeList();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);
    void StartAlias(std::string strMaxAlias);
    void StartAll(std::string strCommand = "start-all-max");

private:
    QMenu* contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateMyMaxnodeInfo(QString strMaxAlias, QString strAddr, CMaxnode* pmax);
    void updateMyNodeList(bool fForce = false);

Q_SIGNALS:

private:
    QTimer* timer;
    Ui::MaxnodeList* ui;
    ClientModel* clientModel;
    WalletModel* walletModel;
    CCriticalSection cs_maxlistupdate;
    QString strCurrentFilter;

private Q_SLOTS:
    void showContextMenu(const QPoint&);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    void on_startMissingButton_clicked();
    void on_tableWidgetMyMaxnodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
};
#endif // MAXNODELIST_H
