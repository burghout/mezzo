#include "controlstrategies.h"

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
		if (requestSet.size() >= drt_min_occupancy) //do not attempt to generate trip unless requestSet is greater than the desired occupancy
		{
			DEBUG_MSG("------------Trip Generating Naively!-------------");
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

			if (sortedODcounts.front().second < drt_min_occupancy)
			{
				DEBUG_MSG("No trip generated! Maximum OD count in request set " << sortedODcounts.front().second << " is smaller than min occupancy " << drt_min_occupancy);
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

//MatchingStrategy
void MatchingStrategy::assign_idlevehicle_to_trip(Busstop* currentStop, Bus* veh, Bustrip* trip, double starttime)
{
	assert(veh);
	assert(trip);
	assert(!veh->get_curr_trip()); //this particular bus instance (remember there may be copies of it if there is a trip chain, should not have a trip)
	assert(veh->is_idle());

	DEBUG_MSG("---Assigning vehicle " << veh->get_bus_id() << " to trip " << trip->get_id() << "---");

	trip->set_busv(veh); //assign bus to the trip
	veh->set_curr_trip(trip); //assign trip to the bus
	trip->set_bustype(veh->get_bus_type());//set the bus type of the trip (so trip has access to this bustypes dwell time function)
	trip->get_busv()->set_on_trip(true); //flag the bus as busy for all trips except the first on its chain (TODO might move to dispatcher)

	vector<Start_trip*> driving_roster; //contains all other trips in this trips chain (if there are any) (TODO might move to dispatcher)
	Start_trip* st = new Start_trip(trip, starttime);

	driving_roster.push_back(st);
	trip->add_trips(driving_roster); //save the driving roster at the trip level to conform to interfaces of Busline::execute, Bustrip::activate etc. 
	double delay = starttime - trip->get_starttime();
	trip->set_starttime(starttime); //reset scheduled dispatch from origin to given starttime

	DEBUG_MSG("Delay in start time for trip " << trip->get_id() << ": " << delay);

	//Busstop* stop = veh->get_last_stop_visited();

	if (!currentStop->remove_unassigned_bus(veh)) //bus is no longer unassigned and is removed from vector of unassigned buses at whatever stop the vehicle is waiting idle at
												  //trip associated with this bus will be added later to expected_bus_arrivals via
												  //Busline::execute -> Bustrip::activate -> Origin::insert_veh -> Link::enter_veh -> Bustrip::book_stop_visit -> Busstop::book_bus_arrival
	{
		DEBUG_MSG_V("Busstop::remove_unassigned_bus failed for bus " << veh->get_bus_id() << " and stop " << currentStop->get_id());
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

			//check if one of the unassigned transit vehicles at the origin stop (or its opposing stop TODO: add a smarter solution for the opposing stops when you have more time) is currently serving the route of the trip
			for (const pair<Bus*, double>& ua_bus : ua_buses_at_stop)
			{
				set<Bus*>::iterator c_bus_it;
				c_bus_it = candidate_buses.find(ua_bus.first); //first bus in list should be the bus that arrived to the stop earliest

				if (c_bus_it != candidate_buses.end()) //a bus match has been found
				{
					DEBUG_MSG("Trip - vehicle match found!");
					veh = (*c_bus_it);
					assign_idlevehicle_to_trip(origin_stop, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

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
					assign_idlevehicle_to_trip(origin_stop_opp, veh, unmatchedTrip, time); //schedule the vehicle to perform the trip at this time

					return true;
				}
			}
		}
		DEBUG_MSG("No trip - vehicle match found!");
	}
	return false;
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

void DispatchingStrategy::update_schedule(Bustrip* trip, double new_starttime)
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
		trip->convert_stops_vector_to_map(); //TODO: not sure why this is necessary but is done for other trips so
	}
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
			if (trip->get_starttime() < time) //if dispatch call was made after the trip was matched
				update_schedule(trip, time); //update schedule for dispatch and stop visits according to new starttime

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