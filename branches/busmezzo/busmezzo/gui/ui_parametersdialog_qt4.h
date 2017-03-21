/********************************************************************************
** Form generated from reading UI file 'parametersdialog_qt4.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PARAMETERSDIALOG_QT4_H
#define UI_PARAMETERSDIALOG_QT4_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QSpinBox>
#include <QtGui/QTabWidget>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include <QtGui/QColorDialog>
#include <QtGui/QMatrix>

QT_BEGIN_NAMESPACE

class Ui_ParametersDialog
{
public:
    QVBoxLayout *vboxLayout;
    QTabWidget *tabWidget;
    QWidget *Widget8;
    QLabel *textLabel3;
    QLabel *textLabel3_2;
    QLabel *textLabel1_3;
    QLabel *textLabel1_3_2;
    QLabel *textLabel1_2;
    QLabel *textLabel1;
    QLabel *textLabel1_3_2_2;
    QPushButton *NodeColorButton;
    QPushButton *QueueColorButton;
    QCheckBox *ShowBgImage;
    QCheckBox *LinkIds;
    QPushButton *BgColorButton;
    QPushButton *LinkColorButton;
    QSpinBox *NodeThickness;
    QSpinBox *NodeRadius;
    QSpinBox *LinkThickness;
    QSpinBox *QueueThickness;
    QWidget *tab;
    QLabel *TextLabel2;
    QSpinBox *refreshrate;
    QLabel *TextLabel4;
    QLabel *TextLabel5;
    QLabel *TextLabel3;
    QLabel *TextLabel6;
    QSpinBox *simspeed;
    QLabel *TextLabel7;
    QLabel *TextLabel11;
    QLabel *TextLabel10;
    QLabel *TextLabel8;
    QSpinBox *updatefactor;
    QLabel *TextLabel9;
    QSpinBox *zoomfactor;
    QSpinBox *panfactor;
    QHBoxLayout *hboxLayout;
    QPushButton *buttonHelp;
    QSpacerItem *spacerItem;
    QPushButton *buttonOk;
    QPushButton *buttonCancel;

    void setupUi(QDialog *ParametersDialog)
    {
        if (ParametersDialog->objectName().isEmpty())
            ParametersDialog->setObjectName(QString::fromUtf8("ParametersDialog"));
        ParametersDialog->resize(449, 328);
        const QIcon icon = QIcon(QString::fromUtf8(":/images/icon-tool.png"));
        ParametersDialog->setWindowIcon(icon);
        ParametersDialog->setSizeGripEnabled(true);
        vboxLayout = new QVBoxLayout(ParametersDialog);
        vboxLayout->setSpacing(6);
        vboxLayout->setContentsMargins(11, 11, 11, 11);
        vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
        vboxLayout->setContentsMargins(11, 11, 11, 11);
        tabWidget = new QTabWidget(ParametersDialog);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setEnabled(true);
        Widget8 = new QWidget();
        Widget8->setObjectName(QString::fromUtf8("Widget8"));
        textLabel3 = new QLabel(Widget8);
        textLabel3->setObjectName(QString::fromUtf8("textLabel3"));
        textLabel3->setGeometry(QRect(300, 10, 66, 20));
        textLabel3->setWordWrap(false);
        textLabel3_2 = new QLabel(Widget8);
        textLabel3_2->setObjectName(QString::fromUtf8("textLabel3_2"));
        textLabel3_2->setGeometry(QRect(370, 10, 66, 20));
        textLabel3_2->setWordWrap(false);
        textLabel1_3 = new QLabel(Widget8);
        textLabel1_3->setObjectName(QString::fromUtf8("textLabel1_3"));
        textLabel1_3->setGeometry(QRect(10, 70, 61, 20));
        textLabel1_3->setWordWrap(false);
        textLabel1_3_2 = new QLabel(Widget8);
        textLabel1_3_2->setObjectName(QString::fromUtf8("textLabel1_3_2"));
        textLabel1_3_2->setGeometry(QRect(10, 100, 61, 20));
        textLabel1_3_2->setWordWrap(false);
        textLabel1_2 = new QLabel(Widget8);
        textLabel1_2->setObjectName(QString::fromUtf8("textLabel1_2"));
        textLabel1_2->setGeometry(QRect(10, 40, 61, 20));
        textLabel1_2->setWordWrap(false);
        textLabel1 = new QLabel(Widget8);
        textLabel1->setObjectName(QString::fromUtf8("textLabel1"));
        textLabel1->setGeometry(QRect(110, 10, 61, 20));
        textLabel1->setWordWrap(false);
        textLabel1_3_2_2 = new QLabel(Widget8);
        textLabel1_3_2_2->setObjectName(QString::fromUtf8("textLabel1_3_2_2"));
        textLabel1_3_2_2->setGeometry(QRect(10, 130, 76, 20));
        textLabel1_3_2_2->setWordWrap(false);
        NodeColorButton = new QPushButton(Widget8);
        NodeColorButton->setObjectName(QString::fromUtf8("NodeColorButton"));
        NodeColorButton->setGeometry(QRect(190, 70, 82, 25));
        QueueColorButton = new QPushButton(Widget8);
        QueueColorButton->setObjectName(QString::fromUtf8("QueueColorButton"));
        QueueColorButton->setGeometry(QRect(190, 100, 82, 25));
        ShowBgImage = new QCheckBox(Widget8);
        ShowBgImage->setObjectName(QString::fromUtf8("ShowBgImage"));
        ShowBgImage->setGeometry(QRect(100, 130, 79, 20));
        ShowBgImage->setChecked(true);
        LinkIds = new QCheckBox(Widget8);
        LinkIds->setObjectName(QString::fromUtf8("LinkIds"));
        LinkIds->setGeometry(QRect(100, 40, 79, 20));
        BgColorButton = new QPushButton(Widget8);
        BgColorButton->setObjectName(QString::fromUtf8("BgColorButton"));
        BgColorButton->setGeometry(QRect(190, 130, 82, 25));
        LinkColorButton = new QPushButton(Widget8);
        LinkColorButton->setObjectName(QString::fromUtf8("LinkColorButton"));
        LinkColorButton->setGeometry(QRect(190, 40, 82, 25));
        NodeThickness = new QSpinBox(Widget8);
        NodeThickness->setObjectName(QString::fromUtf8("NodeThickness"));
        NodeThickness->setGeometry(QRect(310, 70, 29, 20));
        NodeThickness->setValue(1);
        NodeRadius = new QSpinBox(Widget8);
        NodeRadius->setObjectName(QString::fromUtf8("NodeRadius"));
        NodeRadius->setGeometry(QRect(370, 70, 29, 20));
        NodeRadius->setMinimum(1);
        NodeRadius->setValue(4);
        LinkThickness = new QSpinBox(Widget8);
        LinkThickness->setObjectName(QString::fromUtf8("LinkThickness"));
        LinkThickness->setGeometry(QRect(310, 40, 29, 20));
        LinkThickness->setValue(1);
        QueueThickness = new QSpinBox(Widget8);
        QueueThickness->setObjectName(QString::fromUtf8("QueueThickness"));
        QueueThickness->setGeometry(QRect(310, 100, 29, 20));
        QueueThickness->setValue(6);
        tabWidget->addTab(Widget8, QString());
        tab = new QWidget();
        tab->setObjectName(QString::fromUtf8("tab"));
        TextLabel2 = new QLabel(tab);
        TextLabel2->setObjectName(QString::fromUtf8("TextLabel2"));
        TextLabel2->setGeometry(QRect(10, 30, 71, 20));
        TextLabel2->setWordWrap(false);
        refreshrate = new QSpinBox(tab);
        refreshrate->setObjectName(QString::fromUtf8("refreshrate"));
        refreshrate->setGeometry(QRect(90, 30, 80, 25));
        refreshrate->setFocusPolicy(Qt::ClickFocus);
        refreshrate->setMaximum(5000);
        refreshrate->setValue(100);
        TextLabel4 = new QLabel(tab);
        TextLabel4->setObjectName(QString::fromUtf8("TextLabel4"));
        TextLabel4->setGeometry(QRect(390, 30, 24, 22));
        TextLabel4->setWordWrap(false);
        TextLabel5 = new QLabel(tab);
        TextLabel5->setObjectName(QString::fromUtf8("TextLabel5"));
        TextLabel5->setGeometry(QRect(180, 80, 24, 22));
        TextLabel5->setWordWrap(false);
        TextLabel3 = new QLabel(tab);
        TextLabel3->setObjectName(QString::fromUtf8("TextLabel3"));
        TextLabel3->setGeometry(QRect(180, 30, 24, 22));
        TextLabel3->setWordWrap(false);
        TextLabel6 = new QLabel(tab);
        TextLabel6->setObjectName(QString::fromUtf8("TextLabel6"));
        TextLabel6->setGeometry(QRect(390, 80, 24, 22));
        TextLabel6->setWordWrap(false);
        simspeed = new QSpinBox(tab);
        simspeed->setObjectName(QString::fromUtf8("simspeed"));
        simspeed->setGeometry(QRect(90, 130, 80, 25));
        simspeed->setFocusPolicy(Qt::ClickFocus);
        simspeed->setMinimum(0);
        simspeed->setMaximum(2000);
        simspeed->setSingleStep(10);
        simspeed->setValue(100);
        TextLabel7 = new QLabel(tab);
        TextLabel7->setObjectName(QString::fromUtf8("TextLabel7"));
        TextLabel7->setGeometry(QRect(180, 130, 24, 22));
        TextLabel7->setWordWrap(false);
        TextLabel11 = new QLabel(tab);
        TextLabel11->setObjectName(QString::fromUtf8("TextLabel11"));
        TextLabel11->setGeometry(QRect(10, 130, 61, 20));
        TextLabel11->setWordWrap(false);
        TextLabel10 = new QLabel(tab);
        TextLabel10->setObjectName(QString::fromUtf8("TextLabel10"));
        TextLabel10->setGeometry(QRect(230, 80, 61, 20));
        TextLabel10->setWordWrap(false);
        TextLabel8 = new QLabel(tab);
        TextLabel8->setObjectName(QString::fromUtf8("TextLabel8"));
        TextLabel8->setGeometry(QRect(220, 30, 81, 20));
        TextLabel8->setWordWrap(false);
        updatefactor = new QSpinBox(tab);
        updatefactor->setObjectName(QString::fromUtf8("updatefactor"));
        updatefactor->setGeometry(QRect(300, 30, 80, 25));
        updatefactor->setFocusPolicy(Qt::ClickFocus);
        updatefactor->setMinimum(1);
        updatefactor->setMaximum(100);
        updatefactor->setSingleStep(1);
        updatefactor->setValue(100);
        TextLabel9 = new QLabel(tab);
        TextLabel9->setObjectName(QString::fromUtf8("TextLabel9"));
        TextLabel9->setGeometry(QRect(10, 80, 71, 20));
        TextLabel9->setWordWrap(false);
        zoomfactor = new QSpinBox(tab);
        zoomfactor->setObjectName(QString::fromUtf8("zoomfactor"));
        zoomfactor->setGeometry(QRect(90, 80, 80, 25));
        zoomfactor->setFocusPolicy(Qt::ClickFocus);
        zoomfactor->setMinimum(1);
        zoomfactor->setMaximum(200);
        zoomfactor->setSingleStep(1);
        zoomfactor->setValue(150);
        panfactor = new QSpinBox(tab);
        panfactor->setObjectName(QString::fromUtf8("panfactor"));
        panfactor->setGeometry(QRect(300, 80, 80, 25));
        panfactor->setFocusPolicy(Qt::ClickFocus);
        panfactor->setMinimum(1);
        panfactor->setMaximum(200);
        panfactor->setSingleStep(10);
        panfactor->setValue(20);
        tabWidget->addTab(tab, QString());

        vboxLayout->addWidget(tabWidget);

        hboxLayout = new QHBoxLayout();
        hboxLayout->setSpacing(6);
        hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
        hboxLayout->setContentsMargins(0, 0, 0, 0);
        buttonHelp = new QPushButton(ParametersDialog);
        buttonHelp->setObjectName(QString::fromUtf8("buttonHelp"));
        buttonHelp->setAutoDefault(true);

        hboxLayout->addWidget(buttonHelp);

        spacerItem = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout->addItem(spacerItem);

        buttonOk = new QPushButton(ParametersDialog);
        buttonOk->setObjectName(QString::fromUtf8("buttonOk"));
        buttonOk->setAutoDefault(true);
        buttonOk->setDefault(true);

        hboxLayout->addWidget(buttonOk);

        buttonCancel = new QPushButton(ParametersDialog);
        buttonCancel->setObjectName(QString::fromUtf8("buttonCancel"));
        buttonCancel->setAutoDefault(true);

        hboxLayout->addWidget(buttonCancel);


        vboxLayout->addLayout(hboxLayout);


        retranslateUi(ParametersDialog);
        QObject::connect(buttonOk, SIGNAL(clicked()), ParametersDialog, SLOT(accept()));
        QObject::connect(buttonCancel, SIGNAL(clicked()), ParametersDialog, SLOT(reject()));
        QObject::connect(ShowBgImage, SIGNAL(toggled(bool)), ParametersDialog, SLOT(ShowBgImage_toggled(bool)));

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(ParametersDialog);
    } // setupUi

    void retranslateUi(QDialog *ParametersDialog)
    {
        ParametersDialog->setWindowTitle(QApplication::translate("ParametersDialog", "Parameters", 0, QApplication::UnicodeUTF8));
        textLabel3->setText(QApplication::translate("ParametersDialog", "<b>Thickness</b>", 0, QApplication::UnicodeUTF8));
        textLabel3_2->setText(QApplication::translate("ParametersDialog", "<b>Radius</b>", 0, QApplication::UnicodeUTF8));
        textLabel1_3->setText(QApplication::translate("ParametersDialog", "<b><i>Node</i></b>", 0, QApplication::UnicodeUTF8));
        textLabel1_3_2->setText(QApplication::translate("ParametersDialog", "<b><i>Queue</i></b>", 0, QApplication::UnicodeUTF8));
        textLabel1_2->setText(QApplication::translate("ParametersDialog", "<i><b>Link</b></i>", 0, QApplication::UnicodeUTF8));
        textLabel1->setText(QApplication::translate("ParametersDialog", "<b>Show</b>", 0, QApplication::UnicodeUTF8));
        textLabel1_3_2_2->setText(QApplication::translate("ParametersDialog", "<b><i>Background</i></b>", 0, QApplication::UnicodeUTF8));
        NodeColorButton->setText(QApplication::translate("ParametersDialog", "Node Color...", 0, QApplication::UnicodeUTF8));
        QueueColorButton->setText(QApplication::translate("ParametersDialog", "Queue Color...", 0, QApplication::UnicodeUTF8));
        ShowBgImage->setText(QApplication::translate("ParametersDialog", "Image", 0, QApplication::UnicodeUTF8));
        LinkIds->setText(QApplication::translate("ParametersDialog", "Link IDs", 0, QApplication::UnicodeUTF8));
        BgColorButton->setText(QApplication::translate("ParametersDialog", "Bg Color...", 0, QApplication::UnicodeUTF8));
        LinkColorButton->setText(QApplication::translate("ParametersDialog", "Link Color...", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(Widget8), QApplication::translate("ParametersDialog", "Display", 0, QApplication::UnicodeUTF8));
        TextLabel2->setText(QApplication::translate("ParametersDialog", "Refresh rate:", 0, QApplication::UnicodeUTF8));
        TextLabel4->setText(QApplication::translate("ParametersDialog", "%", 0, QApplication::UnicodeUTF8));
        TextLabel5->setText(QApplication::translate("ParametersDialog", "%", 0, QApplication::UnicodeUTF8));
        TextLabel3->setText(QApplication::translate("ParametersDialog", "ms", 0, QApplication::UnicodeUTF8));
        TextLabel6->setText(QApplication::translate("ParametersDialog", "px", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        simspeed->setToolTip(QApplication::translate("ParametersDialog", "100% is 1:1", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_WHATSTHIS
        simspeed->setWhatsThis(QApplication::translate("ParametersDialog", "Updatefactor determines how much time is reserved for simulation and how much for updating the screen. 100% means fastest simulation. 99% is already much slower, but the GUI responds much nicer.", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS
        TextLabel7->setText(QApplication::translate("ParametersDialog", "%", 0, QApplication::UnicodeUTF8));
        TextLabel11->setText(QApplication::translate("ParametersDialog", "Sim speeds:", 0, QApplication::UnicodeUTF8));
        TextLabel10->setText(QApplication::translate("ParametersDialog", "Pan factor:", 0, QApplication::UnicodeUTF8));
        TextLabel8->setText(QApplication::translate("ParametersDialog", "Update factor:", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        updatefactor->setToolTip(QApplication::translate("ParametersDialog", "100% is max speed", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_WHATSTHIS
        updatefactor->setWhatsThis(QApplication::translate("ParametersDialog", "Updatefactor determines how much time is reserved for simulation and how much for updating the screen. 100% means fastest simulation. 99% is already much slower, but the GUI responds much nicer.", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS
        TextLabel9->setText(QApplication::translate("ParametersDialog", "Zoom factor:", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        zoomfactor->setToolTip(QApplication::translate("ParametersDialog", "100% is 1:1", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_WHATSTHIS
        zoomfactor->setWhatsThis(QApplication::translate("ParametersDialog", "Updatefactor determines how much time is reserved for simulation and how much for updating the screen. 100% means fastest simulation. 99% is already much slower, but the GUI responds much nicer.", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS
#ifndef QT_NO_TOOLTIP
        panfactor->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_WHATSTHIS
        panfactor->setWhatsThis(QString());
#endif // QT_NO_WHATSTHIS
        tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("ParametersDialog", "Simulation", 0, QApplication::UnicodeUTF8));
        buttonHelp->setText(QApplication::translate("ParametersDialog", "&Help", 0, QApplication::UnicodeUTF8));
        buttonHelp->setShortcut(QApplication::translate("ParametersDialog", "F1", 0, QApplication::UnicodeUTF8));
        buttonOk->setText(QApplication::translate("ParametersDialog", "&OK", 0, QApplication::UnicodeUTF8));
        buttonOk->setShortcut(QString());
        buttonCancel->setText(QApplication::translate("ParametersDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
        buttonCancel->setShortcut(QString());
    } // retranslateUi

};

namespace Ui {
    class ParametersDialog: public Ui_ParametersDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PARAMETERSDIALOG_QT4_H
