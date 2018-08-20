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

bool RequestHandler::addRequest(const Request req)
{
	if (find(requestSet_.begin(), requestSet_.end(), req) == requestSet_.end()) //if request does not already exist in set (which it shouldn't)
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
BustripGenerator::BustripGenerator(TripGenerationStrategy* generationStrategy, TripGenerationStrategy* emptyVehicleStrategy) : generationStrategy_(generationStrategy), emptyVehicleStrategy_(emptyVehicleStrategy)
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

void BustripGenerator::cancelRebalancingTrip(Bustrip* trip)
{
	assert(trip->driving_roster.empty());
	if (unmatchedTripsRebalancing_.count(trip) != 0)
	{
		delete trip;
		unmatchedTripsRebalancing_.erase(trip);
	}
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	if (generationStrategy_)
	{
		return generationStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedTrips_); //returns true if trip has been generated and added to the unmatchedTrips_
	}
	return false;
}

bool BustripGenerator::requestRebalancingTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	if (emptyVehicleStrategy_)
	{
		return emptyVehicleStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedTripsRebalancing_); //returns true if trip has been generated and added to the unmatchedTripsRebalancing_
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
	else if (type == emptyVehicleStrategyType::NearestLongestQueue)
		emptyVehicleStrategy_ = new NearestLongestQueueEVTripGeneration();
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

//VehicleDispatcher
VehicleDispatcher::VehicleDispatcher(Eventlist* eventlist, DispatchingStrategy* dispatchingStrategy) : eventlist_(eventlist), dispatchingStrategy_(dispatchingStrategy)
{
	DEBUG_MSG("Constructing FD");
}
VehicleDispatcher::~VehicleDispatcher()
{
	DEBUG_MSG("Destroying FD");
	if(dispatchingStrategy_)
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

//Controlcenter
Controlcenter::Controlcenter(Eventlist* eventlist, int id, int tg_strategy, int ev_strategy, int tvm_strategy, int vd_strategy, QObject* parent)
	: QObject(parent), vd_(eventlist), id_(id), tg_strategy_(tg_strategy), ev_strategy_(ev_strategy), tvm_strategy_(tvm_strategy), vd_strategy_(vd_strategy)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes
	DEBUG_MSG("Constructing CC" << id_);

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tg_.setEmptyVehicleStrategy(ev_strategy); //set the initial empty vehicle strategy of BustripGenerator
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
	tg_.reset(tg_strategy_, ev_strategy_); 
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
	fleetState_.clear();
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

void Controlcenter::matchVehiclesToTrips(double time)
{
	if(tvm_.matchVehiclesToTrips(tg_, time))
		emit tripVehicleMatchFound(time);
}

void Controlcenter::dispatchMatchedTrips(double time)
{
	vd_.dispatchMatchedTrips(tvm_, time);
}
