#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <disp2/sunxi_display2.h>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	connect(ui->slider, SIGNAL(valueChanged(int)), this, SLOT(sliderchanged(int)));
	connect(ui->exitBtn, &QPushButton::clicked, this, [=](){
		close();		
	});

	fd = ::open("/dev/disp", O_RDWR|O_NONBLOCK);
	ui->slider->setRange(0, 255);
	ui->slider->setValue(200);
}

MainWindow::~MainWindow()
{
	::close(fd);
}

void MainWindow::sliderchanged(int v)
{
	set_brightness(v);
}

int MainWindow::get_brightness() {

	unsigned long args[3];
	int err, i;
	int value = 0;
	for(i = 0; i < 2; i++)
	{
		args[0] = i;
		if(ioctl(fd, DISP_GET_OUTPUT_TYPE,args) == DISP_OUTPUT_TYPE_LCD)
		{
			args[1]  = (unsigned long)&value;
			args[2]  = 0;
			err = ioctl(fd, DISP_LCD_GET_BRIGHTNESS, args);
			return value;	
		}
	}
	return -1;
}

int MainWindow::set_brightness(int value) {
	unsigned long args[3];
	int err, i;
	for(i = 0; i < 2; i++)
	{
		args[0] = i;
		if(ioctl(fd, DISP_GET_OUTPUT_TYPE,args) == DISP_OUTPUT_TYPE_LCD)
		{
			args[1]  = value;
			args[2]  = 0;
			err = ioctl(fd, DISP_LCD_SET_BRIGHTNESS, args);
			return err;	
		}
	}
	return -1;
}
