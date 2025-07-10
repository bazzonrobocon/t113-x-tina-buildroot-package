/********************************************************************************
** Form generated from reading UI file 'MainWidget.ui'
**
** Created by: Qt User Interface Compiler version 5.12.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWIDGET_H
#define UI_MAINWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Widget
{
public:
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_2;
    QLabel *mainIcon;
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer;
    QLabel *mainLabel;
    QHBoxLayout *horizontalLayout;
    QToolButton *buttonShutdown;
    QSpacerItem *horizontalSpacer_2;
    QToolButton *buttonHelp;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *horizontalSpacer;
    QSplitter *splitter;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout_2;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QToolButton *buttonUpDown;
    QTextBrowser *logView;

    void setupUi(QWidget *Widget)
    {
        if (Widget->objectName().isEmpty())
            Widget->setObjectName(QString::fromUtf8("Widget"));
        Widget->resize(471, 521);
        verticalLayout_3 = new QVBoxLayout(Widget);
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setContentsMargins(11, 11, 11, 11);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        mainIcon = new QLabel(Widget);
        mainIcon->setObjectName(QString::fromUtf8("mainIcon"));
        QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(mainIcon->sizePolicy().hasHeightForWidth());
        mainIcon->setSizePolicy(sizePolicy);
        mainIcon->setMinimumSize(QSize(96, 96));
        mainIcon->setMaximumSize(QSize(96, 96));

        horizontalLayout_2->addWidget(mainIcon);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalSpacer = new QSpacerItem(20, 13, QSizePolicy::Minimum, QSizePolicy::Maximum);

        verticalLayout->addItem(verticalSpacer);

        mainLabel = new QLabel(Widget);
        mainLabel->setObjectName(QString::fromUtf8("mainLabel"));
        QPalette palette;
        QBrush brush(QColor(32, 74, 135, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        QBrush brush1(QColor(190, 190, 190, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush1);
        mainLabel->setPalette(palette);
        QFont font;
        font.setPointSize(16);
        font.setBold(true);
        font.setWeight(75);
        mainLabel->setFont(font);

        verticalLayout->addWidget(mainLabel);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        buttonShutdown = new QToolButton(Widget);
        buttonShutdown->setObjectName(QString::fromUtf8("buttonShutdown"));

        horizontalLayout->addWidget(buttonShutdown);

        horizontalSpacer_2 = new QSpacerItem(18, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        buttonHelp = new QToolButton(Widget);
        buttonHelp->setObjectName(QString::fromUtf8("buttonHelp"));

        horizontalLayout->addWidget(buttonHelp);


        verticalLayout->addLayout(horizontalLayout);

        verticalSpacer_2 = new QSpacerItem(17, 13, QSizePolicy::Minimum, QSizePolicy::Maximum);

        verticalLayout->addItem(verticalSpacer_2);


        horizontalLayout_2->addLayout(verticalLayout);

        horizontalSpacer = new QSpacerItem(108, 18, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout_3->addLayout(horizontalLayout_2);

        splitter = new QSplitter(Widget);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Vertical);
        layoutWidget = new QWidget(splitter);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        verticalLayout_2 = new QVBoxLayout(layoutWidget);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        scrollArea = new QScrollArea(layoutWidget);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 451, 104));
        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_2->addWidget(scrollArea);

        buttonUpDown = new QToolButton(layoutWidget);
        buttonUpDown->setObjectName(QString::fromUtf8("buttonUpDown"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(buttonUpDown->sizePolicy().hasHeightForWidth());
        buttonUpDown->setSizePolicy(sizePolicy1);
        buttonUpDown->setIconSize(QSize(12, 12));
        buttonUpDown->setAutoRaise(true);
        buttonUpDown->setArrowType(Qt::DownArrow);

        verticalLayout_2->addWidget(buttonUpDown);

        splitter->addWidget(layoutWidget);
        logView = new QTextBrowser(splitter);
        logView->setObjectName(QString::fromUtf8("logView"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Maximum);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(logView->sizePolicy().hasHeightForWidth());
        logView->setSizePolicy(sizePolicy2);
        splitter->addWidget(logView);

        verticalLayout_3->addWidget(splitter);


        retranslateUi(Widget);

        QMetaObject::connectSlotsByName(Widget);
    } // setupUi

    void retranslateUi(QWidget *Widget)
    {
        mainLabel->setText(QApplication::translate("Widget", "XXXXXXXXXXXXXX", nullptr));
        buttonShutdown->setText(QApplication::translate("Widget", "Terminate Launcher", nullptr));
        buttonHelp->setText(QApplication::translate("Widget", "?", nullptr));
        buttonUpDown->setText(QApplication::translate("Widget", "...", nullptr));
        Q_UNUSED(Widget);
    } // retranslateUi

};

namespace Ui {
    class Widget: public Ui_Widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWIDGET_H
