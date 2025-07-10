/********************************************************************************
** Form generated from reading UI file 'keytest.ui'
**
** Created by: Qt User Interface Compiler version 5.12.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KEYTEST_H
#define UI_KEYTEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Keytest
{
public:
    QWidget *layoutWidget;
    QGridLayout *mainLayout;
    QPushButton *pbt_up;
    QPushButton *pbt_down;
    QPushButton *pbt_home;
    QPushButton *pbt_menu;
    QPushButton *pbt_enter;
    QPushButton *exitBtn;

    void setupUi(QWidget *Keytest)
    {
        if (Keytest->objectName().isEmpty())
            Keytest->setObjectName(QString::fromUtf8("Keytest"));
        Keytest->resize(407, 168);
        Keytest->setAutoFillBackground(false);
        layoutWidget = new QWidget(Keytest);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(10, 10, 391, 151));
        mainLayout = new QGridLayout(layoutWidget);
        mainLayout->setSpacing(6);
        mainLayout->setContentsMargins(11, 11, 11, 11);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(0, 0, 0, 0);
        pbt_up = new QPushButton(layoutWidget);
        pbt_up->setObjectName(QString::fromUtf8("pbt_up"));
        pbt_up->setEnabled(true);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pbt_up->sizePolicy().hasHeightForWidth());
        pbt_up->setSizePolicy(sizePolicy);

        mainLayout->addWidget(pbt_up, 0, 0, 1, 1);

        pbt_down = new QPushButton(layoutWidget);
        pbt_down->setObjectName(QString::fromUtf8("pbt_down"));
        pbt_down->setEnabled(true);
        sizePolicy.setHeightForWidth(pbt_down->sizePolicy().hasHeightForWidth());
        pbt_down->setSizePolicy(sizePolicy);

        mainLayout->addWidget(pbt_down, 0, 1, 1, 1);

        pbt_home = new QPushButton(layoutWidget);
        pbt_home->setObjectName(QString::fromUtf8("pbt_home"));
        pbt_home->setEnabled(true);
        sizePolicy.setHeightForWidth(pbt_home->sizePolicy().hasHeightForWidth());
        pbt_home->setSizePolicy(sizePolicy);

        mainLayout->addWidget(pbt_home, 1, 0, 1, 1);

        pbt_menu = new QPushButton(layoutWidget);
        pbt_menu->setObjectName(QString::fromUtf8("pbt_menu"));
        pbt_menu->setEnabled(true);
        sizePolicy.setHeightForWidth(pbt_menu->sizePolicy().hasHeightForWidth());
        pbt_menu->setSizePolicy(sizePolicy);

        mainLayout->addWidget(pbt_menu, 1, 1, 1, 1);

        pbt_enter = new QPushButton(layoutWidget);
        pbt_enter->setObjectName(QString::fromUtf8("pbt_enter"));
        pbt_enter->setEnabled(true);
        sizePolicy.setHeightForWidth(pbt_enter->sizePolicy().hasHeightForWidth());
        pbt_enter->setSizePolicy(sizePolicy);

        mainLayout->addWidget(pbt_enter, 2, 0, 1, 1);

        exitBtn = new QPushButton(layoutWidget);
        exitBtn->setObjectName(QString::fromUtf8("exitBtn"));

        mainLayout->addWidget(exitBtn, 2, 1, 1, 1);


        retranslateUi(Keytest);

        QMetaObject::connectSlotsByName(Keytest);
    } // setupUi

    void retranslateUi(QWidget *Keytest)
    {
        Keytest->setWindowTitle(QApplication::translate("Keytest", "Keytest", nullptr));
        pbt_up->setText(QApplication::translate("Keytest", "V+", nullptr));
        pbt_down->setText(QApplication::translate("Keytest", "V-", nullptr));
        pbt_home->setText(QApplication::translate("Keytest", "Home", nullptr));
        pbt_menu->setText(QApplication::translate("Keytest", "Menu", nullptr));
        pbt_enter->setText(QApplication::translate("Keytest", "Enter", nullptr));
        exitBtn->setText(QApplication::translate("Keytest", "Exit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Keytest: public Ui_Keytest {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KEYTEST_H
