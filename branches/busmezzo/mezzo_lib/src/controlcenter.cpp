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
	DEBUG_MSG("Passenger request " << req.pass_id << " at time " << req.time << " already exists in request set!");
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
		DEBUG_MSG_V("removeRequest for pass id " << pass_id << " failed. Passenger not found in requestSet.");
}

//BustripGenerator
BustripGenerator::BustripGenerator(TripGenerationStrategy* generationStrategy) : generationStrategy_(generationStrategy)
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
	for (Bustrip* trip : unmatchedTrips_) //clean up planned trips that were never matched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	unmatchedTrips_.clear();
	setTripGenerationStrategy(generation_strategy_type);
}

void BustripGenerator::addServiceRoute(Busline * line)
{
	serviceRoutes_.push_back(line);
}

void BustripGenerator::cancelUnmatchedTrip(Bustrip* trip)
{
	assert(trip->driving_roster.empty());
	if (unmatchedTrips_.count(trip) != 0)
	{
		delete trip;
		unmatchedTrips_.erase(trip);
	} 
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, double time)
{
	if (generationStrategy_)
	{
		return generationStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, time, unmatchedTrips_); //returns true if trip has been generated and added to the unmatchedTrips_
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

//TripGenerationStrategy
map<pair<int, int>, int> TripGenerationStrategy::countRequestsPerOD(const set<Request>& requestSet) const
{
	map<pair<int, int>, int> odcounts;

	for (const Request& req : requestSet)
	{
		int oid = req.ostop_id;
		int did = req.dstop_id;

		++odcounts[make_pair(oid, did)];
	}
	return odcounts;
}

bool TripGenerationStrategy::line_exists_in_tripset(const set<Bustrip*>& tripSet, const Busline* line) const
{
	assert(line);
	int lineid = line->get_id();
	set<Bustrip*>::iterator it = find_if(tripSet.begin(), tripSet.end(), 
		[lineid](Bustrip* trip) 
		{
			return trip->get_line()->get_id() == lineid;
		}
	);

	if (it != tripSet.end())
		return true;

	return false;
}

vector<Busline*> TripGenerationStrategy::get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const
{
	vector<Busline*> lines_between_stops; // lines between stops given as input
	if (!lines.empty())
	{
		for (Busline* line : lines)
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
Bustrip* TripGenerationStrategy::create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& desired_schedule) const
{
	assert(line);
	int trip_id; 
	Bustrip* trip;

	trip_id = line->generate_new_trip_id(); //get new trip id for this line
	assert(line->is_unique_tripid(trip_id));

	trip = new Bustrip(trip_id, desired_dispatch_time, line); //create trip

	//initialize trip
	trip->add_stops(desired_schedule); //add scheduled stop visits to trip
	trip->convert_stops_vector_to_map(); //TODO: not sure why this is necessary but is done for other trips so
	trip->set_last_stop_visited(trip->stops.front()->first);  //sets last stop visited to the origin stop of the trip
	trip->set_flex_trip(true);
	return trip;

}

vector<Visit_stop*> TripGenerationStrategy::create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const
{
	assert(init_dispatch_time >= 0);
	vector<Visit_stop*> schedule;
	double arrival_time_at_stop = init_dispatch_time;

	//add init_dispatch_time to all stop deltas for schedule
	for (const pair<Busstop*, double>& stop_delta : time_between_stops)
	{
		arrival_time_at_stop += stop_delta.second;
		Visit_stop* vs = new Visit_stop((stop_delta.first), arrival_time_at_stop);
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
		if (requestSet.size() % 10 == 0) //generate a trip every ten requests in the requestSet
		{
			DEBUG_MSG("------------Trip Generating Naively!-------------");
			//find od pair with the highest frequency in requestSet
			map<pair<int, int>, int> odcounts = countRequestsPerOD(requestSet);
			typedef pair<pair<int,int>,int> od_count;
			vector<od_count> sortedODcounts;

			for (auto it = odcounts.begin(); it != odcounts.end(); ++it)
			{
				sortedODcounts.push_back(*it);
			}
			assert(!sortedODcounts.empty());
			sort(sortedODcounts.begin(), sortedODcounts.end(), 
				[](const od_count& p1, const od_count& p2)->bool {return p1.second > p2.second; });

			/*attempt to generate trip for odpair with largest demand, 
			if candidate lines connecting this od stop pair already have an unmatched trip existing for them 
			continue to odpair with second highest demand ... etc.*/
			for (const od_count& od : sortedODcounts)
			{
				Busline* line = nullptr; //line that connects odpair without an unmatched trip for it yet
				int ostop_id;
				int dstop_id;

				ostop_id = od.first.first;
				dstop_id = od.first.second;

				vector<Busline*> lines_between_stops;
				lines_between_stops = get_lines_between_stops(candidateServiceRoutes, ostop_id, dstop_id); //check if any candidate service route connects the OD pair (even for segments of the route)

				bool found = false; //true only if one of the candidate lines that connects the od stop pair does not have a trip in the tripSet yet
				for (Busline* candidateLine : lines_between_stops)//if all lines have a trip already planned for them without a vehicle then we have saturated planned trips for this od stop pair
				{
					if (!line_exists_in_tripset(tripSet, candidateLine))
					{
						line = candidateLine;
						found = true;
						break;
					}
				}

				if (found)
				{
					assert(line);
					DEBUG_MSG("Trip found! Generating trip for line " << line->get_id());

					vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
					Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
					tripSet.insert(newtrip);//add this trip to the tripSet
					return true;
				}
			}
			DEBUG_MSG("No trip found!");
		}
	}
	
	return false; 
}

//BustripVehicleMatcher
BustripVehicleMatcher::BustripVehicleMatcher(MatchingStrategy* matchingStrategy): matchingStrategy_(matchingStrategy)
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
	for (Bustrip* trip : matchedTrips_) //clean up matched trips that were not dispatched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	matchedTrips_.clear();
	candidateVehicles_per_SRoute_.clear();
	setMatchingStrategy(matching_strategy_type);
}

void BustripVehicleMatcher::addVehicleToServiceRoute(int line_id, Bus* transitveh)
{
	assert(transitveh);
	candidateVehicles_per_SRoute_[line_id].insert(transitveh);
}

void BustripVehicleMatcher::removeVehicleFromServiceRoute(int line_id, Bus * transitveh)
{
	assert(transitveh);
	set<Bus*> candidateVehicles = candidateVehicles_per_SRoute_[line_id];
	if (candidateVehicles.count(transitveh) != 0)
		candidateVehicles_per_SRoute_[line_id].erase(transitveh);
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
	//DEBUG_MSG("BustripVehicleMatcher is matching trips to vehicles at time " << time);
	if (matchingStrategy_)
	{
		bool matchfound = false;
		for (set<Bustrip*>::iterator it = tg.unmatchedTrips_.begin(); it != tg.unmatchedTrips_.end();) //attempt to match all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(trip, candidateVehicles_per_SRoute_, time, matchedTrips_))
			{
				matchfound = true;
				it = tg.unmatchedTrips_.erase(it);
				matchedTrips_.insert(trip);
			}
			else
				++it;
		}
		return matchfound;
	}
	return false;
}

//MatchingStrategy
void MatchingStrategy::assign_idlevehicle_to_trip(Bus* veh, Bustrip* trip, double starttime)
{
	assert(veh);
	assert(trip);
	assert(!veh->get_curr_trip()); //this particular bus instance (remember there may be copies of it if there is a trip chain, should not have a trip)
	assert(veh->is_idle());

	DEBUG_MSG("---Assigning vehicle " << veh->get_bus_id() << " to trip " << trip->get_id() << "---");

	trip->set_busv(veh); //assign bus to the trip
	veh->set_curr_trip(trip); //assign trip to the bus
	trip->set_bustype(veh->get_bus_type());//set the bustype of the trip (so trip has access to this bustypes dwell time function)
	trip->get_busv()->set_on_trip(true); //flag the bus as busy for all trips except the first on its chain (TODO might move to dispatcher)

	vector<Start_trip*> driving_roster; //contains all other trips in this trips chain (if there are any) (TODO might move to dispatcher)
	Start_trip* st = new Start_trip(trip, starttime);

	driving_roster.push_back(st);
	trip->add_trips(driving_roster); //save the driving roster at the trip level to conform to interfaces of Busline::execute, Bustrip::activate etc. 
	double delay = starttime - trip->get_starttime();
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

Bustrip* MatchingStrategy::find_most_recent_trip(const set<Bustrip*>& trips) const
{
	Bustrip* latest = nullptr;
	if (!trips.empty())
	{
		auto maxit = max_element(trips.begin(), trips.end(), 
			[](const Bustrip* t1, const Bustrip* t2)->bool
				{
					return t1->get_starttime() < t2->get_starttime();
				}
			);
		latest = *maxit;
	}
	return latest;
}


bool NullMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips)
{
	Q_UNUSED(unmatchedTrip);
	Q_UNUSED(veh_per_sroute);
	Q_UNUSED(time);
	Q_UNUSED(matchedTrips);

	return false;
}

bool NaiveMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips)
{
	//attempt to match the first trip among unmatchedTrips with first idle vehicle found at the origin stop of the trip
	if (unmatchedTrip && !veh_per_sroute.empty())
	{
		DEBUG_MSG("------------Matching Naively!-------------");
		Bus* veh = nullptr; //the transit veh that we wish to match to a trip
		Busline* sroute = unmatchedTrip->get_line(); //get the line/service route of this trip
		set<Bus*> candidate_buses = veh_per_sroute[sroute->get_id()]; //get all transit vehicles that have this route in their service area

		if (!candidate_buses.empty()) //there is at least one bus serving the route of this trip
		{
			Busstop* origin_stop = unmatchedTrip->get_last_stop_visited(); //get the initial stop of the trip
			vector<pair<Bus*, double>> ua_buses_at_stop = origin_stop->get_unassigned_buses_at_stop(); 

			//check if one of the unassigned transit vehicles at the origin stop is currently serving the route of the trip
			for (const pair<Bus*, double>& ua_bus : ua_buses_at_stop)
			{
				set<Bus*>::iterator c_bus_it;
				c_bus_it = candidate_buses.find(ua_bus.first); //first bus in list should be the bus that arrived to the stop earliest

				if (c_bus_it != candidate_buses.end()) //a bus match has been found
				{
					DEBUG_MSG("Trip - vehicle match found!");
					veh = (*c_bus_it);
					assign_idlevehicle_to_trip(veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

					return true;
				}
			}
		}
		DEBUG_MSG("No trip - vehicle match found!");
	}
	return false;
}

//VehicleDispatcher
VehicleDispatcher::VehicleDispatcher(Eventlist* eventlist, DispatchingStrategy* dispatchingStrategy) : eventlist_(eventlist), dispatchingStrategy_(dispatchingStrategy)
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

//DispatchingStrategy
bool DispatchingStrategy::dispatch_trip(Eventlist* eventlist, Bustrip* trip)
{
	assert(eventlist);
	assert(trip);

	Busline* line = trip->get_line();
	double starttime = trip->get_starttime();
	assert(line->is_flex_line());
	DEBUG_MSG("DispatchingStrategy::dispatch_trip is dispatching trip " << trip->get_id() << " with start time " << starttime);
	
	line->add_flex_trip(trip); //add trip as a flex trip of line for bookkeeping
	line->add_trip(trip, starttime); //insert trip into the main trips list of the line
	eventlist->add_event(starttime, line); //book the activation of this trip in the eventlist
	return true;
}

bool NullDispatching::calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time)
{
    Q_UNUSED(eventlist);
    Q_UNUSED(unscheduledTrips);
    Q_UNUSED(time);

    return false;
}


bool NaiveDispatching::calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time)
{
	Q_UNUSED(time);
	assert(eventlist);
	if (!unscheduledTrips.empty())
	{
		Bustrip* trip = (*unscheduledTrips.begin());
		Bus* bus = trip->get_busv();
		//check if the bus associated with this trip is available
		if (bus->is_idle() && bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id())
		{
			if (!dispatch_trip(eventlist, trip))
				return false;
			else
			{
				unscheduledTrips.erase(trip); //trip is now scheduled for dispatch
				return true;
			}
		}
		else
		{
			DEBUG_MSG_V("Bus is unavailable for matched trip! Figure out why!");
			abort();
		}
	}

	return false;
}


//Controlcenter
Controlcenter::Controlcenter(Eventlist* eventlist, int id, int tg_strategy, int tvm_strategy, int vd_strategy, QObject* parent)
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
Controlcenter::~Controlcenter()
{
	DEBUG_MSG("Destroying CC" << id_);
}

void Controlcenter::reset()
{
	disconnect(this, 0, 0, 0); //Disconnect all signals of Controlcenter

	connectInternal(); //reconnect internal signal slots

	//Reset all process classes
	rh_.reset();
	tg_.reset(tg_strategy_); 
	tvm_.reset(tvm_strategy_);
	vd_.reset(vd_strategy_);

	//Clear/reset all members of Controlcenter
	for (pair<Bus*, Bustrip*> vehtrip : completedVehicleTrips_)
	{
		//TODO: add to vehicle recycler instead of deleting, maybe add a trips recycler as well
		if (initialVehicles_.count(vehtrip.first) == 0) //do not delete the initial buses
		{
			delete vehtrip.first;
		}
		else
		{
			vehtrip.first->reset(); //reset initial vehicles instead of deleting
		}
		delete vehtrip.second;
	}
	completedVehicleTrips_.clear();
	
	for (pair<int, Bus*> veh : connectedVeh_)
	{
		if (initialVehicles_.count(veh.second) == 0) //do not delete the initial buses
		{
			delete veh.second;
		}
		else
		{
			veh.second->reset(); //reset initial vehicles instead of deleting
		}
	}
	
	connectedVeh_.clear();
	connectedPass_.clear();

}

void Controlcenter::connectInternal()
{
	bool ok;
	ok = QObject::connect(this, &Controlcenter::requestRejected, this, &Controlcenter::on_requestRejected, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::on_requestAccepted, Qt::DirectConnection);
	assert(ok);

	//Triggers to generate trips via BustripGenerator
	ok = QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::requestTrip, Qt::DirectConnection); 
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::requestTrip, Qt::DirectConnection);
	assert(ok);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	ok = QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::on_tripGenerated, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection);
	assert(ok);

	//Triggers to schedule vehicle - trip pairs via VehicleDispatcher
	ok = QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::on_tripVehicleMatchFound, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::dispatchMatchedTrips, Qt::DirectConnection);
	assert(ok);
}

void Controlcenter::connectPassenger(Passenger* pass)
{
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) == 0); //passenger should only be added once
	connectedPass_[pid] = pass;

	if (!QObject::connect(pass, &Passenger::sendRequest, this, &Controlcenter::receiveRequest, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Passenger::sendRequest to Controlcenter::receiveRequest failed!");
		abort();
	}
	if (!QObject::connect(pass, &Passenger::boardedBus, this, &Controlcenter::removeRequest, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Passenger::boardedBus to Controlcenter::removeRequest failed!");
		abort();
	}
}
void Controlcenter::disconnectPassenger(Passenger* pass)
{
	assert(pass);
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) != 0);

	connectedPass_.erase(connectedPass_.find(pid));
	bool ok = QObject::disconnect(pass, &Passenger::sendRequest, this, &Controlcenter::receiveRequest);
	assert(ok);
	ok = QObject::disconnect(pass, &Passenger::boardedBus, this, &Controlcenter::removeRequest);
	assert(ok);
}

void Controlcenter::connectVehicle(Bus* transitveh)
{
	assert(transitveh);
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) == 0); //vehicle should only be added once
	connectedVeh_[bvid] = transitveh;

	//connect bus state changes to control center
	if (!QObject::connect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState, Qt::DirectConnection))
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Bus::stateChanged with Controlcenter::updateFleetState failed!");
		abort();
	}
}
void Controlcenter::disconnectVehicle(Bus* transitveh)
{
	assert(transitveh);
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) != 0); //only disconnect vehicles that have been added

	connectedVeh_.erase(connectedVeh_.find(bvid));
	bool ok = QObject::disconnect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState);
	assert(ok);
}

void Controlcenter::addServiceRoute(Busline* line)
{
	assert(line->is_flex_line()); //only flex lines should be added to control center for now
	tg_.addServiceRoute(line);
}

void Controlcenter::addVehicleToServiceRoute(int line_id, Bus* transitveh)
{
	assert(transitveh);
	tvm_.addVehicleToServiceRoute(line_id, transitveh);
}

void Controlcenter::removeVehicleFromServiceRoute(int line_id, Bus* transitveh)
{
	assert(transitveh);
	tvm_.removeVehicleFromServiceRoute(line_id, transitveh);
}

void Controlcenter::addInitialVehicle(Bus * transitveh)
{
	assert(transitveh);
	if (initialVehicles_.count(transitveh) == 0)
	{
		initialVehicles_.insert(transitveh);
	}
}

void Controlcenter::addCompletedVehicleTrip(Bus * transitveh, Bustrip * trip)
{
	assert(transitveh);
	assert(trip);
	completedVehicleTrips_.push_back(make_pair(transitveh, trip));
}

//Slot implementations
void Controlcenter::receiveRequest(Request req, double time)
{
	assert(req.time >= 0 && req.load > 0); //assert that request is valid
	rh_.addRequest(req) ? emit requestAccepted(time) : emit requestRejected(time);
}

void Controlcenter::removeRequest(int pass_id)
{
	DEBUG_MSG(Q_FUNC_INFO);
	rh_.removeRequest(pass_id);
}

void Controlcenter::on_requestAccepted(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Accepted at time " << time);
}
void Controlcenter::on_requestRejected(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Request Rejected at time " << time);
}

void Controlcenter::on_tripGenerated(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Trip Generated at time " << time);
}

void Controlcenter::on_tripVehicleMatchFound(double time)
{
	DEBUG_MSG(Q_FUNC_INFO << ": Vehicle - Trip match found at time " << time);
}

void Controlcenter::updateFleetState(int bus_id, BusState newstate, double time)
{
	if (newstate == BusState::IdleEmpty)
	{
		//Ugly solution! TODO: Maybe add an additional state 'OnCall' to BusStates. add_unassigned_vehicle in charge of setting vehicle state to OnCall
		assert(connectedVeh_.count(bus_id) != 0);
		Bus* bus = connectedVeh_[bus_id]; //check if bus has arrived unassigned to a stop
		Busstop* stop = bus->get_last_stop_visited();
		vector<pair<Bus*, double>> ua_at_stop = stop->get_unassigned_buses_at_stop();
		vector<pair<Bus*, double>>::iterator it;
		it = find_if(ua_at_stop.begin(), ua_at_stop.end(), [bus_id](const pair<Bus*, double>& ua_bus)->bool {return ua_bus.first->get_bus_id() == bus_id; });
		if(it != ua_at_stop.end())
			emit newUnassignedVehicle(time);
	}
}

void Controlcenter::requestTrip(double time)
{
	if (tg_.requestTrip(rh_, time))
		emit tripGenerated(time);
}

void Controlcenter::matchVehiclesToTrips(double time)
{
	if(tvm_.matchVehiclesToTrips(tg_, time))
		emit tripVehicleMatchFound(time);
}

void Controlcenter::dispatchMatchedTrips(double time)
{
	vd_.dispatchMatchedTrips(tvm_, time);
}
