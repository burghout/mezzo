#include "controlstrategies.h"
#include "vehicle.h"
#include "busline.h"
#include "network.h"

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
Request::Request(int pid, int oid, int did, int l, double dt, double t) : pass_id(pid), ostop_id(oid), dstop_id(did), load(l), desired_departure_time(dt), time(t)
{
	qRegisterMetaType<Request>(); //register Request as a metatype for QT signal arguments
}

bool Request::operator==(const Request & rhs) const
{
	return (pass_id == rhs.pass_id && ostop_id == rhs.ostop_id && dstop_id == rhs.dstop_id && load == rhs.load && desired_departure_time == rhs.desired_departure_time && time == rhs.time);
}

bool Request::operator<(const Request & rhs) const
{
    if (desired_departure_time != rhs.desired_departure_time)
        return desired_departure_time < rhs.desired_departure_time;
	else if (time != rhs.time)
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

vector<Busline*> TripGenerationStrategy::find_lines_connecting_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const
{
	vector<Busline*> lines_connecting_stops; // lines between stops given as input
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
						lines_connecting_stops.push_back(line); //add line as a possible transit connection between these stops
				}
			}
		}
	}

	return lines_connecting_stops;
}

vector<Busline*> TripGenerationStrategy::find_lines_between_stops_and_opp_stops(const vector<Busline*>& lines, const Busstop* start_stop, const Busstop* end_stop) const
{
	vector<Busline*> lines_between_stops;
	
	const Busstop* start_stop_opp = start_stop->get_opposing_stop(); //stop opposing start stop
	const Busstop* end_stop_opp = end_stop->get_opposing_stop(); //stop opposing end stop

	for (Busline* line : lines)
	{
		bool connected = false; //true if the start and end stops are connected in some sense
		
		if (!line->stops.empty())
		{
			const Busstop* first_stop = line->stops.front(); 
			const Busstop* last_stop = line->stops.back();
			if (first_stop == start_stop && last_stop == end_stop)
				connected = true;
			
			if (first_stop == start_stop_opp && last_stop == end_stop)
				connected = true;

			if (first_stop == start_stop && last_stop == end_stop_opp)
				connected = true;

			if (first_stop == start_stop_opp && last_stop == end_stop_opp)
				connected = true;
		}

		if (connected)
			lines_between_stops.push_back(line);
	}

	return lines_between_stops;
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

	int rootlink_id = origin_stop->get_link_id();
	int dest_node_id = destination_stop->get_dest_node()->get_id(); //TODO: can change these to look between upstream and downstream junction nodes as well

	vector<Link*> rlinks = theNetwork->shortest_path_to_node(rootlink_id, dest_node_id, start_time);

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

bool NullTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet) const
{
	Q_UNUSED(requestSet);
	Q_UNUSED(candidateServiceRoutes);
	Q_UNUSED(fleetState);
	Q_UNUSED(time);
	Q_UNUSED(unmatchedTripSet);

	return false;
}

bool NaiveTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet) const
{
	Q_UNUSED(fleetState);

	if (!requestSet.empty() && !candidateServiceRoutes.empty())
	{
        if (requestSet.size() >= (std::size_t) drt_min_occupancy) //do not attempt to generate trip unless requestSet is greater than the desired occupancy
		{
			DEBUG_MSG(endl << "------------Trip Generating Naively!-------------");
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

                            for_each(deltas1.begin(), deltas1.end(), [&](const pair<Busstop*, double> delta){ ivt1 += delta.second; }); //scheduled ivt for line1
                            for_each(deltas2.begin(), deltas2.end(), [&](const pair<Busstop*, double> delta) { ivt2 += delta.second; }); //scheduled ivt for line2

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
					DEBUG_MSG("Trip found! Generating trip for line " << line->get_id());

					vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
					Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
					unmatchedTripSet.insert(newtrip);//add this trip to the unmatchedTripSet
					return true;
				}
			}
			DEBUG_MSG("No trip found!");
		}
	}
	return false;
}

//Empty vehicle trip generation
NaiveEmptyVehicleTripGeneration::NaiveEmptyVehicleTripGeneration(Network* theNetwork) : theNetwork_(theNetwork){}
bool NaiveEmptyVehicleTripGeneration::calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& unmatchedTripSet) const
{
	if (!requestSet.empty() && !candidateServiceRoutes.empty()) //Reactive strategy so only when requests exist
	{
		if (fleetState.find(BusState::OnCall) == fleetState.end()) //a drt vehicle must have been initialized
			return false;
		if (fleetState.at(BusState::OnCall).empty()) //a drt vehicle must be available
			return false;

		DEBUG_MSG(endl << "------------Nearest Neighbour Longest Queue EV Trip Generation-------------");
		//find od pair with the highest frequency in requestSet (highest source of shareable demand)
		map<pair<int, int>, int> odcounts = countRequestsPerOD(requestSet);
		typedef pair<pair<int, int>, int> od_count;
		vector<od_count> sortedODcounts;

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
		assert(stopsmap.count(largest_demand_stop_id) != 0);

		//find the opposing stop to the largest source of demand to use as a destination for an empty vehicle trip if this stop is not a destination stop for any service route
		Busstop* largest_demand_stop = theNetwork_->get_stopsmap()[largest_demand_stop_id];
		if (!largest_demand_stop->is_line_end())
			largest_demand_stop = largest_demand_stop->get_opposing_stop();

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
                DEBUG_MSG("Warning - vehicle location is already at the source of demand when planning empty vehicle trips.");
                abort();
            }
			vector<Busline*> vehicle_serviceRoutes; //all service routes between the vehicle location (current stop and opposing stop) and the demand source (current stop and opposing stop)

			//collect a vector of all possible service routes between the vehicles current location and the demand source
			vehicle_serviceRoutes = find_lines_between_stops_and_opp_stops(candidateServiceRoutes, vehloc, largest_demand_stop);

			if (!vehicle_serviceRoutes.empty())
			{
				vector<Link*> shortestpath = find_shortest_path_between_stops(theNetwork_, vehloc, largest_demand_stop, time);
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
		if (closestVehicle)
		{
			Busline* line = find_shortest_busline(closestVehicle_serviceRoutes, time);
			assert(line);

			if (!line_exists_in_tripset(unmatchedTripSet, line)) //if this trip does not already exist in unmatchedRebalancing trip set
			{
				DEBUG_MSG("Empty vehicle trip found! Generating trip for line " << line->get_id() << " between last location stop " << closestVehicle->get_last_stop_visited()->get_name() << " of vehicle " << closestVehicle->get_bus_id() << " and source of demand stop " << largest_demand_stop->get_name());

				vector<Visit_stop*> schedule = create_schedule(time, line->get_delta_at_stops()); //build the schedule of stop visits for this trip (we visit all stops along the candidate line)
				Bustrip* newtrip = create_unassigned_trip(line, time, schedule); //create a new trip for this line using now as the dispatch time
                unmatchedTripSet.insert(newtrip);//add this trip to the unmatchedTripSet
				return true;
			}
		}

		DEBUG_MSG("No rebalancing trip found!");
	}
	return false;
}

//MatchingStrategy
void MatchingStrategy::assign_oncall_vehicle_to_trip(Busstop* currentStop, Bus* transitveh, Bustrip* trip, double starttime)
{
	assert(transitveh);
	assert(trip);
	assert(!transitveh->get_curr_trip()); //this particular bus instance (remember there may be copies of it if there is a trip chain, should not have a trip)
	assert(transitveh->is_oncall());

	DEBUG_MSG("---Assigning vehicle " << transitveh->get_bus_id() << " to trip " << trip->get_id() << "---");

	trip->set_busv(transitveh); //assign bus to the trip
	transitveh->set_curr_trip(trip); //assign trip to the bus
	trip->set_bustype(transitveh->get_bus_type());//set the bus type of the trip (so trip has access to this bustypes dwell time function)
	trip->get_busv()->set_on_trip(true); //flag the bus as busy for all trips except the first on its chain (TODO might move to dispatcher)

	vector<Start_trip*> driving_roster; //contains all other trips in this trips chain (if there are any) (TODO might move to dispatcher)
	Start_trip* st = new Start_trip(trip, starttime);

	driving_roster.push_back(st);
	trip->add_trips(driving_roster); //save the driving roster at the trip level to conform to interfaces of Busline::execute, Bustrip::activate etc. 
	double delay = starttime - trip->get_starttime();
	trip->set_starttime(starttime); //reset scheduled dispatch from origin to given starttime

	DEBUG_MSG("Delay in start time for trip " << trip->get_id() << ": " << delay);

	if (!currentStop->remove_unassigned_bus(transitveh, starttime)) //bus is no longer unassigned and is removed from vector of unassigned buses at whatever stop the vehicle is waiting on call at
												  //trip associated with this bus will be added later to expected_bus_arrivals via
												  //Busline::execute -> Bustrip::activate -> Origin::insert_veh -> Link::enter_veh -> Bustrip::book_stop_visit -> Busstop::book_bus_arrival
	{
		DEBUG_MSG_V("Busstop::remove_unassigned_bus failed for bus " << transitveh->get_bus_id() << " and stop " << currentStop->get_id());
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


bool NullMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, const set<Bustrip*>& matchedTrips)
{
	Q_UNUSED(unmatchedTrip);
	Q_UNUSED(veh_per_sroute);
	Q_UNUSED(time);
	Q_UNUSED(matchedTrips);

	return false;
}

bool NaiveMatching::find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, const set<Bustrip*>& matchedTrips)
{
	Q_UNUSED(matchedTrips);

	//attempt to match unmatchedTrip with first on-call vehicle found at the origin stop of the trip
	if (unmatchedTrip && !veh_per_sroute.empty())
	{
		DEBUG_MSG(endl << "------------Matching Naively!-------------");
		Bus* veh = nullptr; //the transit veh that we wish to match to a trip
		Busline* sroute = unmatchedTrip->get_line(); //get the line/service route of this trip
		set<Bus*> candidate_buses = veh_per_sroute[sroute->get_id()]; //get all transit vehicles that have this route in their service area

		if (!candidate_buses.empty()) //there is at least one bus serving the route of this trip
		{
			Busstop* origin_stop = unmatchedTrip->get_last_stop_visited(); //get the initial stop of the trip
			vector<pair<Bus*, double>> ua_buses_at_stop = origin_stop->get_unassigned_buses_at_stop();

			//check if one of the unassigned transit vehicles at the origin stop (or its opposing stop TODO: add a smarter solution for the opposing stops when you have more time) is currently serving the route of the trip
			for (const pair<Bus*, double>& ua_bus : ua_buses_at_stop)
			{
				set<Bus*>::iterator c_bus_it;
				c_bus_it = candidate_buses.find(ua_bus.first); //first bus in list should be the bus that arrived to the stop earliest

				if (c_bus_it != candidate_buses.end()) //a bus match has been found
				{
					DEBUG_MSG("Trip - vehicle match found!");
					veh = (*c_bus_it);
					assign_oncall_vehicle_to_trip(origin_stop, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

					return true;
				}
			}

			//check opposing stop to origin stop of line TODO: add a smarter solution to this, e.g. have bus in a 'middle' state as OnCall for both the last stop it visited and the opposing stop that it turns from with some kind of turning time
			Busstop* origin_stop_opp = origin_stop->get_opposing_stop();
			vector<pair<Bus*, double>> ua_buses_at_stop_opp = origin_stop_opp->get_unassigned_buses_at_stop();

			for (const pair<Bus*, double>& ua_bus : ua_buses_at_stop_opp)
			{
				set<Bus*>::iterator c_bus_it;
				c_bus_it = candidate_buses.find(ua_bus.first); //first bus in list should be the bus that arrived to the stop earliest

				if (c_bus_it != candidate_buses.end()) //a bus match has been found
				{
					DEBUG_MSG("Trip - vehicle match found!");
					veh = (*c_bus_it);
					veh->set_last_stop_visited(origin_stop); //TODO: last_stop_visited used in dispatcher to check if vehicle is at the origin stop of the trip. We teleport it there for now ugly solution!
					assign_oncall_vehicle_to_trip(origin_stop_opp, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

					return true;
				}
			}
		}
		DEBUG_MSG("No trip - vehicle match found!");
	}
	return false;
}

//SchedulingStrategy
bool SchedulingStrategy::book_trip_dispatch(Eventlist* eventlist, Bustrip* trip)
{
	assert(eventlist);
	assert(trip);

	Busline* line = trip->get_line();
	double starttime = trip->get_starttime();
    assert(line);
	assert(line->is_flex_line());

	DEBUG_MSG("SchedulingStrategy::book_trip_dispatch is scheduling trip " << trip->get_id() << " with start time " << starttime);

	line->add_flex_trip(trip); //add trip as a flex trip of line for bookkeeping
	line->add_trip(trip, starttime); //insert trip into the main trips list of the line
	eventlist->add_event(starttime, line); //book the activation of this trip in the eventlist
    trip->set_scheduled_for_dispatch(true);
	return true;
}

void SchedulingStrategy::update_schedule(Bustrip* trip, double new_starttime)
{
	assert(trip);
	assert(new_starttime >= 0);

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

bool NullScheduling::schedule_trips(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, const double time)
{
	Q_UNUSED(eventlist);
	Q_UNUSED(unscheduledTrips);
	Q_UNUSED(time);

	return false;
}


bool NaiveScheduling::schedule_trips(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, const double time)
{
	assert(eventlist);
	if (!unscheduledTrips.empty())
	{
		Bustrip* trip = (*unscheduledTrips.begin()); 
		Bus* bus = trip->get_busv();

		//check if the bus associated with this trip is available
		if (bus->get_last_stop_visited()->get_id() == trip->get_last_stop_visited()->get_id()) //vehicle should already be located at the first stop of the trip
		{
			if (trip->get_starttime() < time) //if scheduling call was made after the original planned dispatch of the trip
				update_schedule(trip, time); //update schedule for dispatch and stop visits according to new starttime

			if (!book_trip_dispatch(eventlist, trip))
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
