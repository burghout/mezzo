/*
* implementation of the graphic interface
*
* last modification: Xiaoliang Ma 2007-10-31
*	
* solve the zooming problem by reimplementation through matrix operations
*	(It is necessary to create a document to explain the principle later since
*    the implementation is quite tricky and the code needs to be further clean 
*    up as while!)
*
* change the GUI: move the paramter setting to the parameter dialog
* coding the object selection functions
*
*/

#include <qmessagebox.h>
 #include <QKeyEvent>
#include "canvas_qt4.h"

MainForm::MainForm(QWidget *parent): QMainWindow(parent)
{
	setupUi(this);
	
	// INITIALIZATION

	// first create a Network object
	theNetwork = new Network();
	setWindowTitle(QString::fromStdString(version));
	fn = "";
	started_from_commandline=false;
	panelx=Canvas->width();
	panely=Canvas->height();
	start_x=Canvas->x();
	start_y=Canvas->y();
	yadjust_=54;
	canvasOffset=QSize((width()-panelx),(height()-panely));
	scalefactor=1.50;
	panfactor=20;
	panpixels=20;
	scale=1.0;
	dx=0;
	dy=0;

	// states
	initialised=false;
	lmouse_pressed_=false;
	inselection_=false;
	keyN_pressed_=false;
	keyL_pressed_=false;
	nodes_sel_=vector<Node*>();
	links_sel_=vector<Link*>();

	// implementation of view zooming and panning
	//xiaoliang
	mod2stdViewMat_=QMatrix(1,0,0,1,0,0);
	viewMat_=QMatrix(1,0,0,1,0,0);
	viewSize_=QSize(panelx,panely);
	pm1=QPixmap(viewSize_);
	pm1.fill();
	pm2=pm1;
	//xiaoliang
	
	timer = new QTimer( this );
	breaknow=false;
	connect( timer, SIGNAL(timeout()), this, SLOT(loop()) );
	runtime=1.0; 
	currtime=0.0;
	exited = false;
	theParameters=theNetwork->get_parameters();
	
	// need to figure out if this affect the simulation
	// COMMENT WILCO: this is to slow down the simulation
//	simspeed->setValue(static_cast<int> (  theParameters->sim_speed_factor * 100 ));
	
	// dialogs
	pmdlg = new ParametersDialog (this);
	QObject::connect(pmdlg, SIGNAL(paintRequest()), 
					 this, SLOT(updateCanvas()));
	brdlg = new BatchrunDlg(this);
	QObject::connect(brdlg, SIGNAL(paintRequest()), 
					 this, SLOT(updateCanvas()));
	QObject::connect(brdlg, SIGNAL(center_image()), 
					 this, SLOT(center_image()));
	QObject::connect(brdlg, SIGNAL(activateAnalyzeOutput()), 
					 this, SLOT( activate_AnalyzeOutput()   ));

	outputview = new OutputView(this);
	QObject::connect(outputview, SIGNAL(paintRequest()), 
					 this, SLOT(updateCanvas()));

	posbackground = new PositionBackground(this);
	finddialog = new FindDialog(this);
	QObject::connect(finddialog, SIGNAL(paintRequest()), 
					 this, SLOT(updateCanvas()));
	// construction of the MezzoAnalyzer dialog 
	od_analyser_=new ODCheckerDlg();
	QObject::connect(od_analyser_, SIGNAL(paintRequest()), 
					 this, SLOT(copyPixmap()));


	// zoom by window
	zoomrect_=0;
	zoombywin_triggered_=false;
	QObject::connect(zoombywin, SIGNAL(toggled(bool)), this,
					 SLOT(on_zoombywin_triggered(bool)));
	QObject::connect(inselectmode, SIGNAL(toggled(bool)), this,
					 SLOT(on_inselectmode_triggered(bool)));
	// show link handles
	QObject::connect(linkhandlemark, SIGNAL(toggled(bool)), this,
					 SLOT(showhandle_triggered(bool)));

	// connect the signals from parameter dialog
	QObject::connect(pmdlg, SIGNAL(activateZoomFactor(int)), this,
					 SLOT(zoomfactor_valueChanged(int)));
	QObject::connect(pmdlg, SIGNAL(activatePanFactor(int)), this,
					 SLOT(panfactor_valueChanged(int)));
	QObject::connect(pmdlg, SIGNAL(activateSimSpeed(int)), this,
					 SLOT(simspeed_valueChanged(int)));

	// deactive the actions except the open masterfile
	activateToolbars(false);
    // hide the slider for output analysis
	horizontalSlider->hide();
	actionAnalyzeOutput->setEnabled(false);

	// WILCO: move all the status widgets to the Statusbar

	statusBar()->addWidget (status_label);
	statusBar()->addWidget(simprogress_widget);
	statusBar()->addWidget(this->TextLabel12,10);
	statusBar()->addWidget(this->LCDNumber),10;
	statusBar()->addWidget(this->progressbar),10;
	statusBar()->addWidget(mouse_label),10;
	
	simprogress_widget->setVisible(false);
}

MainForm::~MainForm()

{
	if (NULL!=theNetwork)
		delete theNetwork;
	if (NULL!=od_analyser_)
		delete od_analyser_;
	if (NULL!=outputview)
		delete outputview;
	if (NULL!=brdlg)
		delete brdlg;
	if (NULL!=pmdlg)
		delete pmdlg;
	if (NULL!=finddialog)
		delete finddialog;
	if (NULL!=posbackground)
		delete posbackground;
}

void MainForm::activateToolbars(bool activated)
{
	
	savescreenshot->setEnabled(activated);
    run->setEnabled(activated);
    breakoff->setEnabled(activated);
	stop->setEnabled(activated);
	closenetwork->setEnabled(activated);
    zoomin->setEnabled(activated);
    zoomout->setEnabled(activated);
	zoombywin->setEnabled(activated);
	linkhandlemark->setEnabled(activated);
    viewSet_ParametersAction->setEnabled(activated);
    parametersdialog->setEnabled(activated);
    loadbackground->setEnabled(activated);
    saveresults->setEnabled(activated);
    inspectdialog->setEnabled(activated);
	batch_run->setEnabled(activated);
	openmasterfile->setEnabled(!activated); // should be only be enabled when everything else is not
	if(activated)
		status_label->setText("Network initialized");
	else
		status_label->setText("No network loaded");

}

void MainForm::showCanvasinfo()
{
	panelx=Canvas->width();
	panely=Canvas->height();
	start_x=Canvas->x();
	start_y=Canvas->y();
	QString mesg=QString("Canvas X:%1, Y:%2, W:%3, H:%4"
			                 ).arg(start_x).arg(start_y).arg(panelx).arg(panely);
	mouse_label->setText(mesg);
}

void MainForm::updateCanvas()
{
	theNetwork->redraw();
	copyPixmap();
}

// AUTOCONNECTED SLOTS
void MainForm::on_closenetwork_triggered()
{
	//break current simulation
	breaknow = true;
	// Reset all the internal variables
	runtime=1.0; 
	currtime =0.0;
	initialised=false;
	lmouse_pressed_=false;
	inselection_=false;
	keyN_pressed_=false;
	keyL_pressed_=false;
	nodes_sel_.clear();
	links_sel_.clear();

	mod2stdViewMat_=QMatrix(1,0,0,1,0,0);
	viewMat_=QMatrix(1,0,0,1,0,0);
	viewSize_=QSize(panelx,panely);
	pm1=QPixmap(viewSize_);
	pm1.fill();
	pm2=pm1;

	exited = false;
	theParameters=theNetwork->get_parameters();
	zoomrect_=0;
	zoombywin_triggered_=false;

	activateToolbars(false);
	simprogress_widget->setVisible(false);
	actionAnalyzeOutput->setChecked(false);
	actionAnalyzeOutput->setEnabled(false);

	// Delete the current network 
	delete theNetwork;

	// Create a new Network
	theNetwork = new Network();
	fn ="";
	setWindowTitle(QString::fromStdString(version));

	//display empty pixmap
	copyPixmap();
}


void MainForm::on_stop_triggered()
{
	//break current simulation
	breaknow = true;

	// stop and init the network
	runtime = theNetwork->reset();

	// Reset all the internal variables
	
	currtime =0.0;
	lmouse_pressed_=false;
	inselection_=false;
	keyN_pressed_=false;
	keyL_pressed_=false;
	nodes_sel_.clear();
	links_sel_.clear();
	zoomrect_=0;
	zoombywin_triggered_=false;
	// !!! I AM SURE THIS IS INCOMPLETE !!!

	run->setEnabled(true);
	batch_run->setEnabled(true);
	actionAnalyzeOutput->setChecked(false);
	actionAnalyzeOutput->setEnabled(false);

	//update the canvas
	center_image();
	updateCanvas();
}



void MainForm::on_quit_triggered()
{ 
	
	closing();
}


void MainForm::on_openmasterfile_triggered()
{
	fn = "";
	fn = (QFileDialog::getOpenFileName(this, "Select a MEZZO master file", QString::null,"Mezzo Files (*.mime *.mezzo)") );
	// Open master file
	process_masterfile();	
}

void MainForm::process_masterfile()
{
	if ( !fn.isEmpty() ) 
	{
		// strip the dir from the filename and give to the Network
		int pos = fn.lastIndexOf ('/');
		QString workingdir = fn.left(pos+1);
		theNetwork->set_workingdir (string(workingdir.toLatin1()));
		// make a STL compatible string
		string name=string(fn.toLatin1().data());
		if (theNetwork->readmaster(name))
			runtime=theNetwork->executemaster(&pm2,&wm);
		else
		{
			eout << "ERROR READING THE MASTER FILE: " << name.c_str() << " Exiting" << endl;
			close();
		}
		string title = version + " - " + name;
		setWindowTitle(title.data());
		// initialize the buttons
		initialised=true;
		activateToolbars(initialised);

		// initialize the network graphic
		mod2stdViewMat_=theNetwork->netgraphview_init();
		wm=mod2stdViewMat_;
		updateCanvas();
		//statusbar->message("Initialised");
	}
}

void MainForm::on_zoomin_triggered()
{
	// the view center
	int xviewcenter=viewSize_.width()/2;
	int yviewcenter=viewSize_.height()/2;
	QMatrix tempMat;
	scale*=scalefactor;

	// transfer matrix
	tempMat.reset();
	tempMat.translate(xviewcenter, yviewcenter);
	tempMat.scale(scalefactor, scalefactor);
	tempMat.translate(-xviewcenter,-yviewcenter);

	// new transfer matrix from standard view to current view 
	viewMat_=viewMat_*tempMat;
	// new general tranfer matrix
	wm=mod2stdViewMat_*viewMat_;
	panfactor=static_cast<int>(0.5+(double)panpixels/scale);
	//update the canvas
	updateCanvas();
}

void MainForm::on_zoomout_triggered()
{	
	// the view center
	int xviewcenter=viewSize_.width()/2;
	int yviewcenter=viewSize_.height()/2;
	QMatrix tempMat;

	scale/=scalefactor;
	tempMat.reset();
	tempMat.translate(xviewcenter, yviewcenter);
	tempMat.scale(1/scalefactor, 1/scalefactor);
	tempMat.translate(-xviewcenter,-yviewcenter);
	viewMat_=viewMat_*tempMat;
	wm=mod2stdViewMat_*viewMat_;
	panfactor=static_cast<int>(0.5+(double)panpixels/scale);

	updateCanvas();
}

void MainForm::on_zoombywin_triggered(bool triggered)
{
	zoombywin_triggered_=triggered;
}

void MainForm::showhandle_triggered(bool triggered)
{
	//update all linkicons property on handle 
	if(this->initialised){
		map <int, Link*> alllinks=theNetwork->get_links();
		//for( unsigned i=0; i<alllinks.size(); i++)
		//	alllinks[i]->get_icon()->sethandle(triggered);
		map <int,Link*>::iterator l_iter=alllinks.begin();
		for(l_iter;l_iter!=alllinks.end();l_iter++)
		{
			(*l_iter).second->get_icon()->sethandle(triggered);
		}
		updateCanvas();
	}
}

void MainForm::on_inselectmode_triggered(bool triggered)
{
	inselection_=triggered;
}

void MainForm::on_savescreenshot_triggered()
{
	QString fn = QFileDialog::getSaveFileName(this, "Save Image", QString::null, "PNG Files (*.png)" );
    if (!fn.isEmpty())
       pm1.save(fn,"PNG");
}

void MainForm::on_loadbackground_triggered()
{
	QString fn( QFileDialog::getOpenFileName(this, "Open background image",QString::null,"PNG Files (*.png)" ) );
    if (!fn.isEmpty())
    {
		string tempname=string(fn.toLatin1().data());
		theNetwork->set_background (tempname);
    }
}

void MainForm::on_breakoff_triggered()
{
	breaknow=true;
	run->setEnabled(true);
}

void MainForm::on_run_triggered()
{
	simprogress_widget->setVisible(true);
	run->setEnabled(false);
	batch_run->setEnabled(false);
	breaknow=false;
	theNetwork->reset_link_icons();
	loop();
}

void MainForm::zoomfactor_valueChanged( int value)
{
	scalefactor = value / 100.0;
}


void MainForm::panfactor_valueChanged( int value )
{
   panpixels = value;
}

void MainForm::on_parametersdialog_triggered()
{
	if (initialised)
	{
		pmdlg->set_parameters(theNetwork->get_parameters());
		pmdlg->show();
		pmdlg->raise();
		pmdlg->activateWindow();
	}
} 

void MainForm::on_batch_run_triggered()
{
	if (initialised)
	{	
		brdlg->setNetwork(theNetwork);
		brdlg->show();
		brdlg->raise();
		brdlg->activateWindow();

	}
}


/**
* if the mezzoAnalyzer is triggered
*/
void MainForm::on_inspectdialog_triggered()
{
    // if initialized (network is built)
	if (this->initialised){

		// set the network if necessary
		if(!od_analyser_->getNetworkState()){
			od_analyser_->setNetwork(theNetwork);
			od_analyser_->setNetworkState(true); 
		}
		
		// start mezzo analyzer dialog
		od_analyser_->loadSelectOD(nodes_sel_);
		od_analyser_->show();
		od_analyser_->activateWindow();
	
	}else{ 
		// warn to load the network 
		QMessageBox::warning(this, "Network not loaded", "Please load a network first!", 
			QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
	}

}

void MainForm::on_finddialog_triggered()
{
	if (this->initialised){
		finddialog->set_network(theNetwork);
		finddialog->show();
	}

}


void MainForm::on_saveresults_triggered()
{
	 theNetwork->writeall();
}

void MainForm::on_actionAnalyzeOutput_toggled()
{
	if (actionAnalyzeOutput->isChecked())
	{
		int nr_periods;
		horizontalSlider->show();
		displaytime(0.0);
		// default: select flow MOE as main variable, later make a dialogue for selecting MOE
		theNetwork->set_output_moe_thickness(1); // set flows as thickness
		theNetwork->set_output_moe_colour (3); // set speeds as colour
		theParameters->inverse_colour_scale = true; // so high speeds are green and low speeds red
		nr_periods=static_cast<int>((theParameters->running_time/theParameters->moe_outflow_update)+0.5);
		horizontalSlider->setRange(0,nr_periods);
		theParameters->viewmode = 1; // set view mode to output
		theParameters->show_period = 0; // show the first period
		outputview->setNetwork(theNetwork);
		outputview->show();
	}
	else
	{
		theParameters->viewmode = 0; // set view mode to output
		horizontalSlider->hide();
		outputview->hide();
	}
	updateCanvas();

}



void MainForm::activate_AnalyzeOutput() 
{	
	actionAnalyzeOutput->setEnabled(true);
	actionAnalyzeOutput->setChecked(true);
	on_actionAnalyzeOutput_toggled();
}


void MainForm::on_actionSlow_toggled(bool value)
{
	if (value)
	{
		theParameters->gui_update_step=1;
		pmdlg->updatefactor->setValue(1);
	}
	else
	{
		theParameters->gui_update_step=1000;
		pmdlg->updatefactor->setValue(100);
	}

}



void MainForm::on_horizontalSlider_valueChanged()
{
	theParameters->show_period = horizontalSlider->value();
	double show_time = theParameters->moe_outflow_update * theParameters->show_period;
	displaytime(show_time);
		updateCanvas();
}

void MainForm::simspeed_valueChanged( int  value)
{
    theParameters->sim_speed_factor =(value/100);
}

void MainForm::on_actionPositionBackground_triggered()
{
	posbackground->set_network(theNetwork);
	posbackground->show();
	updateCanvas();
}

////////////////////////////////////////////////
// NORMAL PRIVATE METHODS
////////////////////////////////////////////////

void MainForm::loop()
{
	copyPixmap();
	int msecs (pmdlg->refreshrate->value());
	int updatefac (pmdlg->updatefactor->value());
	if (!breaknow)
	{
		timer->setSingleShot(true);
		timer->start( msecs ); // ... mseconds single-shot timer 
	}
	currtime=theNetwork->step(((updatefac/100)*msecs/1000.0));
	progressbar->setValue(static_cast<int>(100.0*currtime/runtime));
	displaytime(currtime);

	QString mesg=QString("Veh:%1").arg(theParameters->veh_in_network);
	status_label->setText(mesg);

	if (currtime>=runtime)
	{
		//breaknow=false;
		breaknow=true;
		actionAnalyzeOutput->setEnabled(true);
		updateCanvas();
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
	Canvas->setPixmap(pm1);
	Canvas->repaint();  
}

void MainForm::paintEvent(QPaintEvent *  event )
{
		
}

void MainForm::seed(int sd )
{
   theNetwork->seed(sd);
}





/**
* key press event handling
*/
void MainForm::keyPressEvent( QKeyEvent *e )
{
	if (!initialised) return;
		 
	panfactor=static_cast<int>(0.5+(double)panpixels / wm.m11());
	switch (e->key() ) 
	{
		case (Qt::Key_Plus):	// zoom in
            on_zoomin_triggered();
			break;
		case (Qt::Key_Minus):	// zoom out
            on_zoomout_triggered();
			break;
		case (Qt::Key_Up):		// pan up
			dy=panfactor;
			wm.translate(0,dy);
			viewMat_=mod2stdViewMat_.inverted()*wm;
			updateCanvas();
			break;   
		case (Qt::Key_Down):	// pan down
			dy=-panfactor;
			wm.translate(0,dy);	
			viewMat_=mod2stdViewMat_.inverted()*wm;
			updateCanvas();
			break;       
		case (Qt::Key_Left):	// pan left
			dx=panfactor;
			wm.translate(dx,0);
			viewMat_=mod2stdViewMat_.inverted()*wm;
			updateCanvas();
			break; 
		case (Qt::Key_Right):	// pan right
			dx=-panfactor;
			wm.translate(dx,0);
			viewMat_=mod2stdViewMat_.inverted()*wm;
			updateCanvas();
			break;
		case (Qt::Key_C):		// return to initial view with a central image
			wm=mod2stdViewMat_;
			viewMat_.reset();
			scale=1;
			panfactor=panpixels;
			updateCanvas();
			break;
		case(Qt::Key_N):
			keyN_pressed_=true;
			break;
		case(Qt::Key_L):
			keyL_pressed_=true;
			break;
	} // end of switch
}

/*
* key release event handling
*/
void MainForm::keyReleaseEvent(QKeyEvent *kev)
{
	if (!initialised) return;
	switch (kev->key()){
		case(Qt::Key_N):
			keyN_pressed_=false;
			break;
		case(Qt::Key_L):
			keyL_pressed_=false;
			break;
	}
}

/**
* mouse press event handling
*/
void MainForm::mousePressEvent ( QMouseEvent * event )
{
	if (!initialised) return;

	// left mouse button pressed
	if (event->button() == Qt::LeftButton) 
	{
		lmouse_pressed_=true;
		// mouse position relative to the up-left corner of the Canvas
		int	x_current = event->x() - start_x; 
		int	y_current = event->y() - start_y - yadjust_; 
		
		
		if (keyN_pressed_) //node selecting mode
		{
			QPoint pos_view = QPoint(x_current,y_current);
			QString mesg=QString("Mouse_X %1, Mouse_Y %2").arg(x_current).arg(y_current);
			mouse_label->setText(mesg);

			QMatrix inv = wm.inverted();
			QPoint pos_model = inv.map(pos_view);
			selectNodes(pos_model);
		}
		else if(keyL_pressed_) //link selecting mode
		{
			QPoint pos_view = QPoint (x_current,y_current);
			QString mesg=QString("Mouse_X %1, Mouse_Y %2").arg(x_current).arg(y_current);
			mouse_label->setText(mesg);

			QMatrix inv = wm.inverted();
			QPoint pos_model = inv.map(pos_view);
			selectLinks(pos_model);
		}
		else // none selection mode
		{   
			// when zoombywin button triggered
			if (zoombywin_triggered_)
				zoomrect_=new QRect(x_current, y_current, 0, 0);
			
			// clear the selected nodes and links 
			unselectNodes();
			unselectLinks();

			// show the current mouse position 
			QPoint pos_view = QPoint (x_current,y_current);
			QString mesg=QString("Mouse: X %1, Y %2").arg(x_current).arg(y_current);
			mouse_label->setText(mesg);
		}
		// redraw network so that selcted or unselected 
		// nodes or links are redrawn 
		updateCanvas();
    }
	else if(event->button()==Qt::RightButton)
	{
		
	}
}

/**
* mouse move event handling
*/
void MainForm::mouseMoveEvent(QMouseEvent* mev)
{
	if (!lmouse_pressed_||keyN_pressed_||keyL_pressed_) return;

	if(zoombywin_triggered_)
	{
		// do nothing if no zoom window is drawn
		if(!zoomrect_) return;
		
		// mouse position relative to the up-left corner of the Canvas
		int	x_current = mev->x() - start_x; 
		int	y_current = mev->y() - start_y - yadjust_;
			
		// neglect the rectangle extending to the negative direction
		if (x_current<=zoomrect_->left()||y_current<=zoomrect_->top())
		{
			// remove the drawn rectangle by copy the stored
			// pixelmap
			copyPixmap();
			return;
		}
		else // draw a rectangle
		{
			zoomrect_->setRight(x_current);
			zoomrect_->setBottom(y_current);

			// draw the rect on pm1 but keep pm2 untouched 
			// so that need not to draw network when updating 
			drawZoomRect();
		}
	}
}

/**
* mouse release event
*/
void MainForm::mouseReleaseEvent(QMouseEvent* mev) 
{
	if(lmouse_pressed_)
	{
		if(zoombywin_triggered_&&zoomrect_)
			zoomRectArea();
	}
	else 
	{
	}
	
	if(zoomrect_)
	{
		delete zoomrect_;
		// it is very important to make sure 
		// that null pointer value is 0
		zoomrect_=0; 
	}
	lmouse_pressed_=false;
}

void MainForm::mouseDoubleClickEvent(QMouseEvent* mev)
{
}

/*
* draw the rectangle for zooming
*/
void MainForm::drawZoomRect()
{
	// compute the rectangle to the current view
	int rect_w = zoomrect_->width();
	int rect_h = zoomrect_->height();
	int view_w = viewSize_.width();
	int view_h = viewSize_.height();

	// do nothing when rectangle is a point 
	if (rect_w<=3||rect_h<=3) return;

	double scale_x = (double)view_w/(double)rect_w;
    double scale_y = (double)view_h/(double)rect_h;
	if (scale_x >= scale_y){	
		double newscale=scale_x;	
		int newheight=view_h/newscale;
		zoomrect_->setHeight(newheight);
   }
   else{
		double newscale=scale_y;	
		int newwidth=view_w/newscale;
		zoomrect_->setWidth(newwidth);
   }

	// copy pixmap
	pm1=pm2;
	
	//start painter
	QPainter paint(&pm1); 
	paint.setRenderHint(QPainter::Antialiasing); // smooth lines
	// set the pen
	QPen pen1(Qt::black, 1);
	pen1.setStyle(Qt::DashLine);
	paint.setPen(pen1);
	paint.drawRect(*zoomrect_); 
	paint.end();
	// update the canvas with pm1 and leave pm2 as a copy 
	// of the network
	Canvas->setPixmap(pm1);
	Canvas->repaint();
}

/** 
* zoom the window area selected 
*/
void MainForm::zoomRectArea()
{
	// do nothing when rectangle is a point 
	if (zoomrect_->width()<=3||zoomrect_->height()<=3) return;

	// compute the scale
	double rect2view_factor=viewSize_.width()/zoomrect_->width();
	scale*=rect2view_factor;

	// transfer matrix
	QMatrix tempMat;
	tempMat.reset();
	tempMat.scale(rect2view_factor, rect2view_factor);
	tempMat.translate(-zoomrect_->left(),-zoomrect_->top());
	
	// new transfer matrix from standard view to current view 
	viewMat_=viewMat_*tempMat;
	// new general tranfer matrix
	wm=mod2stdViewMat_*viewMat_;
	panfactor=static_cast<int>(0.5+(double)panpixels/scale);
	//update the canvas
	updateCanvas();	
}

/** 
* select nodes by mouse pointer
*/
void MainForm::selectNodes(QPoint pos)
{
	// remove links selected
	unselectLinks();

	//vector<Node*> allnodes=theNetwork->get_nodes();
	map<int,Node*> allnodes=theNetwork->get_nodes();

	int rad=theParameters->node_radius/mod2stdViewMat_.m11();
	map<int,Node*>::iterator n_iter = allnodes.begin();
	for( n_iter; n_iter!=allnodes.end(); n_iter++){
		if ((*n_iter).second->get_icon()->within_boundary(pos.x(),pos.y(),rad))
		{
			nodes_sel_.push_back((*n_iter).second);
			break;
		}
	}
	QString mesg;
	for( unsigned i=0; i<nodes_sel_.size();i++){
		nodes_sel_[i]->get_icon()->set_selected(true);
		nodes_sel_[i]->get_icon()->set_selected_color(Qt::blue);
		if (i==0)
			mesg=QString("Selected node: %1").arg(nodes_sel_[0]->get_id());
		else
			mesg+=QString(",%1").arg(nodes_sel_[i]->get_id());		
	}	
	mouse_label->setText(mesg);
}

/**
* selct links by mouse pointer
*/
void MainForm::selectLinks(QPoint pos)
{
	// remove nodes selected
	unselectNodes();

	map <int, Link*> alllinks=theNetwork->get_links();
	int rad=5;
	
	map <int,Link*>::iterator l_iter = alllinks.begin();
	for (l_iter;l_iter != alllinks.end(); l_iter++)
	{
		if ((*l_iter).second->get_icon()->within_boundary(pos.x(),pos.y(),rad))
		{
			links_sel_.push_back((*l_iter).second);
			break;
		}
	}
	/*
	for( unsigned i=0; i<alllinks.size(); i++){
		if (alllinks[i]->get_icon()->within_boundary(pos.x(),pos.y(),rad))
		{
			links_sel_.push_back(alllinks[i]);
			break;
		}
	}
*/
	QString mesg;
	for( unsigned i=0; i<links_sel_.size();i++){
		links_sel_[i]->set_selected(true);
		links_sel_[i]->set_selected_color(Qt::blue);
		if (i==0)
			mesg=QString("Selected link: %1").arg(links_sel_[0]->get_id());
		else
			mesg+=QString(",%1").arg(links_sel_[i]->get_id());		
	}	
	mouse_label->setText(mesg);
}

/**
* unselect nodes
*/
void MainForm::unselectNodes()
{
	for( unsigned i=0; i<nodes_sel_.size(); i++){
		nodes_sel_[i]->get_icon()->set_selected(false);
	}
	nodes_sel_.clear();		
}

/**
* unselect links
*/
void MainForm::unselectLinks()
{
	for( unsigned i=0; i<links_sel_.size(); i++){
		links_sel_[i]->set_selected(false);
	} 
	links_sel_.clear();
}

// Use the resize event to resize the Canvas. The bl**dy spacers and layouts dont seem to work!
void MainForm::resizeEvent(QResizeEvent* event)
{	
	panelx=width()-canvasOffset.width(); // offset is from right and bottom of form.
	panely=height()-canvasOffset.height();

	// resize the drawing widgets
	QSize oldviewsize=viewSize_;
	viewSize_=QSize(panelx,panely);
	pm1 = QPixmap (viewSize_);
	pm2 = QPixmap (viewSize_);
	Canvas->resize (panelx,panely); // resize the canvas

	if (!initialised) return; 

	// the view center offset
	int dxviewC=viewSize_.width()/2-oldviewsize.width()/2;
	int dyviewC=viewSize_.height()/2-oldviewsize.height()/2;
	wm.translate(dxviewC, dyviewC);
	mod2stdViewMat_=wm*viewMat_.inverted();
	//update the canvas
	updateCanvas();
	
	// Call the parent's resizeEvent		
	QWidget::resizeEvent(event);
}


void MainForm::center_image()
{
	wm=mod2stdViewMat_;
	viewMat_.reset();
	scale=1;
	panfactor=panpixels;
}
