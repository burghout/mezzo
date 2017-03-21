/********************************************************************************
** Form generated from reading UI file 'positionbackground.ui'
**
** Created by: Qt User Interface Compiler version 4.8.6
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POSITIONBACKGROUND_H
#define UI_POSITIONBACKGROUND_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>

QT_BEGIN_NAMESPACE

class Ui_PositionBackground
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;
    QSpinBox *xpos;
    QSpinBox *ypos;
    QLabel *label_4;
    QSpinBox *scale;

    void setupUi(QDialog *PositionBackground)
    {
        if (PositionBackground->objectName().isEmpty())
            PositionBackground->setObjectName(QString::fromUtf8("PositionBackground"));
        PositionBackground->resize(259, 129);
        buttonBox = new QDialogButtonBox(PositionBackground);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(70, 90, 171, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        label = new QLabel(PositionBackground);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(20, 10, 41, 17));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        label->setFont(font);
        label_2 = new QLabel(PositionBackground);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(20, 30, 21, 17));
        label_3 = new QLabel(PositionBackground);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(20, 50, 16, 17));
        xpos = new QSpinBox(PositionBackground);
        xpos->setObjectName(QString::fromUtf8("xpos"));
        xpos->setGeometry(QRect(40, 30, 51, 23));
        xpos->setMinimum(-10000);
        xpos->setMaximum(10000);
        ypos = new QSpinBox(PositionBackground);
        ypos->setObjectName(QString::fromUtf8("ypos"));
        ypos->setGeometry(QRect(40, 50, 51, 23));
        ypos->setMinimum(-10000);
        ypos->setMaximum(10000);
        label_4 = new QLabel(PositionBackground);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(140, 10, 41, 17));
        label_4->setFont(font);
        scale = new QSpinBox(PositionBackground);
        scale->setObjectName(QString::fromUtf8("scale"));
        scale->setGeometry(QRect(130, 30, 81, 21));
        scale->setMinimum(1);
        scale->setMaximum(10000);
        scale->setSingleStep(1);
        scale->setValue(100);

        retranslateUi(PositionBackground);
        QObject::connect(buttonBox, SIGNAL(accepted()), PositionBackground, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), PositionBackground, SLOT(reject()));

        QMetaObject::connectSlotsByName(PositionBackground);
    } // setupUi

    void retranslateUi(QDialog *PositionBackground)
    {
        PositionBackground->setWindowTitle(QApplication::translate("PositionBackground", "Position Background", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("PositionBackground", "Origin", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("PositionBackground", "x", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("PositionBackground", "y", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("PositionBackground", "Scale", 0, QApplication::UnicodeUTF8));
        scale->setSuffix(QApplication::translate("PositionBackground", "%", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class PositionBackground: public Ui_PositionBackground {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POSITIONBACKGROUND_H
