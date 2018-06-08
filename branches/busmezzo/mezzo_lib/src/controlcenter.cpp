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

// Request

Request::Request(int pid, int oid, int did, int l, double t) : pass_id(pid), ostop_id(oid), dstop_id(did), load(l), time(t)
{
    qRegisterMetaType<Request>(); //register Request as a metatype for QT signal arguments
}

bool Request::operator==(const Request & rhs) const
{
	return (pass_id == rhs.pass_id && ostop_id == rhs.ostop_id && dstop_id == rhs.dstop_id && load == rhs.load && time == rhs.time);
}

bool Request::operator<(const Request & rhs) const
{
	if (time != rhs.time)
		return time < rhs.time;
	else if (load != rhs.load)
		return load < rhs.load;
	else if (ostop_id != rhs.ostop_id)
		return ostop_id < rhs.ostop_id;
	else if (dstop_id != rhs.dstop_id)
		return dstop_id < rhs.dstop_id;
	else
		return pass_id < rhs.pass_id;
}

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

void BustripGenerator::reset(int generation_strategy_type)
{
	for (Bustrip* trip : plannedTrips_)
	{
		delete trip;
	}
	plannedTrips_.clear();
	setTripGenerationStrategy(generation_strategy_type);
}

void BustripGenerator::addServiceRoute(Busline * line)
{
	serviceRoutes_.push_back(line);
}

void BustripGenerator::cancelPlannedTrip(Bustrip* trip)
{
	assert(trip->driving_roster.empty());
	if (plannedTrips_.count(trip) != 0)
	{
		delete trip;
		plannedTrips_.erase(trip);
	} 
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, double time)
{
	DEBUG_MSG("BustripGenerator is requesting a trip at time " << time);
	if (generationStrategy_)
	{
		return generationStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, time, plannedTrips_); //returns true if trip has been generated and added to the plannedTrips_
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
	{
		DEBUG_MSG("This generation strategy is not recognized!");
		generationStrategy_ = nullptr;
	}
}

//ITripGenerationStrategy
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
	Bustrip* trip;

	trip_id = line->generate_new_trip_id(); //get new trip id for this line
	assert(line->is_unique_tripid(trip_id));

	trip = new Bustrip(trip_id, desired_dispatch_time, line); //create trip

	//initialize trip
	trip->add_stops(desired_schedule); //add scheduled stop visits to trip
	trip->convert_stops_vector_to_map(); //TODO: not sure why this is necessary but is done for other trips so
	trip->set_last_stop_visited(trip->stops.front()->first);  //sets last stop visited to the origin stop of the trip

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

bool NullTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const
{
	Q_UNUSED(requestSet);
	Q_UNUSED(candidateServiceRoutes);
	Q_UNUSED(time);
	Q_UNUSED(tripSet);

	return false;
}

bool NaiveTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const
{
	if (!requestSet.empty() && !candidateServiceRoutes.empty())
	{
		if (requestSet.size() >= 10) //generate a trip if there are at least ten requests in the requestSet
		{
			int ostop_id = (*requestSet.begin()).ostop_id;//find the OD pair for the first request in the request set
			int dstop_id = (*requestSet.begin()).dstop_id;
			vector<Busline*> lines_between_stops;

			lines_between_stops = get_lines_between_stops(candidateServiceRoutes, ostop_id, dstop_id); //check if any candidate service route connects the OD pair
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
	delete matchingStrategy_;
}

void BustripVehicleMatcher::reset(int matching_strategy_type)
{
	matchedTrips_.clear();
	candidateVehicles_per_SRoute_.clear();
	setMatchingStrategy(matching_strategy_type);
}

void BustripVehicleMatcher::addVehicleToServiceRoute(int line_id, Bus* transitveh)
{
	candidateVehicles_per_SRoute_[line_id].push_back(transitveh);
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
	{
		DEBUG_MSG("This matching strategy is not recognized!");
		matchingStrategy_ = nullptr;
	}
}

bool BustripVehicleMatcher::matchVehiclesToTrips(BustripGenerator& tg, double time)
{
	DEBUG_MSG("BustripVehicleMatcher is matching trips to vehicles at time " << time);
	if (matchingStrategy_)
	{
		return matchingStrategy_->find_tripvehicle_match(tg.plannedTrips_, candidateVehicles_per_SRoute_, time, matchedTrips_);
	}
	return false;
}

//IMatchingStrategy
void IMatchingStrategy::assign_idlevehicle_to_trip(Bus* veh, Bustrip* trip, double starttime)
{
	assert(!veh->get_curr_trip()); //this particular bus instance (remember there may be copies of it if there is a trip chain, should not have a trip)
	assert(veh->is_idle());

	DEBUG_MSG("---Assigning vehicle " << veh->get_bus_id() << " to trip " << trip->get_id() << "---");

	trip->set_busv(veh); //assign bus to the trip
	veh->set_curr_trip(trip); //assign trip to the bus
	trip->set_bustype(veh->get_bus_type());//set the bustype of the trip (so trip has access to this bustypes dwell time function)
	trip->get_busv()->set_on_trip(true); //flag the bus as busy for all trips except the first on its chain (it shouldnt) TODO: maybe move this to dispatcher
	
	vector<Start_trip*> driving_roster; //contains all other trips in this trips chain (if there are any) (TODO might move to dispatcher
	Start_trip* st = new Start_trip(trip, starttime);

	driving_roster.push_back(st);
	trip->add_trips(driving_roster); //save the driving roster at the trip level to conform to interfaces of Busline::execute, Bustrip::activate etc. 
	double delay = trip->get_starttime() - starttime;
	trip->set_starttime(starttime); //reset scheduled dispatch from origin to given starttime
	
	DEBUG_MSG("Delay in start time for trip " << trip->get_id() << ": " << delay);

	Busstop* stop = veh->get_last_stop_visited();
	
	if (!stop->remove_unassigned_bus(veh)) //bus is no longer unassigned and is removed from vector of unassigned buses at whatever stop the vehicle is waiting idle at
											//trip associated with this bus will be added later to expected_bus_arrivals via
											//Busline::execute -> Bustrip::activate -> Origin::insert_veh -> Link::enter_veh -> Bustrip::book_stop_visit -> Busstop::book_bus_arrival
	{
		DEBUG_MSG_V("Busstop::remove_unassigned_bus failed for bus " << veh->get_bus_id() << " and stop " << stop->get_id());
	}

}

bool NullMatching::find_tripvehicle_match(set<Bustrip*>& plannedTrips, map<int, vector<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips)
{
	Q_UNUSED(plannedTrips);
	Q_UNUSED(veh_per_sroute);
	Q_UNUSED(time);
	Q_UNUSED(matchedTrips);

	return false;
}

bool NaiveMatching::find_tripvehicle_match(set<Bustrip*>& plannedTrips, map<int, vector<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips)
{
	DEBUG_MSG("------------Matching Naively!-------------");
	//attempt to match the first trip among plannedTrips with first idle vehicle found at the origin stop of the trip
	if(!plannedTrips.empty() && !veh_per_sroute.empty())
	{
		Bustrip* trip = (*plannedTrips.begin()); //choose the first trip among planned trips
		Bus* veh = nullptr; //the transit veh that we wish to match to a trip
		Busline* sroute = trip->get_line(); //get the line/service route of this trip
		vector<Bus*> candidate_buses = veh_per_sroute[sroute->get_id()]; //get all transit vehicles that have this route in their service area

		if (!candidate_buses.empty()) //there is at least one bus serving the route of this trip
		{
			Busstop* origin_stop = trip->get_last_stop_visited(); //get the initial stop of the trip
			vector<pair<Bus*, double>> ua_buses_at_stop = origin_stop->get_unassigned_buses_at_stop(); 

			//check if one of the unassigned transit vehicles at the origin stop is currently serving the route of the trip
			for (const pair<Bus*, double>& ua_bus : ua_buses_at_stop)
			{
				vector<Bus*>::iterator c_bus_it;
				c_bus_it = find_if(candidate_buses.begin(), candidate_buses.end(), 
					[ua_bus](const Bus* c_bus)->bool { 
						return c_bus->get_bus_id() == ua_bus.first->get_bus_id();  //first bus in list should be the bus that arrived to the stop earliest
					}
				);
				if (c_bus_it != candidate_buses.end()) //a bus match has been found
				{
					DEBUG_MSG("Trip - vehicle match found!");
					veh = (*c_bus_it);
					assign_idlevehicle_to_trip(veh, trip, time); //schedule the vehicle to perform the trip at this time
					plannedTrips.erase(trip); //planned trip is now a matched trip so remove from plannedTrips set
					matchedTrips.insert(trip); //and insert it into the match trips set
					return true;
				}
			}
		}
	}

	return false;
}

//VehicleDispatcher
VehicleDispatcher::VehicleDispatcher(Eventlist* eventlist, IDispatchingStrategy* dispatchingStrategy) : eventlist_(eventlist), dispatchingStrategy_(dispatchingStrategy)
{
	DEBUG_MSG("Constructing FD");
}
VehicleDispatcher::~VehicleDispatcher()
{
	DEBUG_MSG("Destroying FD");
	delete dispatchingStrategy_;
}

void VehicleDispatcher::reset(int dispatching_strategy_type)
{
	setDispatchingStrategy(dispatching_strategy_type);
}

bool VehicleDispatcher::dispatchMatchedTrips(BustripVehicleMatcher& tvm, double time)
{
	DEBUG_MSG("VehicleDispatcher is scheduling matched trips at time " << time);
	if (dispatchingStrategy_)
	{
		return dispatchingStrategy_->calc_dispatch_time(eventlist_, tvm.matchedTrips_, time);
	}

	return false;
}

void VehicleDispatcher::setDispatchingStrategy(int d_strategy_type)
{
	if (dispatchingStrategy_)
		delete dispatchingStrategy_;

	DEBUG_MSG("Changing dispatching strategy to " << d_strategy_type);
	if (d_strategy_type == Null)
		dispatchingStrategy_ = new NullDispatching();
	else if (d_strategy_type == Naive)
		dispatchingStrategy_ = new NaiveDispatching();
	else
	{
		DEBUG_MSG("This dispatching strategy is not recognized!");
		dispatchingStrategy_ = nullptr;
	}
}

//IDispatchingStrategy
bool IDispatchingStrategy::dispatch_trip(Eventlist* eventlist, Bustrip* trip)
{
	Busline* line = trip->get_line();
	double starttime = trip->get_starttime();
	assert(line->is_flex_line());
	//line->add_flex_trip(trip, starttime);
	//trip->activate(trip->get_starttime(), line->get_busroute(), line->get_odpair(), eventlist);
	
	//line->add_trip(trip, starttime);
	//eventlist->add_event(starttime, line); //book the dispatch of this trip in the eventlist
	return false;
}

bool NaiveDispatching::calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time)
{
	if (!unscheduledTrips.empty())
	{
		Bustrip* trip = (*unscheduledTrips.begin());
		Bus* bus = trip->get_busv();
		//check if the bus associated with this trip is available (TODO: remember that buses are deleted and copied when they finish their trip)
		if (bus->is_idle() && bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id())
		{
			if (!dispatch_trip(eventlist, trip))
				return false;
			else
				return true;
		}
		else
		{
			DEBUG_MSG_V("Bus is unavailable for matched trip! Figure out why!");
			abort();
		}
	}

	return false;
}


//ControlCenter
ControlCenter::ControlCenter(Eventlist* eventlist, int id, int tg_strategy, int tvm_strategy, int vd_strategy, QObject* parent)
	: QObject(parent), vd_(eventlist), id_(id), tg_strategy_(tg_strategy), tvm_strategy_(tvm_strategy), vd_strategy_(vd_strategy)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes
	DEBUG_MSG("Constructing CC" << id_);

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tvm_.setMatchingStrategy(tvm_strategy);	//set initial matching strategy of BustripVehicleMatcher
	vd_.setDispatchingStrategy(vd_strategy); //set initial dispatching strategy of VehicleDispatcher
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
	vd_.reset(vd_strategy_);

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
	//ok = QObject::connect(this, &ControlCenter::fleetStateChanged, this, &ControlCenter::requestTrip, Qt::DirectConnection);
	//assert(ok);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	ok = QObject::connect(this, &ControlCenter::tripGenerated, this, &ControlCenter::on_tripGenerated, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &ControlCenter::tripGenerated, this, &ControlCenter::matchVehiclesToTrips, Qt::DirectConnection);
	assert(ok);

	//Triggers to schedule vehicle - trip pairs via VehicleDispatcher
	ok = QObject::connect(this, &ControlCenter::tripVehicleMatchFound, this, &ControlCenter::on_tripVehicleMatchFound, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &ControlCenter::tripVehicleMatchFound, this, &ControlCenter::dispatchMatchedTrips, Qt::DirectConnection);
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
	assert(pass);
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) != 0);

	connectedPass_.erase(connectedPass_.find(pid));
	bool ok = QObject::disconnect(pass, &Passenger::sendRequest, this, &ControlCenter::recieveRequest);
	assert(ok);
	ok = QObject::disconnect(pass, &Passenger::boardedBus, this, &ControlCenter::removeRequest);
	assert(ok);
}

void ControlCenter::connectVehicle(Bus* transitveh)
{
	assert(transitveh);
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
	assert(transitveh);
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) != 0); //only disconnect vehicles that have been added

	connectedVeh_.erase(connectedVeh_.find(bvid));
	bool ok = QObject::disconnect(transitveh, &Bus::stateChanged, this, &ControlCenter::updateFleetState);
	assert(ok);
}

void ControlCenter::addServiceRoute(Busline* line)
{
	assert(line->is_flex_line()); //only flex lines should be added to control center for now
	tg_.addServiceRoute(line);
}

void ControlCenter::addVehicleToServiceRoute(int line_id, Bus* transitveh)
{
	tvm_.addVehicleToServiceRoute(line_id, transitveh);
}

//Slot implementations
void ControlCenter::recieveRequest(Request req, double time)
{
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

void ControlCenter::on_tripVehicleMatchFound(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Vehicle - Trip match found at time " << time);
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

void ControlCenter::matchVehiclesToTrips(double time)
{
	if(tvm_.matchVehiclesToTrips(tg_, time))
		emit tripVehicleMatchFound(time);
}

void ControlCenter::dispatchMatchedTrips(double time)
{
	vd_.dispatchMatchedTrips(tvm_, time);
}
