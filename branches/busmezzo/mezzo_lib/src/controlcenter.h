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

#include <map>
#include <qobject.h>
#include <set>
#include <vector>

#include "busline.h"

struct Request;
class Bus;
enum class BusState;
enum class PassengerState;
class TripGenerationStrategy;
class MatchingStrategy;
class SchedulingStrategy;
class Eventlist;
class Passenger;
class Network;
class Busline;
class Busstop;
class Bustrip;
class Controlcenter;

/**
 * @ingroup DRT
 *
 * @brief data used in strategies for assigning vehicles to dynamically generated trips
 */
struct DRTAssignmentData
{
	void reset();
	set<Bustrip*,ptr_less<Bustrip*> > unmatched_trips;
	set<Bustrip*,ptr_less<Bustrip*> > unmatched_empty_trips;
	set<Bustrip*,ptr_less<Bustrip*> > unscheduled_trips;
    set<Bustrip*,ptr_less<Bustrip*> > active_trips; //!< all matched and scheduled trips that have ever been generated have not completed

	set<Request*, ptr_less<Request*> > active_requests; //!< all non-completed requests sent to and accepted by the ControlCenter
	set<Request*, ptr_less<Request*> > rejected_requests; //!< all requests rejected by ControlCenter
	set<Request*, ptr_less<Request*> > completed_requests; //!< all requests that were picked up and dropped off

    map<BusState, set<Bus*> > fleet_state; //!< all candidate vehicles to be assigned, or reassigned to activeTrips

	Controlcenter* cc_owner = nullptr;
	void print_state(double time) const;

	int planned_capacity = 0; //!< @todo PARTC specific, assumes the entire drt fleet of each Controlcenter has the same capacity, remove
};

struct Controlcenter_SummaryData
{
	void reset();
	unsigned int requests_recieved = 0;
	unsigned int requests_accepted = 0;
	unsigned int requests_served = 0;
	unsigned int requests_rejected = 0;
};


//!< just used as key for caching shortest path calls for now
struct Controlcenter_OD
{
	Controlcenter_OD() = default;
	~Controlcenter_OD() = default;
	Controlcenter_OD(Busstop* orig, Busstop* dest);
	
	bool operator==(const Controlcenter_OD& rhs) const;
	bool operator<(const Controlcenter_OD& rhs) const;

	Busstop* orig = nullptr;
	Busstop* dest = nullptr;
};

//! @brief responsible for adding and removing a passenger Request to/from a set of requests as well as sorting and distributing requests to process classes of a Controlcenter
/*!
    Groups the request handling processes of a Controlcenter. 

    @todo 
          1. Add strategy pattern for bundling requests to RequestHandler, extract all request bundling supporting methods from BustripGenerator and move these to e.g. RequestBundlingStrategy
          2. Associate each request, or bundle of requests with the unmatched trip or trips that they end up generating (for use in e.g. trip-cancellation). In other words separate requestSet 
          into e.g. unserved requests and scheduled requests.
*/
class RequestHandler
{
public:
	RequestHandler();
	~RequestHandler();

	void reset(); //!< resets members between simulation replications

    bool addRequest(DRTAssignmentData& assignment_data, Request* req, const set<Busstop*, ptr_less<Busstop*> > &serviceArea); //!< adds request passenger Request to the requestSet
	void removeActiveRequest(DRTAssignmentData& assignment_data, int pass_id); //!< removes requests with pass_id from the requestSet if it exists
    bool isFeasibleRequest(const Request* req, const set<Busstop*, ptr_less<Busstop*>>& serviceArea) const; //!< returns true if request is feasible for a given service area, false otherwise
};

//! @brief responsible for generating an unassigned trip for any Busline (in this context a line is equivalent a sequence of scheduled Busstops along a Busroute) within a given service area
/*!
    BustripGenerator is responsible for generating planned trips for a given service area (i.e. lines within serviceRoutes_) and adding these to a set of unmatched trips.
    These planned/unmatched trips are later assigned vehicles via a BustripVehicleMatcher.
    Conditions and algorithms (TripGenerationStrategy) that determine the generation of a planned passenger carrying or empty-vehicle pick-up/rebalancing trips are governed by
    the BustripGenerator's generationStrategy_ and emptyVehicleStrategy_.
*/
class BustripGenerator
{
    enum generationStrategyType { Null = 0, Naive, Simple }; //!< ids of passenger trip generation strategies known to BustripGenerator
	enum emptyVehicleStrategyType {	EVNull = 0, EVNaive, EVSimple, EVMaxWait, EVCumWait }; //!< ids of empty-vehicle redistribution strategies known to BustripGenerator
	friend class BustripVehicleMatcher; //!< gives BustripVehicleMatcher access to serviceRoutes for initializing vehicles

public:
	explicit BustripGenerator(Network* theNetwork = nullptr, TripGenerationStrategy* generationStrategy = nullptr, TripGenerationStrategy* emptyVehicleStrategy = nullptr);
	~BustripGenerator();

    void reset(int generation_strategy_type, int empty_vehicle_strategy_type); //!< resets members and trip generation strategies

	bool requestTrip(DRTAssignmentData& assignment_data, double time); //!< returns true if an unassigned trip has been generated and false otherwise
	bool requestEmptyTrip(DRTAssignmentData& assignment_data, double time); //!< returns true if an unassigned empty pick-up or rebalancing trip has been generated and false otherwise

	void setTripGenerationStrategy(int type); //!< destroy current generationStrategy_ and set to new type
	void setEmptyVehicleStrategy(int type); //!< destroy current emptyVehicleStrategy_ and set to new type

	TripGenerationStrategy* getGenerationStratgy() const;

	void addServiceRoute(Busline* line); //!< add a potential service route that this BustripGenerator can plan trips for
    vector<Busline*> getServiceRoutes() const;

private:
    vector<Busline*> serviceRoutes_; //!< lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes need to know about this? Currently we never reset this either)

	TripGenerationStrategy* generationStrategy_; //!< strategy for generating planned passenger carrying trips
	TripGenerationStrategy* emptyVehicleStrategy_; //!< strategy for generating planned empty-vehicle trips

	Network* theNetwork_; //!< for calculating shortest paths
};

//! @brief assigns transit vehicles to planned/unmatched trips
/*!
   BustripVehicleMatcher is responsible for assigning planned/unmatched trips to candidate transit vehicles and adding these to a set of unscheduled_trips. What is considered
   a candidate vehicle as well as the conditions that determine the vehicle - trip match are governed by the BustripVehicleMatcher's MatchingStrategy. Matched but unscheduled trips
   are later scheduled for dispatch via a VehicleScheduler
*/
class BustripVehicleMatcher
{
	enum matchingStrategyType { Null = 0, Naive }; //!< ids of each MatchingStrategy known to BustripVehicleMatcher

public:
	explicit BustripVehicleMatcher(MatchingStrategy* matchingStrategy = nullptr);
	~BustripVehicleMatcher();

	void reset(int matching_strategy_type); //!< resets members and matching strategy 

	//find_candidate_vehicles
    void addVehicleToAllServiceRoutes(const BustripGenerator& tg, Bus* transitveh); //!< adds vehicle as candidate to serve ALL service routes in BustripGenerator
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //!< add vehicle to vector of vehicles assigned to serve the given line
	void removeVehicleFromServiceRoute(int line_id, Bus* transitveh); //!< remove vehicle from vector of vehicles assigned to serve the given line
	void setMatchingStrategy(int type); //!< destroy current matchingStrategy_ and set to new type

	bool matchVehiclesToTrips(DRTAssignmentData& assignment_data, double time); //!< returns true if at LEAST one unmatched trip was assigned to a vehicle
	bool matchVehiclesToEmptyVehicleTrips(DRTAssignmentData& assignment_data, double time); //!< returns true if at LEAST one unmatched empty trip was assigned to a vehicle

private:
	map<int, set<Bus*>> vehicles_per_service_route_; //!< maps lineIDs among service routes for this control center to vector of candidate transit vehicles

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
	enum schedulingStrategyType {Null = 0, Naive = 1, LatestDeparture = 2}; //!< ids of strategies for scheduling trips that are known to the VehicleScheduler

public:
	explicit VehicleScheduler(Eventlist* eventlist = nullptr, SchedulingStrategy* SchedulingStrategy = nullptr);
	~VehicleScheduler();

	void reset(int scheduling_strategy_type); //!< resets members and scheduling strategy

	bool scheduleMatchedTrips(DRTAssignmentData& assignment_data, double time); //!< returns true if a trip has successfully been scheduled and booked for dispatch on its line/service route in the eventlist
	void setSchedulingStrategy(int type); //!< destroy current schedulingStrategy_ and set to new type

private:
	Eventlist* eventlist_; //!< currently needed by the scheduling strategy to book a busline event (vehicle - trip dispatching event)

	SchedulingStrategy* schedulingStrategy_; //!< strategy that is used to determine the start time (for dispatch) of flexible transit vehicle trips and scheduled visits to stops
};

//*! @brief groups together objects that control or modify demand-responsive transit objects @todo update description, alot of refactoring has been done, the signal slot calls are the same but the ownership and responsibilities of process classes have changed
/*!
    Controlcenter functions as a facade for four process classes ((1) RequestHandler, (2) BustripGenerator, (3), BustripVehicleMatcher, (4) VehicleScheduler)
    used for dynamically assigning transit vehicles to passenger travel requests. The Controlcenter registers (unregisters) service vehicles and passengers, and
    functions as an interface between connected vehicles and connected passengers at connected busstops.

    The process of recieving a passenger request to assigning a vehicle to this request via the control center roughly follows the following sequence:
       RequestHandler -> [requestSet]
    -> BustripGenerator -> [unmatched_trips] & [unmatched_empty_trips]
    -> BustripVehicleMatcher -> [unscheduled_trips]
    -> VehicleScheduler -> [active_trips]
*/
class Controlcenter : public QObject
{
	Q_OBJECT
    friend class TestControlcenter; //!< for writing unit tests for Controlcenter
	friend class TestDRTAlgorithms;
	friend class TestFixedWithFlexible_walking; //!< for writing integration tests for FWF network
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
		double rebalancing_interval = 0.0,
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
	int getID() const;
	Controlcenter_SummaryData getSummaryData() const;
    set<Busstop *, ptr_less<Busstop *> > getServiceArea() const;
    vector<Busline*> getServiceRoutes() const;
    map<int,Bus*> getConnectedVehicles() const;

	map<BusState, set<Bus*> > getFleetState() const;
	void printFleetState() const; //!< for printing the state of the entire fleet for debugging

	set<Bus*> getAllVehicles();
	set<Bus*> getVehiclesDrivingToStop(Busstop* stop); //!< get connected vehicles that are driving to target stop
	set<Bus*> getVehiclesEnRouteToStop(Busstop* stop); //!< get connected vehicles that are assigned to a trip with stop included in their downstream route
	set<Bus*> getOnCallVehiclesAtStop(Busstop* stop); //!< get connected vehicles that are currently on-call at target stop
	pair<Bus*,double> getClosestVehicleToStop(Busstop* stop, double time); //returns closest vehicle to stop and shortest expected time to get there
	
	double calc_route_travel_time(const vector<Link*>& routelinks, double time);
	vector<Link*> find_shortest_path_between_stops(Busstop * origin_stop, Busstop * destination_stop, double start_time);

    bool isInServiceArea(Busstop* stop) const; //!< true if stop is included in service area of Controlcenter, false otherwise
	bool getGeneratedDirectRoutes();
	void setGeneratedDirectRoutes(bool generate_direct_routes);

	//methods related to rebalancing
    double get_rebalancing_interval () const { return rebalancing_interval_; }
	void add_collection_stop(Busstop* stop); //!< add stop to collection_stops_ the set of stops within the service of this control center's fleet that are used as targets for rebalancing

	//methods for connecting passengers, vehicles, stops and lines
	void connectPassenger(Passenger* pass); //!< connects passenger signals to control center slots
	void disconnectPassenger(Passenger* pass); //!< disconnects passenger signals from control center slots
	
	void connectVehicle(Bus* transitveh); //!< connects transit vehicle signals to control center slots
	void disconnectVehicle(Bus* transitveh); //!< disconnects transit vehicle signals from control center slots

    void addStopToServiceArea(Busstop* stop); //!< add stop to serviceArea_ the set of stops within the service area of this control center's fleet
	void addServiceRoute(Busline* line); //!< add line to BustripGenerator's map of possible lines to create trips for
    void addVehicleToAllServiceRoutes(Bus* transitveh); //!< add transit vehicle as a candidate vehicle to be assigned trips for to all service routes in BustripGenerator
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //!< add transit vehicle to vector of candidate vehicles that may be assigned trips for a given line/service route
	void removeVehicleFromServiceRoute(int line_id, Bus* transitveh); //!< remove bus from vector of candidate vehicles that may be assigned trips for a given line/service route

	void addInitialVehicle(Bus* transitveh); //!< add vehicle assigned to this control center on input (that should be preserved between resets)
	void addCompletedVehicleTrip(Bus* transitveh, Bustrip* trip); //!< add vehicle - trip pair to vector of completed trips
    vector<pair<Bus*, Bustrip*> > getCompletedVehicleTrips() const;

    // Methods to retrieve level of service attributes for an individual 'line' or trip
	double calc_expected_ivt(Busline* service_route, Busstop* start_stop, Busstop* end_stop, bool first_line_leg, double time); //!< first_line_leg = false if the exploration estimate should be returned and true if calculated based on fleet state/rti etc..
	double calc_exploration_ivt(Busline* service_route, Busstop* start_stop, Busstop* end_stop); //!< returns an optimistic exploration estimate of IVT for a given line leg (shortest IVT between the stops), independent of time or RTI level
	double calc_expected_wt(Busline* service_route, Busstop* start_stop, Busstop* end_stop, bool first_line_leg, double walking_time_to_start_stop, double arrival_time_to_start_stop); //!< first_line_leg = false if the exploration estimate should be returned and true if calculated based on fleet state/rti etc.., Returns wt in seconds, walking time argument also in seconds
	double calc_exploration_wt(); //!< returns an optimistic exploration estimate of WT (currently zero seconds, immediate service) for a given line leg, independent of time or RTI level

    void removeActiveRequest(int pass_id); //!< remove active request with pass_id

signals:
	void requestAccepted(double time); //!< emitted when request has been recieved by RequestHandler
	void requestRejected(double time); //!< emitted when a request has been rejected by RequestHandler

	void newUnassignedVehicle(double time); //!< emitted when a connected transit vehicle has changed its state to OnCall

	void tripGenerated(double time);
	void tripNotGenerated(double time);
	void emptyTripGenerated(double time);
	void emptyTripNotGenerated(double time);

	void tripVehicleMatchFound(double time); 
	void tripVehicleMatchNotFound(double time);

	void emptyTripVehicleMatchNotFound(double time);

private slots:
	//request related
	void receiveRequest(Request* req, double time); //!< checks if request is feasible and decides to either accept or reject it
	void updateRequestState(Passenger* pass, PassengerState oldstate, PassengerState newstate, double time);

	//fleet related
	void updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time); //!< updates fleetState every time a connected transit vehicle changes its state
	
	void requestTrip(double time); //!< delegates to BustripGenerator to generate a planned passenger carrying trip
	void requestEmptyTrip(double time); //!< delegates to BustripGenerator to generate a planned empty-vehicle trip

	void matchVehiclesToTrips(double time); //!< delegates to BustripVehicleMatcher to assign connected transit vehicles to planned passenger carrying trips
	void matchEmptyVehiclesToTrips(double time); //!< delegates to BustripVehicleMatcher to assign connected transit vehicles to planned empty-vehicle trips

	void scheduleMatchedTrips(double time); //!< delegates to VehicleScheduler to schedule matched trip - vehicle pairs for dispatch on their service route

    //for debug output
    void on_requestAccepted(double time); 
    void on_requestRejected(double time); 

    void on_tripGenerated(double time);
    void on_tripNotGenerated(double time);
    void on_emptyTripNotGenerated(double time);
    void on_tripVehicleMatchFound(double time);
    void on_tripVehicleMatchNotFound(double time);
	void on_emptyTripVehicleMatchNotFound(double time);

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_; //!< id of control center
	const int tg_strategy_; //!< initial trip generation strategy
	const int ev_strategy_; //!< initial empty vehicle strategy
	const int tvm_strategy_; //!< initial trip - vehicle matching strategy
	const int vs_strategy_; //!< initial vehicle scheduling strategy

	bool generated_direct_routes_ = false; //!< true if direct routes have been generated and added as service routes between all stops in the service area of this control center

	const double rebalancing_interval_; //!< time interval in between each rebalancing call @note e.g. first rebalancing call will be <rebalancing_interval_> seconds after start_pass_generation, <rebalancing_interval_> seconds after that etc..
	set<Busstop*,ptr_less<Busstop*> > collection_stops_; //!< set of stops used as targets for rebalancing

	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //!< passengers currently connected to Controlcenter 
	map<int, Bus*> connectedVeh_; //!< transit vehicles currently connected to Controlcenter

	/**
	 * Functions for askng RequestHandler for unserved Requests to map to passengers etc... matched Requests 
	 * @todo
	 *	Double-mapping between connected Vehicles and Passengers
	 *  Which connected passengers are assigned to a vehicle and which vehicle a passenger has been assigned
	 *  
	 *  Vehicle-trips & passenger-trips...
	 */

    //process classes of control center
	RequestHandler rh_;
	BustripGenerator tg_;
	BustripVehicleMatcher tvm_;
	VehicleScheduler vs_;

    set<Busstop*, ptr_less<Busstop*> > serviceArea_; //!< set of stops in the service area of this control center's fleet of vehicles. In other words the stops for which this control center can generate trips between
    set<Bus*> initialVehicles_; //!< vehicles assigned to this control center on input (that should be preserved between resets)
	vector<pair<Bus*, Bustrip*> > completedVehicleTrips_; //!< used for bookkeeping dynamically generated buses and bustrips (similar to busvehicles and bustrips in network) for writing output and deleting between resets
	DRTAssignmentData assignment_data_; //!< stores dynamic vehicle, trip and request data used in assignment pipeline

	map<Controlcenter_OD, vector<Link*> > shortestPathCache; //!< cache for the first shortest path calls made between stops of this Controlcenter @todo add time-dependent caches maybe, currently only the initial calls are stored
	Controlcenter_SummaryData summarydata_; //!< collection of data for summarizing Controlcenter performance.
	Network* theNetwork_=nullptr; //!< again, ugly way of bringing in shortest path methods from the network to Controlcenter
};

#endif
/**@}*/
