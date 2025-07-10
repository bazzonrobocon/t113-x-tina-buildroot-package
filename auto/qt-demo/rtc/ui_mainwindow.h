/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QHBoxLayout *horizontalLayout;
    QGridLayout *gridLayout;
    QSpinBox *day;
    QSpinBox *hour;
    QSpinBox *minute;
    QSpinBox *second;
    QPushButton *set;
    QSpinBox *year;
    QSpacerItem *verticalSpacer;
    QSpinBox *month;
    QPushButton *exitBtn;
    QMenuBar *menuBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(440, 295);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        horizontalLayout = new QHBoxLayout(centralWidget);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        day = new QSpinBox(centralWidget);
        day->setObjectName(QString::fromUtf8("day"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(day->sizePolicy().hasHeightForWidth());
        day->setSizePolicy(sizePolicy);
        day->setMaximum(3000);

        gridLayout->addWidget(day, 0, 2, 1, 1);

        hour = new QSpinBox(centralWidget);
        hour->setObjectName(QString::fromUtf8("hour"));
        sizePolicy.setHeightForWidth(hour->sizePolicy().hasHeightForWidth());
        hour->setSizePolicy(sizePolicy);
        hour->setMaximum(3000);

        gridLayout->addWidget(hour, 0, 3, 1, 1);

        minute = new QSpinBox(centralWidget);
        minute->setObjectName(QString::fromUtf8("minute"));
        sizePolicy.setHeightForWidth(minute->sizePolicy().hasHeightForWidth());
        minute->setSizePolicy(sizePolicy);
        minute->setMaximum(3000);

        gridLayout->addWidget(minute, 0, 4, 1, 1);

        second = new QSpinBox(centralWidget);
        second->setObjectName(QString::fromUtf8("second"));
        sizePolicy.setHeightForWidth(second->sizePolicy().hasHeightForWidth());
        second->setSizePolicy(sizePolicy);
        second->setMaximum(3000);

        gridLayout->addWidget(second, 0, 5, 1, 1);

        set = new QPushButton(centralWidget);
        set->setObjectName(QString::fromUtf8("set"));

        gridLayout->addWidget(set, 1, 5, 1, 1);

        year = new QSpinBox(centralWidget);
        year->setObjectName(QString::fromUtf8("year"));
        sizePolicy.setHeightForWidth(year->sizePolicy().hasHeightForWidth());
        year->setSizePolicy(sizePolicy);
        year->setMouseTracking(true);
        year->setMaximum(3000);

        gridLayout->addWidget(year, 0, 0, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer, 3, 3, 1, 1);

        month = new QSpinBox(centralWidget);
        month->setObjectName(QString::fromUtf8("month"));
        sizePolicy.setHeightForWidth(month->sizePolicy().hasHeightForWidth());
        month->setSizePolicy(sizePolicy);
        month->setMaximum(3000);

        gridLayout->addWidget(month, 0, 1, 1, 1);

        exitBtn = new QPushButton(centralWidget);
        exitBtn->setObjectName(QString::fromUtf8("exitBtn"));

        gridLayout->addWidget(exitBtn, 2, 5, 1, 1);


        horizontalLayout->addLayout(gridLayout);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 440, 23));
        MainWindow->setMenuBar(menuBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWindow->setStatusBar(statusBar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "RTC", nullptr));
        set->setText(QApplication::translate("MainWindow", "set", nullptr));
        exitBtn->setText(QApplication::translate("MainWindow", "exit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
