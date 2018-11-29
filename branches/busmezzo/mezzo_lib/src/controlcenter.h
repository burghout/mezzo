/**
 * @defgroup DRT Simulation of demand-responsive transit
 * @{
 */

/**@file   controlcenter.h
 * @brief  Contains Controlcenter and supporting process classes used for communication between demand-responsive transit vehicles and potential passengers
 *
 */
#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

#include <vector>
#include <map>
#include <set>
#include <qobject.h>

#include "busline.h"

struct Request;
class Bus;
enum class BusState;
class TripGenerationStrategy;
class MatchingStrategy;
class SchedulingStrategy;
class Eventlist;
class Passenger;

//! @brief responsible for adding and removing a passenger Request to/from a requestSet as well as sorting and distributing requests in the requestSet to process classes of a Controlcenter
/*!
    groups the request handling processes of a Controlcenter. The responsibilities of the RequestHandler are to
    add a passenger travel request to a requestSet, as well as sorting and distributing this
    requestSet to dynamically generate demand-responsive trips via the Controlcenter's BustripGenerator.

    @todo 
          1. Add strategy pattern for bundling requests to RequestHandler, extract all request bundling supporting methods from BustripGenerator and move these to e.g. RequestBundlingStrategy
          2. Associate each request, or bundle of requests with the unmatched trip or trips that they end up generating (for use in e.g. trip-cancellation). In other words separate requestSet 
          into e.g. unserved requests and scheduled requests.
*/
class RequestHandler
{
	friend class TestControlcenter; //!< for writing unit tests for RequestHandler
	friend class BustripGenerator; //!< BustripGenerator receives access to the requestSet_ as input to trip generation decisions

public:
	RequestHandler();
	~RequestHandler();

	void reset(); //!< resets members between simulation replications

	bool addRequest(const Request req, const set<Busstop*>& serviceArea); //!< adds request passenger Request to the requestSet
	void removeRequest(const int pass_id); //!< removes requests with pass_id from the requestSet if it exists
    bool isFeasibleRequest(const Request& req, const set<Busstop*>& serviceArea) const; //!< returns true if request is feasible for a given service area, false otherwise

private:
	set<Request> requestSet_; //!< set of received requests sorted by desired departure time
};

//! @brief responsible for generating an unassigned trip for any Busline (in this context a line is equivalent a sequence of scheduled Busstops along a Busroute) within a given service area
/*!
    BustripGenerator is responsible for generating planned trips for a given service area (i.e. lines within serviceRoutes_) and adding these to a set of unmatched trips.
    These planned/unmatched trips are later assigned vehicles via a BustripVehicleMatcher. Transit supply rebalancing trips may also be generated.
    Conditions and algorithms (TripGenerationStrategy) that determine the generation of a planned passenger carrying or empty-vehicle rebalancing trips are governed by
    the BustripGenerator's generationStrategy_ and emptyVehicleStrategy_.
*/
class BustripGenerator
{
    friend class TestDRT; //!< for writing drt integration tests

	enum generationStrategyType { Null = 0, Naive }; //!< ids of passenger trip generation strategies known to BustripGenerator
	enum emptyVehicleStrategyType {	EVNull = 0, EVNaive }; //!< ids of empty-vehicle redistribution strategies known to BustripGenerator
	friend class BustripVehicleMatcher; //!< give matcher class access to unmatchedTrips_. May remove trip from this set without destroying it if it has been matched. Also gives VehicleMatcher access to serviceRoutes for initializing vehicles

public:
	explicit BustripGenerator(Network* theNetwork = nullptr, TripGenerationStrategy* generationStrategy = nullptr, TripGenerationStrategy* emptyVehicleStrategy = nullptr);
	~BustripGenerator();

    void reset(int generation_strategy_type, int empty_vehicle_strategy_type); //!< resets members and trip generation strategies

	bool requestTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time); //!< returns true if an unassigned trip has been generated and added to unmatchedTrips_ and false otherwise
	bool requestRebalancingTrip(const RequestHandler& rh, const map<BusState,set<Bus*>>& fleetState, double time); //!< returns true if an unassigned rebalancing trip has been generated and added to unmatchedRebalancingTrips_ and false otherwise

	void setTripGenerationStrategy(int type); //!< destroy current generationStrategy_ and set to new type
	void setEmptyVehicleStrategy(int type); //!< destroy current emptyVehicleStrategy_ and set to new type

	void addServiceRoute(Busline* line); //!< add a potential service route that this BustripGenerator can plan trips for
	
	void cancelUnmatchedTrip(Bustrip* trip); //!< destroy and remove trip from set of unmatchedTrips_
	void cancelRebalancingTrip(Bustrip* trip); //!< destroy and remove rebalancing trip from set of unmatchedRebalancingTrips_

private:
	set<Bustrip*> unmatchedTrips_; //!< set of planned passenger carrying trips to be performed that have not been matched to vehicles yet
	set<Bustrip*> unmatchedRebalancingTrips_; //!< set of planned empty-vehicle rebalancing trips to be performed that have not been matched to vehicles yet
	vector<Busline*> serviceRoutes_; //!< lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes need to know about this? Currently we never reset this either)

	TripGenerationStrategy* generationStrategy_; //!< strategy for generating planned passenger carrying trips
	TripGenerationStrategy* emptyVehicleStrategy_; //!< strategy for generating planned empty-vehicle rebalancing trips

	Network* theNetwork_; //!< ugly hopefully temporary solution but a reference to the network is kept by each control center for calculating shortest paths with dynamic travel times
};

//! @brief assigns transit vehicles to planned/unmatched trips
/*!
   BustripVehicleMatcher is responsible for assigning planned/unmatched trips to candidate transit vehicles and adding these to a set of matchedTrips_. What is considered
   a candidate vehicle as well as the conditions that determine the vehicle - trip match are governed by the BustripVehicleMatcher's MatchingStrategy. These matched but unscheduled trips
   are later scheduled for dispatch via a VehicleScheduler
*/
class BustripVehicleMatcher
{
	enum matchingStrategyType { Null = 0, Naive }; //!< ids of each MatchingStrategy known to BustripVehicleMatcher
	friend class VehicleScheduler; //!< give VehicleScheduler access to matchedTrips_, will remove trips from this set when they are scheduled for dispatch

public:
	explicit BustripVehicleMatcher(MatchingStrategy* matchingStrategy = nullptr);
	~BustripVehicleMatcher();

	void reset(int matching_strategy_type); //!< resets members and matching strategy 

	//find_candidate_vehicles
    void addVehicleToAllServiceRoutes(const BustripGenerator& tg, Bus* transitveh); //!< adds vehicle as candidate to serve ALL service routes in BustripGenerator
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //!< add vehicle to vector of vehicles assigned to serve the given line
	void removeVehicleFromServiceRoute(int line_id, Bus* transitveh); //!< remove vehicle from vector of vehicles assigned to serve the given line
	void setMatchingStrategy(int type); //!< destroy current matchingStrategy_ and set to new type

	bool matchVehiclesToTrips(BustripGenerator& tg, double time); //!< returns true if at LEAST one unmatched trip was assigned to a vehicle
	bool matchVehiclesToEmptyVehicleTrips(BustripGenerator& tg, double time); //!< returns true if at LEAST one unmatched rebalancing trip was assigned to a vehicle

private:
	set<Bustrip*> matchedTrips_; //!< set of trips that have been matched with a transit vehicle but have not yet been dispatched
	map<int, set<Bus*>> vehicles_per_service_route; //!< maps lineIDs among service routes for this control center to vector of candidate transit vehicles

	MatchingStrategy* matchingStrategy_; //!< strategy for assigning unmatched trips to candidate transit vehicles
};

//*! @brief responsible for scheduling dispatch and stop visits of dynamically generated transit vehicle - trip pairs
/*!
    VehicleScheduler adds a schedule (e.g. an expected arrival time to the sequence of stops) to an unscheduled trip as well as a start time for when
    the transit vehicle assigned to an unscheduled trip should be dispatched from the first stop on its service route. The strategy used for adjusting scheduled stop visits
    and dispatch times are governed by the VehicleScheduler's SchedulingStrategy.
*/
class VehicleScheduler
{
	enum schedulingStrategyType {Null = 0, Naive = 1}; //!< ids of strategies for scheduling trips that are known to the VehicleScheduler

public:
	explicit VehicleScheduler(Eventlist* eventlist = nullptr, SchedulingStrategy* SchedulingStrategy = nullptr);
	~VehicleScheduler();

	void reset(int scheduling_strategy_type); //!< resets members and scheduling strategy

	bool scheduleMatchedTrips(BustripVehicleMatcher& tvm, double time); //!< returns true if a trip has successfully been scheduled and booked for dispatch on its line/service route in the eventlist
	void setSchedulingStrategy(int type); //!< destroy current schedulingStrategy_ and set to new type

private:
	Eventlist* eventlist_; //!< currently needed by the scheduling strategy to book a busline event (vehicle - trip dispatching event)

	SchedulingStrategy* schedulingStrategy_; //!< strategy that is used to determine the start time (for dispatch) of flexible transit vehicle trips and scheduled visits to stops
};

//*! @brief groups together objects that control or modify demand-responsive transit objects
/*!
    Controlcenter functions as a facade for four process classes ((1) RequestHandler, (2) BustripGenerator, (3), BustripVehicleMatcher, (4) VehicleScheduler)
    used for dynamically assigning transit vehicles to passenger travel requests. The Controlcenter registers (unregisters) service vehicles and passengers, and
    functions as an interface between connected vehicles and connected passengers at connected busstops.

    The process of recieving a passenger request to assigning a vehicle to this request via the control center roughly follows the following sequence:
       RequestHandler -> [requestSet]
    -> BustripGenerator -> [unmatchedTrips] & [unmatchedRebalancingTrips]
    -> BustripVehicleMatcher -> [matchedTrips]
    -> VehicleScheduler -> scheduled trip
*/
class Controlcenter : public QObject
{
	Q_OBJECT
    friend class TestControlcenter; //!< for writing unit tests for Controlcenter
    friend class TestDRT; //!< for writing drt integration tests
	friend class Network; //!< for writing results of completed trips to output files, and for generating direct lines between connectedStops_

public:
	explicit Controlcenter(
		Eventlist* eventlist = nullptr, //currently the vehicle scheduler needs the eventlist to book Busline (vehicle - trip dispatch) events
		Network* theNetwork = nullptr, //currently trip generation strategies need the network to calculate shortest paths
		int id = 0,
		int tg_strategy = 0,
		int ev_strategy = 0,
		int tvm_strategy = 0,
		int vs_strategy = 0,
		QObject* parent = nullptr
	);
	~Controlcenter();

	void reset(); /*!< Disconnects all current signal slots and resets these to initial state.
                       Calls reset for all member process classes. Clears all members and resets
                       connected transit vehicles to their initial state. */

private:
	void connectInternal(); /*!< Connects internal signal slots that should always be connected for each Controlcenter initially and between resets.
                                 Connects signal slot triggers between member process classes */

public:
    set<Busstop*> getServiceArea() const;
    bool isInServiceArea(Busstop* stop) const; //!< true if stop is included in service area of Controlcenter, false otherwise

	//methods for connecting passengers, vehicles, stops and lines
	void connectPassenger(Passenger* pass); //!< connects passenger signals to control center slots
	void disconnectPassenger(Passenger* pass); //!< disconnects passenger signals from control center slots
	
	void connectVehicle(Bus* transitveh); //!< connects transit vehicle signals to control center slots
	void disconnectVehicle(Bus* transitveh); //!< disconnects transit vehicle signals from control center slots

    void addStopToServiceArea(Busstop* stop); //!< add stop to stations_ the set of stops within the service area of this control center's fleet
	void addServiceRoute(Busline* line); //!< add line to BustripGenerator's map of possible lines to create trips for
    void addVehicleToAllServiceRoutes(Bus* transitveh); //!< add transit vehicle as a candidate vehicle to be assigned trips for to all service routes in BustripGenerator
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //!< add transit vehicle to vector of candidate vehicles that may be assigned trips for a given line/service route
	void removeVehicleFromServiceRoute(int line_id, Bus* transitveh); //!< remove bus from vector of candidate vehicles that may be assigned trips for a given line/service route

	void addInitialVehicle(Bus* transitveh); //!< add vehicle assigned to this control center on input (that should be preserved between resets)
	void addCompletedVehicleTrip(Bus* transitveh, Bustrip* trip); //!< add vehicle - trip pair to vector of completed trips

signals:
	void requestAccepted(double time); //!< emitted when request has been recieved by RequestHandler
	void requestRejected(double time); //!< emitted when a request has been rejected by RequestHandler

	void newUnassignedVehicle(double time); //!< emitted when a connected transit vehicle has changed its state to OnCall

	void tripGenerated(double time); //!< emitted when a planned passenger carrying trip (unmatched trip) has been generated by BustripGenerator
	void emptyVehicleTripGenerated(double time); //!< emitted when a planned empty-vehicle rebalancing (unmatched rebalancing trip) has been generated by BustripGenerator

	void tripVehicleMatchFound(double time); //!< emitted when a vehicle has been assigned to an unmatched passenger carrying trip or an unmatched rebalancing trip
	void tripVehicleMatchNotFound(double time); //!< emitted when an attempt to match vehicles to unmatched passenger carrying trips has been made but no match was found

private slots:
	//request related
	void receiveRequest(Request req, double time); //<! delegates to RequestHandler to add the request to its requestSet
	void removeRequest(int pass_id); //!< remove request with pass_id from requestSet in RequestHandler

	//fleet related
	void updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time); //!< updates fleetState every time a connected transit vehicle changes its state
	
	void requestTrip(double time); //!< delegates to BustripGenerator to generate a planned passenger carrying trip
	void requestRebalancingTrip(double time); //!< delegates to BustripGenerator to generate a planned empty-vehicle rebalancing trip

	void matchVehiclesToTrips(double time); //!< delegates to BustripVehicleMatcher to assign connected transit vehicles to planned passenger carrying trips
	void matchEmptyVehiclesToTrips(double time); //!< delegates to BustripVehicleMatcher to assign connected transit vehicles to planned empty-vehicle rebalancing trips

	void scheduleMatchedTrips(double time); //!< delegates to VehicleScheduler to schedule matched trip - vehicle pairs for dispatch on their service route

    //for debug output
    void on_requestAccepted(double time); //!< currently only used for debug messaging to console
    void on_requestRejected(double time); //!< currently only used for debug messaging to console

    void on_tripGenerated(double time); //!< currently only used for debug messaging to console
    void on_tripVehicleMatchFound(double time); //!< currently only used for debug messaging to console

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_;	//<! id of control center
	const int tg_strategy_;	//<! initial trip generation strategy
	const int ev_strategy_; //<! initial empty vehicle strategy
	const int tvm_strategy_; //<! initial trip - vehicle matching strategy
	const int vs_strategy_; //<! initial vehicle scheduling strategy
	
	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //!< passengers currently connected to Controlcenter 
	map<int, Bus*> connectedVeh_; //!< transit vehicles currently connected to Controlcenter
	map<BusState, set<Bus*>> fleetState_; //!< holds set of connected transit vehicles per transit vehicle state 

    //process classes of control center
	RequestHandler rh_;
	BustripGenerator tg_;
	BustripVehicleMatcher tvm_;
	VehicleScheduler vs_;

    set<Busstop*> serviceArea_; //!< set of stops in the service area of this control center's fleet of vehicles. In other words the stops for which this control center can generate trips between
	set<Bus*> initialVehicles_; //!< vehicles assigned to this control center on input (that should be preserved between resets)
	vector<pair<Bus*, Bustrip*>> completedVehicleTrips_; //!< used for bookkeeping dynamically generated buses and bustrips (similar to busvehicles and bustrips in network) for writing output and deleting between resets
};

#endif
/**@}*/