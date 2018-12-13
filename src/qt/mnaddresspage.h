// Copyright (c) 2018 The Phore developers
// Copyright (c) 2018 The Curium developers
// Copyright (c) 2018 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PRIVKEYPAGE_H
#define BITCOIN_QT_PRIVKEYPAGE_H

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>
#include <univalue.h>

class WalletModel;

namespace Ui
{
class MNAddressPage;
}

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class MNAddressPage : public QDialog
{
    Q_OBJECT

public:

    explicit MNAddressPage(QWidget* parent);
    ~MNAddressPage();
	QString  createmasternodekey();
    const QString& getReturnValue() const { return returnValue; }

public slots:

private:
    Ui::MNAddressPage* ui;
    QMenu* contextMenu;
	QString key;
    QString returnValue;

private slots:
    /** Copy address of currently selected address entry to clipboard */
    void on_copyAddress_clicked();

signals:
    void getMNAddress(QString addr);
};

#endif // BITCOIN_QT_ADDRESSBOOKPAGE_H
