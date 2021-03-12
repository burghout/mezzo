#include "controlstrategies.h"
#include "passenger.h"
#include "vehicle.h"
#include "controlcenter.h"
#include "network.h"
#include "MMath.h"

#include <algorithm>
#include <iterator>
#include <cassert>


namespace cc_helper_functions
{
	//!< Removes request from 'Bustrip::scheduled_requests' for any bustrip in driving_roster
	void removeRequestFromTripChain(const Request* req, const vector<Start_trip*>& driving_roster)
	{
		for (auto trip_dispatch : driving_roster)
		{
			trip_dispatch->first->remove_request(req);
		}
	}
}

void Controlcenter_SummaryData::reset()
{
	requests_recieved = 0;
	requests_accepted = 0;
	requests_served = 0;
	requests_rejected = 0;
}


//RequestHandler
RequestHandler::RequestHandler(){}
RequestHandler::~RequestHandler(){}

void RequestHandler::reset()
{
	for (auto req : requestSet_)
		delete req;
	requestSet_.clear();
}

bool RequestHandler::addRequest(Request* req, const set<Busstop*, ptr_less<Busstop*> >& serviceArea)
{
    if (!isFeasibleRequest(req, serviceArea))
    {
        DEBUG_MSG("INFO::RequestHandler::addRequest : rejecting request to travel between origin stop " << req->ostop_id << 
                    " and destination stop " << req->dstop_id << ", destination stop not reachable within service area");
		
		// Currently if a request is rejected it is deleted, and passengers pointer to it is reset to nullptr @todo Perhaps move this to a different owner
		req->pass_owner->set_curr_request(nullptr);
		delete req;
        
		return false;
    }

	if (find(requestSet_.begin(), requestSet_.end(), req) == requestSet_.end()) //if request does not already exist in set (which it shouldn't)
	{
		requestSet_.insert(req);
		req->set_state(RequestState::Unmatched);
		return true;
	}
    else
    {
        // Currently just ignore addRequest call if request already exists in request set
        DEBUG_MSG("DEBUG: RequestHandler::addRequest : passenger request " << req->pass_id << " at time " << req->time_request_generated << " already exists in request set!");

        //req->pass_owner->set_curr_request(nullptr);
        //delete req;

        return false;
    }
}

void RequestHandler::removeRequest(const int pass_id)
{
    set<Request*, ptr_less<Request>>::iterator it;
    it = find_if(requestSet_.begin(), requestSet_.end(),
        [pass_id](const Request* req) -> bool
        {
            return req->pass_id == pass_id;
        }
    );

    if (it != requestSet_.end())
	{
		Request* req = *it;
		req->pass_owner->set_curr_request(nullptr);
		if(req->assigned_trip != nullptr) // if request has been assigned to a trip-chain
			cc_helper_functions::removeRequestFromTripChain(req, req->assigned_trip->driving_roster); //!< remove this request from scheduled request of any trip it might be scheduled to @todo perhaps change this to a 'ServedFinished' RequestState and save deletion for later in the future
        requestSet_.erase(req);
		delete req;
	}
    else
        DEBUG_MSG_V("DEBUG: RequestHandler::removeRequest : Passenger id " << pass_id << " not found in requestSet.");
}

bool RequestHandler::isFeasibleRequest(const Request* req, const set<Busstop*, ptr_less<Busstop*>>& serviceArea) const
{    
    //check if origin stop and destination stop are the same
    if (req->ostop_id == req->dstop_id) 
        return false;

    //check if destination stop of request is within serviceArea
    auto it = find_if(serviceArea.begin(), serviceArea.end(), [req](const Busstop* stop) -> bool {return stop->get_id() == req->dstop_id; });
    if (it == serviceArea.end()) 
        return false;

    //check if origin stop of request is within serviceArea
    it = find_if(serviceArea.begin(), serviceArea.end(), [req](const Busstop* stop) -> bool {return stop->get_id() == req->ostop_id; });
    if (it == serviceArea.end()) 
        return false;

    return true;
}

//BustripGenerator
BustripGenerator::BustripGenerator(Network* theNetwork, TripGenerationStrategy* generationStrategy, TripGenerationStrategy* emptyVehicleStrategy) :
                                  generationStrategy_(generationStrategy), emptyVehicleStrategy_(emptyVehicleStrategy), theNetwork_(theNetwork)
{}

BustripGenerator::~BustripGenerator()
{
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

	for (Bustrip* trip : unmatchedRebalancingTrips_)
	{
		delete trip;
	}
	unmatchedTrips_.clear();
	unmatchedRebalancingTrips_.clear();

	setTripGenerationStrategy(generation_strategy_type);
	setEmptyVehicleStrategy(empty_vehicle_strategy_type);
}

void BustripGenerator::addServiceRoute(Busline* line)
{
    assert(line);
	serviceRoutes_.push_back(line);
}

vector<Busline*> BustripGenerator::getServiceRoutes() const
{
    return serviceRoutes_;
}

void BustripGenerator::cancelUnmatchedTrip(Bustrip* trip)
{
	if (unmatchedTrips_.count(trip) != 0)
	{
		delete trip;
		unmatchedTrips_.erase(trip);
	} 
}

void BustripGenerator::cancelRebalancingTrip(Bustrip* trip)
{
	if (unmatchedRebalancingTrips_.count(trip) != 0)
	{
		delete trip;
		unmatchedRebalancingTrips_.erase(trip);
	}
}

bool BustripGenerator::requestTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	/*qDebug() << "BustripGenerator is requesting a passenger-carrying trip at time " << time;
	qDebug() << "\tSize of requestSet: " << rh.requestSet_.size();*/
    if (generationStrategy_ != nullptr)
    {
        bool trip_found = generationStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedTrips_, unmatchedRebalancingTrips_); //returns true if trip has been generated and added to the unmatchedTrips_

        if (!trip_found && !unmatchedTrips_.empty()) //if no trip was found but an unmatched trip remains in the unmatchedTrips set
		{ 
            trip_found = true;
        }

        return trip_found;
    }
    return false;
}

bool BustripGenerator::requestRebalancingTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time)
{
	/*qDebug() << "BustripGenerator is requesting a rebalancing trip at time " << time;
	qDebug() << "\tSize of requestSet: " << rh.requestSet_.size();*/
	if (emptyVehicleStrategy_ != nullptr)
	{
		return emptyVehicleStrategy_->calc_trip_generation(rh.requestSet_, serviceRoutes_, fleetState, time, unmatchedTrips_, unmatchedRebalancingTrips_); //returns true if trip has been generated and added to the unmatchedRebalancingTrips_
	}
	return false;
}

void BustripGenerator::setTripGenerationStrategy(int type)
{
	if (generationStrategy_)
		delete generationStrategy_;

	//DEBUG_MSG("Changing trip generation strategy to " << type);
	if (type == generationStrategyType::Null) 
	{
		generationStrategy_ = new NullTripGeneration();
	} 
	else if (type == generationStrategyType::Naive) 
	{
		generationStrategy_ = new NaiveTripGeneration();
	} 
    else if (type == generationStrategyType::Simple)
        {
            generationStrategy_ = new SimpleTripGeneration();
        }

	else
	{
		DEBUG_MSG("BustripGenerator::setTripGenerationStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
		generationStrategy_ = nullptr;
	}
}

void BustripGenerator::setEmptyVehicleStrategy(int type)
{
	if (emptyVehicleStrategy_)
		delete emptyVehicleStrategy_;

	//DEBUG_MSG("Changing empty vehicle strategy to " << type);
	if (type == emptyVehicleStrategyType::EVNull) 
	{
		emptyVehicleStrategy_ = new NullTripGeneration();
	} 
	else if (type == emptyVehicleStrategyType::EVNaive)
	{
		if (theNetwork_ == nullptr)
		{
			DEBUG_MSG_V("Problem with BustripGenerator::setEmptyVehicleStrategy - switching to " << type << " strategy failed due to theNetwork nullptr. Aborting...");
			abort();
		}
		emptyVehicleStrategy_ = new NaiveEmptyVehicleTripGeneration(theNetwork_);
	}
	else if (type == emptyVehicleStrategyType::EVSimple)
	{
		if (theNetwork_ == nullptr)
		{
			DEBUG_MSG_V("Problem with BustripGenerator::setEmptyVehicleStrategy - switching to " << type << " strategy failed due to theNetwork nullptr. Aborting...");
			abort();
		}
		emptyVehicleStrategy_ = new SimpleEmptyVehicleTripGeneration(theNetwork_);
	}
	else
	{
		DEBUG_MSG("BustripGenerator::setEmptyVehicleStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
		emptyVehicleStrategy_ = nullptr;
	}
}

//BustripVehicleMatcher
BustripVehicleMatcher::BustripVehicleMatcher(MatchingStrategy* matchingStrategy): matchingStrategy_(matchingStrategy){}
BustripVehicleMatcher::~BustripVehicleMatcher()
{
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

    //DEBUG_MSG("Changing trip - vehicle matching strategy to " << type);
    if (type == matchingStrategyType::Null) 
	{
        matchingStrategy_ = new NullMatching();
    }
    else if (type == matchingStrategyType::Naive) 
	{
        matchingStrategy_ = new NaiveMatching();
    }
    else
    {
		DEBUG_MSG("BustripVehicleMatcher::setMatchingStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
        matchingStrategy_ = nullptr;
    }
}

bool BustripVehicleMatcher::matchVehiclesToTrips(BustripGenerator& tg, double time)
{
	/*qDebug() << "BustripVehicleMatcher is matching passenger carrying trips to vehicles at time " << time;
	qDebug() << "\tSize of unmatchedTrips: " << tg.unmatchedTrips_.size();*/
	if (matchingStrategy_ != nullptr)
	{
		bool matchfound = false;
		for (auto it = tg.unmatchedTrips_.begin(); it != tg.unmatchedTrips_.end();) //attempt to match vehicles to all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(trip, vehicles_per_service_route, time, matchedTrips_))
			{
				matchfound = true;
				it = tg.unmatchedTrips_.erase(it);
				matchedTrips_.insert(trip);
			}
			else 
			{
				++it;
			}
		}
		return matchfound;
	}
	return false;
}

bool BustripVehicleMatcher::matchVehiclesToEmptyVehicleTrips(BustripGenerator& tg, double time)
{
	/*qDebug() << "BustripVehicleMatcher is matching empty vehicle trips to vehicles at time " << time;
	qDebug() << "\tSize of unmatchedRebalancingTrips: " << tg.unmatchedRebalancingTrips_.size();*/
	if (matchingStrategy_ != nullptr)
	{
		bool matchfound = false;
		for (auto it = tg.unmatchedRebalancingTrips_.begin(); it != tg.unmatchedRebalancingTrips_.end();) //attempt to match all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(trip, vehicles_per_service_route, time, matchedTrips_))
			{
				matchfound = true;
				it = tg.unmatchedRebalancingTrips_.erase(it);
				matchedTrips_.insert(trip);
			}
			else 
			{
				++it;
			}
		}
		return matchfound;
	}
	return false;
}

//VehicleScheduler
VehicleScheduler::VehicleScheduler(Eventlist* eventlist, SchedulingStrategy* schedulingStrategy) : eventlist_(eventlist), schedulingStrategy_(schedulingStrategy){}
VehicleScheduler::~VehicleScheduler()
{
	if(schedulingStrategy_)
		delete schedulingStrategy_;
}

void VehicleScheduler::reset(int scheduling_strategy_type)
{
	setSchedulingStrategy(scheduling_strategy_type);
}

bool VehicleScheduler::scheduleMatchedTrips(BustripVehicleMatcher& tvm, double time)
{
	/*qDebug() << "VehicleScheduler is scheduling matched trips at time " << time;
	qDebug() << "\tSize of matchedTrips set: " << tvm.matchedTrips_.size();*/
	if (schedulingStrategy_ != nullptr)
	{
		return schedulingStrategy_->schedule_trips(eventlist_, tvm.matchedTrips_, time);
	}

	return false;
}

void VehicleScheduler::setSchedulingStrategy(int type)
{
	if (schedulingStrategy_)
		delete schedulingStrategy_;

	//DEBUG_MSG("Changing scheduling strategy to " << type);
	if (type == schedulingStrategyType::Null) 
	{
		schedulingStrategy_ = new NullScheduling();
	} 
	else if (type == schedulingStrategyType::Naive) 
	{
		schedulingStrategy_ = new NaiveScheduling();
	}
	else if (type == schedulingStrategyType::LatestDeparture)
	{
		schedulingStrategy_ = new LatestDepartureScheduling();
	}
	else
	{
		DEBUG_MSG("VehicleScheduler::setSchedulingStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
		schedulingStrategy_ = nullptr;
	}
}

//Controlcenter
Controlcenter::Controlcenter(Eventlist* eventlist, Network* theNetwork, int id, int tg_strategy, int ev_strategy, int tvm_strategy, int vs_strategy, QObject* parent)
    : QObject(parent), id_(id), tg_strategy_(tg_strategy), ev_strategy_(ev_strategy), tvm_strategy_(tvm_strategy), vs_strategy_(vs_strategy), tg_(theNetwork), vs_(eventlist)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes

	theNetwork_ = theNetwork;

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tg_.setEmptyVehicleStrategy(ev_strategy); //set the initial empty vehicle strategy of BustripGenerator
	tvm_.setMatchingStrategy(tvm_strategy);	//set initial matching strategy of BustripVehicleMatcher
	vs_.setSchedulingStrategy(vs_strategy); //set initial scheduling strategy of VehicleScheduler
	connectInternal(); //connect internal signal slots
}
Controlcenter::~Controlcenter(){}

void Controlcenter::reset()
{
	disconnect(this, nullptr, nullptr, nullptr); //Disconnect all signals of Controlcenter

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
		//!< @todo clean up all trips that are chained at this point. Assumes that the first trip in the roster is always 'responsible' for the others...not sure how to clean up without invalidating driving_roster at the moment
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
	
	shortestPathCache.clear(); //TODO: Perhaps also cache over runs as well
	summarydata_.reset(); //TODO: either aggregate over resets or save between them.
}

void Controlcenter::connectInternal()
{
	//Note: the order in which the signals are connected to the slots matters! For example when newUnassignedVehicle is signaled, requestTrip will be called before matchVehiclesToTrips
	//signal slots for debug messages TODO: remove later
	bool ok;
	ok = (QObject::connect(this, &Controlcenter::requestRejected, this, &Controlcenter::on_requestRejected, Qt::DirectConnection) != nullptr);
	assert(ok);
	ok = (QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::on_requestAccepted, Qt::DirectConnection) != nullptr);
	assert(ok);
    ok = (QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::on_tripVehicleMatchFound, Qt::DirectConnection) != nullptr);
    assert(ok);

	//Triggers to generate trips via BustripGenerator
	ok = (QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::requestTrip, Qt::DirectConnection) != nullptr); 
	assert(ok);
	ok = (QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::requestTrip, Qt::DirectConnection) != nullptr);
	assert(ok);
	ok = (QObject::connect(this, &Controlcenter::tripVehicleMatchNotFound, this, &Controlcenter::requestRebalancingTrip, Qt::DirectConnection) != nullptr);
	assert(ok);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	ok = (QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::on_tripGenerated, Qt::DirectConnection) != nullptr); //currently used for debugging only
	assert(ok);
	ok = (QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection) != nullptr);
	assert(ok);
	//ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection); //removed to avoid double call to matchVehicleToTrips
	//assert(ok);
	ok = (QObject::connect(this, &Controlcenter::emptyVehicleTripGenerated, this, &Controlcenter::matchEmptyVehiclesToTrips, Qt::DirectConnection) != nullptr);
	assert(ok);

	//Triggers to schedule vehicle - trip pairs via VehicleScheduler
	ok = (QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::scheduleMatchedTrips, Qt::DirectConnection) != nullptr);
	assert(ok);
}

set<Bus*> Controlcenter::getAllVehicles()
{
	set<Bus*> vehs;
	for (auto veh : connectedVeh_)
	{
		if(!veh.second->is_null())
			vehs.insert(veh.second);
	}
	return vehs;
}

set<Bus*> Controlcenter::getVehiclesDrivingToStop(Busstop* end_stop)
{
	assert(end_stop);
	set<Bus*> vehs_driving;
	set<Bus*> vehs_enroute;

	//get driving vehicles
	for (auto state : { BusState::DrivingEmpty, BusState::DrivingPartiallyFull, BusState::DrivingFull })
	{
		if(fleetState_.count(state) != 0)
			vehs_driving.insert(fleetState_[state].begin(), fleetState_[state].end());
	}
	
	//see which ones are currently headed to end_stop
	for (auto veh : vehs_driving)
	{
		assert(veh->is_driving());
		assert(veh->get_on_trip());
		assert(veh->get_curr_trip() != nullptr);

		Busstop* next_stop = veh->get_next_stop(); //note assumes that all driving vehicles have a trip and a next stop....
		if (next_stop != nullptr && next_stop->get_id() == end_stop->get_id())
			vehs_enroute.insert(veh);
	}

	return vehs_enroute;
}

set<Bus*> Controlcenter::getOnCallVehiclesAtStop(Busstop* stop)
{
	assert(stop);
	set<Bus*> vehs_atstop;

	if (fleetState_.count(BusState::OnCall) != 0)
	{
		for (auto veh : fleetState_[BusState::OnCall])
		{
			Busstop* curr_stop = veh->get_last_stop_visited();
			if (curr_stop != nullptr && curr_stop->get_id() == stop->get_id())
				vehs_atstop.insert(veh);
		}
	}

	return vehs_atstop;
}

double Controlcenter::calc_route_travel_time(const vector<Link*>& routelinks, double time)
{
	double expected_travel_time = 0;
	for (const Link* link : routelinks)
	{
		expected_travel_time += link->get_cost(time);
	}

	return expected_travel_time;
}

vector<Link*> Controlcenter::find_shortest_path_between_stops(Busstop * origin_stop, Busstop * destination_stop, double start_time)
{
	assert(origin_stop);
	assert(destination_stop);
	assert(theNetwork_);
	assert(start_time >= 0);

	vector<Link*> rlinks;
	Controlcenter_OD od = Controlcenter_OD(origin_stop, destination_stop);
	if (shortestPathCache.count(od) != 0)
	{
        rlinks = shortestPathCache[od]; // get cached results if called for this OD before
    }
	else
	{
        if (origin_stop && destination_stop)
        {
            int rootlink_id = origin_stop->get_link_id();
            int dest_node_id = destination_stop->get_dest_node()->get_id(); //!< @todo can change these to look between upstream and downstream junction nodes as well

            rlinks = theNetwork_->shortest_path_to_node(rootlink_id, dest_node_id, start_time);

			shortestPathCache[od] = rlinks; // cache shortest path results
        }
	}
	return rlinks;
}

pair<Bus*,double> Controlcenter::getClosestVehicleToStop(Busstop* stop, double time)
{
	assert(stop);
	assert(isInServiceArea(stop));
	pair<Bus*,double> closest = make_pair(nullptr, DBL_INF);
	
	//check on-call vehicles
	set<Bus*> oncall = getOnCallVehiclesAtStop(stop);
	if (!oncall.empty())
		return make_pair(*oncall.begin(), 0.0);

	//check en-route vehicles
	set<Bus*> enroute = getVehiclesDrivingToStop(stop);
	double shortest_tt = DBL_INF;

	for (auto veh : enroute)
	{
		Busstop* laststop = veh->get_last_stop_visited();
		vector<Link*> shortestpath = find_shortest_path_between_stops(laststop, stop, time);
		double expected_tt = calc_route_travel_time(shortestpath, time);
		double departuretime = veh->get_curr_trip()->get_last_stop_exit_time();
		double time_to_stop = expected_tt - (time - departuretime); // expected time it will take for vehicle to arrive at stop

		if (time_to_stop < shortest_tt)
		{
			closest = make_pair(veh, time_to_stop);
			shortest_tt = time_to_stop;
		}
	}

	//check remaining vehicles
	set<Bus*> allvehs = getAllVehicles();
	set<Bus*> checked;
	set<Bus*> remaining;
	set_union(oncall.begin(), oncall.end(), enroute.begin(), enroute.end(), inserter(checked, checked.begin()));
	set_difference(allvehs.begin(), allvehs.end(), checked.begin(), checked.end(), inserter(remaining, remaining.begin()));

	for (auto veh : remaining)
	{
		double expected_tt = DBL_INF;
		double time_to_stop = DBL_INF;
		vector<Link*> shortestpath;

		if (veh->is_driving())
		{
			double departuretime = veh->get_curr_trip()->get_last_stop_exit_time();
			Busstop* nextstop = veh->get_next_stop();
		
			//to next stop
			expected_tt = calc_route_travel_time(veh->get_curr_trip()->get_line()->get_busroute()->get_links(),time); // @todo assumes direct 'service routes' only
			time_to_stop = expected_tt - (time - departuretime);
			
			//from next stop to target stop
			shortestpath = find_shortest_path_between_stops(nextstop, stop, time);
			time_to_stop += calc_route_travel_time(shortestpath, time);
		}
		else
		{
			Busstop* laststop = veh->get_last_stop_visited(); //current stop if not driving
			shortestpath = find_shortest_path_between_stops(laststop, stop, time);
			expected_tt = calc_route_travel_time(shortestpath, time);
			time_to_stop = expected_tt;
		}

		if (time_to_stop < shortest_tt)
		{
			closest = make_pair(veh, time_to_stop);
			shortest_tt = time_to_stop;
		}
	}

	return closest;
}

int Controlcenter::getID() const
{
	return id_;
}

Controlcenter_SummaryData Controlcenter::getSummaryData() const
{
	return summarydata_;
}

set<Busstop*, ptr_less<Busstop*>> Controlcenter::getServiceArea() const { return serviceArea_; }
vector<Busline*> Controlcenter::getServiceRoutes() const { return tg_.getServiceRoutes(); }

map<int, Bus *> Controlcenter::getConnectedVehicles() const
{
    return connectedVeh_;
}

map<BusState, set<Bus*>> Controlcenter::getFleetState() const
{
	return fleetState_;
}

void Controlcenter::printFleetState() const
{
	for (const auto& state : fleetState_)
	{
		for (auto veh : state.second)
		{
			veh->print_state();
		}
	}
}

bool Controlcenter::isInServiceArea(Busstop* stop) const
{
    if(serviceArea_.count(stop) != 0) {
        return true;
}
    return false;
}

bool Controlcenter::getGeneratedDirectRoutes()
{
	return generated_direct_routes_;
}

void Controlcenter::setGeneratedDirectRoutes(bool generate_direct_routes)
{
	generated_direct_routes_ = generate_direct_routes;
}

void Controlcenter::connectPassenger(Passenger* pass)
{
	assert(pass->is_flexible_user()); // passenger should only be connected to a CC if they have chosen a flexible, or on-demand mode
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) == 0); //passenger should only be added once
	
	connectedPass_[pid] = pass;

	if (QObject::connect(pass, &Passenger::sendRequest, this, &Controlcenter::receiveRequest, Qt::DirectConnection) == nullptr)
	{
		DEBUG_MSG_V(Q_FUNC_INFO << " connecting Passenger::sendRequest to Controlcenter::receiveRequest failed!");
		abort();
	}
	if (QObject::connect(pass, &Passenger::boardedBus, this, &Controlcenter::removeRequest, Qt::DirectConnection) == nullptr)
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
	if (transitveh)
	{
		int bvid = transitveh->get_bus_id();
		assert(connectedVeh_.count(bvid) == 0); //vehicle should only be added once
		connectedVeh_[bvid] = transitveh;

		//connect bus state changes to control center
		if (QObject::connect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState, Qt::DirectConnection) == nullptr)
		{
			DEBUG_MSG_V(Q_FUNC_INFO << " connecting Bus::stateChanged with Controlcenter::updateFleetState failed!");
			abort();
		}

		transitveh->set_control_center(this);
	}
}
void Controlcenter::disconnectVehicle(Bus* transitveh)
{
	assert(transitveh);
	if(transitveh)
	{ 
		int bvid = transitveh->get_bus_id();
		assert(connectedVeh_.count(bvid) != 0); //only disconnect vehicles that have been added

		connectedVeh_.erase(connectedVeh_.find(bvid));
		bool ok = QObject::disconnect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState);
		assert(ok);

		transitveh->set_control_center(nullptr);
	}
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

	//whenever a line is added as a service route to a CC, the line also knows of the CC it is a service route for...
	line->add_CC(this); // may create a tight coupling between Controlcenter's and Busline, but added so that Pass_path can ask specific buslines for LoS estimates. 
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
	if(transitveh)
	{
		if (initialVehicles_.count(transitveh) == 0)
		{
			initialVehicles_.insert(transitveh);
		}
	}
}

void Controlcenter::addCompletedVehicleTrip(Bus* transitveh, Bustrip * trip)
{
	assert(transitveh);
	assert(trip);
	if(trip)
		assert(trip->get_next_stop() == trip->stops.end()); //currently a trip is considered completed via Bustrip::advance_next_stop -> Bus::advance_curr_trip
	completedVehicleTrips_.emplace_back(transitveh, trip);
}

double Controlcenter::calc_expected_ivt(Busline* service_route, Busstop* start_stop, Busstop* end_stop, bool first_line_leg, double time)
{
	assert(service_route->is_flex_line()); //otherwise kindof pointless to ask the CC about ivt
	assert(service_route->get_CC()->getID() == id_);

	if (!first_line_leg)
		return calc_exploration_ivt(service_route, start_stop, end_stop); // if no 'RTI' or estimate is available

	vector<Link*> shortestpath = find_shortest_path_between_stops(start_stop, end_stop, time);
	double expected_ivt = calc_route_travel_time(shortestpath, time); // travel time calculated based on link->cost(time)
	
	return expected_ivt; //OBS waiting time is returned in seconds
}

/**
 * @ingroup DRT
 * 
 * @todo Currently the exploration ivt is simply the direct path between the two stops based on delta_at_stops (time-independent scheduled IVT)
 * 
 */
double Controlcenter::calc_exploration_ivt(Busline* service_route, Busstop* start_stop, Busstop* end_stop)
{
	assert(service_route->is_flex_line()); //otherwise kindof pointless to ask the CC about ivt
	assert(service_route->get_CC()->getID() == id_);

	auto delta_at_stops = service_route->get_delta_at_stops();

	bool found_board = false;
	bool found_alight = false;

	vector<pair<Busstop*, double>>::iterator board_stop;
	vector<pair<Busstop*, double>>::iterator alight_stop;
	double earliest_time_ostop = 0.0;
	double cumulative_arrival_time = 0.0; //arrival times starting from zero for initial stop

	// Always use default ivt between stops for the line
	for (vector<pair<Busstop*, double>>::iterator stop = delta_at_stops.begin(); stop != delta_at_stops.end(); ++stop)
	{
		cumulative_arrival_time += (*stop).second;
		if ((*stop).first->get_id() == start_stop->get_id() || (*stop).first->get_name() == start_stop->get_name())
		{
			earliest_time_ostop = cumulative_arrival_time;
			board_stop = stop;
			found_board = true;
		}
		if ((*stop).first->get_id() == end_stop->get_id() || (*stop).first->get_name() == end_stop->get_name())
		{
			alight_stop = stop;
			found_alight = true;
			break;
		}
	}
	if (found_board == false || found_alight == false)
		return 10000; //default in case of no matching

	return cumulative_arrival_time - earliest_time_ostop; //OBS waiting time is returned in seconds
}

double Controlcenter::calc_expected_wt(Busline* service_route, Busstop* start_stop, Busstop* end_stop, bool first_line_leg, double walking_time_to_start_stop, double arrival_time_to_start_stop)
{
	assert(service_route->is_flex_line()); //otherwise kindof pointless to ask the CC about wt
	assert(service_route->get_CC()->getID() == id_);
    Q_UNUSED(end_stop)

	if (!first_line_leg) 
		return calc_exploration_wt(); //no 'RTI' or better estimate is available

	pair<Bus*,double> closest = getClosestVehicleToStop(start_stop, arrival_time_to_start_stop);
	//DEBUG_MSG("Bus " << closest.first->get_bus_id() << " is closest to stop " << start_stop->get_id() << " with time_to_stop " << closest.second);
	//closest.first->print_state();
	if (first_line_leg) // for first transit link traveler can save on waiting time by sending their request for when they expect to arrive
	{
		closest.second = Max(0.0, closest.second - walking_time_to_start_stop); // remove walking time to stop from waiting time if this actually saves any time
	}

	return closest.second; //OBS waiting time is returned in seconds
}

/**
 * @ingroup DRT
 * 
 * @todo currently just returns 0.0 waiting time
 * 
 * @return exploration waiting time
 */
double Controlcenter::calc_exploration_wt()
{
	return ::drt_exploration_wt; //OBS waiting time is returned in seconds
}

//Slot implementations
void Controlcenter::receiveRequest(Request* req, double time)
{
	assert(req->time_desired_departure >= 0 && req->time_request_generated >= 0 && req->load > 0); //assert that request is valid
	summarydata_.requests_recieved += 1;
	rh_.addRequest(req, serviceArea_) ? emit requestAccepted(time) : emit requestRejected(time);
}

void Controlcenter::removeRequest(int pass_id)
{
	//DEBUG_MSG(Q_FUNC_INFO);
	assert(connectedPass_.count(pass_id) != 0); // remove request should only be called for connected travelers
	if(connectedPass_.count(pass_id) != 0)
		assert(connectedPass_[pass_id]->is_flexible_user()); // connected travelers should all be flexible transit users

	rh_.removeRequest(pass_id);
	summarydata_.requests_served += 1;
}

void Controlcenter::on_requestAccepted(double time)
{
	//qDebug() << ": Request Accepted at time " << time;	
    Q_UNUSED(time)
	summarydata_.requests_accepted += 1;
}
void Controlcenter::on_requestRejected(double time)
{
	//qDebug() << ": Request Rejected at time " << time;
    Q_UNUSED(time)
	summarydata_.requests_rejected += 1;
}

void Controlcenter::on_tripGenerated(double time)
{
	//qDebug() << ": Trip Generated at time " << time;
    Q_UNUSED(time)
}

void Controlcenter::on_tripVehicleMatchFound(double time)
{
	//qDebug() << ": Vehicle - Trip match found at time " << time;
    Q_UNUSED(time)
}

void Controlcenter::updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time)
{
	assert(bus->get_state() == newstate); //the newstate should be the current state of the bus
	assert(bus->is_flex_vehicle()); //fixed vehicles do not currently have a control center
	assert(connectedVeh_.count(bus->get_bus_id()) != 0); //assert that bus is connected to this control center (otherwise state change signal should have never been heard)

	//update the fleet state map, vehicles should be null before they are available, and null when they finish a trip and are copied
	if(oldstate != BusState::Null) 
	{
		fleetState_[oldstate].erase(bus);
    }
	if(newstate != BusState::Null) 
	{
		fleetState_[newstate].insert(bus);
    }

	if (newstate == BusState::OnCall)
	{
		emit newUnassignedVehicle(time);
	}
}

void Controlcenter::requestTrip(double time)
{
	if (tg_.requestTrip(rh_, fleetState_, time)) 
	{
		emit tripGenerated(time);
	}
}

void Controlcenter::requestRebalancingTrip(double time)
{
	if (tg_.requestRebalancingTrip(rh_, fleetState_, time)) 
	{
		emit emptyVehicleTripGenerated(time);
	}
}

void Controlcenter::matchVehiclesToTrips(double time)
{
	tvm_.matchVehiclesToTrips(tg_, time) ? emit tripVehicleMatchFound(time) : emit tripVehicleMatchNotFound(time);
}

void Controlcenter::matchEmptyVehiclesToTrips(double time)
{
	if (tvm_.matchVehiclesToEmptyVehicleTrips(tg_, time)) 
	{
		emit tripVehicleMatchFound(time);
	}
}

void Controlcenter::scheduleMatchedTrips(double time)
{
	vs_.scheduleMatchedTrips(tvm_, time);
}

Controlcenter_OD::Controlcenter_OD(Busstop* orig_, Busstop* dest_)
{
	orig = orig_;
	dest = dest_;
}

bool Controlcenter_OD::operator==(const Controlcenter_OD& rhs) const
{
	return (orig == rhs.orig && dest == rhs.dest);
}

bool Controlcenter_OD::operator<(const Controlcenter_OD& rhs) const
{
	if (orig != nullptr && rhs.orig != nullptr)
		return orig->get_id() < rhs.orig->get_id();
	else
		return true;
}
