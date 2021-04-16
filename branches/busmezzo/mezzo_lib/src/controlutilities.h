/**
 * @addtogroup DRT
 * @{
 */
 /**@file   controlutilities.h
  * @brief  Collection of helper methods and custom comparators used by the process classes of Controlcenter to assign demand-responsive transit vehicles to passenger requests
  *
  */

#pragma once

#include "controlstrategies.h"
#include "vehicle.h"
#include "busline.h"

// Helper functions for controlcenter
namespace cc_helper_functions
{
	void removeRequestFromTripChain(const Request* req, const vector<Start_trip*>& driving_roster); //!< Removes request from 'Bustrip::scheduled_requests' for any bustrip in driving_roster
}

 // Helper functions for controlstrategies
namespace cs_helper_functions
{
    // Update trip/trip-chain with new data
    void update_schedule(Bustrip* trip, double new_starttime); //!< takes trip that already has a preliminary schedule for both dispatch and stop visits, and updates this schedule given a new start time    
    void add_driving_roster_to_tripchain(const vector<Bustrip*>& tripchain); //!< Takes a vector of Bustrips and connects them via their driving_roster attribute in the order of the tripchain (i.e. index 0 is first trip, index 1 the second etc.)
    void set_status_of_tripchain(const vector<Start_trip*>& driving_roster, BustripStatus newstatus); //!< Takes a trip chain (Bustrip::driving_roster) and sets the trip status of each trip in this chain to newstatus

    // Find requests by condition
    vector<Request*> getRequestsInTripChain(const vector<Start_trip*>& driving_roster); //!< Takes a trip chain (Bustrip::driving_roster) and returns a vector of ALL requests scheduled to any trip in this chain. 
    bool requestExistsInTripChain(const Request* req, const vector<Start_trip*>& driving_roster);
    set<Request*, ptr_less<Request*> > filterRequestsByState(const set<Request*, ptr_less<Request*> >& oldSet, RequestState state);
    set<Request*, ptr_less<Request*> > filterRequestsByOD(const set<Request*, ptr_less<Request*> >& oldSet, int o_id, int d_id);

    // Assign requests to trips
    void assignRequestsToTrip(const set<Request*, ptr_less<Request*> >& requestSet, Bustrip* tr); // Assign requests to trips
    void assignRequestsToTripSet(const set<Request*, ptr_less<Request*> >& requestSet, const set<Bustrip*, ptr_less<Bustrip*> >& tripSet);

    // Find trip by condition
    set<Bustrip*, ptr_less<Bustrip*> > filterBustripsByStatus(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet, BustripStatus status);
    set<Bustrip*, ptr_less<Bustrip*> > filterRequestAssignedTrips(const set<Bustrip*, ptr_less<Bustrip*> >& oldSet, BustripStatus status); //!< returns trips that are members of oldset with <status> and non-empty scheduled requests members

} // end namespace helper_functions


// Custom comparators
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

struct compareBustripByMaxWait
{
    compareBustripByMaxWait(double time) :time_(time) {}
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_max_wait_requests(time_) != rhs->get_max_wait_requests(time_))
            return lhs->get_max_wait_requests(time_) > rhs->get_max_wait_requests(time_);
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
    double time_;
};

struct compareBustripByCumulativeWait
{
    compareBustripByCumulativeWait(double time) :time_(time) {}
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_cumulative_wait_requests(time_) != rhs->get_cumulative_wait_requests(time_))
            return lhs->get_cumulative_wait_requests(time_) > rhs->get_cumulative_wait_requests(time_);
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
    double time_;
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
