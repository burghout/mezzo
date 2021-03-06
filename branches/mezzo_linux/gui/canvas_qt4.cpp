#include <qmessagebox.h>
#include "canvas_qt4.h"

MainForm::MainForm(QWidget *parent)
: Q3MainWindow(parent)
{
	setupUi(this);
	
	// INIT
	initialised=false;
	panelx=800;
	panely=600;
	scalefactor=1.50;
	panfactor=20;
	panpixels=20;
	breaknow=false;
	pm1.resize(panelx,panely);
	pm1.fill();
	pm2=pm1;
	//Canvas->resize(pm1.width(),pm1.height());
	timer = new QTimer( this );
	connect( timer, SIGNAL(timeout()), this, SLOT(loop()) );
	runtime=1.0; 
	currtime=0.0;
	scale=1.0;
	dx=0;
	dy=0;
	wm.scale(scale,scale); 
	statusbar = this->statusBar();
	statusbar->message("Initialised");
	exited = false;
	theParameters=theNetwork.get_parameters();
	simspeed->setValue(static_cast<int> (  theParameters->sim_speed_factor * 100 ));
	// Parameters dialog
	pmdlg = new ParametersDialog (this);
	
	// construction of the MezzoAnalyzer dialog 
	od_analyser_=new ODCheckerDlg(this);
	QObject::connect(inspectdialog, SIGNAL(triggered()), 
					 this, SLOT(on_inspectdialog_activated()));
}

// AUTOCONNECTED SLOTS
void MainForm::on_quit_activated()
{
	theNetwork.~Network(); 
	close();
}

void MainForm::on_openmasterfile_activated()
{
	QString fn = "";
	fn = (Q3FileDialog::getOpenFileName(QString::null,"mezzo and MiMe Files (*.mime *.mezzo)", this ) );
	// Open master file
	if ( !fn.isEmpty() ) 
	{
		string name=fn.latin1();
		if (theNetwork.readmaster(name))
			runtime=theNetwork.executemaster(&pm2,&wm);
		else
		{
			cout << "ERROR READING THE MASTER FILE: " << name.c_str() << " Exiting" << endl;
			close();
		}
		initialised=true;
		theNetwork.redraw();
		copyPixmap();
	}	
}

void MainForm::on_zoomin_activated()
{
	double w_x= theNetwork.get_width_x();
	double h_y=theNetwork.get_height_y();
	dx=static_cast<int>(0.5*(scalefactor-1)*w_x);
	dy=static_cast<int>(0.5*(scalefactor-1)*h_y);
	wm.translate(-dx,-dy);
	scale=scalefactor;
	wm.scale(scale,scale);
	panfactor=static_cast<int>(0.5+(double)panpixels / wm.m11());
	QString mesg=QString("Scale: %1 Panfactor: %2 DX: %3 DY: %4").arg(wm.m11()).arg(panfactor).arg(wm.dx()).arg(wm.dy());
	statusbar->message(mesg );
}

void MainForm::on_zoomout_activated()
{
	scale=1/scalefactor;
	double w_x= theNetwork.get_width_x();
	double h_y=theNetwork.get_height_y();
	dx=static_cast<int>(0.5*(scale-1)*w_x);
	dy=static_cast<int>(0.5*(scale-1)*h_y);
	wm.translate(-dx,-dy);  
	wm.scale(scale,scale);
	panfactor=static_cast<int>(0.5+(double)panpixels / wm.m11());
	QString mesg=QString("Scale: %1 Panfactor: %2 DX: %3 DY: %4").arg(wm.m11()).arg(panfactor).arg(wm.dx()).arg(wm.dy());
	statusbar->message(mesg );
}

void MainForm::on_savescreenshot_activated()
{
    QString fn = QFileDialog::getSaveFileName("", "PNG Files (*.png)", this );
    if ( !fn.isEmpty() )
    {
            pm1.save(fn,"PNG");
    }
}

void MainForm::on_loadbackground_activated()
{
	QString fn( QFileDialog::getOpenFileName(QString::null,"PNG Files (*.png)", this ) );
    if (!fn.isEmpty())
    {
	string haha=fn.latin1();
	theNetwork.set_background (haha);
    }
}

void MainForm::on_breakoff_activated()
{
	breaknow=true;
}

void MainForm::on_run_activated()
{
	breaknow=false;
	loop();
}

void MainForm::on_zoomfactor_valueChanged( int value)
{
    scalefactor = value / 100.0;
}


void MainForm::on_panfactor_valueChanged( int value )
{
   panpixels = value;
}

void MainForm::on_parametersdialog_activated()
{
	pmdlg->set_parameters(theNetwork.get_parameters());
 //   pmdlg->wm = &wm;
    pmdlg->show();
	pmdlg->raise();
	pmdlg->activateWindow();
}

// if the mezzoAnalyzer is triggered
void MainForm::on_inspectdialog_activated()
{
    // if initialized (network is created)
	if (this->initialised){

		// set the network if necessary
		if(!od_analyser_->getNetworkState()){
			od_analyser_->setNetwork(&theNetwork);
			od_analyser_->setNetworkState(true); 
		}
		
		// start mezzo analyzer dialog
		od_analyser_->show();
	
	}else{ 
		// warn to load the network 
		QMessageBox::warning(this, "Network not loaded", "Please load a network first!", 
			QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
	}

}


void MainForm::on_saveresults_activated()
{
	 theNetwork.writeall();
}
// NORMAL PRIVATE METHODS

void MainForm::loop()
{
	copyPixmap();
	int msecs (refreshrate->value());
	int updatefac (updatefactor->value());
	if (!breaknow)
	timer->start( msecs, TRUE ); // ... mseconds single-shot timer 
	currtime=theNetwork.step(((updatefac/100)*msecs/1000.0));
	progressbar->setProgress(static_cast<int>(100.0*currtime/runtime));
	//LCDNumber1->display(static_cast<int>(currtime));
	displaytime(currtime);
	if (currtime>=runtime)
	{
		//on_quit_activated();  // MAYBE TAKE THIS OUT, i don't want it to exit automatically
		breaknow=false;
	}
}

void MainForm::displaytime(double time)
{
	QString s_secs, s_mins, s_hrs;
    int secs = static_cast<int> (time + theParameters->zerotime);
    int hours = static_cast<int> (secs/3600);
    int mins = static_cast<int> ((secs-(hours*3600))/60);
    secs = static_cast<int> (secs - (hours*3600) - (mins*60));
    if (secs < 10)
	s_secs = QString("0%1").arg(secs);
    else 
	s_secs= QString("%1").arg(secs);
    if (mins < 10)
	s_mins = QString("0%1").arg(mins);
    else 
	s_mins= QString("%1").arg(mins);
    if (hours < 10)
	s_hrs = QString("0%1").arg(hours);
    else 
	s_hrs= QString("%1").arg(hours);
    QString string = s_hrs + ":" + s_mins + ":" + s_secs ;
    LCDNumber->display(string);
}

void MainForm::copyPixmap()
{	
   pm1=pm2;
   //bitBlt(Canvas,0,0,&pm1);
	
	//paintEvent (NULL);
	Canvas->setPixmap(pm1);
	Canvas->repaint();
   
}

void MainForm::paintEvent(QPaintEvent *  event )
{
	/*
	QPainter painter(this);
	painter.drawPixmap(130,10, pm2);
	painter.end();
	*/
}

void MainForm::seed(int sd )
{
   theNetwork.seed(sd);
}

void MainForm::on_simspeed_valueChanged( int  value)
{
    theParameters->sim_speed_factor = (value / 100);
}

void MainForm::keyPressEvent( QKeyEvent *e )
{
	if (initialised)
	{
		 panfactor=static_cast<int>(0.5+(double)panpixels / wm.m11());
		 switch (e->key() ) 
		 {
			case (Qt::Key_Plus):                               // zoom in
			  on_zoomin_activated();
			 break;
			case (Qt::Key_Minus):                               // zoom out
			  on_zoomout_activated();	 
			  break;
			case (Qt::Key_Up):                               // pan up
			  dy=-panfactor;
			  wm.translate(0,dy);	
			  break;   
			case (Qt::Key_Down):                               // pan up
			  dy=panfactor;
			  wm.translate(0,dy);	
			  break;       
			case (Qt::Key_Left):                               // pan left
			  dx=-panfactor;
			  wm.translate(dx,0);	  
			  break; 
			case (Qt::Key_Right):                               // pan right
			  dx=panfactor;
			  wm.translate(dx,0);	    
			  break;
			case (Qt::Key_C):                               // center image
			  theNetwork.recenter_image();	
			  break;
			case(Qt::Key_D):
			  panfactor=static_cast<int>(0.5+(double)panpixels / wm.m11());
			   
	}
	  
    QString mesg=QString("Scale: %1 Panfactor: %2 DX: %3 DY: %4").arg(wm.m11()).arg(panfactor).arg(wm.dx()).arg(wm.dy());
	statusbar->message(mesg );
    theNetwork.redraw();
    copyPixmap();
	}
}


