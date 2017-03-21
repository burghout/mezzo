/********************************************************************************
** Form generated from reading UI file 'odcheckdlg.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ODCHECKDLG_H
#define UI_ODCHECKDLG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QTableView>

QT_BEGIN_NAMESPACE

class Ui_ODCheckerDlg
{
public:
    QGridLayout *gridLayout;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QPushButton *cancelButton;
    QSpacerItem *spacerItem2;
    QPushButton *CheckButton;
    QTableView *ODTableView;
    QSpacerItem *spacerItem3;
    QGroupBox *group1;
    QGridLayout *gridLayout1;
    QSpacerItem *spacerItem4;
    QSpacerItem *spacerItem5;
    QSpacerItem *spacerItem6;
    QComboBox *destcomb;
    QComboBox *origcomb;
    QLabel *Olabel;
    QLabel *Dlabel;
    QLabel *infolabel;

    void setupUi(QDialog *ODCheckerDlg)
    {
        if (ODCheckerDlg->objectName().isEmpty())
            ODCheckerDlg->setObjectName(QString::fromUtf8("ODCheckerDlg"));
        ODCheckerDlg->resize(549, 351);
        const QIcon icon = QIcon(QString::fromUtf8(":/images/inspect.png"));
        ODCheckerDlg->setWindowIcon(icon);
        gridLayout = new QGridLayout(ODCheckerDlg);
#ifndef Q_OS_MAC
        gridLayout->setSpacing(6);
#endif
#ifndef Q_OS_MAC
        gridLayout->setContentsMargins(9, 9, 9, 9);
#endif
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        spacerItem = new QSpacerItem(101, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(spacerItem, 0, 2, 1, 1);

        spacerItem1 = new QSpacerItem(101, 28, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(spacerItem1, 4, 2, 1, 1);

        cancelButton = new QPushButton(ODCheckerDlg);
        cancelButton->setObjectName(QString::fromUtf8("cancelButton"));

        gridLayout->addWidget(cancelButton, 3, 2, 1, 1);

        spacerItem2 = new QSpacerItem(101, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(spacerItem2, 2, 2, 1, 1);

        CheckButton = new QPushButton(ODCheckerDlg);
        CheckButton->setObjectName(QString::fromUtf8("CheckButton"));
        CheckButton->setCheckable(true);
        CheckButton->setDefault(false);

        gridLayout->addWidget(CheckButton, 1, 2, 1, 1);

        ODTableView = new QTableView(ODCheckerDlg);
        ODTableView->setObjectName(QString::fromUtf8("ODTableView"));

        gridLayout->addWidget(ODTableView, 5, 0, 1, 3);

        spacerItem3 = new QSpacerItem(31, 131, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(spacerItem3, 0, 1, 5, 1);

        group1 = new QGroupBox(ODCheckerDlg);
        group1->setObjectName(QString::fromUtf8("group1"));
        gridLayout1 = new QGridLayout(group1);
#ifndef Q_OS_MAC
        gridLayout1->setSpacing(6);
#endif
#ifndef Q_OS_MAC
        gridLayout1->setContentsMargins(9, 9, 9, 9);
#endif
        gridLayout1->setObjectName(QString::fromUtf8("gridLayout1"));
        spacerItem4 = new QSpacerItem(171, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout1->addItem(spacerItem4, 2, 2, 1, 1);

        spacerItem5 = new QSpacerItem(51, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout1->addItem(spacerItem5, 1, 1, 1, 1);

        spacerItem6 = new QSpacerItem(51, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout1->addItem(spacerItem6, 0, 1, 1, 1);

        destcomb = new QComboBox(group1);
        destcomb->setObjectName(QString::fromUtf8("destcomb"));

        gridLayout1->addWidget(destcomb, 1, 2, 1, 1);

        origcomb = new QComboBox(group1);
        origcomb->setObjectName(QString::fromUtf8("origcomb"));

        gridLayout1->addWidget(origcomb, 0, 2, 1, 1);

        Olabel = new QLabel(group1);
        Olabel->setObjectName(QString::fromUtf8("Olabel"));

        gridLayout1->addWidget(Olabel, 0, 0, 1, 1);

        Dlabel = new QLabel(group1);
        Dlabel->setObjectName(QString::fromUtf8("Dlabel"));

        gridLayout1->addWidget(Dlabel, 1, 0, 1, 1);

        infolabel = new QLabel(group1);
        infolabel->setObjectName(QString::fromUtf8("infolabel"));

        gridLayout1->addWidget(infolabel, 2, 0, 1, 1);


        gridLayout->addWidget(group1, 0, 0, 5, 1);

        QWidget::setTabOrder(origcomb, destcomb);
        QWidget::setTabOrder(destcomb, cancelButton);
        QWidget::setTabOrder(cancelButton, CheckButton);

        retranslateUi(ODCheckerDlg);
        QObject::connect(cancelButton, SIGNAL(clicked()), ODCheckerDlg, SLOT(reject()));

        QMetaObject::connectSlotsByName(ODCheckerDlg);
    } // setupUi

    void retranslateUi(QDialog *ODCheckerDlg)
    {
        ODCheckerDlg->setWindowTitle(QApplication::translate("ODCheckerDlg", "Mezzo Analyzer", 0, QApplication::UnicodeUTF8));
        cancelButton->setText(QApplication::translate("ODCheckerDlg", "Cancel", 0, QApplication::UnicodeUTF8));
        CheckButton->setText(QApplication::translate("ODCheckerDlg", "Check", 0, QApplication::UnicodeUTF8));
        group1->setTitle(QApplication::translate("ODCheckerDlg", "&OD inputs", 0, QApplication::UnicodeUTF8));
        destcomb->clear();
        destcomb->insertItems(0, QStringList()
         << QApplication::translate("ODCheckerDlg", "None", 0, QApplication::UnicodeUTF8)
        );
        origcomb->clear();
        origcomb->insertItems(0, QStringList()
         << QApplication::translate("ODCheckerDlg", "None", 0, QApplication::UnicodeUTF8)
        );
        Olabel->setText(QApplication::translate("ODCheckerDlg", "Original node:", 0, QApplication::UnicodeUTF8));
        Dlabel->setText(QApplication::translate("ODCheckerDlg", "Destination node:", 0, QApplication::UnicodeUTF8));
        infolabel->setText(QApplication::translate("ODCheckerDlg", "Please choose a pair of OD", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ODCheckerDlg: public Ui_ODCheckerDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ODCHECKDLG_H
