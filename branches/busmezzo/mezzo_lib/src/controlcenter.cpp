#include "controlcenter.h"
#include <algorithm>

//RequestHandler
RequestHandler::RequestHandler()
{
	DEBUG_MSG("Constructing RH");
}
RequestHandler::~RequestHandler()
{
	DEBUG_MSG("Destroying RH");
}

void RequestHandler::reset()
{
	requestSet.clear();
}
RequestHandler::~RequestHandler(){}

bool RequestHandler::addRequest(Request vehRequest)
{
	if (find(requestSet.begin(), requestSet.end(), vehRequest) == requestSet.end())
	{
		requestSet.push_back(vehRequest);
		return true;
	}
	return false;
}

//TripGenerator
TripGenerator::TripGenerator()
{
	DEBUG_MSG("Constructing TG");
}
TripGenerator::~TripGenerator()
{
	DEBUG_MSG("Destroying TG");
}

void TripGenerator::reset()
{
	tripSet.clear();
}

//TripMatcher
TripMatcher::TripMatcher(IMatchingStrategy* ms): matchingStrategy_(ms)
{
	DEBUG_MSG("Constructing TM");
}
TripMatcher::~TripMatcher()
{
	DEBUG_MSG("Destroying TM");
}

//ControlCenter
ControlCenter::ControlCenter(int id, QObject* parent) : QObject(parent), id_(id)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname);
	DEBUG_MSG("Constructing CC" << id_);

	//connect internal signal slots
	QObject::connect(this, &ControlCenter::requestRejected, this, &ControlCenter::on_requestRejected, Qt::DirectConnection);
	QObject::connect(this, &ControlCenter::requestAccepted, this, &ControlCenter::on_requestAccepted, Qt::DirectConnection);
}

ControlCenter::~ControlCenter()
{
	DEBUG_MSG("Destroying CC" << id_);
}

void ControlCenter::reset()
{
	disconnect(this, 0, 0, 0); //Disconnect all signals of controlcenter

	//reconnect internal signal slots
	QObject::connect(this, &ControlCenter::requestRejected, this, &ControlCenter::on_requestRejected, Qt::DirectConnection);
	QObject::connect(this, &ControlCenter::requestAccepted, this, &ControlCenter::on_requestAccepted, Qt::DirectConnection);

	//Clear all members of process classes
	rh_.reset();
	tg_.reset();

	//Clear all members of ControlCenter
	connectedPass_.clear();
	connectedVeh_.clear();
}

void ControlCenter::connectPassenger(Passenger* pass)
{
	//DEBUG_MSG("Testing unique connection");
	//ok = QObject::connect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
	int id = pass->get_id();
	assert(connectedPass_.count(id) == 0); //passenger should only be added once
	connectedPass_[id] = pass;

	if (!QObject::connect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connection failed!");
		abort();
	}
}

void ControlCenter::disconnectPassenger(Passenger * pass)
{
	connectedPass_.erase(connectedPass_.find(pass->get_id()));
	bool ok = QObject::disconnect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest);
	assert(ok);
}

void ControlCenter::recieveRequest(Request req)
{
	DEBUG_MSG(Q_FUNC_INFO);
	assert(req.time >= 0 && req.load >= 0); //assert that request is valid
	rh_.addRequest(req) ? emit requestAccepted() : emit requestRejected();
}

void ControlCenter::on_requestAccepted()
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Accepted!");
}

void ControlCenter::on_requestRejected()
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Rejected!");
}
