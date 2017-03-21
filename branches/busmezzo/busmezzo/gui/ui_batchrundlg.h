/********************************************************************************
** Form generated from reading UI file 'batchrundlg.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_BATCHRUNDLG_H
#define UI_BATCHRUNDLG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QProgressBar>
#include <QtGui/QRadioButton>
#include <QtGui/QSpinBox>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Batchrun
{
public:
    QDialogButtonBox *buttonBox;
    QTabWidget *tabWidget;
    QWidget *IterationsTab;
    QRadioButton *iterate_traveltimes_rb;
    QGroupBox *convergence_gb;
    QSpinBox *max_iterations_val;
    QCheckBox *max_iterations_cb;
    QCheckBox *max_rmsn_cb;
    QLineEdit *max_rmsn_val;
    QCheckBox *rmsn_link_tt;
    QCheckBox *rmsn_od_tt;
    QLabel *label_5;
    QLabel *rmsn_ltt;
    QLabel *rmsn_odtt;
    QLabel *rmsn_lf;
    QGroupBox *progress_gb;
    QProgressBar *totalPb;
    QLabel *label;
    QLabel *cur_iter;
    QLabel *label_3;
    QLabel *total_iter;
    QProgressBar *currIterPb;
    QLabel *label_2;
    QLabel *label_4;
    QLineEdit *alpha;
    QLabel *label_6;
    QToolButton *runButton;
    QToolButton *stopButton;
    QToolButton *saveButton;
    QWidget *ReplicationsTab;
    QWidget *ODestimationTab;

    void setupUi(QDialog *Batchrun)
    {
        if (Batchrun->objectName().isEmpty())
            Batchrun->setObjectName(QString::fromUtf8("Batchrun"));
        Batchrun->resize(415, 514);
        const QIcon icon = QIcon(QString::fromUtf8(":/images/batchrun.gif"));
        Batchrun->setWindowIcon(icon);
        buttonBox = new QDialogButtonBox(Batchrun);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(40, 480, 341, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::NoButton|QDialogButtonBox::Ok);
        tabWidget = new QTabWidget(Batchrun);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setGeometry(QRect(16, 19, 381, 461));
        IterationsTab = new QWidget();
        IterationsTab->setObjectName(QString::fromUtf8("IterationsTab"));
        iterate_traveltimes_rb = new QRadioButton(IterationsTab);
        iterate_traveltimes_rb->setObjectName(QString::fromUtf8("iterate_traveltimes_rb"));
        iterate_traveltimes_rb->setGeometry(QRect(20, 10, 331, 18));
        iterate_traveltimes_rb->setChecked(true);
        convergence_gb = new QGroupBox(IterationsTab);
        convergence_gb->setObjectName(QString::fromUtf8("convergence_gb"));
        convergence_gb->setGeometry(QRect(10, 120, 351, 151));
        max_iterations_val = new QSpinBox(convergence_gb);
        max_iterations_val->setObjectName(QString::fromUtf8("max_iterations_val"));
        max_iterations_val->setGeometry(QRect(250, 20, 51, 22));
        max_iterations_val->setMinimum(2);
        max_iterations_val->setMaximum(999);
        max_iterations_val->setValue(10);
        max_iterations_cb = new QCheckBox(convergence_gb);
        max_iterations_cb->setObjectName(QString::fromUtf8("max_iterations_cb"));
        max_iterations_cb->setGeometry(QRect(20, 20, 221, 18));
        max_iterations_cb->setCheckable(true);
        max_iterations_cb->setChecked(true);
        max_rmsn_cb = new QCheckBox(convergence_gb);
        max_rmsn_cb->setObjectName(QString::fromUtf8("max_rmsn_cb"));
        max_rmsn_cb->setGeometry(QRect(20, 50, 221, 18));
        max_rmsn_cb->setChecked(true);
        max_rmsn_val = new QLineEdit(convergence_gb);
        max_rmsn_val->setObjectName(QString::fromUtf8("max_rmsn_val"));
        max_rmsn_val->setGeometry(QRect(250, 50, 48, 20));
        max_rmsn_val->setMaxLength(4);
        rmsn_link_tt = new QCheckBox(convergence_gb);
        rmsn_link_tt->setObjectName(QString::fromUtf8("rmsn_link_tt"));
        rmsn_link_tt->setGeometry(QRect(60, 100, 131, 18));
        rmsn_link_tt->setChecked(true);
        rmsn_od_tt = new QCheckBox(convergence_gb);
        rmsn_od_tt->setObjectName(QString::fromUtf8("rmsn_od_tt"));
        rmsn_od_tt->setGeometry(QRect(60, 120, 131, 18));
        rmsn_od_tt->setChecked(false);
        label_5 = new QLabel(convergence_gb);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(260, 80, 71, 20));
        rmsn_ltt = new QLabel(convergence_gb);
        rmsn_ltt->setObjectName(QString::fromUtf8("rmsn_ltt"));
        rmsn_ltt->setGeometry(QRect(280, 100, 41, 16));
        rmsn_ltt->setTextFormat(Qt::PlainText);
        rmsn_odtt = new QLabel(convergence_gb);
        rmsn_odtt->setObjectName(QString::fromUtf8("rmsn_odtt"));
        rmsn_odtt->setGeometry(QRect(280, 120, 41, 16));
        rmsn_odtt->setTextFormat(Qt::PlainText);
        rmsn_lf = new QLabel(convergence_gb);
        rmsn_lf->setObjectName(QString::fromUtf8("rmsn_lf"));
        rmsn_lf->setGeometry(QRect(280, 140, 41, 16));
        rmsn_lf->setTextFormat(Qt::PlainText);
        progress_gb = new QGroupBox(IterationsTab);
        progress_gb->setObjectName(QString::fromUtf8("progress_gb"));
        progress_gb->setGeometry(QRect(0, 320, 371, 111));
        totalPb = new QProgressBar(progress_gb);
        totalPb->setObjectName(QString::fromUtf8("totalPb"));
        totalPb->setEnabled(true);
        totalPb->setGeometry(QRect(110, 80, 241, 20));
        totalPb->setValue(0);
        label = new QLabel(progress_gb);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(120, 30, 51, 16));
        cur_iter = new QLabel(progress_gb);
        cur_iter->setObjectName(QString::fromUtf8("cur_iter"));
        cur_iter->setGeometry(QRect(170, 30, 21, 16));
        label_3 = new QLabel(progress_gb);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(190, 30, 16, 16));
        total_iter = new QLabel(progress_gb);
        total_iter->setObjectName(QString::fromUtf8("total_iter"));
        total_iter->setGeometry(QRect(210, 30, 21, 16));
        currIterPb = new QProgressBar(progress_gb);
        currIterPb->setObjectName(QString::fromUtf8("currIterPb"));
        currIterPb->setEnabled(true);
        currIterPb->setGeometry(QRect(110, 50, 241, 20));
        currIterPb->setValue(0);
        label_2 = new QLabel(progress_gb);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(20, 50, 82, 16));
        label_4 = new QLabel(progress_gb);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(20, 80, 82, 16));
        alpha = new QLineEdit(IterationsTab);
        alpha->setObjectName(QString::fromUtf8("alpha"));
        alpha->setGeometry(QRect(260, 40, 48, 20));
        alpha->setMaxLength(4);
        label_6 = new QLabel(IterationsTab);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setGeometry(QRect(81, 40, 159, 20));
        runButton = new QToolButton(IterationsTab);
        runButton->setObjectName(QString::fromUtf8("runButton"));
        runButton->setGeometry(QRect(50, 280, 31, 31));
        const QIcon icon1 = QIcon(QString::fromUtf8(":/images/bt_play.png"));
        runButton->setIcon(icon1);
        stopButton = new QToolButton(IterationsTab);
        stopButton->setObjectName(QString::fromUtf8("stopButton"));
        stopButton->setGeometry(QRect(80, 280, 31, 31));
        const QIcon icon2 = QIcon(QString::fromUtf8(":/images/bt_stop.png"));
        stopButton->setIcon(icon2);
        saveButton = new QToolButton(IterationsTab);
        saveButton->setObjectName(QString::fromUtf8("saveButton"));
        saveButton->setGeometry(QRect(110, 280, 31, 31));
        const QIcon icon3 = QIcon(QString::fromUtf8(":/images/filesave.png"));
        saveButton->setIcon(icon3);
        tabWidget->addTab(IterationsTab, QString());
        ReplicationsTab = new QWidget();
        ReplicationsTab->setObjectName(QString::fromUtf8("ReplicationsTab"));
        tabWidget->addTab(ReplicationsTab, QString());
        ODestimationTab = new QWidget();
        ODestimationTab->setObjectName(QString::fromUtf8("ODestimationTab"));
        tabWidget->addTab(ODestimationTab, QString());

        retranslateUi(Batchrun);
        QObject::connect(buttonBox, SIGNAL(accepted()), Batchrun, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), Batchrun, SLOT(reject()));

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(Batchrun);
    } // setupUi

    void retranslateUi(QDialog *Batchrun)
    {
        Batchrun->setWindowTitle(QApplication::translate("Batchrun", "Batch Run", 0, QApplication::UnicodeUTF8));
        iterate_traveltimes_rb->setText(QApplication::translate("Batchrun", "Iterate Travel Times - \"Dynamic Equilibrium\"", 0, QApplication::UnicodeUTF8));
        convergence_gb->setTitle(QApplication::translate("Batchrun", "Convergence", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        max_iterations_cb->setToolTip(QApplication::translate("Batchrun", "Max nr of iterations for the Batch run", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        max_iterations_cb->setText(QApplication::translate("Batchrun", "Max Nr Iterations", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        max_rmsn_cb->setToolTip(QApplication::translate("Batchrun", "The maximum Root Mean Square Normalized error between Input-Output traveltimes. between 0.00 and 1.00", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        max_rmsn_cb->setText(QApplication::translate("Batchrun", "Max RMSN difference", 0, QApplication::UnicodeUTF8));
        max_rmsn_val->setInputMask(QApplication::translate("Batchrun", "0.00; ", 0, QApplication::UnicodeUTF8));
        max_rmsn_val->setText(QApplication::translate("Batchrun", "0.02", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        rmsn_link_tt->setToolTip(QApplication::translate("Batchrun", "The maximum Root Mean Square Normalized error between Input-Output traveltimes. between 0.00 and 1.00", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        rmsn_link_tt->setText(QApplication::translate("Batchrun", "Link travel times", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        rmsn_od_tt->setToolTip(QApplication::translate("Batchrun", "The maximum Root Mean Square Normalized error between Input-Output traveltimes. between 0.00 and 1.00", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        rmsn_od_tt->setText(QApplication::translate("Batchrun", "OD travel times", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("Batchrun", "Current RMSN", 0, QApplication::UnicodeUTF8));
        rmsn_ltt->setText(QString());
        rmsn_odtt->setText(QString());
        rmsn_lf->setText(QString());
        progress_gb->setTitle(QApplication::translate("Batchrun", "Progress", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("Batchrun", "Iteration", 0, QApplication::UnicodeUTF8));
        cur_iter->setText(QString());
        label_3->setText(QApplication::translate("Batchrun", "of", 0, QApplication::UnicodeUTF8));
        total_iter->setText(QApplication::translate("Batchrun", "10", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("Batchrun", "Current Iteration", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("Batchrun", "Total", 0, QApplication::UnicodeUTF8));
        alpha->setInputMask(QApplication::translate("Batchrun", "0.00; ", 0, QApplication::UnicodeUTF8));
        alpha->setText(QApplication::translate("Batchrun", "0.60", 0, QApplication::UnicodeUTF8));
        label_6->setText(QApplication::translate("Batchrun", "Smoothing factor link travel times", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        runButton->setToolTip(QApplication::translate("Batchrun", "Run", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        runButton->setText(QApplication::translate("Batchrun", "...", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        stopButton->setToolTip(QApplication::translate("Batchrun", "Stop & reset", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        stopButton->setText(QApplication::translate("Batchrun", "...", 0, QApplication::UnicodeUTF8));
#ifndef QT_NO_TOOLTIP
        saveButton->setToolTip(QApplication::translate("Batchrun", "Stop & reset", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP
        saveButton->setText(QApplication::translate("Batchrun", "...", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(IterationsTab), QApplication::translate("Batchrun", "Iterations", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(ReplicationsTab), QApplication::translate("Batchrun", "Replications", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(ODestimationTab), QApplication::translate("Batchrun", "ODestimation", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class Batchrun: public Ui_Batchrun {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_BATCHRUNDLG_H
