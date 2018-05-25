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
	requestSet_.clear();
}

bool RequestHandler::addRequest(const Request req)
{
	if (find(requestSet_.begin(), requestSet_.end(), req) == requestSet_.end()) //if request does not already exist in set (which it shouldnt)
	{
		requestSet_.insert(req);
		return true;
	}
	return false;
}

void RequestHandler::removeRequest(const int pass_id)
{
	set<Request>::iterator it;
	it = find_if(requestSet_.begin(), requestSet_.end(),
			[pass_id](const Request& req) -> bool
			{
				return req.pass_id == pass_id;
			}
		);

	if (it != requestSet_.end())
		requestSet_.erase(it);
	else
		DEBUG_MSG_V("Request for pass id " << pass_id << " not found.");
}

//BustripGenerator
BustripGenerator::BustripGenerator(ITripGenerationStrategy* generationStrategy) : generationStrategy_(generationStrategy)
{
	DEBUG_MSG("Constructing TG");
}
BustripGenerator::~BustripGenerator()
{
	DEBUG_MSG("Destroying TG");
	delete generationStrategy_;
}

void BustripGenerator::reset(int tg_strategy_type)
{
	for (Bustrip* trip : plannedTrips_)
	{
		delete trip;
	}
	plannedTrips_.clear();
	setTripGenerationStrategy(tg_strategy_type);
}

void BustripGenerator::addCandidateline(Busline * line)
{
	candidateLines_.push_back(line);
}

void BustripGenerator::removeTrip(const int trip_id)
{
	set<Bustrip*>::iterator it;
	it = find_if(plannedTrips_.begin(), plannedTrips_.end(), compare<Bustrip>(trip_id));
	if (it != plannedTrips_.end())
	{
		delete (*it);
		plannedTrips_.erase(it);
	}
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, double time)
{
	DEBUG_MSG("BustripGenerator is requesting a trip at time " << time);
	if (generationStrategy_)
	{
		return generationStrategy_->calc_trip_generation(rh.requestSet_, candidateLines_, time, plannedTrips_); //returns true if trip has been generated and added to the plannedTrips_
	}
	return false;
}

void BustripGenerator::setTripGenerationStrategy(int type)
{
	if (generationStrategy_)
		delete generationStrategy_;

	DEBUG_MSG("Changing trip generation strategy to " << type);
	if (type == Null)
		generationStrategy_ = new NullTripGeneration();
	else if (type == Naive)
		generationStrategy_ = new NaiveTripGeneration();
	else
		generationStrategy_ = nullptr;
}

vector<Busline*> ITripGenerationStrategy::get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const
{
	vector<Busline*> lines_between_stops; // lines between stops given as input
	if (!lines.empty())
	{
		for (auto& line : lines)
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
						lines_between_stops.push_back(line); //add line as a possible transit connection between these stops
				}
			}
		}
	}

	return lines_between_stops;
}
Bustrip* ITripGenerationStrategy::create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& desired_schedule) const
{
	int trip_id; 
	Bustrip* trip = nullptr;

	trip_id = line->generate_new_trip_id(); //get new trip id for this line
	assert(line->is_unique_tripid(trip_id));

	trip = new Bustrip(trip_id, desired_dispatch_time, line); //create trip

	//initialize trip
	trip->add_stops(desired_schedule); //add scheduled stop visits to trip
	trip->convert_stops_vector_to_map(); //TODO: not sure why this is necessary but is done for other trips so
	trip->set_last_stop_visited(trip->stops.front()->first);  //same here unsure why this is necessary (instead of in constructor or activate method) but done in network readers

	return trip;

}

vector<Visit_stop*> ITripGenerationStrategy::create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const
{
	assert(init_dispatch_time >= 0);
	vector<Visit_stop*> schedule;
	double arrival_time_at_stop = init_dispatch_time;

	//add init_dispatch_time to all stop deltas for schedule
	for (const pair<Busstop*, double>& stop_delta : time_between_stops)
	{
		Visit_stop* vs = new Visit_stop((stop_delta.first), stop_delta.second + arrival_time_at_stop);
		schedule.push_back(vs);
	}

	return schedule;
}

bool NaiveTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateLines, const double time, set<Bustrip*>& tripSet) const
{
	if (!requestSet.empty() && !candidateLines.empty())
	{
		if (requestSet.size() >= 10) //generate a trip if there are at least ten requests in the requestSet
		{
			int ostop_id = (*requestSet.begin()).ostop_id;//find the OD pair for the first request in the request set
			int dstop_id = (*requestSet.begin()).dstop_id;
			vector<Busline*> lines_between_stops;

			lines_between_stops = get_lines_between_stops(candidateLines, ostop_id, dstop_id); //check if any candidate line connects the OD pair
			if (!lines_between_stops.empty())//if a connection exists then generate a trip for this line for dynamically generated trips
			{
				Busline* line = lines_between_stops.front(); //choose first feasible line found
				vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)

				Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
				tripSet.insert(newtrip);//add this trip to the tripSet

				return true;
			}
		}
	}
	
	return false;
}

//BustripVehicleMatcher
BustripVehicleMatcher::BustripVehicleMatcher(IMatchingStrategy* matchingStrategy): matchingStrategy_(matchingStrategy)
{
	DEBUG_MSG("Constructing TVM");
}
BustripVehicleMatcher::~BustripVehicleMatcher()
{
	DEBUG_MSG("Destroying TVM");
}

bool BustripVehicleMatcher::matchTrip(const BustripGenerator& tg, double time)
{
	//matchingStrategy_->find_tripvehicle_match(tg.plannedTrips_, double time);
	matchingStrategy_->find_tripvehicle_match();

	return false;
}

void BustripVehicleMatcher::setMatchingStrategy(int type)
{
	if (matchingStrategy_)
		delete matchingStrategy_;

	DEBUG_MSG("Changing trip - vehicle matching strategy to " << type);
	if (type == Null)
		matchingStrategy_ = new NullMatching();
	else if (type == Naive)
		matchingStrategy_ = new NaiveMatching();
	else
		matchingStrategy_ = nullptr;
}

void BustripVehicleMatcher::reset(int matching_strategy_type)
{
	matchedTrips_.clear();
	setMatchingStrategy(matching_strategy_type);
}

//VehicleDispatcher
VehicleDispatcher::VehicleDispatcher()
{
	DEBUG_MSG("Constructing FD");
}
VehicleDispatcher::~VehicleDispatcher()
{
	DEBUG_MSG("Destroying FD");
}

//ControlCenter
ControlCenter::ControlCenter(int id, int tg_strategy, int tvm_strategy, Eventlist* eventlist, QObject* parent) 
	: QObject(parent), id_(id), tg_strategy_(tg_strategy), tvm_strategy_(tvm_strategy), eventlist_(eventlist)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes
	DEBUG_MSG("Constructing CC" << id_);

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tvm_.setMatchingStrategy(tvm_strategy);	//set initial matching strategy of BustripVehicleMatcher
	connectInternal(); //connect internal signal slots
}
ControlCenter::~ControlCenter()
{
	DEBUG_MSG("Destroying CC" << id_);
}

void ControlCenter::reset()
{
	disconnect(this, 0, 0, 0); //Disconnect all signals of controlcenter

	connectInternal(); //reconnect internal signal slots

	//Reset all process classes
	rh_.reset();
	tg_.reset(tg_strategy_); 
	tvm_.reset(tvm_strategy_);
	//fs_.reset();

	//Clear all members of ControlCenter
	connectedPass_.clear();
	connectedVeh_.clear();
}

void ControlCenter::connectInternal()
{
	bool ok;
	ok = QObject::connect(this, &ControlCenter::requestRejected, this, &ControlCenter::on_requestRejected, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &ControlCenter::requestAccepted, this, &ControlCenter::on_requestAccepted, Qt::DirectConnection);
	assert(ok);

	//Triggers to generate trips via BustripGenerator
	ok = QObject::connect(this, &ControlCenter::requestAccepted, this, &ControlCenter::requestTrip, Qt::DirectConnection); 
	assert(ok);
	ok = QObject::connect(this, &ControlCenter::fleetStateChanged, this, &ControlCenter::requestTrip, Qt::DirectConnection);
	assert(ok);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	ok = QObject::connect(this, &ControlCenter::tripGenerated, this, &ControlCenter::on_tripGenerated, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &ControlCenter::tripGenerated, this, &ControlCenter::matchVehicle, Qt::DirectConnection);
	assert(ok);	
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
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Passenger::sendRequest to ControlCenter::recieveRequest failed!");
		abort();
	}
	if (!QObject::connect(pass, &Passenger::boardedBus, this, &ControlCenter::removeRequest, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Passenger::boardedBus to ControlCenter::removeRequest failed!");
		abort();
	}
}
void ControlCenter::disconnectPassenger(Passenger* pass)
{
	assert(connectedPass_.count(pass->get_id() != 0));
	connectedPass_.erase(connectedPass_.find(pass->get_id()));
	bool ok = QObject::disconnect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest);
	assert(ok);
	ok = QObject::disconnect(pass, &Passenger::boardedBus, this, &ControlCenter::removeRequest);
	assert(ok);
}

void ControlCenter::connectVehicle(Bus* transitveh)
{
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) == 0); //vehicle should only be added once
	connectedVeh_[bvid] = transitveh;

	//connect bus state changes to control center
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
	tg_.addCandidateline(line);
}

//Slot implementations
void ControlCenter::recieveRequest(Request req, double time)
{
	DEBUG_MSG(Q_FUNC_INFO);
	assert(req.time >= 0 && req.load > 0); //assert that request is valid
	rh_.addRequest(req) ? emit requestAccepted(time) : emit requestRejected(time);
}

void ControlCenter::removeRequest(int pass_id)
{
	DEBUG_MSG(Q_FUNC_INFO);
	rh_.removeRequest(pass_id);
}

void ControlCenter::on_requestAccepted(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Accepted at time " << time);
}
void ControlCenter::on_requestRejected(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Rejected at time " << time);
}

void ControlCenter::on_tripGenerated(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Trip Generated at time " << time);
}

void ControlCenter::updateFleetState(int bus_id, BusState newstate, double time)
{
	int state = static_cast<int>(newstate);
	DEBUG_MSG("ControlCenter " << id_ << " - Updating fleet state at time " << time);
	DEBUG_MSG("Bus " << bus_id << " - " << state );
	emit fleetStateChanged(time);
}

void ControlCenter::requestTrip(double time)
{
	if (tg_.requestTrip(rh_, time))
		emit tripGenerated(time);
}

void ControlCenter::matchVehicle(double time)
{
	tvm_.matchTrip(tg_, time);
}


