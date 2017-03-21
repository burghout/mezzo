/********************************************************************************
** Form generated from reading UI file 'canvas_qt4.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CANVAS_QT4_H
#define UI_CANVAS_QT4_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLCDNumber>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QProgressBar>
#include <QtGui/QSlider>
#include <QtGui/QToolBar>
#include <QtGui/QWidget>
#include <QtGui/QBitmap>
#include <QtGui/QLCDNumber>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtGui/QStatusBar>
#include <QtCore/QTimer>
#include <string>

QT_BEGIN_NAMESPACE

class Ui_MainForm
{
public:
    QAction *openmasterfile;
    QAction *savescreenshot;
    QAction *stop;
    QAction *run;
    QAction *breakoff;
    QAction *zoomin;
    QAction *zoomout;
    QAction *viewSet_ParametersAction;
    QAction *parametersdialog;
    QAction *loadbackground;
    QAction *saveresults;
    QAction *inspectdialog;
    QAction *zoombywin;
    QAction *linkhandlemark;
    QAction *inselectmode;
    QAction *quit;
    QAction *closenetwork;
    QAction *batch_run;
    QAction *actionAnalyzeOutput;
    QAction *actionPositionBackground;
    QWidget *widget;
    QLabel *Canvas;
    QWidget *layoutWidget;
    QHBoxLayout *hboxLayout;
    QLabel *status_label;
    QWidget *simprogress_widget;
    QLCDNumber *LCDNumber;
    QProgressBar *progressbar;
    QLabel *TextLabel12;
    QLabel *mouse_label;
    QSlider *horizontalSlider;
    QToolBar *toolBar1;
    QToolBar *Toolbar2;
    QMenuBar *menubar;
    QMenu *fileMenu;
    QMenu *EditMenu;
    QMenu *menuSimulation;
    QMenu *menuTools;
    QMenu *menuBackground;
    QMenu *menuAnalysis;

    void setupUi(QMainWindow *MainForm)
    {
        if (MainForm->objectName().isEmpty())
            MainForm->setObjectName(QString::fromUtf8("MainForm"));
        MainForm->resize(815, 674);
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(MainForm->sizePolicy().hasHeightForWidth());
        MainForm->setSizePolicy(sizePolicy);
        MainForm->setMinimumSize(QSize(16, 72));
        QFont font;
        font.setPointSize(12);
        MainForm->setFont(font);
        MainForm->setMouseTracking(true);
        MainForm->setFocusPolicy(Qt::StrongFocus);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/mime.png"), QSize(), QIcon::Normal, QIcon::Off);
        MainForm->setWindowIcon(icon);
        MainForm->setProperty("rightJustification", QVariant(false));
        openmasterfile = new QAction(MainForm);
        openmasterfile->setObjectName(QString::fromUtf8("openmasterfile"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/images/fileopen2.png"), QSize(), QIcon::Normal, QIcon::Off);
        openmasterfile->setIcon(icon1);
        savescreenshot = new QAction(MainForm);
        savescreenshot->setObjectName(QString::fromUtf8("savescreenshot"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/images/background.png"), QSize(), QIcon::Normal, QIcon::Off);
        savescreenshot->setIcon(icon2);
        stop = new QAction(MainForm);
        stop->setObjectName(QString::fromUtf8("stop"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/images/bt_stop.png"), QSize(), QIcon::Normal, QIcon::Off);
        stop->setIcon(icon3);
        run = new QAction(MainForm);
        run->setObjectName(QString::fromUtf8("run"));
        run->setChecked(false);
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/images/bt_play.png"), QSize(), QIcon::Normal, QIcon::Off);
        run->setIcon(icon4);
        breakoff = new QAction(MainForm);
        breakoff->setObjectName(QString::fromUtf8("breakoff"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/images/bt_pause.png"), QSize(), QIcon::Normal, QIcon::Off);
        breakoff->setIcon(icon5);
        zoomin = new QAction(MainForm);
        zoomin->setObjectName(QString::fromUtf8("zoomin"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/images/zoom_in.png"), QSize(), QIcon::Normal, QIcon::Off);
        zoomin->setIcon(icon6);
        zoomout = new QAction(MainForm);
        zoomout->setObjectName(QString::fromUtf8("zoomout"));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/images/zoom_out.png"), QSize(), QIcon::Normal, QIcon::Off);
        zoomout->setIcon(icon7);
        viewSet_ParametersAction = new QAction(MainForm);
        viewSet_ParametersAction->setObjectName(QString::fromUtf8("viewSet_ParametersAction"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/images/icon-tool.png"), QSize(), QIcon::Normal, QIcon::Off);
        viewSet_ParametersAction->setIcon(icon8);
        parametersdialog = new QAction(MainForm);
        parametersdialog->setObjectName(QString::fromUtf8("parametersdialog"));
        parametersdialog->setIcon(icon8);
        loadbackground = new QAction(MainForm);
        loadbackground->setObjectName(QString::fromUtf8("loadbackground"));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/images/background_map.jpg"), QSize(), QIcon::Normal, QIcon::Off);
        loadbackground->setIcon(icon9);
        saveresults = new QAction(MainForm);
        saveresults->setObjectName(QString::fromUtf8("saveresults"));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/images/filesave.png"), QSize(), QIcon::Normal, QIcon::Off);
        saveresults->setIcon(icon10);
        inspectdialog = new QAction(MainForm);
        inspectdialog->setObjectName(QString::fromUtf8("inspectdialog"));
        inspectdialog->setCheckable(false);
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/images/inspect.png"), QSize(), QIcon::Normal, QIcon::Off);
        inspectdialog->setIcon(icon11);
        zoombywin = new QAction(MainForm);
        zoombywin->setObjectName(QString::fromUtf8("zoombywin"));
        zoombywin->setCheckable(true);
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/images/file_explore.png"), QSize(), QIcon::Normal, QIcon::Off);
        zoombywin->setIcon(icon12);
        linkhandlemark = new QAction(MainForm);
        linkhandlemark->setObjectName(QString::fromUtf8("linkhandlemark"));
        linkhandlemark->setCheckable(true);
        linkhandlemark->setChecked(false);
        QIcon icon13;
        icon13.addFile(QString::fromUtf8(":/images/penmark.png"), QSize(), QIcon::Normal, QIcon::Off);
        linkhandlemark->setIcon(icon13);
        inselectmode = new QAction(MainForm);
        inselectmode->setObjectName(QString::fromUtf8("inselectmode"));
        inselectmode->setCheckable(true);
        inselectmode->setChecked(false);
        QIcon icon14;
        icon14.addFile(QString::fromUtf8(":/images/file_edit.png"), QSize(), QIcon::Normal, QIcon::Off);
        inselectmode->setIcon(icon14);
        quit = new QAction(MainForm);
        quit->setObjectName(QString::fromUtf8("quit"));
        QIcon icon15;
        icon15.addFile(QString::fromUtf8(":/images/exit.png"), QSize(), QIcon::Normal, QIcon::Off);
        quit->setIcon(icon15);
        closenetwork = new QAction(MainForm);
        closenetwork->setObjectName(QString::fromUtf8("closenetwork"));
        QIcon icon16;
        icon16.addFile(QString::fromUtf8(":/images/close.png"), QSize(), QIcon::Normal, QIcon::Off);
        closenetwork->setIcon(icon16);
        batch_run = new QAction(MainForm);
        batch_run->setObjectName(QString::fromUtf8("batch_run"));
        QIcon icon17;
        icon17.addFile(QString::fromUtf8(":/images/batchrun.gif"), QSize(), QIcon::Normal, QIcon::Off);
        batch_run->setIcon(icon17);
        actionAnalyzeOutput = new QAction(MainForm);
        actionAnalyzeOutput->setObjectName(QString::fromUtf8("actionAnalyzeOutput"));
        actionAnalyzeOutput->setCheckable(true);
        QIcon icon18;
        icon18.addFile(QString::fromUtf8(":/images/dvi.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAnalyzeOutput->setIcon(icon18);
        actionPositionBackground = new QAction(MainForm);
        actionPositionBackground->setObjectName(QString::fromUtf8("actionPositionBackground"));
        QIcon icon19;
        icon19.addFile(QString::fromUtf8(":/images/move.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionPositionBackground->setIcon(icon19);
        widget = new QWidget(MainForm);
        widget->setObjectName(QString::fromUtf8("widget"));
        Canvas = new QLabel(widget);
        Canvas->setObjectName(QString::fromUtf8("Canvas"));
        Canvas->setGeometry(QRect(5, 0, 800, 581));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(Canvas->sizePolicy().hasHeightForWidth());
        Canvas->setSizePolicy(sizePolicy1);
        Canvas->setMinimumSize(QSize(10, 10));
        QPalette palette;
        QBrush brush(QColor(0, 0, 0, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush1(QColor(255, 255, 255, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette.setBrush(QPalette::Active, QPalette::Light, brush1);
        palette.setBrush(QPalette::Active, QPalette::Midlight, brush1);
        QBrush brush2(QColor(127, 127, 127, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Dark, brush2);
        QBrush brush3(QColor(170, 170, 170, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Mid, brush3);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::BrightText, brush1);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette.setBrush(QPalette::Active, QPalette::Shadow, brush);
        palette.setBrush(QPalette::Active, QPalette::AlternateBase, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Light, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Midlight, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Dark, brush2);
        palette.setBrush(QPalette::Inactive, QPalette::Mid, brush3);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::BrightText, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Shadow, brush);
        palette.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Light, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Midlight, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Dark, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Mid, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::BrightText, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Shadow, brush);
        palette.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush1);
        Canvas->setPalette(palette);
        Canvas->setMouseTracking(true);
        Canvas->setFocusPolicy(Qt::StrongFocus);
        Canvas->setAutoFillBackground(false);
        Canvas->setFrameShape(QFrame::Box);
        Canvas->setFrameShadow(QFrame::Sunken);
        Canvas->setLineWidth(1);
        Canvas->setMidLineWidth(0);
        Canvas->setScaledContents(false);
        Canvas->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        Canvas->setWordWrap(false);
        Canvas->setMargin(0);
        layoutWidget = new QWidget(widget);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(10, 578, 791, 31));
        hboxLayout = new QHBoxLayout(layoutWidget);
        hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
        hboxLayout->setContentsMargins(0, 0, 0, 0);
        status_label = new QLabel(layoutWidget);
        status_label->setObjectName(QString::fromUtf8("status_label"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(status_label->sizePolicy().hasHeightForWidth());
        status_label->setSizePolicy(sizePolicy2);
        status_label->setMinimumSize(QSize(100, 15));
        QFont font1;
        font1.setFamily(QString::fromUtf8("Times New Roman"));
        font1.setPointSize(10);
        status_label->setFont(font1);

        hboxLayout->addWidget(status_label);

        simprogress_widget = new QWidget(layoutWidget);
        simprogress_widget->setObjectName(QString::fromUtf8("simprogress_widget"));
        simprogress_widget->setEnabled(false);
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(simprogress_widget->sizePolicy().hasHeightForWidth());
        simprogress_widget->setSizePolicy(sizePolicy3);
        LCDNumber = new QLCDNumber(simprogress_widget);
        LCDNumber->setObjectName(QString::fromUtf8("LCDNumber"));
        LCDNumber->setGeometry(QRect(40, 3, 81, 16));
        sizePolicy2.setHeightForWidth(LCDNumber->sizePolicy().hasHeightForWidth());
        LCDNumber->setSizePolicy(sizePolicy2);
        LCDNumber->setMinimumSize(QSize(80, 15));
        LCDNumber->setNumDigits(8);
        LCDNumber->setSegmentStyle(QLCDNumber::Flat);
        LCDNumber->setProperty("value", QVariant(0));
        LCDNumber->setProperty("intValue", QVariant(0));
        progressbar = new QProgressBar(simprogress_widget);
        progressbar->setObjectName(QString::fromUtf8("progressbar"));
        progressbar->setGeometry(QRect(130, 5, 120, 12));
        sizePolicy2.setHeightForWidth(progressbar->sizePolicy().hasHeightForWidth());
        progressbar->setSizePolicy(sizePolicy2);
        progressbar->setMinimumSize(QSize(30, 12));
        QFont font2;
        font2.setFamily(QString::fromUtf8("Times New Roman"));
        font2.setPointSize(7);
        progressbar->setFont(font2);
        TextLabel12 = new QLabel(simprogress_widget);
        TextLabel12->setObjectName(QString::fromUtf8("TextLabel12"));
        TextLabel12->setGeometry(QRect(10, 0, 41, 22));
        QSizePolicy sizePolicy4(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(TextLabel12->sizePolicy().hasHeightForWidth());
        TextLabel12->setSizePolicy(sizePolicy4);
        QFont font3;
        font3.setFamily(QString::fromUtf8("Times New Roman"));
        font3.setPointSize(8);
        TextLabel12->setFont(font3);
        TextLabel12->setWordWrap(false);

        hboxLayout->addWidget(simprogress_widget);

        mouse_label = new QLabel(layoutWidget);
        mouse_label->setObjectName(QString::fromUtf8("mouse_label"));
        sizePolicy2.setHeightForWidth(mouse_label->sizePolicy().hasHeightForWidth());
        mouse_label->setSizePolicy(sizePolicy2);
        mouse_label->setMinimumSize(QSize(100, 15));
        mouse_label->setFont(font1);

        hboxLayout->addWidget(mouse_label);

        horizontalSlider = new QSlider(widget);
        horizontalSlider->setObjectName(QString::fromUtf8("horizontalSlider"));
        horizontalSlider->setEnabled(true);
        horizontalSlider->setGeometry(QRect(20, 10, 771, 20));
        horizontalSlider->setAutoFillBackground(false);
        horizontalSlider->setMinimum(0);
        horizontalSlider->setMaximum(4);
        horizontalSlider->setPageStep(1);
        horizontalSlider->setOrientation(Qt::Horizontal);
        horizontalSlider->setTickPosition(QSlider::TicksBelow);
        horizontalSlider->setTickInterval(1);
        MainForm->setCentralWidget(widget);
        toolBar1 = new QToolBar(MainForm);
        toolBar1->setObjectName(QString::fromUtf8("toolBar1"));
        toolBar1->setProperty("horizontallyStretchable", QVariant(true));
        toolBar1->setProperty("verticallyStretchable", QVariant(true));
        MainForm->addToolBar(Qt::TopToolBarArea, toolBar1);
        Toolbar2 = new QToolBar(MainForm);
        Toolbar2->setObjectName(QString::fromUtf8("Toolbar2"));
        MainForm->addToolBar(Qt::TopToolBarArea, Toolbar2);
        menubar = new QMenuBar(MainForm);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 815, 25));
        fileMenu = new QMenu(menubar);
        fileMenu->setObjectName(QString::fromUtf8("fileMenu"));
        EditMenu = new QMenu(menubar);
        EditMenu->setObjectName(QString::fromUtf8("EditMenu"));
        menuSimulation = new QMenu(menubar);
        menuSimulation->setObjectName(QString::fromUtf8("menuSimulation"));
        menuTools = new QMenu(menubar);
        menuTools->setObjectName(QString::fromUtf8("menuTools"));
        menuBackground = new QMenu(menuTools);
        menuBackground->setObjectName(QString::fromUtf8("menuBackground"));
        menuAnalysis = new QMenu(menubar);
        menuAnalysis->setObjectName(QString::fromUtf8("menuAnalysis"));
        MainForm->setMenuBar(menubar);

        toolBar1->addAction(openmasterfile);
        toolBar1->addAction(closenetwork);
        toolBar1->addAction(saveresults);
        toolBar1->addSeparator();
        toolBar1->addAction(run);
        toolBar1->addAction(breakoff);
        toolBar1->addAction(stop);
        toolBar1->addAction(batch_run);
        toolBar1->addSeparator();
        toolBar1->addAction(zoomin);
        toolBar1->addAction(zoomout);
        toolBar1->addAction(zoombywin);
        toolBar1->addSeparator();
        toolBar1->addAction(linkhandlemark);
        toolBar1->addAction(inselectmode);
        Toolbar2->addAction(parametersdialog);
        Toolbar2->addAction(inspectdialog);
        Toolbar2->addAction(actionAnalyzeOutput);
        menubar->addAction(fileMenu->menuAction());
        menubar->addAction(EditMenu->menuAction());
        menubar->addAction(menuSimulation->menuAction());
        menubar->addAction(menuTools->menuAction());
        menubar->addAction(menuAnalysis->menuAction());
        fileMenu->addAction(openmasterfile);
        fileMenu->addAction(closenetwork);
        fileMenu->addSeparator();
        fileMenu->addAction(savescreenshot);
        fileMenu->addAction(saveresults);
        fileMenu->addSeparator();
        fileMenu->addAction(quit);
        EditMenu->addAction(linkhandlemark);
        EditMenu->addAction(inselectmode);
        menuSimulation->addAction(run);
        menuSimulation->addAction(breakoff);
        menuSimulation->addAction(stop);
        menuSimulation->addSeparator();
        menuSimulation->addAction(batch_run);
        menuTools->addAction(menuBackground->menuAction());
        menuTools->addSeparator();
        menuTools->addAction(parametersdialog);
        menuTools->addAction(savescreenshot);
        menuBackground->addAction(loadbackground);
        menuBackground->addAction(actionPositionBackground);
        menuAnalysis->addAction(actionAnalyzeOutput);
        menuAnalysis->addAction(inspectdialog);

        retranslateUi(MainForm);

        QMetaObject::connectSlotsByName(MainForm);
    } // setupUi

    void retranslateUi(QMainWindow *MainForm)
    {
        MainForm->setWindowTitle(QApplication::translate("MainForm", "Mezzo version 0.53", "Prototype Mesomodel", QApplication::UnicodeUTF8));
        MainForm->setWindowIconText(QApplication::translate("MainForm", "Mezzo", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        MainForm->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        openmasterfile->setText(QApplication::translate("MainForm", "&Open Network", 0, QApplication::UnicodeUTF8));
        openmasterfile->setIconText(QApplication::translate("MainForm", "&Open Master file...", 0, QApplication::UnicodeUTF8));
        openmasterfile->setShortcut(QApplication::translate("MainForm", "Ctrl+O", 0, QApplication::UnicodeUTF8));
        savescreenshot->setText(QApplication::translate("MainForm", "&Save Snapshot", 0, QApplication::UnicodeUTF8));
        savescreenshot->setIconText(QApplication::translate("MainForm", "Save Snapshot", 0, QApplication::UnicodeUTF8));
        savescreenshot->setShortcut(QApplication::translate("MainForm", "Ctrl+S", 0, QApplication::UnicodeUTF8));
        stop->setText(QApplication::translate("MainForm", "Stop/Reset", 0, QApplication::UnicodeUTF8));
        stop->setIconText(QApplication::translate("MainForm", "Stop", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        stop->setToolTip(QApplication::translate("MainForm", "Stop & reset", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_STATUSTIP
        stop->setStatusTip(QString());
#endif // QT_NO_STATUSTIP
        run->setText(QApplication::translate("MainForm", "Run/Continue", 0, QApplication::UnicodeUTF8));
        run->setIconText(QApplication::translate("MainForm", "Run", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_WHATSTHIS
        run->setWhatsThis(QApplication::translate("MainForm", "Run the model", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS
        breakoff->setIconText(QApplication::translate("MainForm", "Pause", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_STATUSTIP
        breakoff->setStatusTip(QString());
#endif // QT_NO_STATUSTIP
        breakoff->setShortcut(QString());
        zoomin->setIconText(QApplication::translate("MainForm", "Zoom in", 0, QApplication::UnicodeUTF8));
        zoomout->setIconText(QApplication::translate("MainForm", "Zoom out", 0, QApplication::UnicodeUTF8));
        viewSet_ParametersAction->setIconText(QApplication::translate("MainForm", "&Set Parameters", 0, QApplication::UnicodeUTF8));
        parametersdialog->setText(QApplication::translate("MainForm", "&Parameters", 0, QApplication::UnicodeUTF8));
        parametersdialog->setIconText(QApplication::translate("MainForm", "&Parameters", 0, QApplication::UnicodeUTF8));
        loadbackground->setText(QApplication::translate("MainForm", "Load &Background", 0, QApplication::UnicodeUTF8));
        loadbackground->setIconText(QApplication::translate("MainForm", "Load &Background", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        loadbackground->setToolTip(QApplication::translate("MainForm", "Load Background&", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        saveresults->setText(QApplication::translate("MainForm", "Save Results", 0, QApplication::UnicodeUTF8));
        inspectdialog->setText(QApplication::translate("MainForm", "Inspect Routes", 0, QApplication::UnicodeUTF8));
        zoombywin->setText(QApplication::translate("MainForm", "Zoom window", 0, QApplication::UnicodeUTF8));
        linkhandlemark->setText(QApplication::translate("MainForm", "Link marks", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        linkhandlemark->setToolTip(QApplication::translate("MainForm", "Show link handles", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        inselectmode->setText(QApplication::translate("MainForm", "Selection mode", 0, QApplication::UnicodeUTF8));
        inselectmode->setIconText(QApplication::translate("MainForm", "Selection mode", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        inselectmode->setToolTip(QApplication::translate("MainForm", "Selction mode", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        quit->setText(QApplication::translate("MainForm", "Exit", 0, QApplication::UnicodeUTF8));
        closenetwork->setText(QApplication::translate("MainForm", "Close Network", 0, QApplication::UnicodeUTF8));
        batch_run->setText(QApplication::translate("MainForm", "Batch Run", 0, QApplication::UnicodeUTF8));
        actionAnalyzeOutput->setText(QApplication::translate("MainForm", "Analyze Output", 0, QApplication::UnicodeUTF8));
        actionPositionBackground->setText(QApplication::translate("MainForm", "Position Background", 0, QApplication::UnicodeUTF8));
        Canvas->setText(QString());
        status_label->setText(QApplication::translate("MainForm", "uninitialized", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        LCDNumber->setToolTip(QApplication::translate("MainForm", "Simulated time elapsed", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        TextLabel12->setText(QApplication::translate("MainForm", "Time:", 0, QApplication::UnicodeUTF8));
        mouse_label->setText(QApplication::translate("MainForm", "No mouse event", 0, QApplication::UnicodeUTF8));
        toolBar1->setProperty("label", QVariant(QApplication::translate("MainForm", "Tools", 0, QApplication::UnicodeUTF8)));
        Toolbar2->setProperty("label", QVariant(QApplication::translate("MainForm", "Toolbar", 0, QApplication::UnicodeUTF8)));
        fileMenu->setTitle(QApplication::translate("MainForm", "&File", 0, QApplication::UnicodeUTF8));
        EditMenu->setTitle(QApplication::translate("MainForm", "&Edit", 0, QApplication::UnicodeUTF8));
        menuSimulation->setTitle(QApplication::translate("MainForm", "Simulation", 0, QApplication::UnicodeUTF8));
        menuTools->setTitle(QApplication::translate("MainForm", "Tools", 0, QApplication::UnicodeUTF8));
        menuBackground->setTitle(QApplication::translate("MainForm", "Background", 0, QApplication::UnicodeUTF8));
        menuAnalysis->setTitle(QApplication::translate("MainForm", "Analyze", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainForm: public Ui_MainForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CANVAS_QT4_H
