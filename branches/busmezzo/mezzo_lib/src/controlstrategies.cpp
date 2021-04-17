#include "controlstrategies.h"
#include "vehicle.h"
#include "busline.h"
#include "network.h"
#include "MMath.h"

#include "controlutilities.h"

int Request::id_counter = 0;
// Request
Request::Request(Passenger * pass_owner, int pass_id, int ostop_id, int dstop_id, int load, double t_departure, double t_generated) :
     pass_id(pass_id), ostop_id(ostop_id), dstop_id(dstop_id), load(load), time_desired_departure(t_departure), time_request_generated(t_generated),pass_owner(pass_owner)
{
    id = ++id_counter;
    qRegisterMetaType<Request>(); //register Request as a metatype for QT signal arguments
}

void Request::set_state(RequestState newstate)
{
    // @todo still need to add set state for servedunfinished, served finished, cancelled, rejected etc...
    if (state != newstate)
    {
        switch (newstate) //keep track of currently 'legal' state transitions @todo remove
        {
        case RequestState::Null:
            break;
        case RequestState::Unmatched:
            break;
        case RequestState::Assigned:
            assert(state == RequestState::Unmatched);
            break;
        case RequestState::Matched:
            assert(state == RequestState::Assigned);
            break;
        case RequestState::ServedUnfinished:
            break;
        case RequestState::ServedFinished:
            assert(state == RequestState::ServedUnfinished);
            break;
        case RequestState::Cancelled:
            break;
        case RequestState::Rejected:
            break;
        }
        //RequestState oldstate = state;
        state = newstate;
        //print_state();
        //total_time_spent_in_state[oldstate] += time - time_state_last_entered[oldstate]; // update cumulative time in each state
        //time_state_last_entered[newstate] = time; // start timer for newstate

        // emit stateChanged(this, oldstate, state_, time);
    }
}

void Request::print_state()
{
    qDebug() << "Request" << id << "for passenger" << pass_id  << "is" << state_to_QString(state);
}

QString Request::state_to_QString(RequestState state)
{
    QString state_s = "";
    switch (state)
    {
    case RequestState::Null:
        state_s = "Null";
        break;
    case RequestState::Unmatched:
        state_s = "Unmatched";
        break;
    case RequestState::Matched:
        state_s = "Matched";
        break;
    case RequestState::Assigned:
        state_s = "Assigned";
        break;
    case RequestState::ServedUnfinished:
        state_s = "ServedUnfinished";
        break;
    case RequestState::ServedFinished:
        state_s = "ServedFinished";
        break;
    case RequestState::Cancelled:
        state_s = "Cancelled";
        break;
    case RequestState::Rejected:
        state_s = "Rejected";
        break;
    }

    return state_s;
}


bool Request::operator==(const Request & rhs) const
{
    return (pass_owner == rhs.pass_owner && pass_id == rhs.pass_id && ostop_id == rhs.ostop_id && dstop_id == rhs.dstop_id && load == rhs.load && AproxEqual(time_desired_departure,rhs.time_desired_departure) && AproxEqual(time_request_generated,rhs.time_request_generated));
}

bool Request::operator<(const Request & rhs) const
{
    if (time_desired_departure != rhs.time_desired_departure)
        return time_desired_departure < rhs.time_desired_departure;
    else if (time_request_generated != rhs.time_request_generated)
        return time_request_generated < rhs.time_request_generated;
    else if (load != rhs.load)
        return load < rhs.load;
    else if (ostop_id != rhs.ostop_id)
        return ostop_id < rhs.ostop_id;
    else if (dstop_id != rhs.dstop_id)
        return dstop_id < rhs.dstop_id;
    else
        return pass_id < rhs.pass_id;
}

//TripGenerationStrategy
map<pair<int, int>, int> TripGenerationStrategy::countRequestsPerOD(const set<Request*, ptr_less<Request*> >& requestSet) const
{
    map<pair<int, int>, int> odcounts;

    for (const Request* req : requestSet)
    {
        int oid = req->ostop_id;
        int did = req->dstop_id;

        ++odcounts[make_pair(oid, did)];
    }
    return odcounts;
}


bool TripGenerationStrategy::line_exists_in_tripset(const set<Bustrip*, ptr_less<Bustrip*> >& tripSet, const Busline* line) const
{
    assert(line);
    if (line)
    {
        int lineid = line->get_id();
        auto it = find_if(tripSet.begin(), tripSet.end(),
            [lineid](Bustrip* trip)
            {
                return trip->get_line()->get_id() == lineid;
            }
        );

        if (it != tripSet.end()) {
            return true;
        }
    }
    return false;
}

vector<Busline*> TripGenerationStrategy::find_lines_connecting_stops(const vector<Busline*>& lines, int ostop_id, int dstop_id)
{
    vector<Busline*> lines_connecting_stops; // lines between stops given as input
    if (!lines.empty() && (ostop_id != dstop_id) )
    {
        if (cached_lines_connecting_stops.count(ostop_id) != 0 && cached_lines_connecting_stops[ostop_id].count(dstop_id) != 0)
        {
            lines_connecting_stops = cached_lines_connecting_stops[ostop_id][dstop_id];
        }
        else
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

                        if (dstop_it != line->stops.end()) { //if destination stop exists
                            lines_connecting_stops.push_back(line); //add line as a possible transit connection between these stops
                        }
                    }
                }
            }
            cached_lines_connecting_stops[ostop_id][dstop_id] = lines_connecting_stops; //cache result
        }
    }

    return lines_connecting_stops;
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
        auto* vs = new Visit_stop((stop_delta.first), arrival_time_at_stop);
        schedule.push_back(vs);
    }

    return schedule;
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
    trip->convert_stops_vector_to_map(); // TODO(MrLeffler): not sure why this is necessary but is done for other trips so
    trip->set_last_stop_visited(trip->stops.front()->first);  //sets last stop visited to the origin stop of the trip
    trip->set_flex_trip(true);
    trip->set_status(BustripStatus::Unmatched);

    return trip;

}

set<Bus*> TripGenerationStrategy::get_driving_vehicles(const map<BusState, set<Bus*> >& fleetState) const
{
    set<Bus*> drivingvehicles;
    for (auto vehgroup : fleetState) //collect all driving buses. TODO create a function for this, decide who gets to own it
    {
        BusState state = vehgroup.first;
        switch (state)
        {
        case BusState::DrivingEmpty:
        case BusState::DrivingFull:
        case BusState::DrivingPartiallyFull:
            drivingvehicles.insert(vehgroup.second.begin(), vehgroup.second.end());
            break;
        default:
            break;
        }
    }
    return drivingvehicles;
}

set<Bus*> TripGenerationStrategy::get_vehicles_enroute_to_stop(const Busstop* stop, const set<Bus*>& vehicles) const
{
    assert(stop);
    set<Bus*> enroute_vehicles;
    for (Bus* veh : vehicles)
    {
        if (veh->is_driving() && veh->get_on_trip()) //if the vehicle is driving and has a future stop visit planned (which should always be true if it is on a trip)
        {
            Busstop* next_stop = veh->get_next_stop();
            if (next_stop == stop) {
                enroute_vehicles.insert(veh);
            }
        }
    }
    return enroute_vehicles;
}

double TripGenerationStrategy::calc_route_travel_time(const vector<Link*>& routelinks, double time) const
{
    double expected_travel_time = 0;
    for (const Link* link : routelinks)
    {
        expected_travel_time += link->get_cost(time);
    }

    return expected_travel_time;
}

vector<Link*> TripGenerationStrategy::find_shortest_path_between_stops(Network* theNetwork, const Busstop* origin_stop, const Busstop* destination_stop, double start_time)
{
    assert(origin_stop);
    assert(destination_stop);
    assert(theNetwork);
    assert(start_time >= 0);

    vector<Link*> rlinks;
    int ostop_id = origin_stop->get_id();
    int dstop_id = destination_stop->get_id();
    if (cached_shortest_path_between_stops.count(ostop_id) != 0 && cached_shortest_path_between_stops[ostop_id].count(dstop_id) != 0)
    {
        rlinks = cached_shortest_path_between_stops[ostop_id][dstop_id]; // get cached results if called for this OD before
    }
    else
    {
        if (origin_stop && destination_stop)
        {
            int rootlink_id = origin_stop->get_link_id();
            int dest_node_id = destination_stop->get_dest_node()->get_id(); //!< @todo can change these to look between upstream and downstream junction nodes as well

            rlinks = theNetwork->shortest_path_to_node(rootlink_id, dest_node_id, start_time);

            cached_shortest_path_between_stops[ostop_id][dstop_id] = rlinks;
        }
    }
    return rlinks;
}

Busline* TripGenerationStrategy::find_shortest_busline(const vector<Busline*>& lines, double time) const
{
    Busline* shortestline = nullptr;
    double shortest_tt = HUGE_VAL; //shortest travel time

    for (Busline* line : lines)
    {
        double expected_tt = calc_route_travel_time(line->get_busroute()->get_links(), time); //expected travel time of a line
        if (expected_tt < shortest_tt)
        {
            shortest_tt = expected_tt;
            shortestline = line;
        }
    }

    return shortestline;
}

pair <Bus*, double> TripGenerationStrategy::get_nearest_vehicle(const Busstop* targetStop, set<Bus*> vehicles, Network* theNetwork, double time)
{
    pair<Bus*, double> closest(nullptr, numeric_limits<double>::max());

    for (auto v : vehicles)
    {
        Busstop* laststop = v->get_last_stop_visited(); //current stop if not driving
        vector<Link*> shortestpath = find_shortest_path_between_stops(theNetwork, laststop, targetStop, time);
        auto expected_tt = calc_route_travel_time(shortestpath, time);
        if (expected_tt < closest.second)
        {
            closest.first = v;
            closest.second = expected_tt;
        }
    }

    return closest;
}

bool NullTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    Q_UNUSED(assignment_data)
    Q_UNUSED(candidateServiceRoutes)
    Q_UNUSED(time)

    return false;
}

bool NaiveTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    auto unassignedRequests = cs_helper_functions::filterRequestsByState(assignment_data.active_requests, RequestState::Unmatched);
    if (!unassignedRequests.empty() && !candidateServiceRoutes.empty())
    {
        if (unassignedRequests.size() >= static_cast<std::size_t>(drt_min_occupancy)) //do not attempt to generate trip unless requestSet is greater than the desired occupancy
        {
            //find od pair with the highest frequency in requestSet
            map<pair<int, int>, int> odcounts = countRequestsPerOD(unassignedRequests);
            typedef pair<pair<int, int>, int> od_count;
            vector<od_count> sortedODcounts;

            for (auto it = odcounts.begin(); it != odcounts.end(); ++it)
            {
                sortedODcounts.push_back(*it);
            }
            assert(!sortedODcounts.empty());
            sort(sortedODcounts.begin(), sortedODcounts.end(), [](const od_count& p1, const od_count& p2)->bool {return p1.second > p2.second; }); //sort with the largest at the front

            if (sortedODcounts.front().second < ::drt_min_occupancy)
            {
                qDebug() << "Warning - Ignored trip generation call, maximum OD count in request set " << sortedODcounts.front().second << " is smaller than min occupancy" << ::drt_min_occupancy;
                return false;
            }

            /*attempt to generate trip for odpair with largest demand,
            if candidate lines connecting this od stop pair already have an unmatched trip existing for them
            continue to odpair with second highest demand ... etc.*/
            for (const od_count& od : sortedODcounts)
            {
                Busline* line = nullptr; //line that connects odpair without an unmatched trip for it yet
                int ostop_id = od.first.first;
                int dstop_id = od.first.second;

                vector<Busline*> lines_between_stops;
                lines_between_stops = find_lines_connecting_stops(candidateServiceRoutes, ostop_id, dstop_id); //check if any candidate service route connects the OD pair (even for segments of the route)

                if (lines_between_stops.size() > 1) //if there are several possible service routes connecting ostop and dstop sort these by shortest scheduled ivt to prioritize most direct routes
                {
                    sort(lines_between_stops.begin(), lines_between_stops.end(), [](const Busline* line1, const Busline* line2)
                        {
                            double ivt1 = 0.0;
                            double ivt2 = 0.0;

                            vector<pair<Busstop*, double> > deltas1 = line1->get_delta_at_stops();
                            vector<pair<Busstop*, double> > deltas2 = line2->get_delta_at_stops();

                            for_each(deltas1.begin(), deltas1.end(), [&ivt1](const pair<Busstop*, double> delta) { ivt1 += delta.second; }); //scheduled ivt for line1
                            for_each(deltas2.begin(), deltas2.end(), [&ivt2](const pair<Busstop*, double> delta) { ivt2 += delta.second; }); //scheduled ivt for line2

                            return ivt1 < ivt2;
                        }
                    );
                }

                bool found = false; //true only if one of the candidate lines that connects the od stop pair does not have a trip in the unmatchedTripSet yet
                for (Busline* candidateLine : lines_between_stops)//if all lines have a trip already planned for them without a vehicle then we have saturated planned trips for this od stop pair
                {
                    if (!line_exists_in_tripset(assignment_data.unmatched_trips, candidateLine))
                    {
                        line = candidateLine;
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    if (line)
                    {
                        vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
                        Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
                        assignment_data.unmatched_trips.insert(newtrip);//add this trip to the unmatchedTripSet
                        
                        auto affectedRequests = cs_helper_functions::filterRequestsByOD(unassignedRequests,ostop_id, dstop_id);
                        for (auto rq : affectedRequests)
                        {
                            rq->assigned_trip = newtrip;
                            rq->set_state(RequestState::Assigned);
                        }
                        vector<Bustrip* > tripchain = { newtrip }; //tripchain is only one trip long
                        cs_helper_functions::add_driving_roster_to_tripchain(tripchain); 
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool SimpleTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    // Steps:
    // 1. get unassigned requests
    // 2. get trips (i don't care if they are matched or unmatched...) and assign requests to existing trips
    // 3. create trips for those requests for which I have not found a trip
    // 4. assign requests to new trips
    
    bool trip_generated = false; // true if at least one trip has been generated
    // 1. get unassigned requests
    auto unassignedRequests = cs_helper_functions::filterRequestsByState(assignment_data.active_requests, RequestState::Unmatched);
    //qDebug() << "Size of unassignedRequests: " << unassignedRequests.size();
    if (!unassignedRequests.empty() && !candidateServiceRoutes.empty())
    {
        // 2. get trips (i don't care if they are matched or unmatched...) and assign requests to existing trips

        cs_helper_functions::assignRequestsToTripSet(assignment_data.active_requests, assignment_data.unmatched_trips);
        unassignedRequests = cs_helper_functions::filterRequestsByState(assignment_data.active_requests, RequestState::Unmatched); // redo the filtering after assignRequestsToTripSet
        // TODO: here we want to call it with the emptyTripset as well

        if (unassignedRequests.empty())
            return true;
        // 3. create trips for those requests for which I have not found a trip
        for (auto rq:unassignedRequests)
        {
            auto  lines_between_stops = find_lines_connecting_stops(candidateServiceRoutes, rq->ostop_id, rq->dstop_id); //check if any candidate service route connects the OD pair (even for segments of the route)
            if (lines_between_stops.empty())
            {
                DEBUG_MSG("No line found to serve request from stop " << rq->ostop_id << " to " << rq->dstop_id);
                rq->set_state(RequestState::Null);
            }
            // Now sort
            if (lines_between_stops.size() > 1) //if there are several possible service routes connecting ostop and dstop sort these by shortest scheduled ivt to prioritize most direct routes
            {
                sort(lines_between_stops.begin(), lines_between_stops.end(), [](const Busline* line1, const Busline* line2)
                {
                    double ivt1 = 0.0;
                    double ivt2 = 0.0;

                    vector<pair<Busstop*, double> > deltas1 = line1->get_delta_at_stops();
                    vector<pair<Busstop*, double> > deltas2 = line2->get_delta_at_stops();

                    for_each(deltas1.begin(), deltas1.end(), [&ivt1](const pair<Busstop*, double> delta) { ivt1 += delta.second; }); //scheduled ivt for line1
                    for_each(deltas2.begin(), deltas2.end(), [&ivt2](const pair<Busstop*, double> delta) { ivt2 += delta.second; }); //scheduled ivt for line2

                    return ivt1 < ivt2;
                }
                );
            }
            // take best candidateLine
            auto candidateLine = lines_between_stops.front();
            // create new trip
            vector<Visit_stop*> schedule = create_schedule(time, candidateLine->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
            Bustrip* newtrip = create_unassigned_trip(candidateLine, time, schedule); //create a new trip for this line using now as the dispatch time
            vector<Bustrip* > tripchain = { newtrip }; //tripchain is only one trip long
            cs_helper_functions::add_driving_roster_to_tripchain(tripchain);

            assignment_data.unmatched_trips.insert(newtrip);//add this trip to the unmatchedTripSet
            trip_generated = true;

         // 4. assign the request
            rq->assigned_trip = newtrip;
            newtrip->add_request(rq);
            rq->set_state(RequestState::Assigned);
            // TODO: now check all the remaining requests to see if they can be assigned as well.
            cs_helper_functions::assignRequestsToTrip(assignment_data.active_requests,newtrip);
            
        }
    }
    return trip_generated;
}


//Empty vehicle trip generation
NaiveEmptyVehicleTripGeneration::NaiveEmptyVehicleTripGeneration(Network* theNetwork) : theNetwork_(theNetwork){}
bool NaiveEmptyVehicleTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    if (!assignment_data.active_requests.empty() && !candidateServiceRoutes.empty()) //Reactive strategy so only when requests exist
    {
        if (assignment_data.fleet_state.find(BusState::OnCall) == assignment_data.fleet_state.end())  //a drt vehicle must have been initialized
            return false;

        if (assignment_data.fleet_state.at(BusState::OnCall).empty())  //a drt vehicle must be available
            return false;

        //find od pair with the highest frequency in requestSet (highest source of shareable demand)
        map<pair<int, int>, int> odcounts = countRequestsPerOD(assignment_data.active_requests);
        typedef pair<pair<int, int>, int> od_count;

        vector<od_count> sortedODcounts; //long-winded way to sort odcounts map by value rather than key
        for (auto it = odcounts.begin(); it != odcounts.end(); ++it)
        {
            sortedODcounts.push_back(*it);
        }
        assert(!sortedODcounts.empty());
        sort(sortedODcounts.begin(), sortedODcounts.end(),[](const od_count& p1, const od_count& p2)->bool {return p1.second > p2.second; }); //sort with the largest at the front

        if (sortedODcounts.front().second < ::drt_min_occupancy)
        {
            qDebug() << "Warning - Ignored trip generation call, maximum OD count in request set " << sortedODcounts.front().second << " is smaller than min occupancy" << ::drt_min_occupancy;
            return false;
        }

        int largest_demand_stop_id = sortedODcounts.front().first.first; //id of stop with largest source of demand
        const map<int, Busstop*> stopsmap = theNetwork_->get_stopsmap();
        assert(stopsmap.count(largest_demand_stop_id) != 0); //stop with the highest demand should be known to network

        Busstop* largest_demand_stop = theNetwork_->get_stopsmap()[largest_demand_stop_id];
        assert(largest_demand_stop->is_line_end()); //stop should be at the end of a transit route (otherwise no lines known to the Controlcenter will lead there)
        assert(largest_demand_stop->is_line_begin()); //stop should also be at the beginning of a transit route (otherwise no trip FROM this stop can be generated by a Controlcenter)

        //Check if there is already a vehicle on its way to the origin stop of the passenger
        set<Bus*> drivingveh = get_driving_vehicles(assignment_data.fleet_state);
        if(!get_vehicles_enroute_to_stop(largest_demand_stop, drivingveh).empty())
            return false;

        //find OnCall vehicle that is closest to largest source of demand
        set<Bus*> vehOnCall = assignment_data.fleet_state.at(BusState::OnCall); //vehicles that are currently available for empty vehicle rebalancing
        Bus* closestVehicle = nullptr; //vehicle that is closest to the stop with the largest unserved source of demand
        vector<Busline*> closestVehicle_serviceRoutes; //all service routes between the closest vehicle and the stop with the highest demand
        double shortest_tt = HUGE_VAL;

        for (Bus* veh : vehOnCall)
        {
            Busstop* vehloc = veh->get_last_stop_visited(); //location of on call vehicle
            vector<Busline*> vehicle_serviceRoutes; //all service routes between the vehicle location (current stop and opposing stop) and the demand source (current stop and opposing stop)

            //collect a vector of all possible service routes between the vehicles current location and the demand source
            vehicle_serviceRoutes = find_lines_connecting_stops(candidateServiceRoutes, vehloc->get_id(), largest_demand_stop->get_id());

            if (!vehicle_serviceRoutes.empty())
            {
                vector<Link*> shortestpath = find_shortest_path_between_stops(theNetwork_, vehloc, largest_demand_stop, time); //TODO: if all direct connects are included in the candidateServiceRoutes of the CC then can replace this by comparing the shortest ivts
                assert(!shortestpath.empty()); //the network should be completely connected
                double expected_tt = calc_route_travel_time(shortestpath, time);

                if (expected_tt < shortest_tt)
                {
                    shortest_tt = expected_tt;
                    closestVehicle = veh;
                    closestVehicle_serviceRoutes = vehicle_serviceRoutes;
                }
            }
        }

        //generate an empty vehicle trip between location of closest OnCall vehicle if found and the stop with the largest demand
        if (closestVehicle != nullptr)
        {
            Busline* line = find_shortest_busline(closestVehicle_serviceRoutes, time);
            assert(line);

            if (!line_exists_in_tripset(assignment_data.unmatched_trips, line)) //if this trip does not already exist in unmatchedRebalancing trip set
            {
                /*DEBUG_MSG("INFO::NaiveEmptyVehicleTripGeneration::calc_trip_generation " <<
                    "Empty vehicle trip found! Generating trip for line " << line->get_id()
                    << " between last location stop " << closestVehicle->get_last_stop_visited()->get_name()
                    << " of vehicle " << closestVehicle->get_bus_id() << " and source of demand stop " << largest_demand_stop->get_name());*/

                vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
                Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
                vector<Bustrip* > tripchain = { newtrip }; //tripchain is only one trip long
                cs_helper_functions::add_driving_roster_to_tripchain(tripchain);
                
                assignment_data.unmatched_trips.insert(newtrip);//add this trip to the unmatchedTripSet
                
                return true;
            }
        }
    }
    return false;
}

// SimpleEmptyVehicleTripGeneration
SimpleEmptyVehicleTripGeneration::SimpleEmptyVehicleTripGeneration(Network *theNetwork):theNetwork_(theNetwork) {}
bool SimpleEmptyVehicleTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    // 0. if no unmatched Trips, exit
    if (assignment_data.unmatched_trips.empty())
        return false;
    // 1. see if any vehicles are available, if no, exit
    if (assignment_data.fleet_state.find(BusState::OnCall) == assignment_data.fleet_state.end())  //a drt vehicle must have been initialized
        return false;
    if (assignment_data.fleet_state.at(BusState::OnCall).empty())  //a drt vehicle must be available
        return false;
    // 2. sort unMatchedTrips (by nr of requests)
    set<Bustrip*,compareBustripByNrRequests> sortedTrips (assignment_data.unmatched_trips.begin(), assignment_data.unmatched_trips.end());
    //set<Bustrip*,compareBustripByMaxWait> sortedTrips (unmatchedTripSet.begin(), unmatchedTripSet.end(),time) ;
    //set<Bustrip*,compareBustripByCumulativeWait> sortedTrips (unmatchedTripSet.begin(), unmatchedTripSet.end(),time) ;

    // 3. find nearest vehicle to the unMatchedTrip location (For now: hope for the best :)
    auto selectedTrip = *(sortedTrips.begin());
    Busstop*  tripStartStop = selectedTrip->stops.front()->first;
    auto onCallVehicles = assignment_data.fleet_state.at(BusState::OnCall);
    auto nearestOnCall = get_nearest_vehicle(tripStartStop,onCallVehicles,theNetwork_,time);
    if (nearestOnCall.first == nullptr)
        return false;
    // 4. generate the empty trip
    Busstop* vehicleStartStop = nearestOnCall.first->get_last_stop_visited();
    auto vehicle_serviceRoutes = find_lines_connecting_stops(candidateServiceRoutes,vehicleStartStop->get_id(),tripStartStop->get_id());
    Busline* line = find_shortest_busline(vehicle_serviceRoutes, time);
    assert(line);
 
    auto schedule = create_schedule(time,line->get_delta_at_stops());
    Bustrip* newTrip = create_unassigned_trip(line,time,schedule);

    // 5. associate the chained trips by modifying Bustrip::driving_roster
    vector<Bustrip*> tripchain = { newTrip, selectedTrip };
    cs_helper_functions::update_schedule(selectedTrip, newTrip->stops.back()->second); // Get last stop arrival time of newtrip, and then update the chained selectedTrip with this as dispatch time
    cs_helper_functions::add_driving_roster_to_tripchain(tripchain); // newTrip and selectedTrip now have pointers to eachother as well as info of which order they are in

    // 6. Add newTrip to the unmatchedEmptyTripset
    assignment_data.unmatched_empty_trips.insert(newTrip); // now unmatchedEmptyTripSet will signal matcher to match vehicle to both trips via Bustrip::driving_roster 

    // 7. remove selectedTrip from unMatchedTripset
    assignment_data.unmatched_trips.erase(selectedTrip); 

    // 8. adding potential requests to the empty trip
    cs_helper_functions::assignRequestsToTrip(assignment_data.active_requests,newTrip);

    return true; // emits Signal that empty trip was generated, matcher does the rest.
}

//MaxWaitEmptyVehicleTripGeneration
MaxWaitEmptyVehicleTripGeneration::MaxWaitEmptyVehicleTripGeneration(Network *theNetwork):theNetwork_(theNetwork) {}
bool MaxWaitEmptyVehicleTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    // 0. if no unmatched Trips, exit
    if (assignment_data.unmatched_trips.empty())
        return false;
    // 1. see if any vehicles are available, if no, exit
    if (assignment_data.fleet_state.find(BusState::OnCall) == assignment_data.fleet_state.end())  //a drt vehicle must have been initialized
        return false;
    if (assignment_data.fleet_state.at(BusState::OnCall).empty())  //a drt vehicle must be available
        return false;
    // 2. sort unMatchedTrips (by maximum waiting time)
    set<Bustrip*,compareBustripByMaxWait> sortedTrips (assignment_data.unmatched_trips.begin(), assignment_data.unmatched_trips.end(),time) ;

    // 3. find nearest vehicle to the unMatchedTrip location (For now: hope for the best :)
    auto selectedTrip = *(sortedTrips.begin());
    Busstop*  tripStartStop = selectedTrip->stops.front()->first;
    auto onCallVehicles = assignment_data.fleet_state.at(BusState::OnCall);
    auto nearestOnCall = get_nearest_vehicle(tripStartStop,onCallVehicles,theNetwork_,time);
    if (nearestOnCall.first == nullptr)
        return false;
    // 4. generate the empty trip
    Busstop* vehicleStartStop = nearestOnCall.first->get_last_stop_visited();
    auto vehicle_serviceRoutes = find_lines_connecting_stops(candidateServiceRoutes,vehicleStartStop->get_id(),tripStartStop->get_id());
    Busline* line = find_shortest_busline(vehicle_serviceRoutes, time);
    assert(line);

    auto schedule = create_schedule(time,line->get_delta_at_stops());
    Bustrip* newTrip = create_unassigned_trip(line,time,schedule);

    // 5. associate the chained trips by modifying Bustrip::driving_roster
    vector<Bustrip*> tripchain = { newTrip, selectedTrip };
    cs_helper_functions::update_schedule(selectedTrip, newTrip->stops.back()->second); // Get last stop arrival time of newtrip, and then update the chained selectedTrip with this as dispatch time
    cs_helper_functions::add_driving_roster_to_tripchain(tripchain); // newTrip and selectedTrip now have pointers to eachother as well as info of which order they are in

    // 6. Add newTrip to the unmatchedEmptyTripset
    assignment_data.unmatched_empty_trips.insert(newTrip); // now unmatchedEmptyTripSet will signal matcher to match vehicle to both trips via Bustrip::driving_roster

    // 7. remove selectedTrip from unMatchedTripset
    assignment_data.unmatched_trips.erase(selectedTrip);

    // 8. adding potential requests to the empty trip
    cs_helper_functions::assignRequestsToTrip(assignment_data.active_requests,newTrip);

    return true; // emits Signal that empty trip was generated, matcher does the rest.
}

CumulativeWaitEmptyVehicleTripGeneration::CumulativeWaitEmptyVehicleTripGeneration(Network *theNetwork):theNetwork_(theNetwork) {}
bool CumulativeWaitEmptyVehicleTripGeneration::calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time)
{
    // 0. if no unmatched Trips, exit
    if (assignment_data.unmatched_trips.empty())
        return false;
    // 1. see if any vehicles are available, if no, exit
    if (assignment_data.fleet_state.find(BusState::OnCall) == assignment_data.fleet_state.end())  //a drt vehicle must have been initialized
        return false;
    if (assignment_data.fleet_state.at(BusState::OnCall).empty())  //a drt vehicle must be available
        return false;
    // 2. sort unMatchedTrips (by cumulative waiting time)
    set<Bustrip*,compareBustripByCumulativeWait> sortedTrips (assignment_data.unmatched_trips.begin(), assignment_data.unmatched_trips.end(),time);

    // 3. find nearest vehicle to the unMatchedTrip location (For now: hope for the best :)
    auto selectedTrip = *(sortedTrips.begin());
    Busstop*  tripStartStop = selectedTrip->stops.front()->first;
    auto onCallVehicles = assignment_data.fleet_state.at(BusState::OnCall);
    auto nearestOnCall = get_nearest_vehicle(tripStartStop,onCallVehicles,theNetwork_,time);
    if (nearestOnCall.first == nullptr)
        return false;
    // 4. generate the empty trip
    Busstop* vehicleStartStop = nearestOnCall.first->get_last_stop_visited();
    auto vehicle_serviceRoutes = find_lines_connecting_stops(candidateServiceRoutes,vehicleStartStop->get_id(),tripStartStop->get_id());
    Busline* line = find_shortest_busline(vehicle_serviceRoutes, time);
    assert(line);

    auto schedule = create_schedule(time,line->get_delta_at_stops());
    Bustrip* newTrip = create_unassigned_trip(line,time,schedule);

    // 5. associate the chained trips by modifying Bustrip::driving_roster
    vector<Bustrip*> tripchain = { newTrip, selectedTrip };
    cs_helper_functions::update_schedule(selectedTrip, newTrip->stops.back()->second); // Get last stop arrival time of newtrip, and then update the chained selectedTrip with this as dispatch time
    cs_helper_functions::add_driving_roster_to_tripchain(tripchain); // newTrip and selectedTrip now have pointers to eachother as well as info of which order they are in

    // 6. Add newTrip to the unmatchedEmptyTripset
    assignment_data.unmatched_empty_trips.insert(newTrip); // now unmatchedEmptyTripSet will signal matcher to match vehicle to both trips via Bustrip::driving_roster

    // 7. remove selectedTrip from unMatchedTripset
    assignment_data.unmatched_trips.erase(selectedTrip);

    // 8. adding potential requests to the empty trip
    cs_helper_functions::assignRequestsToTrip(assignment_data.active_requests,newTrip);

    return true; // emits Signal that empty trip was generated, matcher does the rest.
}


//MatchingStrategy
void MatchingStrategy::assign_oncall_vehicle_to_trip(Busstop* currentStop, Bus* transitveh, Bustrip* trip, double starttime)
{
    assert(transitveh);
    assert(trip);
    if (transitveh && trip) //simply to remove warnings, still want things to abort for debug build with asserts above
    {
        assert(!transitveh->get_curr_trip()); //this particular bus instance (remember there may be copies of it if there is a trip chain, should not have a trip)
        assert(transitveh->is_oncall());
        //qDebug() << "Matching vehicle " << transitveh->get_bus_id() << " with trip from stop " << QString::fromStdString(trip->stops.front()->first->get_name()) << " to " << QString::fromStdString(trip->stops.back()->first->get_name());

        // assign bus to the first trip in chain
        trip->set_busv(transitveh); //assign bus to the trip
        transitveh->set_curr_trip(trip); //assign trip to the bus
        trip->set_bustype(transitveh->get_bus_type());//set the bus type of the trip (so trip has access to this bustypes dwell time function)
        trip->get_busv()->set_on_trip(true); //flag the bus as busy for all trips except the first on its chain (TODO might move to dispatcher)

        //!< moved the generation of driving_roster to where TripGenerationStrategy::create_unassigned_trip() calls occur
        //vector<Start_trip*> driving_roster; //contains all other trips in this trips chain (if there are any) (TODO might move to dispatcher)
        //auto* st = new Start_trip(trip, starttime);
        //driving_roster.push_back(st);
        //trip->add_trips(driving_roster); //save the driving roster at the trip level to conform to interfaces of Busline::execute, Bustrip::activate etc.
        
        //double delay = starttime - trip->get_starttime();
        trip->set_starttime(starttime); //reset scheduled dispatch from origin to given starttime

        if (!currentStop->remove_unassigned_bus(transitveh, starttime)) //bus is no longer unassigned and is removed from vector of unassigned buses at whatever stop the vehicle is waiting on call at
            //trip associated with this bus will be added later to expected_bus_arrivals via
            //Busline::execute -> Bustrip::activate -> Origin::insert_veh -> Link::enter_veh -> Bustrip::book_stop_visit -> Busstop::book_bus_arrival
        {
            DEBUG_MSG_V("Busstop::remove_unassigned_bus failed for bus " << transitveh->get_bus_id() << " and stop " << currentStop->get_id());
        }

        // update state of requests (if trip was previously associated with a bundle of requests) in trip-chain
        vector<Request*> all_req = cs_helper_functions::getRequestsInTripChain(trip->driving_roster);
        for (auto rq : all_req)
        {
            assert(rq->state == RequestState::Assigned); //should have been set to assigned when trip was generated
            rq->set_state(RequestState::Matched);
        }

        cs_helper_functions::set_status_of_tripchain(trip->driving_roster,BustripStatus::Matched); //update status of trips in chain to matched
    }
}

Bustrip* MatchingStrategy::find_earliest_planned_trip(const set<Bustrip*>& trips) const
{
    Bustrip* latest = nullptr;
    if (!trips.empty())
    {
        auto maxit = max_element(trips.begin(), trips.end(), [](const Bustrip* t1, const Bustrip* t2)->bool
            {
                return t1->get_starttime() < t2->get_starttime();
            }
        );
        latest = *maxit;
    }
    return latest;
}


bool NullMatching::find_tripvehicle_match(DRTAssignmentData& assignment_data, Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time)
{
    Q_UNUSED(unmatchedTrip)
    Q_UNUSED(veh_per_sroute)
    Q_UNUSED(time)
    Q_UNUSED(assignment_data)

    return false;
}

bool NaiveMatching::find_tripvehicle_match(DRTAssignmentData& assignment_data, Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time)
{
    Q_UNUSED(assignment_data);

    //attempt to match unmatchedTrip with first on-call vehicle found at the origin stop of the trip
    if ((unmatchedTrip != nullptr) && !veh_per_sroute.empty())
    {
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
                    Bus* veh = (*c_bus_it);
                    assign_oncall_vehicle_to_trip(origin_stop, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

                    return true;
                }
            }
        }
    }
    return false;
}

//SchedulingStrategy
bool SchedulingStrategy::book_trip_dispatch(DRTAssignmentData& assignment_data, Eventlist* eventlist, Bustrip* trip)
{
    assert(eventlist);
    assert(trip);
    bool scheduled_trip_success = false;
    if (eventlist && trip)
    {
        for (auto trip_it = trip->driving_roster.begin(); trip_it != trip->driving_roster.end(); ++trip_it)
        {
            Start_trip* trip_dispatch = (*trip_it);
            Bustrip* unscheduled_trip = trip_dispatch->first;
            Busline* line = unscheduled_trip->get_line();
            double starttime = unscheduled_trip->get_starttime(); // @note the start time here is the one passed through the assignment pipeline, may no longer match the dispatch time of driving roster
            assert(line);

            if (line)
            {
                assert(line->is_flex_line());

                //DEBUG_MSG("INFO::SchedulingStrategy::book_trip_dispatch is scheduling trip " << trip->get_id() << " with start time " << starttime);

                line->add_trip(unscheduled_trip, starttime); //insert trip into the main trips list of the line
                
                if (trip_dispatch == trip->driving_roster.front()) // Busline event added only for first trip in chain, the others handled by Bus::advance_curr_trip
                {
                    eventlist->add_event(starttime, line); //book the activation of this trip in the eventlist
                    unscheduled_trip->set_scheduled_for_dispatch(true); //scheduled for dispatch is set in Busline::execute?
                    scheduled_trip_success = true;
                    assignment_data.unscheduled_trips.erase(trip); //trip is now scheduled for dispatch
                    assignment_data.active_trips.insert(trip); //trip should now be added as an active trip
                }
                else
                {
                    //!< @todo Kindof hacky, but add an 'ambitious' dispatch event from Busline for now to mimic previous behavior as closely as possible, activated Busline and have Busline::curr_trip always pointing to the correct trip to be dispatched
                    eventlist->add_event(trip->get_starttime()+0.0001, line); // adds a busline event with the starttime of the first trip in chain + 0.0001 seconds.... Should activate the line if not activated, but ignore activation of the trip (with busv == nullptr)
                    unscheduled_trip->set_scheduled_for_dispatch(true); // this unscheduled trip (chained) is guaranteed to be activated via Bus::advance_curr_trip
                    assignment_data.unscheduled_trips.erase(trip); //trip is now scheduled for dispatch
                    assignment_data.active_trips.insert(trip); //trip should now be added as an active trip
                }
            }
        }
    }
      
    return scheduled_trip_success;
}

bool NullScheduling::schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, const double time)
{
    Q_UNUSED(eventlist)
    Q_UNUSED(assignment_data)
    Q_UNUSED(time)

    return false;
}


bool NaiveScheduling::schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, const double time)
{
    assert(eventlist);
    bool scheduled_trip = false; // true if at least one trip has been scheduled
    set<Bustrip*, compareBustripByEarliestStarttime> sortedTrips(assignment_data.unscheduled_trips.begin(), assignment_data.unscheduled_trips.end()); // process uncheduledTrips in order of earliest to latest starttime
    assert(sortedTrips.size() == assignment_data.unscheduled_trips.size());

    for (auto trip : sortedTrips)
    {
        Bus* bus = trip->get_busv();
        //DEBUG_MSG(endl << "INFO::NaiveScheduling::schedule_trips - scheduling matched trips for dispatch at time " << time);
        //check if the bus associated with this trip is available
        if (bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id()) //vehicle should already be located at the first stop of the trip
        {
            if (trip->get_starttime() < time)  //if scheduling call was made after the original planned dispatch of the trip
                cs_helper_functions::update_schedule(trip, time); //update schedule for dispatch and stop visits according to new starttime

            if (!book_trip_dispatch(assignment_data, eventlist, trip))
            {
                qDebug() << "Warning, NaiveScheduling::book_trip_dispatch() failed for trip" << trip->get_id() << "at time" << time;
                continue;
            }
            scheduled_trip = true;
        }
        else
        {
            DEBUG_MSG_V("ERROR::NaiveScheduling::schedule_trips - Bus is unavailable for matched trip! Figure out why! Aborting...");
            abort();
        }
    }

    return scheduled_trip;
}

bool LatestDepartureScheduling::schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, double time)
{
    /**
    * @todo
    *   - Currently schedules dispatch based on the latest desired departure time of any request in the entire TRIPCHAIN. E.g. a request with a delayed departure might be sent for a vehicle
    *       that must first make an empty trip to pick up the passenger. The empty trip will then be delayed based on the desired departure even when the vehicle should be dispatched immediately...
    *   - Might need to change both the behavior of busstrip execute (that always activates trips immediately
    *   - Either that or add a 'holding at stop' behavior to wait for any matched passengers...
    * 
    *   1. Change LatestDepartureScheduling to only delay dispatch based on first trip in chain (done)
    *   2. Update Busstop::calc_exiting_time() with holding behavior if a flex trip & there are additional matched pass arriving.
    *       - See how the double call to Busstop::passenger_activity_at_stop() effects outputs and behavior with DRT etc...
    *       - Perhaps set up some tests or simple demand-supply instances to check this....
    */

    assert(eventlist);
    bool scheduled_trip = false; // true if at least one trip has been scheduled
    set<Bustrip*, compareBustripByEarliestStarttime> sortedTrips(assignment_data.unscheduled_trips.begin(), assignment_data.unscheduled_trips.end()); // process uncheduledTrips in order of earliest to latest starttime
    assert(sortedTrips.size() == assignment_data.unscheduled_trips.size());
    qDebug() << "LatestDepartureScheduling trips at time" << time << " size of unscheduledTrips: " << sortedTrips.size();

    for (auto trip : sortedTrips)
    {
        Bus* bus = trip->get_busv();
        
        //check if the bus associated with this trip is available
        if (bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id()) //vehicle should already be located at the first stop of the trip
        {
            //find the latest desired departure time among passengers assigned to the first trip in the tripchain
            qDebug() << "\tOriginal starttime of trip" << trip->get_id() << ": " << trip->get_starttime();
            double latest_desired_dep = 0.0;
            vector<Request*> assigned_reqs = trip->get_requests();
            qDebug() << "\tNumber of requests assigned to first trip: " << assigned_reqs.size();
            for (const Request* req : assigned_reqs)
            {
                assert(req->state == RequestState::Matched); //all requests should be matched at this point
                if (req->time_desired_departure > latest_desired_dep)
                    latest_desired_dep = req->time_desired_departure;
            }
            qDebug() << "\tLatest desired departure of trip" << trip->get_id() << ": " << latest_desired_dep;

            //update the schedule of the trip such that dispatch of the tripchain is either now (if the trip was delayed) or at the latest_desired_departure of assigned passengers
            if (latest_desired_dep < time) 
                cs_helper_functions::update_schedule(trip, time); //update schedule for dispatch and stop visits according to new starttime
            else
                cs_helper_functions::update_schedule(trip, latest_desired_dep);


            if (!book_trip_dispatch(assignment_data, eventlist, trip))
            {
                qDebug() << "Warning, LatestDepartureScheduling::book_trip_dispatch() failed for trip" << trip->get_id() << "at time" << time;
                continue;
            }

            scheduled_trip = true;
        }
        else
        {
            DEBUG_MSG_V("ERROR::LatestDepartureScheduling::schedule_trips - Bus is unavailable for matched trip! Figure out why! Aborting...");
            abort();
        }
    }

    return scheduled_trip;
}
