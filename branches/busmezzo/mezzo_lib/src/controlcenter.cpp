#include "controlcenter.h"

//TripGenerator::TripGenerator()
//{
//	cout << "Contructing TG" << endl;
//}
//
//TripGenerator::~TripGenerator()
//{
//	cout << "Destroying TG" << endl;
//}
//
//void TripGenerator::generate_trip(string requestQueue, double time)
//{
//	cout << "Generating " << requestQueue << " at time " << time << endl;
//	
//	//TripQueue.push_back(make_pair(id, trip));
//	//TripQueue.push_back(time);
//}

//ControlCenter::ControlCenter(TripGenerator* tg_) : tg(tg_)
//{
//
//	cout << "Constructing CC" << endl;
//}

ControlCenter::ControlCenter()
{
	cout << "Constructing CC without TG" << endl;
}

ControlCenter::~ControlCenter()
{
	cout << "Destroying CC" << endl;
}