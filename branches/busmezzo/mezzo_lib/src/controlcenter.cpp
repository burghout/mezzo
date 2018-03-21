#include "controlcenter.h"

//RequestHandler
RequestHandler::RequestHandler()
{
	cout << "Constructing RH" << endl;
}
RequestHandler::~RequestHandler(){}

void RequestHandler::generate_request(const string passengerData, const double time)
{
	cout << "Generating request " << passengerData << " at time " << time << endl;
}

//TripGenerator
TripGenerator::TripGenerator()
{
	cout << "Constructing TG" << endl;
}
TripGenerator::~TripGenerator(){}

void TripGenerator::generate_trip(const string requestQueue, const double time)
{
	cout << "Generating trip " << requestQueue << " at time " << time << endl;
	
	//TripQueue.push_back(make_pair(id, trip));
	tripQueue.push_back(time);
}

//TripMatcher
TripMatcher::TripMatcher()
{
	cout << "Constructing TM" << endl;
}
TripMatcher::~TripMatcher(){}

//ControlCenter
//ControlCenter::ControlCenter(TripGenerator* tg) : tg_(tg)
//{
//	assert(tg);
//	cout << "Constructing CC" << endl;
//}

ControlCenter::ControlCenter()
{
	cout << "Constructing CC" << endl;
}

ControlCenter::~ControlCenter()
{
	cout << "Destroying CC" << endl;
}

