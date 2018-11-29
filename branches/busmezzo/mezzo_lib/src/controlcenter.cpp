#include "controlstrategies.h"
#include "passenger.h"
#include "vehicle.h"
#include "controlcenter.h"

#include <algorithm>
#include <assert.h>

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

bool RequestHandler::addRequest(const Request req, const set<Busstop*>& serviceArea)
{
    if (!isFeasibleRequest(req, serviceArea))
    {
        DEBUG_MSG("INFO::RequestHandler::addRequest : rejecting request to travel between origin stop " << req.ostop_id << 
                    " and destination stop " << req.dstop_id << ", destination stop not reachable within service area");
        return false;
    }

	if (find(requestSet_.begin(), requestSet_.end(), req) == requestSet_.end()) //if request does not already exist in set (which it shouldn't)
	{
		requestSet_.insert(req);
		return true;
	}
	DEBUG_MSG("DEBUG: RequestHandler::addRequest : passenger request " << req.pass_id << " at time " << req.time << " already exists in request set!");
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
		DEBUG_MSG_V("DEBUG: RequestHandler::removeRequest : Passenger id " << pass_id << " not found in requestSet.");
}

bool RequestHandler::isFeasibleRequest(const Request& req, const set<Busstop*>& serviceArea) const
{    
    //check if origin stop and destination stop are the same
    if (req.ostop_id == req.dstop_id)
        return false;
    //check if destination stop of request is within serviceArea
    auto it = find_if(serviceArea.begin(), serviceArea.end(), [req](const Busstop* stop) -> bool {return stop->get_id() == req.dstop_id; });
    if (it == serviceArea.end())
        return false;
    //check if origin stop of request is within serviceArea
    it = find_if(serviceArea.begin(), serviceArea.end(), [req](const Busstop* stop) -> bool {return stop->get_id() == req.ostop_id; });
    if (it == serviceArea.end())
        return false;

    return true;

}

//BustripGenerator
BustripGenerator::BustripGenerator(Network* theNetwork, TripGenerationStrategy* generationStrategy, TripGenerationStrategy* emptyVehicleStrategy) :
                                  generationStrategy_(generationStrategy), emptyVehicleStrategy_(emptyVehicleStrategy), theNetwork_(theNetwork)
{
	DEBUG_MSG("Constructing TG");
}
BustripGenerator::~BustripGenerator()
{
	DEBUG_MSG("Destroying TG");
	if(generationStrategy_)
		delete generationStrategy_;
	if (emptyVehicleStrategy_)
		delete emptyVehicleStrategy_;
}

void BustripGenerator::reset(int generation_strategy_type, int empty_vehicle_strategy_type)
{
	for (Bustrip* trip : unmatchedTrips_) //clean up planned trips that were never matched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	unmatchedTrips_.clear();
	setTripGenerationStrategy(generation_strategy_type);
	setEmptyVehicleStrategy(empty_vehicle_strategy_type);
}

void BustripGenerator::addServiceRoute(Busline* line)
{
    assert(line);
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

void BustripGenerator::cancelRebalancingTrip(Bustrip* trip)
{
	assert(trip->driving_roster.empty());
	if (unmatchedRebalancingTrips_.count(trip) != 0)
	{
		delete trip;
		unmatchedRebalancingTrips_.erase(trip);
	}
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	if (generationStrategy_)
	{
        bool trip_found = generationStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedTrips_); //returns true if trip has been generated and added to the unmatchedTrips_

        if (!trip_found && !unmatchedTrips_.empty()) //if no trip was found but an unmatched trip remains in the unmatchedTrips set
            trip_found = true;

        return trip_found;
	}
	return false;
}

bool BustripGenerator::requestRebalancingTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	if (emptyVehicleStrategy_)
	{
		return emptyVehicleStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedRebalancingTrips_); //returns true if trip has been generated and added to the unmatchedRebalancingTrips_
	}
	return false;
}

void BustripGenerator::setTripGenerationStrategy(int type)
{
	if (generationStrategy_)
		delete generationStrategy_;

	DEBUG_MSG("Changing trip generation strategy to " << type);
	if (type == generationStrategyType::Null)
		generationStrategy_ = new NullTripGeneration();
	else if (type == generationStrategyType::Naive)
		generationStrategy_ = new NaiveTripGeneration();
	else
	{
		DEBUG_MSG("This generation strategy is not recognized!");
		generationStrategy_ = nullptr;
	}
}

void BustripGenerator::setEmptyVehicleStrategy(int type)
{
	if (emptyVehicleStrategy_)
		delete emptyVehicleStrategy_;

	DEBUG_MSG("Changing empty vehicle strategy to " << type);
	if (type == emptyVehicleStrategyType::EVNull)
		emptyVehicleStrategy_ = new NullTripGeneration();
	else if (type == emptyVehicleStrategyType::EVNaive)
	{
		if (!theNetwork_)
		{
			DEBUG_MSG_V("Problem with BustripGenerator::setEmptyVehicleStrategy - switching to NearestLongestQueue strategy failed due to theNetwork nullptr. Aborting...");
			abort();
		}
		emptyVehicleStrategy_ = new NaiveEmptyVehicleTripGeneration(theNetwork_);
	}
	else
	{
		DEBUG_MSG("This empty vehicle strategy is not recognized!");
		emptyVehicleStrategy_ = nullptr;
	}
}

//BustripVehicleMatcher
BustripVehicleMatcher::BustripVehicleMatcher(MatchingStrategy* matchingStrategy): matchingStrategy_(matchingStrategy)
{
	DEBUG_MSG("Constructing TVM");
}
BustripVehicleMatcher::~BustripVehicleMatcher()
{
	DEBUG_MSG("Destroying TVM");
	if(matchingStrategy_)
		delete matchingStrategy_;
}

void BustripVehicleMatcher::reset(int matching_strategy_type)
{
	for (Bustrip* trip : matchedTrips_) //clean up matched trips that were not dispatched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	matchedTrips_.clear();
	vehicles_per_service_route.clear();
	setMatchingStrategy(matching_strategy_type);
}

void BustripVehicleMatcher::addVehicleToAllServiceRoutes(const BustripGenerator& tg, Bus* transitveh)
{
    assert(transitveh);
    for (const Busline* line : tg.serviceRoutes_)
    {
        addVehicleToServiceRoute(line->get_id(), transitveh);
    }
}

void BustripVehicleMatcher::addVehicleToServiceRoute(int line_id, Bus* transitveh)
{
	assert(transitveh);
    //TODO need to add check for when line_id that is not included as a service route, maybe move service routes to Controlcenter rather than BustripGenerator
    if (vehicles_per_service_route[line_id].count(transitveh) == 0)
    {
        vehicles_per_service_route[line_id].insert(transitveh);
        transitveh->add_sroute_id(line_id); //vehicle is also aware of its service routes for now. TODO: probably this is a bit unnecessary at the moment, since the vehicle also knows of its CC
    }
}

void BustripVehicleMatcher::removeVehicleFromServiceRoute(int line_id, Bus * transitveh)
{
	assert(transitveh);
	set<Bus*> candidateVehicles = vehicles_per_service_route[line_id];
    if (candidateVehicles.count(transitveh) != 0)
    {
        vehicles_per_service_route[line_id].erase(transitveh);
        transitveh->remove_sroute_id(line_id); //vehicle is also aware of its service routes for now. TODO: probably this is a bit unnecessary at the moment, since the vehicle also knows of its CC
    }
}

void BustripVehicleMatcher::setMatchingStrategy(int type)
{
	if (matchingStrategy_)
		delete matchingStrategy_;

	DEBUG_MSG("Changing trip - vehicle matching strategy to " << type);
	if (type == matchingStrategyType::Null)
		matchingStrategy_ = new NullMatching();
	else if (type == matchingStrategyType::Naive)
		matchingStrategy_ = new NaiveMatching();
	else
	{
		DEBUG_MSG("This matching strategy is not recognized!");
		matchingStrategy_ = nullptr;
	}
}

bool BustripVehicleMatcher::matchVehiclesToTrips(BustripGenerator& tg, double time)
{
	if (matchingStrategy_)
	{
		bool matchfound = false;
		for (set<Bustrip*>::iterator it = tg.unmatchedTrips_.begin(); it != tg.unmatchedTrips_.end();) //attempt to match vehicles to all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(trip, vehicles_per_service_route, time, matchedTrips_))
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

bool BustripVehicleMatcher::matchVehiclesToEmptyVehicleTrips(BustripGenerator& tg, double time)
{
	DEBUG_MSG("BustripVehicleMatcher is matching empty vehicle trips to vehicles at time " << time);
	if (matchingStrategy_)
	{
		bool matchfound = false;
		for (set<Bustrip*>::iterator it = tg.unmatchedRebalancingTrips_.begin(); it != tg.unmatchedRebalancingTrips_.end();) //attempt to match all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(trip, vehicles_per_service_route, time, matchedTrips_))
			{
				matchfound = true;
				it = tg.unmatchedRebalancingTrips_.erase(it);
				matchedTrips_.insert(trip);
			}
			else
				++it;
		}
		return matchfound;
	}
	return false;
}

//VehicleScheduler
VehicleScheduler::VehicleScheduler(Eventlist* eventlist, SchedulingStrategy* schedulingStrategy) : eventlist_(eventlist), schedulingStrategy_(schedulingStrategy)
{
	DEBUG_MSG("Constructing VS");
}
VehicleScheduler::~VehicleScheduler()
{
	DEBUG_MSG("Destroying VS");
	if(schedulingStrategy_)
		delete schedulingStrategy_;
}

void VehicleScheduler::reset(int scheduling_strategy_type)
{
	setSchedulingStrategy(scheduling_strategy_type);
}

bool VehicleScheduler::scheduleMatchedTrips(BustripVehicleMatcher& tvm, double time)
{
	if (schedulingStrategy_)
	{
		return schedulingStrategy_->schedule_trips(eventlist_, tvm.matchedTrips_, time);
	}

	return false;
}

void VehicleScheduler::setSchedulingStrategy(int scheduling_strategy_type)
{
	if (schedulingStrategy_)
		delete schedulingStrategy_;

	DEBUG_MSG("Changing scheduling strategy to " << scheduling_strategy_type);
	if (scheduling_strategy_type == schedulingStrategyType::Null)
		schedulingStrategy_ = new NullScheduling();
	else if (scheduling_strategy_type == schedulingStrategyType::Naive)
		schedulingStrategy_ = new NaiveScheduling();
	else
	{
		DEBUG_MSG("This scheduling strategy is not recognized!");
		schedulingStrategy_ = nullptr;
	}
}

//Controlcenter
Controlcenter::Controlcenter(Eventlist* eventlist, Network* theNetwork, int id, int tg_strategy, int ev_strategy, int tvm_strategy, int vs_strategy, QObject* parent)
    : QObject(parent), id_(id), tg_strategy_(tg_strategy), ev_strategy_(ev_strategy), tvm_strategy_(tvm_strategy), vs_strategy_(vs_strategy), tg_(theNetwork), vs_(eventlist)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes
	DEBUG_MSG("Constructing CC" << id_);

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tg_.setEmptyVehicleStrategy(ev_strategy); //set the initial empty vehicle strategy of BustripGenerator
	tvm_.setMatchingStrategy(tvm_strategy);	//set initial matching strategy of BustripVehicleMatcher
	vs_.setSchedulingStrategy(vs_strategy); //set initial scheduling strategy of VehicleScheduler
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
	tg_.reset(tg_strategy_, ev_strategy_); 
	tvm_.reset(tvm_strategy_);
	vs_.reset(vs_strategy_);

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
	fleetState_.clear();
}

void Controlcenter::connectInternal()
{
	//Note: the order in which the signals are connected to the slots matters! For example when newUnassignedVehicle is signaled, requestTrip will be called before matchVehiclesToTrips
	//signal slots for debug messages TODO: remove later
	bool ok;
	ok = QObject::connect(this, &Controlcenter::requestRejected, this, &Controlcenter::on_requestRejected, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::on_requestAccepted, Qt::DirectConnection);
	assert(ok);
    ok = QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::on_tripVehicleMatchFound, Qt::DirectConnection);
    assert(ok);

	//Triggers to generate trips via BustripGenerator
	ok = QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::requestTrip, Qt::DirectConnection); 
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::requestTrip, Qt::DirectConnection);
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::tripVehicleMatchNotFound, this, &Controlcenter::requestRebalancingTrip, Qt::DirectConnection);
	assert(ok);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	ok = QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::on_tripGenerated, Qt::DirectConnection); //currently used for debugging only
	assert(ok);
	ok = QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection);
	assert(ok);
	//ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection); //removed to avoid double call to matchVehicleToTrips
	//assert(ok);
	ok = QObject::connect(this, &Controlcenter::emptyVehicleTripGenerated, this, &Controlcenter::matchEmptyVehiclesToTrips, Qt::DirectConnection);
	assert(ok);

	//Triggers to schedule vehicle - trip pairs via VehicleScheduler
	ok = QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::scheduleMatchedTrips, Qt::DirectConnection);
	assert(ok);
}

set<Busstop*> Controlcenter::getServiceArea() const { return serviceArea_; }

bool Controlcenter::isInServiceArea(Busstop* stop) const
{
    if(serviceArea_.count(stop) != 0)
        return true;
    return false;
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

    transitveh->set_control_center(this);
}
void Controlcenter::disconnectVehicle(Bus* transitveh)
{
	assert(transitveh);
	int bvid = transitveh->get_bus_id();
	assert(connectedVeh_.count(bvid) != 0); //only disconnect vehicles that have been added

	connectedVeh_.erase(connectedVeh_.find(bvid));
	bool ok = QObject::disconnect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState);
	assert(ok);

    transitveh->set_control_center(nullptr);
}

void Controlcenter::addStopToServiceArea(Busstop* stop)
{
    assert(stop);
    serviceArea_.insert(stop);
}

void Controlcenter::addServiceRoute(Busline* line)
{
	assert(line->is_flex_line()); //only flex lines should be added to control center for now
	tg_.addServiceRoute(line);
}

void Controlcenter::addVehicleToAllServiceRoutes(Bus* transitveh)
{
    assert(transitveh);
    tvm_.addVehicleToAllServiceRoutes(tg_, transitveh);
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

void Controlcenter::addInitialVehicle(Bus* transitveh)
{
	assert(transitveh);
	if (initialVehicles_.count(transitveh) == 0)
	{
		initialVehicles_.insert(transitveh);
	}
}

void Controlcenter::addCompletedVehicleTrip(Bus* transitveh, Bustrip * trip)
{
	assert(transitveh);
	assert(trip);
    assert(trip->get_next_stop() == trip->stops.end()); //currently a trip is considered completed via Bustrip::advance_next_stop -> Bus::advance_curr_trip
	completedVehicleTrips_.push_back(make_pair(transitveh, trip));
}

//Slot implementations
void Controlcenter::receiveRequest(Request req, double time)
{
	assert(req.desired_departure_time >= 0 && req.time >= 0 && req.load > 0); //assert that request is valid
	rh_.addRequest(req, serviceArea_) ? emit requestAccepted(time) : emit requestRejected(time);
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

void Controlcenter::updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time)
{
	assert(bus->get_state() == newstate); //the newstate should be the current state of the bus
	assert(bus->is_flex_vehicle()); //fixed vehicles do not currently have a control center
	assert(connectedVeh_.count(bus->get_bus_id()) != 0); //assert that bus is connected to this control center (otherwise state change signal should have never been heard)

	//update the fleet state map, vehicles should be null before they are available, and null when they finish a trip and are copied
	if(oldstate != BusState::Null)
		fleetState_[oldstate].erase(bus);
	if(newstate != BusState::Null)
		fleetState_[newstate].insert(bus);

	if (newstate == BusState::OnCall)
	{
		emit newUnassignedVehicle(time);
	}
}

void Controlcenter::requestTrip(double time)
{
	if (tg_.requestTrip(rh_, fleetState_, time))
		emit tripGenerated(time);
}

void Controlcenter::requestRebalancingTrip(double time)
{
	if (tg_.requestRebalancingTrip(rh_, fleetState_, time))
		emit emptyVehicleTripGenerated(time);
}

void Controlcenter::matchVehiclesToTrips(double time)
{
	tvm_.matchVehiclesToTrips(tg_, time) ? emit tripVehicleMatchFound(time) : emit tripVehicleMatchNotFound(time);
}

void Controlcenter::matchEmptyVehiclesToTrips(double time)
{
	if (tvm_.matchVehiclesToEmptyVehicleTrips(tg_, time))
		emit tripVehicleMatchFound(time);
}

void Controlcenter::scheduleMatchedTrips(double time)
{
	vs_.scheduleMatchedTrips(tvm_, time);
}
