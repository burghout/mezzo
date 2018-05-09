#include "controlcenter.h"
#include <algorithm>
#include <assert.h>

template<class T>
struct compare
{
	compare(int id_) :id(id_) {}
	bool operator () (T* thing)

	{
		return (thing->get_id() == id);
	}

	int id;
};

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

//FleetDispatcher
FleetDispatcher::FleetDispatcher()
{
	DEBUG_MSG("Constructing FD");
}
FleetDispatcher::~FleetDispatcher()
{
	DEBUG_MSG("Destroying FD");
}

//ControlCenter
ControlCenter::ControlCenter(int id, Eventlist* eventlist, QObject* parent) : QObject(parent), id_(id), eventlist_(eventlist)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes
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
	//tm_.reset();
	//fs_.reset();

	//Clear all members of ControlCenter
	connectedPass_.clear();
	connectedVeh_.clear();
}

void ControlCenter::connectPassenger(Passenger* pass)
{
	//DEBUG_MSG("Testing unique connection");
	//ok = QObject::connect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) == 0); //passenger should only be added once
	connectedPass_[pid] = pass;

	if (!QObject::connect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connectPassenger failed!");
		abort();
	}
}
void ControlCenter::disconnectPassenger(Passenger* pass)
{
	assert(connectedPass_.count(pass->get_id() != 0));
	connectedPass_.erase(connectedPass_.find(pass->get_id()));
	bool ok = QObject::disconnect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest);
	assert(ok);
}

void ControlCenter::connectVehicle(Bus* transitveh)
{
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) == 0); //vehicle should only be added once
	connectedVeh_[bvid] = transitveh;

	if (!QObject::connect(transitveh, &Bus::stateChanged, this, &ControlCenter::updateFleetState, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connectVehicle failed!");
		abort();
	}
}
void ControlCenter::disconnectVehicle(Bus* transitveh)
{
	assert(connectedVeh_.count(transitveh->get_bus_id() != 0));
	connectedVeh_.erase(connectedVeh_.find(transitveh->get_bus_id()));
	bool ok = QObject::disconnect(transitveh, &Bus::stateChanged, this, &ControlCenter::updateFleetState);
	assert(ok);
}

void ControlCenter::addCandidateLine(Busline* line)
{
	assert(line->get_flex_line()); //only flex lines should be added to control center for now
	candidateLines_.push_back(line);
}

vector<Busline*> ControlCenter::get_lines_between_stops(const vector<Busline*>& candidateLines, const int ostop_id, const int dstop_id) const
{
	vector<Busline*> lineConnections; // lines between stops given as input
	if (!candidateLines.empty())
	{
		for(auto& line : candidateLines)
		{
			if (!line->stops.empty())
			{
				vector<Busstop*>::iterator ostop_it; //iterator pointing to origin busstop if it exists for this line
				ostop_it = find_if(line->stops.begin(), line->stops.end(), compare<Busstop>(ostop_id));

				if (ostop_it != line->stops.end()) //if origin busstop does exist on line see if destination stop is downstream from this stop
				{
					vector<Busstop*>::iterator dstop_it; //iterator pointing to destination busstop if it exists downstream of origin busstop for this line
					dstop_it = find_if(ostop_it, line->stops.end(), compare<Busstop>(dstop_id));

					if (dstop_it != line->stops.end()) //if destination stop exists 
						lineConnections.push_back(line); //add line as a possible transit connection between these stops
				}
			}
		}
		
	}

	return lineConnections;
}

//Slot implementations
void ControlCenter::recieveRequest(Request req)
{
	DEBUG_MSG(Q_FUNC_INFO);
	assert(req.time >= 0 && req.load > 0); //assert that request is valid
	rh_.addRequest(req) ? emit requestAccepted() : emit requestRejected();
}
void ControlCenter::on_requestAccepted()
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Accepted!");
	//tg_.generate_trip(eventlist_);
}
void ControlCenter::on_requestRejected()
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Rejected!");
}

void ControlCenter::updateFleetState()
{
	DEBUG_MSG("ControlCenter " << id_ << " - Updating fleet state");
}


