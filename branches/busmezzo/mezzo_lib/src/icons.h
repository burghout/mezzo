/*
	Mezzo Mesoscopic Traffic Simulation 
	Copyright (C) 2008  Wilco Burghout

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/**
 * modification:
 *   add a handle to the linkicon
 *   
 * Xiaoliang Ma 
 * last change: 2007-10-30 
 */
 #ifndef ICONS_HH
#define ICONS_HH
//#undef _NO_GUI
#ifndef _NO_GUI
#include <QPixmap>
//#include <qpixmap>
#include <QPen>
#include <QPainter>
#include <list>
#include "parameters.h"
#include "grid.h"

#define FIXED_RUNNING

using namespace std;

class Icon;
class Node;

class Drawing
{
  public:
  	Drawing();
  	virtual ~Drawing();
  	void set_background (const char* name) {if (bpm) delete bpm; bpm=new QPixmap(name);bg_set=true;basematrix_set=false;}
  	void add_icon (Icon* icon);
  	virtual void draw(QPixmap * pm,QMatrix * wm);
	vector <int> get_boundaries();        // returns [min_x,min_y, max_x, max_y]
  private:
  	list <Icon*> icons;
    bool bg_set;
	bool basematrix_set;
	QMatrix bwm;
  	QPixmap* bpm;
};


class Icon
{
  public:
	  Icon() {selected = false;}
	  Icon( int startx_, int starty_) : startx(startx_), starty(starty_) 
			{selected = false; selected_color = theParameters->selectedcolor;}
	  virtual ~Icon(){};
	  virtual void draw(QPixmap * ,QMatrix * ){};
      virtual bool within_boundary(const double x, const double y, int rad);
	  void settext(const string st) {text=QString(st.c_str());}
      int get_x()  { return startx;}
      int get_y()  { return starty;}
      void set_selected (bool sel) {selected = sel;} // sets if object is selected or not
      bool get_selected () {return selected;} // gets the selected status of object
	  void set_selected_color (const QColor selcolor) {selected_color = selcolor;}
	  const QColor get_selected_color () {return selected_color;}
  protected:
      QString text;
      int startx, starty; // the icon's position
	  bool selected;
	  QColor selected_color; // local selected color, default is taken from Parameters object 
};

class LinkIcon : public Icon
{
  public:
  	LinkIcon(int x, int y, int tox, int toy);
	virtual ~LinkIcon(){};
	void set_pointers(double * q, double * r);
	void set_link(Link* link_) {link=link_;}
	void calc_shift(double q=0.0); //!< calculate the relative shift from the center line between nodes, q is radius. 
	void sethandle(bool handle){handle_on_=handle;}
	bool gethandle(){return handle_on_;}
	int getLinkicon_leng(){return linkicon_leng_;}
    virtual bool within_boundary(const double x, const double y, int rad);
  	virtual void draw(QPixmap * pm,QMatrix * wm);

	void setMOE_thickness (MOE* moe_) {moe_thickness=moe_;} // sets the output MOE (such as flows, links etc.)	
	void setMOE_colour (MOE* moe_) {moe_colour=moe_;} // sets the output MOE (such as flows, links etc.)
  protected:
   Link* link;
  	int stopx, stopy;
	int shiftx, shifty;
	int handlex, handley;
    int x2,x3,y2,y3; // points for the arrowhead
  	double * queuepercentage;
  	double * runningpercentage;
	bool handle_on_;
	int linkicon_leng_;
	MOE* moe_thickness;
	MOE* moe_colour;
};

class VirtualLinkIcon: public LinkIcon
{
 public:
	VirtualLinkIcon(int x, int y, int tox, int toy) : LinkIcon(x,y,tox,toy) {}
	virtual ~VirtualLinkIcon(){};
	virtual void draw(QPixmap * pm,QMatrix * wm);
 private:
};

class NodeIcon : public Icon
{
 public:
  	NodeIcon(int x, int y, Node *node);
	virtual ~NodeIcon(){}
    virtual void draw(QPixmap * pm,QMatrix * wm);
 private:
  	int width, height;
	Node* thenode_;
      
};

class IncidentIcon : public Icon
{
public:
	IncidentIcon (int x, int y);
	virtual ~IncidentIcon () {}
	virtual void draw(QPixmap * pm,QMatrix * wm);
	void set_visible( bool val) {visible = val;}
	bool is_visible () {return visible;}
private:
	int width, height; // probably set this to x*node_radius or something, to avoid yet another parameter
	bool visible;
};

#endif // _NO_GUI

#endif
