#include "controlutilities.h"

//Helper functions for controlcenter
namespace cc_helper_functions
{
	void removeRequestFromTripChain(const Request* req, const vector<Start_trip*>& driving_roster) //!< Removes request from 'Bustrip::assigned_requests' for any bustrip in driving_roster
	{
		for (auto trip_dispatch : driving_roster)
		{
			trip_dispatch->first->remove_request(req);
		}
	}
} //end namespace cc_helper_functions

// Helper functions for controlstrategies
namespace cs_helper_functions
{
    // Update trip/trip-chain with new data
    void update_schedule(Bustrip* trip, double new_starttime)
    {
        assert(trip);
        assert(new_starttime >= 0);

        if (trip && new_starttime >= 0)
        {
            for (auto trip_dispatch : trip->driving_roster) // note that this assumes that the order of the trips in the driving roster is the order of the trips
            {
                Bustrip* trip = trip_dispatch->first;
                if (trip->get_starttime() != new_starttime)
                {
                    double delta = new_starttime - trip->get_starttime(); //positive to shift the schedule later in time, and negative if it should shift earlier in time
                    vector<Visit_stop*> schedule = trip->stops;

                    //add the delta to all the scheduled stop visits
                    for (Visit_stop* stop_arrival : schedule)
                    {
                        stop_arrival->second += delta;
                    }

                    trip_dispatch->second = new_starttime; //update the dispatch time at the tripchain level
                    trip->set_starttime(new_starttime); //set planned dispatch to new start time
                    trip->add_stops(schedule); //overwrite old schedule with the new scheduled stop visits
                    trip->convert_stops_vector_to_map(); //stops map used in some locations, stops vector used in others

                    //move on to next trip in chain
                    new_starttime = schedule.back()->second; //starttime of next trip in chain is the end time of the preceding trip
                }
            }
        }
        else
        {
            DEBUG_MSG("WARNING::update schedule - null trip or invalid starttime arguments");
        }
    }
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
    void set_status_of_tripchain(const vector<Start_trip*>& driving_roster, BustripStatus newstatus)
    {
        for (auto trip_dispatch : driving_roster)
        {
            trip_dispatch->first->set_status(newstatus);
        }
    }

    void set_planned_capacity_of_tripchain(const vector<Start_trip*>& driving_roster, int planned_capacity)
    {
        for (auto trip_dispatch : driving_roster)
        {
            trip_dispatch->first->set_planned_capacity(planned_capacity);
        }
    }

    // Find requests by condition
    vector<Request*> getRequestsInTripChain(const vector<Start_trip*>& driving_roster)
    {
        vector<Request*> all_scheduled_requests;
        for (auto trip_dispatch : driving_roster)
        {
            vector<Request*> trip_requests = trip_dispatch->first->get_assigned_requests();
            all_scheduled_requests.insert(all_scheduled_requests.end(), trip_requests.begin(), trip_requests.end());
        }
        return all_scheduled_requests;
    }

    bool requestExistsInTripChain(const Request* req, const vector<Start_trip*>& driving_roster)
    {
        vector<Request*> all_req = getRequestsInTripChain(driving_roster);
        auto it = find(all_req.begin(), all_req.end(), req);
        return (it != all_req.end());
    }

    set<Request*, ptr_less<Request*>> filterRequestsByState(const set<Request*, ptr_less<Request*> >& oldSet, RequestState state)
    {
        set <Request*, ptr_less<Request*> > newSet;
        copy_if(oldSet.begin(), oldSet.end(), inserter(newSet, newSet.end()), [state](Request* value) {return value->state == state; });
        return newSet;
    }

    set<Request*, ptr_less<Request*>> filterRequestsByOD(const set<Request*, ptr_less<Request*> >& oldSet, int o_id, int d_id)
    {
        set <Request*, ptr_less<Request*>> newSet;
        std::copy_if(oldSet.begin(), oldSet.end(), std::inserter(newSet, newSet.end()), [o_id, d_id](Request* value) {return (value->ostop_id == o_id) && (value->dstop_id == d_id); });
        return newSet;
    }

    // Assign requests to trips
    void assignRequestsToTrip(const set<Request*, ptr_less<Request*> >& requestSet, Bustrip* tr)
    {
        auto unassignedRequests = filterRequestsByState(requestSet, RequestState::Unmatched); // redo the filtering each time
        // TODO Add also for emptyTrips followed by a "selectedTrip"
        for (auto rq : unassignedRequests)
        {
            assert(rq->state == RequestState::Unmatched);
            auto startstop = tr->stops.front()->first; // if trip starts at same stop
            if (startstop->get_id() == rq->ostop_id)
            {
                auto downstreamstops = tr->get_downstream_stops();
                if (downstreamstops.end() != find_if(downstreamstops.begin(), downstreamstops.end(),[rq](const Busstop* st) { return rq->dstop_id == st->get_id(); }))
                { // if rq destination stop is in the downstream stops
                    rq->set_assigned_trip(tr);
                    rq->set_state(RequestState::Assigned);
                }
            }
        }

    }
    void assignRequestsToTripSet(const set<Request*, ptr_less<Request*> >& requestSet, const set<Bustrip*, ptr_less<Bustrip*> >& tripSet)
    {
        for (auto tr : tripSet)
        {
            assignRequestsToTrip(requestSet, tr);
        }
    }

    void assignRequestsToActivatedTrip(const set<Request*, ptr_less<Request*> >& requestSet, Bustrip* tr)
    {
        assert(tr->get_status() == BustripStatus::Activated);
        int cap = tr->get_planned_capacity();
        assert(cap > 0); // should only be 0 if unassigned to a vehicle, in which case this trip should not have been scheduled

        auto unassignedRequests = filterRequestsByState(requestSet, RequestState::Unmatched); // redo the filtering each time
        /**
         *   Check feasibility of stuffing each request into activated trip-chains. Conditions
         *     1. The trip has remaining capacity to pickup the assigned request... @todo feels like even more bookeeping of bus, active trip and following trip-chain is needed. But quickly gets complex
         *     2. Origin stop somewhere downstream 
         *     3. Destination stop is somewhere downstream
         *     4. Origin stop is before destination stop downstream
         */
        if (tr->has_reserve_capacity())
        {
            for (auto rq : unassignedRequests)
            {
                assert(rq->state == RequestState::Unmatched);
                // auto startstop = tr->stops.front()->first; // if trip starts at same stop

                auto downstreamstops = tr->get_downstream_stops(); // all downstream stops of the current trip
                //check if origin is downstream
                auto orig_it = find_if(downstreamstops.begin(), downstreamstops.end(), [rq](const Busstop* st) { return rq->ostop_id == st->get_id(); });
                auto dest_it = find_if(downstreamstops.begin(), downstreamstops.end(), [rq](const Busstop* st) { return rq->dstop_id == st->get_id(); });
                if (orig_it != downstreamstops.end() && dest_it != downstreamstops.end())
                {
                    assert(orig_it != dest_it);
                    if (orig_it < dest_it) //check if destination is downstream from origin
                    {
                        rq->set_assigned_trip(tr);
                        rq->set_state(RequestState::Matched); //update to matched since the trip already has a vehicle    
                    }
                }
            }
        }
    }

    void assignRequestsToScheduledTrip(const set<Request*, ptr_less<Request*> >& requestSet, Bustrip* tr)
    {
        assert(tr->get_status() == BustripStatus::Scheduled || tr->get_status() == BustripStatus::ScheduledWaitingForVehicle);
        int cap = tr->get_planned_capacity();
        assert(cap > 0); // should only be 0 if unassigned to a vehicle, in which case this trip should not have been scheduled

        auto unassignedRequests = filterRequestsByState(requestSet, RequestState::Unmatched); // redo the filtering each time
        /**
         *   Check feasibility of stuffing each request into scheduled trip-chains. Conditions
         *     1. The trip has remaining capacity to pickup the assigned request... @todo feels like even more bookeeping of bus, active trip and following trip-chain is needed. But quickly gets complex
         *     2. Origin stop is at the origin of the trip @todo PARTC maybe change this later to origin stop anywhere on the route.....
         *     3. Destination stop is somewhere downstream @todo PARTC ntoe that all the travelers using DRT have the same destination (the transfer stop) for the case study anyways
         */
        if (tr->has_reserve_capacity())
        {
            for (auto rq : unassignedRequests)
            {
                assert(rq->state == RequestState::Unmatched);
                auto startstop = tr->stops.front()->first; // if trip starts at same stop
                if (startstop->get_id() == rq->ostop_id)
                {
                    auto downstreamstops = tr->get_downstream_stops();
                    if (downstreamstops.end() != find_if(downstreamstops.begin(), downstreamstops.end(), [rq](const Busstop* st) { return rq->dstop_id == st->get_id(); }))
                    { // if rq destination stop is in the downstream stops
                        rq->set_assigned_trip(tr);
                        rq->set_state(RequestState::Matched); //update to matched since the trip already has a vehicle
                    }
                }
            }
        }
    }

    void assignRequestsToScheduledTrips(const set<Request*, ptr_less<Request*> >& requestSet, const set<Bustrip*, ptr_less<Bustrip*> >& tripSet)
    {
        if (!tripSet.empty() && !requestSet.empty())
        {
            auto scheduled_trips = filterBustripsByStatus(tripSet, BustripStatus::Scheduled); // scheduled but with a vehicle attached to them
            auto scheduled_chained_trips = filterBustripsByStatus(tripSet, BustripStatus::ScheduledWaitingForVehicle); //scheduled but waiting for a vehicle performing an earlier trip in chain

            // @todo note that here we do not care how trips are connected, just what their current capacity is regardless of if they are assigned to the same vehicle or not. 
            for (auto trip : scheduled_trips) // prioritize scheduled trips with an available vehicle first
            {
                assignRequestsToScheduledTrip(requestSet, trip);
            }
            for (auto trip : scheduled_chained_trips)
            {
                assignRequestsToScheduledTrip(requestSet, trip);
            }
        }
    }

    // Find trips by condition
    set<Bustrip*, ptr_less<Bustrip*> > filterBustripsByStatus(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet, BustripStatus status)
    {
        set <Bustrip*, ptr_less<Bustrip*>> newSet;
        copy_if(oldSet.begin(), oldSet.end(), inserter(newSet, newSet.end()), [status](Bustrip* trip) {return trip->get_status() == status; });
        return newSet;
    }
    set<Bustrip*, ptr_less<Bustrip*> > filterRequestAssignedTrips(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet)
    {
        set <Bustrip*, ptr_less<Bustrip*> > newSet = oldSet;
        auto it = newSet.begin();
        while (it != newSet.end())
        {
            if (!(*it)->is_assigned_to_requests())
            {
                it = newSet.erase(it);
            }
            else
            {
                ++it;
            }
        }

        return newSet;
    }

} // end namespace cs_helper_functions
