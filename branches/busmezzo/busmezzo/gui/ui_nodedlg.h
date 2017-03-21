/********************************************************************************
** Form generated from reading UI file 'nodedlg.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_NODEDLG_H
#define UI_NODEDLG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPushButton>
#include <QtGui/QTabWidget>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_NodeDlg
{
public:
    QPushButton *pushButton;
    QTabWidget *tabWidget;
    QWidget *tab1;
    QLabel *label_5;
    QLabel *label_2;
    QLabel *label_3;
    QComboBox *nodeTypeComb;
    QLineEdit *nodeNameText;
    QLineEdit *posXText;
    QLineEdit *posYText;
    QLineEdit *nodeIDtext;
    QLabel *label;
    QLabel *label_4;
    QLineEdit *nodeIDtext_2;
    QWidget *tab2;

    void setupUi(QDialog *NodeDlg)
    {
        if (NodeDlg->objectName().isEmpty())
            NodeDlg->setObjectName(QString::fromUtf8("NodeDlg"));
        NodeDlg->resize(400, 358);
        const QIcon icon = QIcon(QString::fromUtf8(":/images/file_explore.png"));
        NodeDlg->setWindowIcon(icon);
        pushButton = new QPushButton(NodeDlg);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        pushButton->setGeometry(QRect(160, 320, 85, 28));
        tabWidget = new QTabWidget(NodeDlg);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setGeometry(QRect(10, 10, 381, 301));
        tab1 = new QWidget();
        tab1->setObjectName(QString::fromUtf8("tab1"));
        label_5 = new QLabel(tab1);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(190, 60, 51, 16));
        label_2 = new QLabel(tab1);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(190, 20, 61, 21));
        label_3 = new QLabel(tab1);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(25, 62, 46, 14));
        nodeTypeComb = new QComboBox(tab1);
        nodeTypeComb->setObjectName(QString::fromUtf8("nodeTypeComb"));
        nodeTypeComb->setEnabled(false);
        nodeTypeComb->setGeometry(QRect(260, 60, 91, 22));
        nodeNameText = new QLineEdit(tab1);
        nodeNameText->setObjectName(QString::fromUtf8("nodeNameText"));
        nodeNameText->setEnabled(false);
        nodeNameText->setGeometry(QRect(260, 20, 90, 22));
        posXText = new QLineEdit(tab1);
        posXText->setObjectName(QString::fromUtf8("posXText"));
        posXText->setEnabled(false);
        posXText->setGeometry(QRect(80, 60, 40, 22));
        posYText = new QLineEdit(tab1);
        posYText->setObjectName(QString::fromUtf8("posYText"));
        posYText->setEnabled(false);
        posYText->setGeometry(QRect(130, 60, 40, 22));
        nodeIDtext = new QLineEdit(tab1);
        nodeIDtext->setObjectName(QString::fromUtf8("nodeIDtext"));
        nodeIDtext->setEnabled(false);
        nodeIDtext->setGeometry(QRect(80, 20, 90, 22));
        label = new QLabel(tab1);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(25, 22, 46, 14));
        label_4 = new QLabel(tab1);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(25, 105, 46, 14));
        nodeIDtext_2 = new QLineEdit(tab1);
        nodeIDtext_2->setObjectName(QString::fromUtf8("nodeIDtext_2"));
        nodeIDtext_2->setEnabled(false);
        nodeIDtext_2->setGeometry(QRect(80, 100, 92, 22));
        tabWidget->addTab(tab1, QString());
        tab2 = new QWidget();
        tab2->setObjectName(QString::fromUtf8("tab2"));
        tabWidget->addTab(tab2, QString());

        retranslateUi(NodeDlg);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(NodeDlg);
    } // setupUi

    void retranslateUi(QDialog *NodeDlg)
    {
        NodeDlg->setWindowTitle(QApplication::translate("NodeDlg", "Node explore", 0, QApplication::UnicodeUTF8));
        pushButton->setText(QApplication::translate("NodeDlg", "OK", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("NodeDlg", "Node type", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("NodeDlg", "Node name", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("NodeDlg", "Position", 0, QApplication::UnicodeUTF8));
        nodeNameText->setText(QString());
        posXText->setText(QString());
        posYText->setText(QString());
        nodeIDtext->setText(QString());
        label->setText(QApplication::translate("NodeDlg", "Node ID", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("NodeDlg", "Server", 0, QApplication::UnicodeUTF8));
        nodeIDtext_2->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tab1), QApplication::translate("NodeDlg", "Basic properties", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(tab2), QApplication::translate("NodeDlg", "Turning movements", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class NodeDlg: public Ui_NodeDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_NODEDLG_H
