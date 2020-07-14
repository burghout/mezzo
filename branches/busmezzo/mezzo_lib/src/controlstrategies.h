/**
 * @addtogroup DRT
 * @{
 */
/**@file   controlstrategies.h
 * @brief  Collection of algorithms used by the process classes of Controlcenter to assign demand-responsive transit vehicles to passenger requests
 * 
 */

#ifndef CONTROLSTRATEGIES_H
#define CONTROLSTRATEGIES_H 

#include <qmetatype.h>

#include <set>
#include <vector>
#include <map>
#include <utility>

#include "busline.h"

class Bustrip;
class Busline;
class Busstop;
typedef pair<Busstop*, double> Visit_stop;
class Bus;
enum class BusState;
class Network;
class Link;

//! @brief structure representing a passenger message to a control center requesting transport between an origin stop and a destination stop with a desired time of departure
struct Request
{
    int pass_id;    //!< id of passenger that created request
    int ostop_id;   //!< id of origin stop
    int dstop_id;   //!< id of destination stop
    int load;       //!< number of passengers in request
    double desired_departure_time;  //!< desired/earliest departure time for passenger
    double time;                    //!< time request was generated

    Request() = default;
    Request(int pid, int oid, int did, int l, double dt, double t);

    bool operator == (const Request& rhs) const; //!< default equality comparison of Requests
    bool operator < (const Request& rhs) const; //!< default less-than comparison of Requests in the order of smallest departure time, smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id

};
Q_DECLARE_METATYPE(Request);

//Trip Generation Strategies
//! @brief Base class for algorithms for generating an unmatched (unassigned to a vehicle) Bustrip via a BustripGenerator
/*!
    Base class for algorithms that generate planned passenger carrying or supply rebalancing trips in response to passenger requests. Contains several supporting
    methods for prioritizing generating trips for certain Buslines (service routes) over others.

    @todo 
        - separate shortest path methods into e.g. a utility class
        - extract request bundling methods (e.g. countRequestsPerOD) and move to a RequestHandler/RequestBundlingStrategy
*/
class TripGenerationStrategy
{
public:
    virtual ~TripGenerationStrategy() = default;
    virtual bool calc_trip_generation(
        const set<Request>&             requestSet,             //!< set of requests that motivate the potential generation of a trip
        const vector<Busline*>&         candidateServiceRoutes, //!< service routes available to potentially serve the requests
        const map<BusState, set<Bus*> >& fleetState,             //!< state of all vehicles that are potentially available to serve the requests
        const double                    time,                   //!< time for which calc_trip_generation is called
        set<Bustrip*>&                  unmatchedTripSet        //!< set of trips that have been generated to serve requests that have not been assigned to a vehicle yet
    ) const = 0; //!< returns true if a trip was generated and added to the unmatchedTripSet and false otherwise

protected:
    //supporting methods for generating trips with whatever TripGenerationStrategy
    map<pair<int, int>, int> countRequestsPerOD(const set<Request>& requestSet) const; //!< counts the number of requests with a particular od
    bool line_exists_in_tripset(const set<Bustrip*>& tripSet, const Busline* line) const; //!< returns true if a trip exists in trip set for the given bus line
    vector<Busline*> find_lines_connecting_stops(const vector<Busline*>& lines, int ostop_id, int dstop_id) const; //!< returns buslines among lines given as input that run from a given origin stop to a given destination stop (Note: assumes lines are unidirectional and that bus line stops are ordered, which they currently are in BusMezzo)
	vector<Visit_stop*> create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const; //!< creates a vector of scheduled visits to stops starting from the preliminary start time of the trip and given the scheduled in-vehicle time for the trip
	Bustrip* create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& schedule) const; //!< creates a Bustrip for a given line with a desired start time and a scheduled arrival to stops along this line (subject to the availability of a vehicle to serve this trip)
	
	//supporting methods for empty-vehicle redistribution strategies (maybe others in the future too TODO: figure out how best to share these)
    set<Bus*> get_driving_vehicles(const map<BusState, set<Bus*> >& fleetState) const;
    set<Bus*> get_vehicles_enroute_to_stop(const Busstop* stop, const set<Bus*>& vehicles) const; //!< returns true if at least one bus is currently driving to stop (and no intermediate stops) and false otherwise
	double calc_route_travel_time(const vector<Link*>& routelinks, double time) const; //!< returns the sum of dynamic travel time costs over all links in routelinks
    vector<Link*> find_shortest_path_between_stops(Network* theNetwork, const Busstop* origin_stop, const Busstop* destination_stop, double start_time) const; //!< returns the shortest route between a pair of stops for a given time, returns empty vector if none exists
	Busline* find_shortest_busline(const vector<Busline*>& lines, double time) const; //!< returns shortest busline in terms of scheduled in-vehicle time among lines
};

//! @brief null strategy that always returns false
class NullTripGeneration : public TripGenerationStrategy
{
public:
	~NullTripGeneration() override = default;
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, double time, set<Bustrip*>& unmatchedTripSet) const override;
};

//! @brief prioritizes generating trips for OD stop pair with the highest demand and most direct (in terms of scheduled in-vehicle time) service route
/*!
    If an unmatched trip for a given Busline that connects the OD pair with the highest demand already exists in the unmatchedTripSet this trip generation strategy will attempt to generate a trip for the
    second most direct service route. If all routes connecting the OD with the highest demand have an unmatched trip planned for them then this strategy will attempt to generate a trip for
    the OD stop pair with the second highest demand.
*/
class NaiveTripGeneration : public TripGenerationStrategy
{
public:
	~NaiveTripGeneration() override = default;
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, double time, set<Bustrip*>& unmatchedTripSet) const override;
};


//Empty vehicle trip generation strategies
//! @brief Reactive empty vehicle repositioning strategy that attempts to generate a trip from the current stop of the closest on-call transit vehicle to the origin stop of the OD in requestSet with the highest demand
/*!
    @todo
        - Rework ugly hacks for moving vehicles between stop-pairs, navigating between origin and destination nodes and so many other things
        - Also consider vehicles that are in other states besides OnCall (e.g. vehicles en-route to origin stop with the highest demand) when searching for the nearest empty-vehicle, or when prioritizing ODs
*/
class NaiveEmptyVehicleTripGeneration : public TripGenerationStrategy
{
public:
	explicit NaiveEmptyVehicleTripGeneration(Network* theNetwork = nullptr);
	~NaiveEmptyVehicleTripGeneration() override = default;
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, double time, set<Bustrip*>& unmatchedTripSet) const override;

private:
	Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};

//Matching Strategies
//! @brief Base class for algorithms for assigning a Bus to an unmatched Bustrip.
/*!
    Responsible for finding beneficial trip - vehicle matches for dynamically generated planned (unmatched) Bustrips. Groups supporting methods for trip - vehicle assignment

    @todo
        - Matching of en-route vehicles (or any vehicle that is not in the 'OnCall' state) to unmatched trips
    
*/
class MatchingStrategy
{
public:
    virtual ~MatchingStrategy() = default;
    virtual bool find_tripvehicle_match(
        Bustrip*                unmatchedTrip,  //!< planned trip that has not yet been assigned to any transit vehicle
        map<int, set<Bus*>>&    veh_per_sroute, //!< set of candidate vehicles assigned with different service routes
        double            time,           //!< time find_tripvehicle_match is called
        const set<Bustrip*>&    matchedTrips    //!< set of dynamically generated trips that already have a vehicle assigned to them, Note: the idea here is to use these in the future for e.g. dynamic trip-chaining
    ) = 0; //!< returns true if unmatchedTrip was assigned to a transit vehicle from veh_per_sroute

protected:
    void assign_oncall_vehicle_to_trip(Busstop* currentStop, Bus* transitveh, Bustrip* trip, double starttime); //!< performs all operations (similar to Network::read_busvehicle) required in assigning a trip to an on-call vehicle (TODO: assigning driving vehicles). Also removes the the assigned vehicle from vector of unassigned vehicles in currentStop
    Bustrip* find_earliest_planned_trip(const set<Bustrip*>& trips) const; //!< returns trip with the earliest starttime among a set of trips
};

//! @brief Null matching strategy that always returns false
class NullMatching : public MatchingStrategy
{
public:
	~NullMatching() override = default;
	bool find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, double time, const set<Bustrip*>& matchedTrips) override;
};

//! @brief Naive matching strategy always attempts to match the unmatchedTrip to the first candidate transit vehicle found (if any) at the origin stop of the unmatchedTrip
/*!
    @todo
        - Rework ugly hack for moving vehicles between stop pairs, maybe implement a 'perform_Uturn' type method
*/
class NaiveMatching : public MatchingStrategy
{
public:
	~NaiveMatching() override = default;
	bool find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, double time, const set<Bustrip*>& matchedTrips) override;
};

//Scheduling Strategy
//! @brief Algorithms for deciding the scheduled dispatch and stop visits of a dynamically generated trip that has been matched with a vehicle
/*!
    Responsible for booking a Busline::execute event in the eventlist to dispatch an unscheduled trip
*/
class SchedulingStrategy
{
public:
	virtual ~SchedulingStrategy() = default;
	virtual bool schedule_trips(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) = 0; //!< returns true if an unscheduled trip has been given a dispatch time and added to the eventlist as a Busline action. This trip is then removed from the set of unscheduledTrips

protected:
	bool book_trip_dispatch(Eventlist* eventlist, Bustrip* trip); //!< add a matched and scheduled trip (i.e., Bustrip that has a Bus, a Busline, a schedule and a start time) to the trips list of its Busline and add a Busline event to dispatch this trip its given start time
	void update_schedule(Bustrip* trip, double new_starttime); //!< takes trip that already has a preliminary schedule for both dispatch and stop visits, and updates this schedule given a new start time
};

//! @brief Null scheduling strategy that always returns false
class NullScheduling : public SchedulingStrategy
{
public:
	~NullScheduling() override = default;
	bool schedule_trips(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) override;
};

//! @brief Schedules the first trip found in unscheduledTrips to be dispatched at the earliest possible time (i.e. immediately)
/*!
    Currently assumes that the vehicle of an unscheduled trip is already located at the origin stop of the trip. Will call abort otherwise.
*/
class NaiveScheduling : public SchedulingStrategy
{
public:
	~NaiveScheduling() override = default;
	bool schedule_trips(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) override;
};

#endif // ifndef CONTROLSTRATEGIES_H
/**@}*/