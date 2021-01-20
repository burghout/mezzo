#include "controlstrategies.h"
#include "vehicle.h"
#include "busline.h"
#include "network.h"
#include "MMath.h"


// Helper functions
namespace helper_functions
{
    //!< takes trip that already has a preliminary schedule for both dispatch and stop visits, and updates this schedule given a new start time
    void update_schedule(Bustrip* trip, double new_starttime)
    {
        assert(trip);
        assert(new_starttime >= 0);

        //!< @todo might have to add updates to the driving roster of the trip here now as well....

        if (trip && new_starttime >= 0)
        {
            if (trip->get_starttime() != new_starttime)
            {
                double delta = new_starttime - trip->get_starttime(); //positive to shift the schedule later in time, and negative if it should shift earlier in time
                vector<Visit_stop*> schedule = trip->stops;

                //add the delta to all the scheduled stop visits
                for (Visit_stop* stop_arrival : schedule)
                {
                    stop_arrival->second += delta;
                }

                trip->set_starttime(new_starttime); //set planned dispatch to new start time
                trip->add_stops(schedule); //overwrite old schedule with the new scheduled stop visits
                trip->convert_stops_vector_to_map(); //stops map used in some locations, stops vector used in others
            }
        }
        else
        {
            DEBUG_MSG("WARNING::update schedule - null trip or invalid starttime arguments");
        }
    }

    //!< Takes a vector of Bustrips and connects them via their driving_roster attribute in the order of the tripchain (i.e. index 0 is first trip, index 1 the second etc.)
    void add_driving_roster_to_tripchain(const vector<Bustrip*>& tripchain)
    {
        //!< @todo assumes that the trip starttimes make sense I suppose. Not sure if this matters yet, but could in case a trip is 'early for dispatch'
        vector<Start_trip*> driving_roster;
        // build the driving roster (expected dispatch of chained trips between different lines)
        for (auto trip : tripchain)
        {
            // delete old driving roster if it exists
            if (!trip->driving_roster.empty() && !trip->deleted_driving_roster) //!< ugly hack to ensure that we do not delete driving roster twice, in case delete has been called for two trips in the same driving roster
            {
                for (auto trip_start : trip->driving_roster)
                {
                    trip_start->first->deleted_driving_roster = true;
                }
                for (Start_trip* trip_start : trip->driving_roster)
                {
                    delete trip_start;
                }
            }
            trip->driving_roster.clear();
            trip->deleted_driving_roster = false; // now reset driving roster deletion flag to false

            Start_trip* st = new Start_trip(trip, trip->get_starttime());
            driving_roster.push_back(st);
        }
        // save the driving roster at the trip level for each trip on the chain
        for (auto it = driving_roster.begin(); it != driving_roster.end(); ++it)
        {
            (*it)->first->add_trips(driving_roster);
        }

        //result should be that each trip in "tripchain" knows of eachother and we can throw this into the Busline::execute, Bustrip::activate, Bus::advance_curr_trip loop
    }

    std::set<Request*, ptr_less<Request*>> filterRequestsByState(const set<Request*, ptr_less<Request*>>& oldSet, RequestState state)
    {
        set <Request*, ptr_less<Request*>> newSet;
        std::copy_if(oldSet.begin(), oldSet.end(), std::inserter(newSet, newSet.end()), [state](Request* value) {return value->state == state; });
        return newSet;
    }

    std::set<Request*, ptr_less<Request*>> filterRequestsByOD(const set<Request*, ptr_less<Request*>>& oldSet, int o_id, int d_id)
    {
        set <Request*, ptr_less<Request*>> newSet;
        std::copy_if(oldSet.begin(), oldSet.end(), std::inserter(newSet, newSet.end()), [o_id, d_id](Request* value) {return (value->ostop_id == o_id) && (value->dstop_id == d_id); });
        return newSet;
    }

    // Assign requests to trips
    void assignRequestsToTrip(const set<Request*,ptr_less<Request*>>& requestSet, Bustrip* tr)
    {
        auto unassignedRequests = helper_functions::filterRequestsByState(requestSet, RequestState::Unmatched); // redo the filtering each time
        // TODO Add also for emptyTrips followed by a "selectedTrip"
        for (auto rq:unassignedRequests)
        {
            auto startstop = tr->stops.front()->first; // if trip starts at same stop
            if (startstop->get_id() == rq->ostop_id)
            {
                auto downstreamstops = tr->get_downstream_stops();
                if (downstreamstops.end() != find_if(downstreamstops.begin(), downstreamstops.end(),
                                                     [rq](Busstop* st) { return rq->dstop_id == st->get_id();}) )
                { // if rq destination stop is in the downstream stops
                    rq->assigned_trip = tr;
                    rq->set_state(RequestState::Assigned);
                    tr->add_request(rq);
                }
            }
        }

    }

    void assignRequestsToTripSet(const set<Request*,ptr_less<Request*>>& requestSet, const set<Bustrip*>& tripSet)
    {
        for (auto tr:tripSet)
        {
           assignRequestsToTrip(requestSet,tr);
        }
    }

} // end namespace helper_functions

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


struct compareBustripByNrRequests
{
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_requests().size() != rhs->get_requests().size())
            return lhs->get_requests().size() > rhs->get_requests().size();
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
};

struct compareBustripByEarliestStarttime
{
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_starttime() != rhs->get_starttime())
            return lhs->get_starttime() < rhs->get_starttime();
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
};

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
    if (state != newstate)
    {
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
    cout << endl << "Request " << pass_id << " is ";
    //qDebug() << Request::state_to_string(state);
    switch (state)
    {
    case RequestState::Unmatched:
        cout << "Unmatched";
        break;
    case RequestState::Null:
        cout << "Null";
        break;
    case RequestState::Assigned:
        cout << "Assigned";
        break;
    case RequestState::Matched:
        cout << "Matched";
        break;
        //	default:
        //		DEBUG_MSG_V("Something went very wrong");
        //		abort();
    }
    cout << endl;
}

QString Request::state_to_string(RequestState state)
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
map<pair<int, int>, int> TripGenerationStrategy::countRequestsPerOD(const set<Request *, ptr_less<Request *> > &requestSet) const
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


bool TripGenerationStrategy::line_exists_in_tripset(const set<Bustrip*>& tripSet, const Busline* line) const
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

vector<Link*> TripGenerationStrategy::find_shortest_path_between_stops(Network* theNetwork, const Busstop* origin_stop, const Busstop* destination_stop, const double start_time) const
{
    assert(origin_stop);
    assert(destination_stop);
    assert(theNetwork);
    assert(start_time >= 0);

    vector<Link*> rlinks;
    if(origin_stop && destination_stop)
    {
        int rootlink_id = origin_stop->get_link_id();
        int dest_node_id = destination_stop->get_dest_node()->get_id(); //!< @todo can change these to look between upstream and downstream junction nodes as well

        rlinks = theNetwork->shortest_path_to_node(rootlink_id, dest_node_id, start_time);
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

pair <Bus*,double> TripGenerationStrategy::get_nearest_vehicle(const Busstop *targetStop, set<Bus*> vehicles, Network* theNetwork, double time) const
{
    pair<Bus*,double> closest (nullptr,numeric_limits<double>::max());

    for (auto v:vehicles)
    {
        Busstop* laststop = v->get_last_stop_visited(); //current stop if not driving
        vector<Link*> shortestpath = find_shortest_path_between_stops(theNetwork,laststop, targetStop, time);
        auto expected_tt = calc_route_travel_time(shortestpath, time);
        if (expected_tt < closest.second)
        {
            closest.first = v;
            closest.second = expected_tt;
        }

    }


    return closest;
}

bool NullTripGeneration::calc_trip_generation(const set<Request*, ptr_less<Request*>>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet, set<Bustrip*>& unmatchedEmptyTripSet)
{
    Q_UNUSED(requestSet)
    Q_UNUSED(candidateServiceRoutes)
    Q_UNUSED(fleetState)
    Q_UNUSED(time)
    Q_UNUSED(unmatchedTripSet)
    Q_UNUSED(unmatchedEmptyTripSet)

    return false;
}

bool NaiveTripGeneration::calc_trip_generation(const set<Request*,ptr_less<Request*>>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet, set<Bustrip*>& unmatchedEmptyTripSet)
{
    Q_UNUSED(fleetState)
    Q_UNUSED(unmatchedEmptyTripSet)
    // UPDATE TO ONLY unmatched requests
    if (!requestSet.empty() && !candidateServiceRoutes.empty())
    {
        if (requestSet.size() >= static_cast<std::size_t>(drt_min_occupancy)) //do not attempt to generate trip unless requestSet is greater than the desired occupancy
        {
            //DEBUG_MSG(endl << "INFO::NaiveTripGeneration::calc_trip_generation - finding possible passenger carrying trip at time " << time);
            //find od pair with the highest frequency in requestSet
            map<pair<int, int>, int> odcounts = countRequestsPerOD(requestSet);
            typedef pair<pair<int, int>, int> od_count;
            vector<od_count> sortedODcounts;

            for (auto it = odcounts.begin(); it != odcounts.end(); ++it)
            {
                sortedODcounts.push_back(*it);
            }
            assert(!sortedODcounts.empty());
            sort(sortedODcounts.begin(), sortedODcounts.end(),
                 [](const od_count& p1, const od_count& p2)->bool {return p1.second > p2.second; }); //sort with the largest at the front

            if (sortedODcounts.front().second < ::drt_min_occupancy)
            {
                DEBUG_MSG("No trip generated! Maximum OD count in request set " << sortedODcounts.front().second << " is smaller than min occupancy " << ::drt_min_occupancy);
                return false;
            }

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
                    if (!line_exists_in_tripset(unmatchedTripSet, candidateLine))
                    {
                        line = candidateLine;
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    assert(line);
                    if (line)
                    {
                        //DEBUG_MSG("INFO::NaiveTripGeneration::calc_trip_generation - Trip found! Generating trip for line " << line->get_id());

                        vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
                        Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
                        unmatchedTripSet.insert(newtrip);//add this trip to the unmatchedTripSet
                        // WILCO TODO ADD TO REQUEST BOOKKEEPING
                        auto affectedRequests = helper_functions::filterRequestsByOD(requestSet,ostop_id, dstop_id);
                        for (auto rq:affectedRequests)
                        {
                            rq->assigned_trip = newtrip;
                            rq->set_state(RequestState::Assigned);
                        }
                        vector<Bustrip* > tripchain = { newtrip }; //tripchain is only one trip long
                        helper_functions::add_driving_roster_to_tripchain(tripchain); 
                        return true;
                    }
                }
            }
            //DEBUG_MSG("INFO::NaiveTripGeneration::calc_trip_generation - No trip found!");
        }
    }
    return false;
}

bool SimpleTripGeneration::calc_trip_generation(const set<Request*,ptr_less<Request*>>& requestSet, const vector<Busline*>& candidateServiceRoutes,
                                                const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet, set<Bustrip*>& unmatchedEmptyTripSet)
{
    // Steps:
    // 1. get unassigned requests
    // 2. get trips (i don't care if they are matched or unmatched...) and assign requests to existing trips
    // 3. create trips for those requests for which I have not found a trip
    // 4. assign requests to new trips
    Q_UNUSED(fleetState)
    Q_UNUSED(unmatchedEmptyTripSet)
    // 1. get unassigned requests
    auto unassignedRequests = helper_functions::filterRequestsByState(requestSet, RequestState::Unmatched);
    if (!unassignedRequests.empty() && !candidateServiceRoutes.empty())
    {
        // 2. get trips (i don't care if they are matched or unmatched...) and assign requests to existing trips

        helper_functions::assignRequestsToTripSet(requestSet,unmatchedTripSet);
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
            helper_functions::add_driving_roster_to_tripchain(tripchain);

            unmatchedTripSet.insert(newtrip);//add this trip to the unmatchedTripSet
         // 4. assign the request
            rq->assigned_trip = newtrip;
            newtrip->add_request(rq);
            rq->set_state(RequestState::Assigned);
            // TODO: now check all the remaining requests to see if they can be assigned as well.
            helper_functions::assignRequestsToTrip(requestSet,newtrip);
//            auto affectedRequests = filterRequestsByOD(unassignedRequests,rq->ostop_id, rq->dstop_id);
//            for (auto arq:affectedRequests)
//            {
//                arq->assigned_trip = newtrip;
//                newtrip->add_request(arq);
//                arq->set_state(RequestState::Assigned);
//            }
            return true;
        }
    }
    return false;
}






//Empty vehicle trip generation
NaiveEmptyVehicleTripGeneration::NaiveEmptyVehicleTripGeneration(Network* theNetwork) : theNetwork_(theNetwork){}

bool NaiveEmptyVehicleTripGeneration::calc_trip_generation(const set<Request*,ptr_less<Request*>>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, 
                                                           const double time, set<Bustrip*>& unmatchedTripSet, set<Bustrip*>& unmatchedEmptyTripSet)
{
    Q_UNUSED(unmatchedEmptyTripSet)

    if (!requestSet.empty() && !candidateServiceRoutes.empty()) //Reactive strategy so only when requests exist
    {
        if (fleetState.find(BusState::OnCall) == fleetState.end())  //a drt vehicle must have been initialized
            return false;

        if (fleetState.at(BusState::OnCall).empty())  //a drt vehicle must be available
            return false;

        //DEBUG_MSG(endl << "INFO::NaiveEmptyVehicleTripGeneration::calc_trip_generation - finding possible rebalancing trip at time " << time);
        //find od pair with the highest frequency in requestSet (highest source of shareable demand)
        map<pair<int, int>, int> odcounts = countRequestsPerOD(requestSet);
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
            DEBUG_MSG("No trip generated! Maximum OD count in request set " << sortedODcounts.front().second << " is smaller than min occupancy " << ::drt_min_occupancy);
            return false;
        }

        int largest_demand_stop_id = sortedODcounts.front().first.first; //id of stop with largest source of demand
        map<int, Busstop*> stopsmap = theNetwork_->get_stopsmap();
        assert(stopsmap.count(largest_demand_stop_id) != 0); //stop with the highest demand should be known to network

        Busstop* largest_demand_stop = theNetwork_->get_stopsmap()[largest_demand_stop_id];
        assert(largest_demand_stop->is_line_end()); //stop should be at the end of a transit route (otherwise no lines known to the Controlcenter will lead there)
        assert(largest_demand_stop->is_line_begin()); //stop should also be at the beginning of a transit route (otherwise no trip FROM this stop can be generated by a Controlcenter)

        //Check if there is already a vehicle on its way to the origin stop of the passenger
        set<Bus*> drivingveh = get_driving_vehicles(fleetState);
        if(!get_vehicles_enroute_to_stop(largest_demand_stop, drivingveh).empty())
            return false;

        //find OnCall vehicle that is closest to largest source of demand
        set<Bus*> vehOnCall = fleetState.at(BusState::OnCall); //vehicles that are currently available for empty vehicle rebalancing
        Bus* closestVehicle = nullptr; //vehicle that is closest to the stop with the largest unserved source of demand
        vector<Busline*> closestVehicle_serviceRoutes; //all service routes between the closest vehicle and the stop with the highest demand
        double shortest_tt = HUGE_VAL;

        for (Bus* veh : vehOnCall)
        {
            Busstop* vehloc = veh->get_last_stop_visited(); //location of on call vehicle
            if (vehloc == largest_demand_stop)
            {
                DEBUG_MSG_V("ERROR::NaiveEmptyVehicleTripgeneration::calc_trip_generation - vehicle location is already at the source of demand when planning empty vehicle trips. Aborting...");
                //abort();
            }
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

            if (!line_exists_in_tripset(unmatchedTripSet, line)) //if this trip does not already exist in unmatchedRebalancing trip set
            {
                /*DEBUG_MSG("INFO::NaiveEmptyVehicleTripGeneration::calc_trip_generation " <<
                    "Empty vehicle trip found! Generating trip for line " << line->get_id()
                    << " between last location stop " << closestVehicle->get_last_stop_visited()->get_name()
                    << " of vehicle " << closestVehicle->get_bus_id() << " and source of demand stop " << largest_demand_stop->get_name());*/

                vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
                Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
                vector<Bustrip* > tripchain = { newtrip }; //tripchain is only one trip long
                helper_functions::add_driving_roster_to_tripchain(tripchain);
                
                unmatchedTripSet.insert(newtrip);//add this trip to the unmatchedTripSet
                
                return true;
            }
        }

        //DEBUG_MSG("INFO::NaiveEmptyVehicleTripGeneration::calc_trip_generation - No rebalancing trip found!");
    }
    return false;
}

// SimpleEmptyVehicleTripGeneration
SimpleEmptyVehicleTripGeneration::SimpleEmptyVehicleTripGeneration(Network *theNetwork):theNetwork_(theNetwork) {}

bool SimpleEmptyVehicleTripGeneration::calc_trip_generation(const set<Request *, ptr_less<Request *> > &requestSet, const vector<Busline *> &candidateServiceRoutes, const map<BusState, set<Bus *> > &fleetState, double time, set<Bustrip *> &unmatchedTripSet, set<Bustrip*>& unmatchedEmptyTripSet)
{
    Q_UNUSED(requestSet)

    // 0. if no unmatched Trips, exit
    if (unmatchedTripSet.empty())
        return false;
    // 1. see if any vehicles are available, if no, exit
    if (fleetState.find(BusState::OnCall) == fleetState.end())  //a drt vehicle must have been initialized
        return false;
    if (fleetState.at(BusState::OnCall).empty())  //a drt vehicle must be available
        return false;
    // 2. sort unMatchedTrips (by nr of requests)
    set<Bustrip*,compareBustripByNrRequests> sortedTrips (unmatchedTripSet.begin(), unmatchedTripSet.end());

    // 3. find nearest vehicle to the unMatchedTrip location (For now: hope for the best :)
    auto selectedTrip = *(sortedTrips.begin());
    Busstop*  tripStartStop = selectedTrip->stops.front()->first;
    auto onCallVehicles = fleetState.at(BusState::OnCall);
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
    helper_functions::update_schedule(selectedTrip, newTrip->stops.back()->second); // Get last stop arrival time of newtrip, and then update the chained selectedTrip with this as dispatch time
    helper_functions::add_driving_roster_to_tripchain(tripchain); // newTrip and selectedTrip now have pointers to eachother as well as info of which order they are in

    // 6. Add newTrip to the unmatchedEmptyTripset
    unmatchedEmptyTripSet.insert(newTrip); // now unmatchedEmptyTripSet will signal matcher to match vehicle to both trips via Bustrip::driving_roster

    // 7. remove selectedTrip from unMatchedTripset
    unmatchedTripSet.erase(selectedTrip); 

    // 8. adding the requests to the empty trip as well (why?)
    for (auto rq : selectedTrip->get_requests())
    {
        //!< @todo maybe remove the scheduled requests from the later chained trips, matcher currently only changes request state to matched for the first trip in driving roster chain
        newTrip->add_request(rq);
    }
    // 9. adding potential requests to the empty trip
    helper_functions::assignRequestsToTrip(requestSet,newTrip);

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

        // update state of requests (if trip was previously associated with a bundle of requests)
        for (auto rq : trip->get_requests())
        {
            assert(rq->state == RequestState::Assigned); //should have been set to assigned when trip was generated
            rq->set_state(RequestState::Matched);
        }
    }
}

Bustrip* MatchingStrategy::find_earliest_planned_trip(const set<Bustrip*>& trips) const
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


bool NullMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, const set<Bustrip*, ptr_less<Bustrip*> >& matchedTrips)
{
    Q_UNUSED(unmatchedTrip)
    Q_UNUSED(veh_per_sroute)
    Q_UNUSED(time)
    Q_UNUSED(matchedTrips)

    return false;
}

bool NaiveMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, const set<Bustrip*, ptr_less<Bustrip*> >& matchedTrips)
{
    Q_UNUSED(matchedTrips)

    //attempt to match unmatchedTrip with first on-call vehicle found at the origin stop of the trip
    if ((unmatchedTrip != nullptr) && !veh_per_sroute.empty())
    {
        //DEBUG_MSG(endl << "INFO::NaiveMatching::find_tripvehicle_match - finding vehicles to match to planned trips at time " << time);
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
                    //DEBUG_MSG("INFO::NaiveMatching::find_tripvehicle_match - Match found!");
                    veh = (*c_bus_it);
                    assign_oncall_vehicle_to_trip(origin_stop, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

                    return true;
                }
            }
        }
        //DEBUG_MSG("INFO::NaiveMatching::find_tripvehicle_match - No trip - vehicle match found!");
    }
    return false;
}

//SchedulingStrategy
bool SchedulingStrategy::book_trip_dispatch(Eventlist* eventlist, Bustrip* trip)
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

                line->add_flex_trip(unscheduled_trip); //add trip as a flex trip of line for bookkeeping
                line->add_trip(unscheduled_trip, starttime); //insert trip into the main trips list of the line
                
                if (trip_dispatch == trip->driving_roster.front()) // Busline event added only for first trip in chain, the others handled by Bus::advance_curr_trip
                {
                    eventlist->add_event(starttime, line); //book the activation of this trip in the eventlist
                    unscheduled_trip->set_scheduled_for_dispatch(true); //scheduled for dispatch is set in Busline::execute?
                    scheduled_trip_success = true;
                }
                else
                {
                    //!< @todo Kindof hacky, but add an 'ambitious' dispatch event from Busline for now to mimic previous behavior as closely as possible, activated Busline and have Busline::curr_trip always pointing to the correct trip to be dispatched
                    eventlist->add_event(trip->get_starttime()+0.0001, line); // adds a busline event with the starttime of the first trip in chain + 0.0001 seconds.... Should activate the line if not activated, but ignore activation of the trip (with busv == nullptr)
                    unscheduled_trip->set_scheduled_for_dispatch(true); // this unscheduled trip (chained) is guaranteed to be activated via Bus::advance_curr_trip
                }
            }
        }
    }
      
    return scheduled_trip_success;
}

bool NullScheduling::schedule_trips(Eventlist* eventlist, set<Bustrip*, ptr_less<Bustrip*> >& unscheduledTrips, const double time)
{
    Q_UNUSED(eventlist)
    Q_UNUSED(unscheduledTrips)
    Q_UNUSED(time)

    return false;
}


bool NaiveScheduling::schedule_trips(Eventlist* eventlist, set<Bustrip*, ptr_less<Bustrip*> >& unscheduledTrips, const double time)
{
    assert(eventlist);
    bool scheduled_trip = false; // true if at least one trip has been scheduled
    set<Bustrip*, compareBustripByEarliestStarttime> sortedTrips(unscheduledTrips.begin(), unscheduledTrips.end()); // process uncheduledTrips in order of earliest to latest starttime
    assert(sortedTrips.size() == unscheduledTrips.size());

    for (auto trip : sortedTrips)
    {
        Bus* bus = trip->get_busv();
        //DEBUG_MSG(endl << "INFO::NaiveScheduling::schedule_trips - scheduling matched trips for dispatch at time " << time);
        //check if the bus associated with this trip is available
        if (bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id()) //vehicle should already be located at the first stop of the trip
        {
            if (trip->get_starttime() < time)  //if scheduling call was made after the original planned dispatch of the trip
                helper_functions::update_schedule(trip, time); //update schedule for dispatch and stop visits according to new starttime

            if (!book_trip_dispatch(eventlist, trip))
                return false;

            unscheduledTrips.erase(trip); //trip is now scheduled for dispatch
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
