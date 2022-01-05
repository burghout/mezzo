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
#include <QString>

#include "busline.h"
#include "controlcenter.h"

struct DRTAssignmentData;
class Bustrip;
class Busline;
class Busstop;
typedef pair<Busstop*, double> Visit_stop;
class Bus;
enum class BusState;
class Network;
class Link;
class Passenger;


/**
 * @ingroup DRT
 * 
 * Enumerates possible states of a Request:
 *  Null - default uninitialized request
 *  Unmatched - request not matched to any vehicle trip
 *  Assigned - request is assigned to a vehicle trip, but not yet matched to a vehicle
 *  Matched - request is matched to a vehicle-trip pair but not picked-up yet
 *  ServedUnfinished - request has been picked-up but not dropped off
 *  ServedFinished - request has been picked-up and dropped off
 *  Rejected - request was rejected by DRT operator
 *  Cancelled - request was cancelled by traveler
 * 
 */
enum class RequestState { Null = 0, Unmatched, Assigned, Matched, ServedUnfinished, ServedFinished, Rejected, Cancelled };

//! @brief structure representing a passenger message to a control center requesting transport between an origin stop and a destination stop with a desired time of departure
struct Request
{
    static int id_counter;
    int id;
    int pass_id = -1;    //!< id of passenger that created request
    int ostop_id = -1;   //!< id of origin stop
    int dstop_id = -1;   //!< id of destination stop
    int load = -1;       //!< number of passengers in request
    double time_desired_departure = -1.0; //!< desired/earliest departure time for passenger
    double time_request_generated = -1.0; //!< time request was generated
    RequestState state = RequestState::Null; //!< current state of the request
    Passenger* pass_owner = nullptr; //!< passenger who sent this request
    //Bus* assigned_veh = nullptr; //!< vehicle that has been assigned to this request, nullptr if none has been assigned
    Bustrip* assigned_trip = nullptr; //!< bustrip that has been assigned to this request, nullptr by default, updated when assigned
    Request() { id = ++id_counter; }
    Request(Passenger* pass_owner, int pass_id, int ostop_id, int dstop_id, int load, double t_departure, double t_generated);

    void set_assigned_trip(Bustrip* newtrip); //!< sets the assigned trip of this request to newtrip and adds this as a 'assigned_request' to that trip. If previously assigned to a trip, the request is removed from that trip

    void set_state(RequestState state);
    void print_state();
    static QString state_to_QString(RequestState state);
    int get_id() const { return id; }

    bool operator == (const Request& rhs) const; //!< default equality comparison of Requests
    bool operator < (const Request& rhs) const; //!< default less-than comparison of Requests in the order of smallest departure time, smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id

};
Q_DECLARE_METATYPE(Request)



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
        DRTAssignmentData& assignment_data, //!< set of requests that motivate the potential generation of a trip
        const vector<Busline*>& candidateServiceRoutes, //!< state of all vehicles that are potentially available to serve the requests
        double time
    ) = 0; //!< returns true if a trip was generated and added to assignment_data.active_trips and false otherwise

public:
    //supporting methods for generating trips with whatever TripGenerationStrategy
    map<pair<int, int>, int> countRequestsPerOD(const set<Request*, ptr_less<Request*>>& requestSet) const; //!< counts the number of requests with a particular od
    bool line_exists_in_tripset(const set<Bustrip*, ptr_less<Bustrip*> >& tripSet, const Busline* line) const; //!< returns true if a trip exists in trip set for the given bus line
    vector<Busline*> find_lines_connecting_stops(const vector<Busline*>& lines, int ostop_id, int dstop_id); //!< returns buslines among lines given as input that run from a given origin stop to a given destination stop (Note: assumes lines are unidirectional and that bus line stops are ordered, which they currently are in BusMezzo)
	vector<Visit_stop*> create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const; //!< creates a vector of scheduled visits to stops starting from the preliminary start time of the trip and given the scheduled in-vehicle time for the trip
	Bustrip* create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& schedule) const; //!< creates a Bustrip for a given line with a desired start time and a scheduled arrival to stops along this line (subject to the availability of a vehicle to serve this trip)
	
	//supporting methods for empty-vehicle redistribution strategies (maybe others in the future too TODO: figure out how best to share these)
    set<Bus*, bus_ptr_less<Bus*> > get_driving_vehicles(const map<BusState, set<Bus*, bus_ptr_less<Bus*> > >& fleetState) const;
    set<Bus*, bus_ptr_less<Bus*> > get_vehicles_enroute_to_stop(const Busstop* stop, const set<Bus*, bus_ptr_less<Bus*> >& vehicles) const; //!< returns true if at least one bus is currently driving to stop (and no intermediate stops) and false otherwise
	double calc_route_travel_time(const vector<Link*>& routelinks, double time) const; //!< returns the sum of dynamic travel time costs over all links in routelinks
    vector<Link*> find_shortest_path_between_stops(Network* theNetwork, const Busstop* origin_stop, const Busstop* destination_stop, double start_time); //!< returns the shortest route between a pair of stops for a given time, returns empty vector if none exists
	Busline* find_shortest_busline(const vector<Busline*>& lines, double time) const; //!< returns shortest busline in terms of scheduled in-vehicle time among lines
    pair <Bus*,double> get_nearest_vehicle(const Busstop* targetStop, const set<Bus*, bus_ptr_less<Bus*> >& vehicles, Network *theNetwork, double time); //!< returns nearest vehicle, returns pair(nullptr,0.0) if none
    vector<pair<Bus*,double> > find_nearest_vehicles(const Busstop* targetStop, const set<Bus*, bus_ptr_less<Bus*> >& vehicles, Network *theNetwork, double time); //!< returns vector of (non-full) vehicles in order of closest expected ivt to targetStop to furthest, returns empty vector if none exists
    vector<pair<Bus*, double>> find_nearest_vehicles(const Busstop* targetStop, const vector<Bus*>& vehicles, Network* theNetwork, double time); //!< returns vector of (non-full) vehicles in order of closest expected ivt to targetStop to furthest, returns empty vector if none exists

protected:
    map<int, map<int, vector<Busline*> > > cached_lines_connecting_stops; //!< cached results of TripGenerationStrategy::find_lines_connecting_stops
    map<int, map<int, vector<Link*> > > cached_shortest_path_between_stops;
};

//! @brief null strategy that always returns false
class NullTripGeneration : public TripGenerationStrategy
{
public:
	~NullTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;
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
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;
};

//! @brief Attempts to improve upon NaiveTripGeneration by taking into account matched requests, add bookkeeping to requests - trips - matched vehicles. Only send vehicles to unmatched requests, take capacity into account.
/*!
* Takes into account capacities of vehicles
* Uses bookkeeping to match passenger requests to trips, and trips to vehicles, explicitly
* Sort the untreated requests by origin, destination, and value (e.g. number): Map (OriginStop, DestinationStop, Value)
* Bundle into trips (called Unmatched Trips in Davids algo), and sort trips into cumulative Value (i.e. importance)
* Treat trips in descending order of Value by sending nearest vehicle to treat trip
* Add bookkeeping so that requests know of their trips and vice versa, etc.
* If passengers board another trip than their planned trip (in request), notify & update
*/
class SimpleTripGeneration : public TripGenerationStrategy
{
public:
    ~SimpleTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;
};


//Empty vehicle trip generation strategies
//! @brief Reactive empty vehicle repositioning strategy that attempts to generate a trip from the current stop of the closest on-call transit vehicle to the origin stop of the OD in requestSet with the highest demand
/*!
    @todo
        - Also consider vehicles that are in other states besides OnCall (e.g. vehicles en-route to origin stop with the highest demand) when searching for the nearest empty-vehicle, or when prioritizing ODs
*/
class NaiveEmptyVehicleTripGeneration : public TripGenerationStrategy
{
public:
	explicit NaiveEmptyVehicleTripGeneration(Network* theNetwork = nullptr);
	~NaiveEmptyVehicleTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

private:
	Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};

class SimpleEmptyVehicleTripGeneration : public TripGenerationStrategy
{
public:
    explicit SimpleEmptyVehicleTripGeneration(Network* theNetwork = nullptr);
    ~SimpleEmptyVehicleTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

private:
    Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};

class MaxWaitEmptyVehicleTripGeneration : public TripGenerationStrategy
{
public:
    explicit MaxWaitEmptyVehicleTripGeneration(Network* theNetwork = nullptr);
    ~MaxWaitEmptyVehicleTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

private:
    Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};

class CumulativeWaitEmptyVehicleTripGeneration : public TripGenerationStrategy
{
public:
    explicit CumulativeWaitEmptyVehicleTripGeneration(Network* theNetwork = nullptr);
    ~CumulativeWaitEmptyVehicleTripGeneration() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

private:
    Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};


//Rebalancing Strategies

/*! @brief Naive rebalancing strategy that distributes on-call vehicles evenly among a set of collection stops
 *  - a set of collection stops and a rebalancing interval are manually selected
    - each rebalancing interval seconds a target capacity at all collection stops is calculated by num_oncall_vehicles / num_collection_stops
    - starting with the collection stop with the lowest capacity a search for the nearest oncall vehicles that are not currently at a collection stop is performed
    - closest vehicles are assigned until target capacity is reached, then move onto stop with next lowest capacity
    - continue until no on-call vehicles (not present at a collection stop) are remaining, or all stops have reached target capacity
 */
class NaiveRebalancing : public TripGenerationStrategy
{
public:
    explicit NaiveRebalancing(Network* theNetwork = nullptr);
    ~NaiveRebalancing() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

private:
    Network* theNetwork_; //!< currently needs access to the network to find the closest on-call Bus to origin stop of highest OD demand
};

//!< @brief Same as Naive rebalancing but includes en-route vehicles on top of on-call vehicles in target capacity calculation, collection stops with on-call vehicles in excess capacity are also rebalanced
class SimpleRebalancing : public TripGenerationStrategy
{
public:
    explicit SimpleRebalancing(Network* theNetwork = nullptr);
    ~SimpleRebalancing() override = default;
    bool calc_trip_generation(DRTAssignmentData& assignment_data, const vector<Busline*>& candidateServiceRoutes, double time) override;

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
        DRTAssignmentData& assignment_data,
        Bustrip* unmatchedTrip,
        double time //!< time find_tripvehicle_match is called
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
	bool find_tripvehicle_match(DRTAssignmentData& assignment_data, Bustrip* unmatchedTrip, double time) override;
};

//! @brief Naive matching strategy always attempts to match the unmatchedTrip to the first candidate transit vehicle found (if any) at the origin stop of the unmatchedTrip
class NaiveMatching : public MatchingStrategy
{
public:
	~NaiveMatching() override = default;
	bool find_tripvehicle_match(DRTAssignmentData& assignment_data, Bustrip* unmatchedTrip, double time) override;
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
	virtual bool schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, double time) = 0; //!< returns true if an unscheduled trip has been given a dispatch time and added to the eventlist as a Busline action. This trip is then removed from the set of unscheduledTrips

protected:
	bool book_trip_dispatch(DRTAssignmentData& assignment_data, Eventlist* eventlist, Bustrip* trip); //!< add a matched and scheduled trip (i.e., Bustrip that has a Bus, a Busline, a schedule and a start time) to the trips list of its Busline and add a Busline event to dispatch this trip its given start time
};

//! @brief Null scheduling strategy that always returns false
class NullScheduling : public SchedulingStrategy
{
public:
	~NullScheduling() override = default;
	bool schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, double time) override;
};

//! @brief Schedules all trips found in unscheduledTrips to be dispatched at the earliest possible time (i.e. immediately)
/*!
    Currently assumes that the vehicle of an unscheduled trip is already located at the origin stop of the trip. Will call abort otherwise.
*/
class NaiveScheduling : public SchedulingStrategy
{
public:
	~NaiveScheduling() override = default;
	bool schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, double time) override;
};

//! @brief Schedules all trips found in unscheduledTrips to be dispatched according to the latest desired departure time among requests assigned to the trip
/*!
*   Currently assumes that the vehicle of an unscheduled trip is already located at the origin stop of the trip. Will call abort otherwise.
*   The vehicle will remain in an 'IdleEmpty' state until dispatched. Note that this vehicle will thus be ignored in trip-generation/matching calls that search for OnCall vehicles
*   
*   
*/
class LatestDepartureScheduling : public SchedulingStrategy
{
public:
    ~LatestDepartureScheduling() override = default;
    bool schedule_trips(DRTAssignmentData& assignment_data, Eventlist* eventlist, double time) override;
};


#endif // ifndef CONTROLSTRATEGIES_H
/**@}*/
