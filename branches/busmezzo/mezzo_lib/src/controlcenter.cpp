#include "controlcenter.h"
#include <algorithm> //for std::find

//RequestHandler
RequestHandler::RequestHandler()
{
	cout << "Constructing RH" << endl;
}
RequestHandler::~RequestHandler(){}

bool RequestHandler::addRequest(Request vehRequest)
{
	if (find(requestSet.begin(), requestSet.end(), vehRequest) != requestSet.end())
	{
		requestSet.push_back(vehRequest);
		return true;
	}
	return false;
}

//TripGenerator
TripGenerator::TripGenerator()
{
	cout << "Constructing TG" << endl;
}
TripGenerator::~TripGenerator(){}

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

void ControlCenter::connectPassenger(const Passenger* pass) const
{
	QObject::connect(pass, SIGNAL(tappedInAtStop(Request)), this, SLOT(recieveRequest(Request)));
}

void ControlCenter::recieveRequest(Request req)
{
	if (rh_.addRequest(req))
		emit requestAccepted();
}

