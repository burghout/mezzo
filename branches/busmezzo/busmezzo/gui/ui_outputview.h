/********************************************************************************
** Form generated from reading UI file 'outputview.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OUTPUTVIEW_H
#define UI_OUTPUTVIEW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QGroupBox>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>
#include <QtGui/QTabWidget>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_OutputView
{
public:
    QTabWidget *tabWidget;
    QWidget *tab_1;
    QGroupBox *groupBox_2;
    QLabel *label_3;
    QComboBox *ThicknessMOE;
    QLabel *thickness_legend;
    QLabel *thickness_min;
    QLabel *thickness_max;
    QSpinBox *maxThickness;
    QLabel *label_5;
    QLabel *thickness_unit;
    QGroupBox *groupBox;
    QCheckBox *inverseColourScale;
    QComboBox *ColourMOE;
    QLabel *colour_legend;
    QLabel *label_4;
    QLabel *colour_min;
    QLabel *colour_max;
    QLabel *colour_unit;
    QWidget *tab_2;
    QSpinBox *cutoff;
    QLabel *label_6;
    QLabel *label_7;
    QCheckBox *showLinkNames;
    QCheckBox *showLinkIds;

    void setupUi(QDialog *OutputView)
    {
        if (OutputView->objectName().isEmpty())
            OutputView->setObjectName(QString::fromUtf8("OutputView"));
        OutputView->setWindowModality(Qt::NonModal);
        OutputView->resize(259, 279);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/mime.png"), QSize(), QIcon::Normal, QIcon::Off);
        OutputView->setWindowIcon(icon);
        tabWidget = new QTabWidget(OutputView);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setGeometry(QRect(0, 0, 271, 281));
        tab_1 = new QWidget();
        tab_1->setObjectName(QString::fromUtf8("tab_1"));
        groupBox_2 = new QGroupBox(tab_1);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        groupBox_2->setGeometry(QRect(0, 0, 251, 131));
        label_3 = new QLabel(groupBox_2);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(10, 20, 61, 31));
        ThicknessMOE = new QComboBox(groupBox_2);
        ThicknessMOE->setObjectName(QString::fromUtf8("ThicknessMOE"));
        ThicknessMOE->setGeometry(QRect(70, 20, 161, 23));
        thickness_legend = new QLabel(groupBox_2);
        thickness_legend->setObjectName(QString::fromUtf8("thickness_legend"));
        thickness_legend->setGeometry(QRect(70, 80, 161, 31));
        thickness_legend->setAutoFillBackground(true);
        thickness_legend->setFrameShape(QFrame::StyledPanel);
        thickness_legend->setMargin(1);
        thickness_min = new QLabel(groupBox_2);
        thickness_min->setObjectName(QString::fromUtf8("thickness_min"));
        thickness_min->setGeometry(QRect(60, 111, 61, 20));
        thickness_max = new QLabel(groupBox_2);
        thickness_max->setObjectName(QString::fromUtf8("thickness_max"));
        thickness_max->setGeometry(QRect(220, 110, 61, 20));
        maxThickness = new QSpinBox(groupBox_2);
        maxThickness->setObjectName(QString::fromUtf8("maxThickness"));
        maxThickness->setGeometry(QRect(160, 50, 71, 23));
        maxThickness->setMaximum(50);
        maxThickness->setValue(20);
        label_5 = new QLabel(groupBox_2);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(70, 50, 81, 21));
        thickness_unit = new QLabel(groupBox_2);
        thickness_unit->setObjectName(QString::fromUtf8("thickness_unit"));
        thickness_unit->setGeometry(QRect(120, 110, 71, 20));
        groupBox = new QGroupBox(tab_1);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        groupBox->setGeometry(QRect(0, 130, 251, 121));
        inverseColourScale = new QCheckBox(groupBox);
        inverseColourScale->setObjectName(QString::fromUtf8("inverseColourScale"));
        inverseColourScale->setGeometry(QRect(70, 50, 121, 21));
        ColourMOE = new QComboBox(groupBox);
        ColourMOE->setObjectName(QString::fromUtf8("ColourMOE"));
        ColourMOE->setGeometry(QRect(70, 20, 161, 23));
        colour_legend = new QLabel(groupBox);
        colour_legend->setObjectName(QString::fromUtf8("colour_legend"));
        colour_legend->setGeometry(QRect(70, 80, 161, 21));
        colour_legend->setAutoFillBackground(false);
        colour_legend->setFrameShape(QFrame::StyledPanel);
        colour_legend->setMargin(1);
        label_4 = new QLabel(groupBox);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(10, 20, 61, 31));
        colour_min = new QLabel(groupBox);
        colour_min->setObjectName(QString::fromUtf8("colour_min"));
        colour_min->setGeometry(QRect(60, 100, 61, 20));
        colour_max = new QLabel(groupBox);
        colour_max->setObjectName(QString::fromUtf8("colour_max"));
        colour_max->setGeometry(QRect(220, 100, 61, 20));
        colour_unit = new QLabel(groupBox);
        colour_unit->setObjectName(QString::fromUtf8("colour_unit"));
        colour_unit->setGeometry(QRect(120, 100, 71, 20));
        tabWidget->addTab(tab_1, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QString::fromUtf8("tab_2"));
        cutoff = new QSpinBox(tab_2);
        cutoff->setObjectName(QString::fromUtf8("cutoff"));
        cutoff->setGeometry(QRect(120, 20, 51, 23));
        cutoff->setMaximum(100);
        cutoff->setValue(5);
        label_6 = new QLabel(tab_2);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setGeometry(QRect(180, 20, 81, 21));
        label_7 = new QLabel(tab_2);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setGeometry(QRect(30, 10, 71, 31));
        showLinkNames = new QCheckBox(tab_2);
        showLinkNames->setObjectName(QString::fromUtf8("showLinkNames"));
        showLinkNames->setGeometry(QRect(30, 60, 201, 21));
        showLinkIds = new QCheckBox(tab_2);
        showLinkIds->setObjectName(QString::fromUtf8("showLinkIds"));
        showLinkIds->setGeometry(QRect(30, 80, 201, 21));
        tabWidget->addTab(tab_2, QString());

        retranslateUi(OutputView);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(OutputView);
    } // setupUi

    void retranslateUi(QDialog *OutputView)
    {
        OutputView->setWindowTitle(QApplication::translate("OutputView", "Output View", 0, QApplication::UnicodeUTF8));
        groupBox_2->setTitle(QApplication::translate("OutputView", "Link Thickness", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("OutputView", "Thickness\n"
"MOE", 0, QApplication::UnicodeUTF8));
        thickness_legend->setText(QString());
        thickness_min->setText(QApplication::translate("OutputView", "Min", 0, QApplication::UnicodeUTF8));
        thickness_max->setText(QApplication::translate("OutputView", "Max", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("OutputView", "Max thickness", 0, QApplication::UnicodeUTF8));
        thickness_unit->setText(QApplication::translate("OutputView", "Unit", 0, QApplication::UnicodeUTF8));
        groupBox->setTitle(QApplication::translate("OutputView", "Link Colour", 0, QApplication::UnicodeUTF8));
        inverseColourScale->setText(QApplication::translate("OutputView", "Inverse scale", 0, QApplication::UnicodeUTF8));
        colour_legend->setText(QString());
        label_4->setText(QApplication::translate("OutputView", "Colour\n"
"MOE", 0, QApplication::UnicodeUTF8));
        colour_min->setText(QApplication::translate("OutputView", "Min", 0, QApplication::UnicodeUTF8));
        colour_max->setText(QApplication::translate("OutputView", "Max", 0, QApplication::UnicodeUTF8));
        colour_unit->setText(QApplication::translate("OutputView", "Unit", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(tab_1), QApplication::translate("OutputView", "Output", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        cutoff->setToolTip(QApplication::translate("OutputView", "Hides colour and thickness for small data values", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        label_6->setText(QApplication::translate("OutputView", "% of max", 0, QApplication::UnicodeUTF8));
        label_7->setText(QApplication::translate("OutputView", "Don't show\n"
" data if <", 0, QApplication::UnicodeUTF8));
        showLinkNames->setText(QApplication::translate("OutputView", "Show Link Names", 0, QApplication::UnicodeUTF8));
        showLinkIds->setText(QApplication::translate("OutputView", "Show Link IDs", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("OutputView", "More Settings", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class OutputView: public Ui_OutputView {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OUTPUTVIEW_H
