#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

/**************************

General Notes:

**************************/

/*! ControlCenter groups together objects that control or modify transit objects
- 
*/

//#include "busline.h"
#include <iostream>
#include <string>

using namespace std;
//
//class TripGenerator
//{
//public:
//	TripGenerator();
//	~TripGenerator();
//
//	void generate_trip(string requestQueue, double time);
//
//private:
//	//vector<double> TripQueue;
//};

class ControlCenter
{
public:
	//ControlCenter(TripGenerator* tg_);
	ControlCenter();
	~ControlCenter();

	//void generate_trip(string requestQueue, double time)
	//{
	//	tg->generate_trip(requestQueue, time);
	//}

private:
	//TripGenerator* tg;
	/*
	RequestHandler* tg;
	*/
};

extern ControlCenter* CC;

#endif
