/********************************************************************************
** Form generated from reading UI file 'launcheritem.ui'
**
** Created by: Qt User Interface Compiler version 5.12.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LAUNCHERITEM_H
#define UI_LAUNCHERITEM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LauncherItem
{
public:
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QToolButton *iconButton;
    QSpacerItem *horizontalSpacer_2;
    QLabel *textLabel;

    void setupUi(QWidget *LauncherItem)
    {
        if (LauncherItem->objectName().isEmpty())
            LauncherItem->setObjectName(QString::fromUtf8("LauncherItem"));
        LauncherItem->resize(108, 112);
        LauncherItem->setStyleSheet(QString::fromUtf8("QToolButton:checked {\n"
"	background-color: rgb(252, 175, 62);\n"
"}"));
        gridLayout = new QGridLayout(LauncherItem);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        horizontalSpacer = new QSpacerItem(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 0, 1, 1);

        iconButton = new QToolButton(LauncherItem);
        iconButton->setObjectName(QString::fromUtf8("iconButton"));
        iconButton->setIconSize(QSize(64, 64));
        iconButton->setCheckable(true);
        iconButton->setAutoRaise(true);

        gridLayout->addWidget(iconButton, 0, 1, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 0, 2, 1, 1);

        textLabel = new QLabel(LauncherItem);
        textLabel->setObjectName(QString::fromUtf8("textLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(textLabel->sizePolicy().hasHeightForWidth());
        textLabel->setSizePolicy(sizePolicy);
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setWordWrap(true);

        gridLayout->addWidget(textLabel, 1, 0, 1, 3);


        retranslateUi(LauncherItem);

        QMetaObject::connectSlotsByName(LauncherItem);
    } // setupUi

    void retranslateUi(QWidget *LauncherItem)
    {
        Q_UNUSED(LauncherItem);
    } // retranslateUi

};

namespace Ui {
    class LauncherItem: public Ui_LauncherItem {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LAUNCHERITEM_H
