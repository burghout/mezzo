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
    bool vehicle_is_at_location(Bus* veh, set<Busstop*,ptr_less<Busstop*> > stops)
    {
        if (!stops.empty() && !veh->is_driving())
        {
            if (stops.find(veh->get_last_stop_visited()) != stops.end())
            {
                return true;
            }
        }
        return false;
    }

    // Update trip/trip-chain with new data
    void update_schedule(Bustrip* trip, double new_starttime)
    {
        assert(trip);
        assert(new_starttime >= 0);

        if (trip && new_starttime >= 0)
        {
            for (auto trip_dispatch : trip->driving_roster) // note that this assumes that the order of the trips in the driving roster is the order of the trips
            {
                Bustrip* scheduled_trip = trip_dispatch->first;
                if (scheduled_trip->get_starttime() != new_starttime)
                {
                    double delta = new_starttime - scheduled_trip->get_starttime(); //positive to shift the schedule later in time, and negative if it should shift earlier in time
                    vector<Visit_stop*> schedule = scheduled_trip->stops;

                    //add the delta to all the scheduled stop visits
                    for (Visit_stop* stop_arrival : schedule)
                    {
                        stop_arrival->second += delta;
                    }

                    trip_dispatch->second = new_starttime; //update the dispatch time at the tripchain level
                    scheduled_trip->set_starttime(new_starttime); //set planned dispatch to new start time
                    scheduled_trip->add_stops(schedule); //overwrite old schedule with the new scheduled stop visits
                    scheduled_trip->convert_stops_vector_to_map(); //stops map used in some locations, stops vector used in others

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

    set<Request*, ptr_less<Request*> > filterRequestsByState(const set<Request*, ptr_less<Request*> >& oldSet, RequestState state)
    {
        set <Request*, ptr_less<Request*> > newSet;
        copy_if(oldSet.begin(), oldSet.end(), inserter(newSet, newSet.end()), [state](Request* value) {return value->state == state; });
        return newSet;
    }

    set<Request*, ptr_less<Request*> > filterRequestsByOD(const set<Request*, ptr_less<Request*> >& oldSet, int o_id, int d_id)
    {
        set <Request*, ptr_less<Request*>> newSet;
        std::copy_if(oldSet.begin(), oldSet.end(), std::inserter(newSet, newSet.end()), [o_id, d_id](Request* value) {return (value->ostop_id == o_id) && (value->dstop_id == d_id); });
        return newSet;
    }

    // Assign requests to trips
    void assignRequestsToTrip(const set<Request*, ptr_less<Request*> >& requestSet, Bustrip* tr, int planned_capacity)
    {
        auto unassignedRequests = filterRequestsByState(requestSet, RequestState::Unmatched); // redo the filtering each time
        for (auto rq : unassignedRequests)
        {
            assert(rq->state == RequestState::Unmatched);
            if (tr->is_feasible_request_assignment(rq, planned_capacity))
            { // if rq destination stop is in the downstream stops
                rq->set_assigned_trip(tr);
            }
        }
    }

    void assignRequestsToTripSet(const set<Request*, ptr_less<Request*> >& requestSet, const set<Bustrip*, ptr_less<Bustrip*> >& tripSet, int planned_capacity)
    {
        for (auto tr : tripSet)
        {
            assignRequestsToTrip(requestSet, tr, planned_capacity);
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
    set<Bustrip*, ptr_less<Bustrip*> > filterRebalancingTrips(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet)
    {
        set <Bustrip*, ptr_less<Bustrip*>> newSet;
        copy_if(oldSet.begin(), oldSet.end(), inserter(newSet, newSet.end()), [](Bustrip* trip) {return trip->is_rebalancing(); });
        return newSet;
    }
    set<Bustrip*, ptr_less<Bustrip*> > filterRebalancingTripsToStop(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet, Busstop* target_stop)
    {
        set <Bustrip*, ptr_less<Bustrip*> > newSet;
        copy_if(oldSet.begin(), oldSet.end(), inserter(newSet, newSet.end()), [target_stop](Bustrip* trip)
        {
            return trip->is_rebalancing() && trip->stops.back()->first == target_stop;
        });
        return newSet;
    }

} // end namespace cs_helper_functions
