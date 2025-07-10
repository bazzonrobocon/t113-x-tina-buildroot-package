#include <unistd.h>
#include <fcntl.h>
#include <qmessagebox.h>
#include <qsocketnotifier.h>
#include <QVBoxLayout>
#include "keytest.h"
#include "ui_keytest.h"
#include <linux/input.h>

Keytest::Keytest(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::Keytest)

{
	ui->setupUi(this);
	ui->mainLayout;
	setLayout(ui->mainLayout);

	m_fd = open("/dev/input/event2", O_RDONLY | O_NONBLOCK);

	if (m_fd < 0)
	{
		QMessageBox::information(this,"Error", "Fail to open ,Please check your key  device");
		return;
	}

	memset(m_oldButtonsState, 0, sizeof(m_oldButtonsState));
	m_notifyObject = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
	connect (m_notifyObject, SIGNAL(activated(int)), this, SLOT(keyEvent()));

    	connect(ui->exitBtn, &QPushButton::clicked, this, [=](){
	        close();		    
        });
}

Keytest::~Keytest()
{
	delete m_notifyObject;
	::close(m_fd);
	delete ui;
}

void Keytest::keyEvent()
{
	char buffer[KEYMAXNUM];
	memset(buffer, 0, sizeof(buffer));
	struct input_event event;
	int nResult=::read(m_fd, &event, sizeof(event));
	if (nResult !=  sizeof(event))
	{
		QMessageBox::information(this,"Debug","read error");
		return;
	}
	switch(event.code)
	{
		case KEY_V_UP:
			buffer[0]=event.value;
			break;
		case KEY_V_DOWN:
			buffer[1]=event.value;
			break;
		case KEY_V_HOME:
			buffer[2]=event.value;
			break;
		case KEY_MENU:
			buffer[3]=event.value;
			break;
		case KEY_ENTER:
			buffer[4]=event.value;
			break;
		default:
			return;
	}

	for (unsigned i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++)
	{
		bool oldState = m_oldButtonsState[i];
		bool isOn = ((buffer[i] & 0x01) | (buffer[i] & 0x02));
		if (oldState != isOn)
		{
			m_oldButtonsState[i] = isOn;
			update();  //this function will call paintEvent
		}
	}
}


void Keytest::paintEvent(QPaintEvent*)
{
	for(unsigned i = 0; i < KEYMAXNUM; i++)
	{
		if(m_oldButtonsState[i])
		{
			switch(i)
			{
				case 0://up
					ui->pbt_up ->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 255);"));
					break;

				case 1://down
					ui->pbt_down->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 255);"));
					break;

				case 2://home
					ui->pbt_home->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 255);"));
					break;

				case 3://menu
					ui->pbt_menu->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 255);"));
					break;

				case 4://enter
					ui->pbt_enter->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 255);"));
					break;
				default:
					break;
			}
		}
		else
		{
			switch(i)
			{
				case 0://up
					ui->pbt_up ->setStyleSheet(QString::fromUtf8("background-color: rgb(206, 206,206);"));
					break;

				case 1://down
					ui->pbt_down->setStyleSheet(QString::fromUtf8("background-color: rgb(206, 206,206);"));
					break;

				case 2://home
					ui->pbt_home->setStyleSheet(QString::fromUtf8("background-color: rgb(206, 206,206);"));
					break;

				case 3://menu
					ui->pbt_menu->setStyleSheet(QString::fromUtf8("background-color: rgb(206, 206,206);"));
					break;

				case 4://enter
					ui->pbt_enter->setStyleSheet(QString::fromUtf8("background-color: rgb(206, 206,206);"));
					break;

				default:
					break;
			}
		}
	}
}











