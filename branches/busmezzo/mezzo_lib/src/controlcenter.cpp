#include "controlstrategies.h"
#include "controlutilities.h"
#include "passenger.h"
#include "vehicle.h"
#include "controlcenter.h"
#include "network.h"
#include "MMath.h"

#include <algorithm>
#include <iterator>
#include <cassert>

void DRTAssignmentData::reset()
{
    for (Bustrip* trip : unmatched_trips) //clean up planned trips that were never matched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	for (Bustrip* trip : unmatched_empty_trips)
	{
		delete trip;
	}
	for (Bustrip* trip : unscheduled_trips) //clean up matched trips that were not dispatched. Note: currently network reset is called even after the last simulation replication
	{
		delete trip;
	}
	for (Bustrip* trip : active_trips) //clean up scheduled trips that were never completed, ControlCenter manages cleanups of completed trips
	{
		delete trip;
	}

	unmatched_trips.clear();
	unmatched_empty_trips.clear();
	unscheduled_trips.clear();
	active_trips.clear();

    for (auto req : active_requests)
    {
        delete req;
    }
    for (auto req : rejected_requests)
    {
        delete req;
    }
	for (auto req : completed_requests)
	{
	    delete req;
	}

	active_requests.clear();
	rejected_requests.clear();
	completed_requests.clear();

    fleet_state.clear();
}

void DRTAssignmentData::print_state(double time) const
{
    qDebug() << "Controlcenter" << cc_owner->getID() << "assignment_data at time" << time;
    qDebug() << "\tunmatched_trips      :" << unmatched_trips.size();
    qDebug() << "\tunmatched_empty_trips:" << unmatched_empty_trips.size();
    qDebug() << "\tunscheduled_trips    :" << unscheduled_trips.size();
    qDebug() << "\tactive_trips         :" << active_trips.size();
	qDebug() << "\t\tpassenger_trips:" << cs_helper_functions::filterRequestAssignedTrips(active_trips).size(); // @note the number of active passenger carrying trips should pretty much always match the number of busy vehicles without rebalancing (unless the vehicle-trip hasnt been scheduled yet)
	qDebug() << "\t\tempty_trips    :" << active_trips.size() - cs_helper_functions::filterRequestAssignedTrips(active_trips).size();
    qDebug() << "\tcompleted_trips      :" << cc_owner->getCompletedVehicleTrips().size() << endl;

    qDebug() << "\trejected_requests    :" << rejected_requests.size();
    qDebug() << "\tactive_requests      :" << active_requests.size();
	qDebug() << "\t\tunmatched_requests :" << cs_helper_functions::filterRequestsByState(active_requests,RequestState::Unmatched).size();
	qDebug() << "\t\tassigned_requests  :" << cs_helper_functions::filterRequestsByState(active_requests,RequestState::Assigned).size();
	qDebug() << "\t\tmatched_requests   :" << cs_helper_functions::filterRequestsByState(active_requests,RequestState::Matched).size();
	qDebug() << "\t\tin_service_requests:" << cs_helper_functions::filterRequestsByState(active_requests,RequestState::ServedUnfinished).size();
    qDebug() << "\tcompleted_requests   :" << completed_requests.size() << endl;

    if (fleet_state.find(BusState::OnCall) != fleet_state.end())
        qDebug() << "\toncall_vehicles      :" << fleet_state.at(BusState::OnCall).size() << endl;
    else
        qDebug() << "\tno vehicles intialized!";
}

void Controlcenter_SummaryData::reset()
{
	requests_recieved = 0;
	requests_accepted = 0;
	requests_served = 0;
	requests_rejected = 0;
}


//RequestHandler
RequestHandler::RequestHandler() = default;
RequestHandler::~RequestHandler() = default;

void RequestHandler::reset()
{
}

bool RequestHandler::addRequest(DRTAssignmentData& assignment_data, Request* req, const set<Busstop*, ptr_less<Busstop*> >& serviceArea)
{
    if (!isFeasibleRequest(req, serviceArea))
    {
        qDebug() << "Info - rejecting request to travel between origin stop " << req->ostop_id << 
                    " and destination stop " << req->dstop_id << ", destination stop not reachable within service area";

        // Currently if a request is rejected passengers pointer to it is reset to nullptr and state of request is set to rejected
        req->pass_owner->set_curr_request(nullptr);
        req->set_state(RequestState::Rejected);
        assignment_data.rejected_requests.insert(req);

        return false;
    }

    if (find(assignment_data.active_requests.begin(), assignment_data.active_requests.end(), req) != assignment_data.active_requests.end()) //if request was sent twice
    {
        qDebug() << "Warning - passenger request" << req->pass_id << "generated at time" << req->time_request_generated << "already exists in request set!";
    }
    assignment_data.active_requests.insert(req);
    req->set_state(RequestState::Unmatched);
    return true;
	
}

void RequestHandler::removeActiveRequest(DRTAssignmentData& assignment_data, const int pass_id)
{
    set<Request*, ptr_less<Request>>::iterator it;
    it = find_if(assignment_data.active_requests.begin(), assignment_data.active_requests.end(),
        [pass_id](const Request* req) -> bool
        {
            return req->pass_id == pass_id;
        }
    );

    if (it != assignment_data.active_requests.end())
	{
		Request* req = *it;
		req->pass_owner->set_curr_request(nullptr);
		if(req->assigned_trip != nullptr) // if request has been assigned to a trip-chain
			cc_helper_functions::removeRequestFromTripChain(req, req->assigned_trip->driving_roster); //!< remove this request from scheduled request of any trip it might be scheduled to @todo perhaps change this to a 'ServedFinished' RequestState and save deletion for later in the future
        assignment_data.active_requests.erase(req);
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
BustripGenerator::BustripGenerator(Network* theNetwork, TripGenerationStrategy* generationStrategy, TripGenerationStrategy* emptyVehicleStrategy, TripGenerationStrategy* rebalancingStrategy) :
                                  generationStrategy_(generationStrategy), emptyVehicleStrategy_(emptyVehicleStrategy), rebalancingStrategy_(rebalancingStrategy), theNetwork_(theNetwork)
{}

BustripGenerator::~BustripGenerator()
{
    delete generationStrategy_;
    delete emptyVehicleStrategy_;
	delete rebalancingStrategy_;
}

void BustripGenerator::reset(int generation_strategy_type, int empty_vehicle_strategy_type, int rebalancing_strategy_type)
{
	setTripGenerationStrategy(generation_strategy_type);
	setEmptyVehicleStrategy(empty_vehicle_strategy_type);
	setRebelancingStrategy(rebalancing_strategy_type);
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

bool BustripGenerator::requestTrip(DRTAssignmentData& assignment_data, double time)
{
    if (generationStrategy_ != nullptr)
    {
        bool trip_found = generationStrategy_->calc_trip_generation(assignment_data, serviceRoutes_, time); //returns true if trip has been generated and added to the unmatchedTrips_

        if (!trip_found && !assignment_data.unmatched_trips.empty()) //if no trip was found but an unmatched trip remains in the unmatchedTrips set
		{ 
            trip_found = true;
        }

        return trip_found;
    }
    return false;
}

bool BustripGenerator::requestEmptyTrip(DRTAssignmentData& assignment_data, double time)
{
	if (emptyVehicleStrategy_ != nullptr)
	{
		return emptyVehicleStrategy_->calc_trip_generation(assignment_data, serviceRoutes_, time); //returns true if trip has been generated and added to the unmatchedRebalancingTrips_
	}
	return false;
}

bool BustripGenerator::requestRebalancingTrip(DRTAssignmentData& assignment_data, double time)
{
	if(rebalancingStrategy_ != nullptr)
	{
	    return rebalancingStrategy_->calc_trip_generation(assignment_data,serviceRoutes_,time);
	}
	return false;
}

void BustripGenerator::setTripGenerationStrategy(int type)
{
    delete generationStrategy_;

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
    delete emptyVehicleStrategy_;

	if (type == emptyVehicleStrategyType::EVNull) 
	{
		emptyVehicleStrategy_ = new NullTripGeneration();
	} 
	else if (type == emptyVehicleStrategyType::EVNaive)
	{
		emptyVehicleStrategy_ = new NaiveEmptyVehicleTripGeneration(theNetwork_);
	}
	else if (type == emptyVehicleStrategyType::EVSimple)
	{
		emptyVehicleStrategy_ = new SimpleEmptyVehicleTripGeneration(theNetwork_);
	}
	else if (type == emptyVehicleStrategyType::EVMaxWait)
	{
		emptyVehicleStrategy_ = new MaxWaitEmptyVehicleTripGeneration(theNetwork_);
	}
	else if (type == emptyVehicleStrategyType::EVCumWait)
	{
		emptyVehicleStrategy_ = new CumulativeWaitEmptyVehicleTripGeneration(theNetwork_);
	}
	else
	{
		DEBUG_MSG("BustripGenerator::setEmptyVehicleStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
		emptyVehicleStrategy_ = nullptr;
	}
}

void BustripGenerator::setRebelancingStrategy(int type)
{
	delete rebalancingStrategy_;
	if (type == rebalancingStrategyType::RBNull) 
	{
		rebalancingStrategy_ = new NullTripGeneration();
	} 
	else if (type == rebalancingStrategyType::RBNaive)
	{
		rebalancingStrategy_ = new NaiveRebalancing(theNetwork_);
	}
	else
	{
		DEBUG_MSG("BustripGenerator::setEmptyVehicleStrategy() - strategy " << type << " is not recognized! Setting strategy to nullptr. ");
		rebalancingStrategy_ = nullptr;
	}
}

TripGenerationStrategy* BustripGenerator::getGenerationStratgy() const
{
	return generationStrategy_;
}

//BustripVehicleMatcher
BustripVehicleMatcher::BustripVehicleMatcher(MatchingStrategy* matchingStrategy): matchingStrategy_(matchingStrategy){}
BustripVehicleMatcher::~BustripVehicleMatcher()
{
    delete matchingStrategy_;
}

void BustripVehicleMatcher::reset(int matching_strategy_type)
{
	vehicles_per_service_route_.clear();
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
    vehicles_per_service_route_[line_id].insert(transitveh);
    transitveh->add_sroute_id(line_id); //vehicle is also aware of its service routes for now. TODO: probably this is a bit unnecessary at the moment, since the vehicle also knows of its CC
}

void BustripVehicleMatcher::removeVehicleFromServiceRoute(int line_id, Bus * transitveh)
{
	assert(transitveh);
	set<Bus*> candidateVehicles = vehicles_per_service_route_[line_id];
    if (candidateVehicles.count(transitveh) != 0)
    {
        vehicles_per_service_route_[line_id].erase(transitveh);
        transitveh->remove_sroute_id(line_id); //vehicle is also aware of its service routes for now. TODO: probably this is a bit unnecessary at the moment, since the vehicle also knows of its CC
    }
}

void BustripVehicleMatcher::setMatchingStrategy(int type)
{
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

bool BustripVehicleMatcher::matchVehiclesToTrips(DRTAssignmentData& assignment_data, double time)
{
	if (matchingStrategy_ != nullptr)
	{
		bool matchfound = false;
		for (auto it = assignment_data.unmatched_trips.begin(); it != assignment_data.unmatched_trips.end();) //attempt to match vehicles to all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(assignment_data, trip, vehicles_per_service_route_, time))
			{
				matchfound = true;
				it = assignment_data.unmatched_trips.erase(it);
				assignment_data.unscheduled_trips.insert(trip);
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

bool BustripVehicleMatcher::matchVehiclesToEmptyVehicleTrips(DRTAssignmentData& assignment_data, double time)
{
	if (matchingStrategy_ != nullptr)
	{
		bool matchfound = false;
		for (auto it = assignment_data.unmatched_empty_trips.begin(); it != assignment_data.unmatched_empty_trips.end();) //attempt to match all trips in planned trips
		{
			Bustrip* trip = *it;
			if (matchingStrategy_->find_tripvehicle_match(assignment_data, trip, vehicles_per_service_route_, time))
			{
				matchfound = true;
				it = assignment_data.unmatched_empty_trips.erase(it);
				assignment_data.unscheduled_trips.insert(trip);
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
    delete schedulingStrategy_;
}

void VehicleScheduler::reset(int scheduling_strategy_type)
{
	setSchedulingStrategy(scheduling_strategy_type);
}

bool VehicleScheduler::scheduleMatchedTrips(DRTAssignmentData& assignment_data, double time)
{
	if (schedulingStrategy_ != nullptr)
	{
		return schedulingStrategy_->schedule_trips(assignment_data, eventlist_, time);
	}

	return false;
}

void VehicleScheduler::setSchedulingStrategy(int type)
{
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
Controlcenter::Controlcenter(
	Eventlist* eventlist, 
	Network* theNetwork, 
	int id, 
	int tg_strategy, 
	int ev_strategy, 
	int tvm_strategy, 
	int vs_strategy,
	int rb_strategy,
	double rebalancing_interval, 
	QObject* parent)
    : QObject(parent), id_(id), tg_strategy_(tg_strategy), ev_strategy_(ev_strategy), tvm_strategy_(tvm_strategy), vs_strategy_(vs_strategy), rb_strategy_(rb_strategy), rebalancing_interval_(rebalancing_interval), tg_(theNetwork), vs_(eventlist)
{
	QString qname = QString::fromStdString(to_string(id));
	this->setObjectName(qname); //name of control center does not really matter but useful for debugging purposes

	theNetwork_ = theNetwork;
	assignment_data_.cc_owner = this;

	tg_.setTripGenerationStrategy(tg_strategy); //set the initial generation strategy of BustripGenerator
	tg_.setEmptyVehicleStrategy(ev_strategy); //set the initial empty vehicle strategy of BustripGenerator
	tg_.setRebelancingStrategy(rb_strategy); //set the initial rebalancing strategy of BustripGenerator
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
	tg_.reset(tg_strategy_, ev_strategy_, rb_strategy_); 
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

	assignment_data_.reset();
	
	shortestPathCache.clear(); //TODO: Perhaps also cache over runs as well
	summarydata_.reset(); //TODO: either aggregate over resets or save between them.
}

void Controlcenter::connectInternal()
{
	//Note: the order in which the signals are connected to the slots matters! For example when newUnassignedVehicle is signaled, requestTrip will be called before matchVehiclesToTrips
	//signal slots for debug messages TODO: remove later
	QObject::connect(this, &Controlcenter::requestRejected, this, &Controlcenter::on_requestRejected, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::on_requestAccepted, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::tripNotGenerated, this, &Controlcenter::on_tripNotGenerated, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::emptyTripNotGenerated, this, &Controlcenter::on_emptyTripNotGenerated, Qt::DirectConnection);
    QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::on_tripVehicleMatchFound, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::tripVehicleMatchNotFound, this, &Controlcenter::on_tripVehicleMatchNotFound, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::emptyTripVehicleMatchNotFound, this, &Controlcenter::on_emptyTripVehicleMatchNotFound, Qt::DirectConnection);

	//Triggers to generate trips via BustripGenerator
	QObject::connect(this, &Controlcenter::requestAccepted, this, &Controlcenter::requestTrip, Qt::DirectConnection); 
	QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::requestTrip, Qt::DirectConnection);
	QObject::connect(this, &Controlcenter::tripVehicleMatchNotFound, this, &Controlcenter::requestEmptyTrip, Qt::DirectConnection);

	//Triggers to match vehicles in trips via BustripVehicleMatcher
	QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::on_tripGenerated, Qt::DirectConnection); //currently used for debugging only
	QObject::connect(this, &Controlcenter::tripGenerated, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection);
	//ok = QObject::connect(this, &Controlcenter::newUnassignedVehicle, this, &Controlcenter::matchVehiclesToTrips, Qt::DirectConnection); //removed to avoid double call to matchVehicleToTrips
	//assert(ok);
	QObject::connect(this, &Controlcenter::emptyTripGenerated, this, &Controlcenter::matchEmptyVehiclesToTrips, Qt::DirectConnection);

	//Triggers to schedule vehicle - trip pairs via VehicleScheduler
	QObject::connect(this, &Controlcenter::tripVehicleMatchFound, this, &Controlcenter::scheduleMatchedTrips, Qt::DirectConnection);
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
		if(assignment_data_.fleet_state.count(state) != 0)
			vehs_driving.insert(assignment_data_.fleet_state[state].begin(), assignment_data_.fleet_state[state].end());
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

set<Bus*> Controlcenter::getVehiclesEnRouteToStop(Busstop* stop)
{
	assert(stop);
	set<Bus*> vehs_enroute;
	for(const auto& vehs_in_state : assignment_data_.fleet_state)
	{
	    if(vehs_in_state.first != BusState::Null && vehs_in_state.first != BusState::OnCall) // if not Null or Oncall then assigned to trip
	    {
			for(auto veh : vehs_in_state.second)
			{
                Bustrip* trip = veh->get_curr_trip();

                if (trip)
                {
                    // add if vehicle is Idle, and at the current stop or has it downstream on trip
                    if (veh->is_idle())
                    {
                        if (trip->get_last_stop_visited() == stop)
                            vehs_enroute.insert(veh);
                        if (trip->has_stop_downstream(stop))
                            vehs_enroute.insert(veh);
                    }
                    // add if vehicle is Driving, and has the stop downstream on trip
                    if (veh->is_driving())
                    {
                        if (trip->has_stop_downstream(stop))
                            vehs_enroute.insert(veh);
                    }
                }
				else
				{
				    qDebug() << "Warning - ignoring vehicle" << veh->get_bus_id() << "in state" << BusState_to_QString(veh->get_state()) << "without a trip assigned to it, when searching for vehicles enroute to stop" << stop->get_id();
				}
			}
	    }
	}
	return vehs_enroute;
}

set<Bus*> Controlcenter::getOnCallVehiclesAtStop(Busstop* stop)
{
	assert(stop);
	set<Bus*> vehs_atstop;

	if (assignment_data_.fleet_state.count(BusState::OnCall) != 0)
	{
		for (auto veh : assignment_data_.fleet_state[BusState::OnCall])
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
	pair<Bus*,double> closest = make_pair(nullptr, numeric_limits<double>::max());
	
	//check on-call vehicles
	set<Bus*> oncall = getOnCallVehiclesAtStop(stop);
	if (!oncall.empty())
		return make_pair(*oncall.begin(), 0.0);

	//check en-route vehicles
	set<Bus*> enroute = getVehiclesDrivingToStop(stop);
	double shortest_tt = numeric_limits<double>::max();

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
		double expected_tt = numeric_limits<double>::max();
		double time_to_stop = numeric_limits<double>::max();
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
	return assignment_data_.fleet_state;
}

void Controlcenter::printFleetState() const
{
	for (const auto& state : assignment_data_.fleet_state)
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

double Controlcenter::get_rebalancing_interval() const
{
    return rebalancing_interval_;
}

set<Busstop*, ptr_less<Busstop*> > Controlcenter::get_collection_stops() const
{
    return collection_stops_;
}

void Controlcenter::add_collection_stop(Busstop* stop)
{
	//check existance in service area first
	assert(isInServiceArea(stop));
	collection_stops_.insert(stop);
}

void Controlcenter::connectPassenger(Passenger* pass)
{
	assert(pass->is_flexible_user()); // passenger should only be connected to a CC if they have chosen a flexible, or on-demand mode
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) == 0); //passenger should only be added once
	
	connectedPass_[pid] = pass;

	QObject::connect(pass, &Passenger::sendRequest, this, &Controlcenter::receiveRequest, Qt::DirectConnection);
	//QObject::connect(pass, &Passenger::boardedBus, this, &Controlcenter::removeActiveRequest, Qt::DirectConnection);
	QObject::connect(pass, &Passenger::stateChanged, this, &Controlcenter::updateRequestState, Qt::DirectConnection);
}
void Controlcenter::disconnectPassenger(Passenger* pass)
{
	assert(pass);
	int pid = pass->get_id();
	assert(connectedPass_.count(pid) != 0);

	connectedPass_.erase(connectedPass_.find(pid));

    QObject::disconnect(pass, &Passenger::sendRequest, this, &Controlcenter::receiveRequest);
	//QObject::disconnect(pass, &Passenger::boardedBus, this, &Controlcenter::removeActiveRequest);
	QObject::disconnect(pass, &Passenger::stateChanged, this, &Controlcenter::updateRequestState);
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
		QObject::connect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState, Qt::DirectConnection);

		transitveh->set_control_center(this);

		if(assignment_data_.planned_capacity == 0) //!< @todo PARTC specific remove
			assignment_data_.planned_capacity = transitveh->get_capacity();
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
		QObject::disconnect(transitveh, &Bus::stateChanged, this, &Controlcenter::updateFleetState);

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
        initialVehicles_.insert(transitveh);
    }
}

void Controlcenter::addCompletedVehicleTrip(Bus* transitveh, Bustrip * trip)
{
	assert(transitveh);
	assert(trip);
	
	completedVehicleTrips_.emplace_back(transitveh, trip);
	trip->set_status(BustripStatus::Completed); // update status of trip

	assignment_data_.active_trips.erase(trip); 
}

vector<pair<Bus*,Bustrip*> > Controlcenter::getCompletedVehicleTrips() const
{
    return completedVehicleTrips_;
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

	double earliest_time_ostop = 0.0;
	double cumulative_arrival_time = 0.0; //arrival times starting from zero for initial stop

	// Always use default ivt between stops for the line
	for (auto stop = delta_at_stops.begin(); stop != delta_at_stops.end(); ++stop)
    {
		cumulative_arrival_time += (*stop).second;
		if ((*stop).first->get_id() == start_stop->get_id() || (*stop).first->get_name() == start_stop->get_name())
		{
			earliest_time_ostop = cumulative_arrival_time;
			found_board = true;
		}
		if ((*stop).first->get_id() == end_stop->get_id() || (*stop).first->get_name() == end_stop->get_name())
		{
			found_alight = true;
			break;
		}
	}
	if (found_board == false || found_alight == false)
		return ::drt_default_large_ivt; //default in case of no matching

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
	rh_.addRequest(assignment_data_, req, serviceArea_) ? emit requestAccepted(time) : emit requestRejected(time);
}

void Controlcenter::updateRequestState(Passenger* pass, PassengerState oldstate, PassengerState newstate, double time)
{
	Q_UNUSED(time) // @todo might be useful to update request states in the future

    // Sanity checks
    assert(pass->get_state() == newstate); //the newstate should be the current state of the passenger
    assert(pass->is_flexible_user()); // fixed users are not connected to a cc
	assert(newstate != PassengerState::WaitingForFixed);
	assert(newstate != PassengerState::WaitingForFixedDenied);
    assert(connectedPass_.count(pass->get_id()) != 0); //assert that pass is connected to this control center (otherwise state change signal should have never been heard)

	Request* req = pass->get_curr_request();
	assert(req != nullptr);

    //update passengers request, depending on the passenger state change
	switch(newstate)
	{
    case PassengerState::Null: break;
    case PassengerState::Walking: break;
    case PassengerState::ArrivedToStop:
        assert(oldstate == PassengerState::Null || oldstate == PassengerState::OnBoard); // passenger either just started or alighted
        if (oldstate == PassengerState::OnBoard) // Passenger just alighted at a stop
        {
            //assert(pass->get_OD_stop()->get_origin()->get_id() == req->dstop_id); // the stop the passenger just alighted at (and set its new od to) should be the destination of the request
			if(req->assigned_trip)
			    req->assigned_trip->remove_request(req);
            req->set_state(RequestState::ServedFinished);
            //removeActiveRequest(pass->get_id()); //!< @todo remove and delete via assignment_data instead, just testing incrementally if we can repeat results this way
            assignment_data_.active_requests.erase(req);
            assignment_data_.completed_requests.insert(req);
            pass->set_curr_request(nullptr);
            summarydata_.requests_served += 1;
        }
        break;
    case PassengerState::WaitingForFixed: abort(); break;
	case PassengerState::WaitingForFlex: assert(req->state == RequestState::Matched || req->state == RequestState::Assigned || req->state == RequestState::Unmatched); break;
    case PassengerState::WaitingForFixedDenied: abort(); break;
    case PassengerState::WaitingForFlexDenied: break;
    case PassengerState::OnBoard:
		// passengers may board vehicles opportunistically, check if they boarded their assigned trip or another one
		// if a different trip was boarded, remove the passenger from the one they were originally assigned to and reassign it to the new one
		Bustrip* trip_boarded = pass->get_last_selected_path_trip();
		if(trip_boarded != req->assigned_trip) 
		{
		    req->set_assigned_trip(trip_boarded);
		}
        req->set_state(RequestState::ServedUnfinished);
        break;

    }
}

void Controlcenter::removeActiveRequest(int pass_id)
{
	assert(connectedPass_.count(pass_id) != 0); // remove request should only be called for connected travelers
	if(connectedPass_.count(pass_id) != 0)
		assert(connectedPass_[pass_id]->is_flexible_user()); // connected travelers should all be flexible transit users

	rh_.removeActiveRequest(assignment_data_, pass_id);
	summarydata_.requests_served += 1;
}

void Controlcenter::on_requestAccepted(double time)
{
    Q_UNUSED(time)
	summarydata_.requests_accepted += 1;
}
void Controlcenter::on_requestRejected(double time)
{
    Q_UNUSED(time)
	summarydata_.requests_rejected += 1;
}

void Controlcenter::on_tripGenerated(double time)
{
	//qDebug() << ": Trip Generated at time " << time;
    Q_UNUSED(time)
}

void Controlcenter::on_tripNotGenerated(double time)
{
 //   qDebug() << "Trip not generated at time" << time;
	//assignment_data_.print_state(time);
}

void Controlcenter::on_emptyTripNotGenerated(double time)
{
 //   qDebug() << "Empty-trip not generated at time" << time;
	//assignment_data_.print_state(time);
}

void Controlcenter::on_tripVehicleMatchFound(double time)
{
	//qDebug() << ": Vehicle - Trip match found at time " << time;
    Q_UNUSED(time)
}

void Controlcenter::on_tripVehicleMatchNotFound(double time)
{
	//qDebug() << ": Vehicle - Trip match not found at time " << time;
    //assignment_data_.print_state(time);
}

void Controlcenter::on_emptyTripVehicleMatchNotFound(double time)
{
    /*qDebug() << ": Vehicle - Trip match not found at time " << time;
    assignment_data_.print_state(time);*/
}


void Controlcenter::updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time)
{
	assert(bus->get_state() == newstate); //the newstate should be the current state of the bus
	assert(bus->is_flex_vehicle()); //fixed vehicles do not currently have a control center
	assert(connectedVeh_.count(bus->get_bus_id()) != 0); //assert that bus is connected to this control center (otherwise state change signal should have never been heard)

	//update the fleet state map, vehicles should be null before they are available, and null when they finish a trip and are copied
	if(oldstate != BusState::Null) 
	{
		assignment_data_.fleet_state[oldstate].erase(bus);
    }
	if(newstate != BusState::Null) 
	{
		assignment_data_.fleet_state[newstate].insert(bus);
    }

	if (newstate == BusState::OnCall)
	{
		emit newUnassignedVehicle(time);
	}
}

void Controlcenter::requestTrip(double time)
{
    tg_.requestTrip(assignment_data_, time) ? emit tripGenerated(time) : emit tripNotGenerated(time);    
}

void Controlcenter::requestEmptyTrip(double time)
{
    tg_.requestEmptyTrip(assignment_data_, time) ? emit emptyTripGenerated(time) : emit emptyTripNotGenerated(time);
}

void Controlcenter::requestRebalancingTrip(double time)
{
	tg_.requestRebalancingTrip(assignment_data_, time) ? emit emptyTripGenerated(time) : emit emptyTripNotGenerated(time);
}

void Controlcenter::matchVehiclesToTrips(double time)
{
    tvm_.matchVehiclesToTrips(assignment_data_, time) ? emit tripVehicleMatchFound(time) : emit tripVehicleMatchNotFound(time);
}

void Controlcenter::matchEmptyVehiclesToTrips(double time)
{
    tvm_.matchVehiclesToEmptyVehicleTrips(assignment_data_, time) ? emit tripVehicleMatchFound(time) : emit emptyTripVehicleMatchNotFound(time);
}

void Controlcenter::scheduleMatchedTrips(double time)
{
    vs_.scheduleMatchedTrips(assignment_data_, time);
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
