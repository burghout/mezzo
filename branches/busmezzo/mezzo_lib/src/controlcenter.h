#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

/**************************

General Notes:

**************************/

/*! Controlcenter groups together objects that control or modify transit objects
- RequestHandler
- BustripGenerator
- BustripVehicleMatcher
- VehicleDispatcher

Registers (unregisters) service vehicles
Offers interface to connected vehicles as well as connected passengers

*/

#include <vector>
#include <map>
#include <set>
#include <qobject.h>

#include "passenger.h"
#include "vehicle.h"
#include "busline.h"

/*structure that corresponds to a request from a passenger for a vehicle to travel between an origin stop and a destination stop*/
struct Request
{
	int pass_id;		//id of passenger that created request
	int ostop_id;		//id of origin stop
	int dstop_id;		//id of destination stop
	int load;			//number of passengers in request
	double time;		//time request was generated

	Request() {};
    Request(int pid, int oid, int did, int l, double t);

	bool operator == (const Request& rhs) const;
	bool operator < (const Request& rhs) const; // default less-than comparison of Requests in the order of smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id

};
Q_DECLARE_METATYPE(Request);


/*Responsible for adding Requests to requestSet as well as sorting and distributing the requestSet*/
class RequestHandler
{
	friend class TestControlcenter;
	friend class BustripGenerator; //BustripGenerator receives access to the requestSet as input to trip generation decisions

public:
	RequestHandler();
	~RequestHandler();

	void reset();

	bool addRequest(const Request req); //adds request vehRequest to the requestSet
	void removeRequest(const int pass_id); //removes requests with pass_id from the requestSet if it exists

private:
	set<Request> requestSet_; //set of received requests sorted by time
};



/*Responsible for generating planned trips without vehicles and adding these to a set of planned trips*/
class Bustrip;
class Busline;
class TripGenerationStrategy;
class BustripGenerator
{
	enum generationStrategyType { Null = 0, Naive }; //ids of trip generation strategies known to BustripGenerator
	friend class BustripVehicleMatcher; //give matcher class access to unmatchedTrips_. May remove trip from this set without destroying it if it has been matched

public:
	explicit BustripGenerator(TripGenerationStrategy* generationStrategy = nullptr);
	~BustripGenerator();

	bool requestTrip(const RequestHandler& rh, double time); //returns true if an unassigned trip has been generated and added to unmatchedTrips_ and false otherwise
	void setTripGenerationStrategy(int type);

	void reset(int generation_strategy_type);
	void addServiceRoute(Busline* line);
	
	void cancelUnmatchedTrip(Bustrip* trip); //destroy and remove trip from set of unmatchedTrips_
	

private:
	set<Bustrip*> unmatchedTrips_; //set of trips to be performed that have not been matched to vehicles yet
	vector<Busline*> serviceRoutes_; //lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes do not need to know about this?)

	TripGenerationStrategy* generationStrategy_;
};

/*Algorithms for generating a Bustrip (unassigned to a vehicle)*/
class Busstop;
typedef pair<Busstop*, double> Visit_stop; 
class TripGenerationStrategy
{
public:
	virtual ~TripGenerationStrategy() {}
	virtual bool calc_trip_generation(
		const set<Request>&		requestSet,				//set of requests that motivate the potential generation of a trip
		const vector<Busline*>& candidateServiceRoutes, //service routes available to potentially serve the requests
		const double			time,					//time for which calc_trip_generation is called
		set<Bustrip*>&			tripSet					//set of trips that have been generated to serve requests that have not been assigned to a vehicle yet
	) const = 0; //returns true if a trip was generated and added to tripSet container according to some strategy and false otherwise

protected:
	//supporting methods for generating trips with whatever TripGenerationStrategy
	map<pair<int, int>, int> countRequestsPerOD(const set<Request>& requestSet) const; //counts the number of requests with a particular od
	bool line_exists_in_tripset(const set<Bustrip*>& tripSet, const Busline* line) const; //returns true if a trip exists in tripset for the given busline
	vector<Busline*> get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const; //returns buslines among lines given as input that run from a given originstop to a given destination stop (Note: assumes lines are uni-directional and that busline stops are ordered, which they currently are in BusMezzo)
	vector<Visit_stop*> create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const; //creates a vector of scheduled visits to stops starting from the dispatch time (given by simulation time)
	Bustrip* create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& schedule) const; //creates a Bustrip for a given line with a desired start time and a scheduled arrival to stops along this line (subject to the availablility of a vehicle to serve this trip)
};
/*Null strategy that always returns false*/
class NullTripGeneration : public TripGenerationStrategy
{
public:
	~NullTripGeneration() override {}
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const override;
};
/*Matches trip according to oldest unassigned request & first line found to serve this request*/
class NaiveTripGeneration : public TripGenerationStrategy
{
public:
	~NaiveTripGeneration() override {}
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const override;
};



/*Responsible for assigning trips to transit vehicles and adding these to matchedTripSet as well as the trip's associated Busline*/
class Bus;
class MatchingStrategy; //e.g. hungarian, k-opt, insertion
class BustripVehicleMatcher
{
	enum matchingStrategyType { Null = 0, Naive }; //ids of trip vehicle matching strategies known to BustripVehicleMatcher
	friend class VehicleDispatcher; //give vehicle dispatcher access to matchedTrips

public:
	explicit BustripVehicleMatcher(MatchingStrategy* matchingStrategy = nullptr);
	~BustripVehicleMatcher();

	void reset(int matching_strategy_type);

	//find_candidate_vehicles
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //add vehicle to vector of vehicles assigned to serve the given line
	void removeVehicleFromServiceRoute(int line_id, Bus* transitveh); //remove vehicle from vector of vehicles assigned to serve the given line
	void setMatchingStrategy(int type);

	bool matchVehiclesToTrips(BustripGenerator& tg, double time);

private:
	set<Bustrip*> matchedTrips_; //set of trips that have been matched with a transit vehicle
	map<int, set<Bus*>> candidateVehicles_per_SRoute_; //maps lineIDs among service routes for this control center to vector of candidate transit vehicles

	MatchingStrategy* matchingStrategy_;
};

/*Algorithms for assigning a transit vehicle to a Bustrip*/
class MatchingStrategy
{
public:
	virtual ~MatchingStrategy() {}
	virtual bool find_tripvehicle_match(
		Bustrip*						unmatchedTrip,					//set of trips that are currently not assigned to any vehicle
		map<int, set<Bus*>>&			candidateVehicles_per_SRoute,	//set of candidate vehicles assigned with different service routes
		const double					time,							//time for which find_tripvehicle_match is called
		set<Bustrip*>&					matchedTrips					//set of trips that each have a vehicle assigned to them
	) = 0; //returns true if a trip from unmatchedTrips has been matched with a vehicle from candidateVehicles_per_SRoute and added to matchedTrips. The trip is in this case also removed from unmatchedTrips

protected:
	void assign_idlevehicle_to_trip(Busstop* currentStop, Bus* idletransitveh, Bustrip* trip, double starttime); //performs all operations (similar to Network::read_busvehicle) required in assigning a trip to an idle vehicle (TODO: assigning driving vehicles). Also removes the the assigned vehicle from vector of unassigned vehicles at stop from currentStop
	Bustrip* find_most_recent_trip(const set<Bustrip*>& trips) const; //returns trip with the earliest starttime among a set of trips
};
/*Null matching strategy that always returns false*/
class NullMatching : public MatchingStrategy
{
public:
	~NullMatching() override {}
	bool find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips) override;
};

/*Naive matching strategy always matches the first trip in unmatchedTrips to the first candidate transit veh found (if any) at the origin stop of the trip.*/
class NaiveMatching : public MatchingStrategy
{
public:
	~NaiveMatching() override {}
	bool find_tripvehicle_match(Bustrip* unmatchedTrip, map<int, set<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips) override;
};

/*In charge of dispatching transit vehicle - trip pairs*/
class DispatchingStrategy;
class VehicleDispatcher
{
	enum dispatchStrategyType {Null = 0, Naive = 1};

public:
	explicit VehicleDispatcher(Eventlist* eventlist = nullptr, DispatchingStrategy* dispatchingStrategy = nullptr);
	~VehicleDispatcher();

	void reset(int dispatching_strategy_type);

	bool dispatchMatchedTrips(BustripVehicleMatcher& tvm, double time); //returns true if a trip has successfully been scheduled and added to its line/service route
	void setDispatchingStrategy(int type);

private:
	Eventlist* eventlist_; //currently the dispatching strategy books a busline event (vehicle - trip dispatching event)

	DispatchingStrategy* dispatchingStrategy_; //strategy that is used to determine the start time (for dispatch) of flexible transit vehicle trips
};

/*Algorithms for deciding the start time of a trip that has been matched with a vehicle*/
class DispatchingStrategy
{
public:
	virtual ~DispatchingStrategy() {}
	virtual bool calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) = 0; //returns true if an unscheduled trip has been given a dispatch time and added to its line

protected:
	bool dispatch_trip(Eventlist* eventlist, Bustrip* trip); //add a matched trip (i.e., trip has a vehicle, a line, a schedule and a start time) to trips vector of its Busline and adds a Busline event to dispatch this at the start time of the trip
	void update_schedule(Bustrip* trip, double new_starttime); //takes trip that already has a desired schedule for both dispatch and stop visits, and updates both given a new start time
};

/*Null dispatching rule that always returns false*/
class NullDispatching : public DispatchingStrategy
{
public:
	~NullDispatching() override {}
    bool calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) override;
};

/*Dispatches the first trip found in unscheduledTrips at the earliest possible time*/
class NaiveDispatching : public DispatchingStrategy
{
public:
	~NaiveDispatching() override {}
	bool calc_dispatch_time(Eventlist* eventlist, set<Bustrip*>& unscheduledTrips, double time) override;
};

/*Groups togethers processes that control or modify transit Vehicles and provides an interface to Passenger*/
class Passenger;
enum class BusState;
class Controlcenter : public QObject
{
	Q_OBJECT
    friend class TestControlcenter;
	friend class Network; //for writing outputs

public:
	explicit Controlcenter(
		Eventlist* eventlist = nullptr, //currently the dispatcher needs the eventlist to book Busline (vehicle - trip dispatching) events
		int id = 0,
		int tg_strategy = 0,
		int tvm_strategy = 0,
		int vd_strategy = 0,
		QObject* parent = nullptr
	);
	~Controlcenter();

	void reset();

private:
	void connectInternal(); //connects internal signal slots (e.g. requestAccepted with on_requestAccepted) that should always be connected for each Controlcenter and between resets

public:
	//methods for connecting passengers, vehicles and lines
	void connectPassenger(Passenger* pass); //connects Passenger signals to receiveRequest slot
	void disconnectPassenger(Passenger* pass); //disconnects Passenger signals to receiveRequest slot
	
	void connectVehicle(Bus* transitveh); //connects Vehicle to CC
	void disconnectVehicle(Bus* transitveh); //disconnect Vehicle from CC

	void addServiceRoute(Busline* line); //add line to BustripGenerator map of possible lines to create trips for
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //add bus to vector of candidate vehicles that may be assigned trips for this line
	void removeVehicleFromServiceRoute(int lind_id, Bus* transitveh); //remove bus from vector of candidate vehicles that may be assigned trips for this line

	void addInitialVehicle(Bus* transitveh); 
	void addCompletedVehicleTrip(Bus* transitveh, Bustrip* trip);

signals:
	void requestAccepted(double time);
	void requestRejected(double time);

	void newUnassignedVehicle(double time);

	void tripGenerated(double time);
	void tripVehicleMatchFound(double time);

private slots:
	
	//request related
	void receiveRequest(Request req, double time); //time refers to when the request was received, may or may not be the same as the request time
	void removeRequest(int pass_id); //remove request with pass_id from requestSet in RequestHandler

	void on_requestAccepted(double time);
	void on_requestRejected(double time);

	void on_tripGenerated(double time);
	void on_tripVehicleMatchFound(double time);

	//fleet related
	void updateFleetState(int bus_id, BusState newstate, double time);
	void requestTrip(double time); //delegates to BustripGenerator to generate a trip depending on whatever strategy it currently uses and the state of the RequestHandler and add this to its list of trips

	void matchVehiclesToTrips(double time);
	void dispatchMatchedTrips(double time);

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_;	//id of control center
	const int tg_strategy_;	//initial trip generation strategy
	const int tvm_strategy_; //initial trip - vehicle matching strategy
	const int vd_strategy_; //initial vehicle dispatching strategy
	
	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //passengers currently connected to Controlcenter 
	map<int, Bus*> connectedVeh_; //transit vehicles currently connected to Controlcenter
	//map<BusState, vector<Bus*>> fleetState; //among transit vehicles connected to Controlcenter keeps track of which are in each possible bus vehicle (i.e. transit vehicle) state 
	
	RequestHandler rh_;
	BustripGenerator tg_;
	BustripVehicleMatcher tvm_;
	VehicleDispatcher vd_;

	set<Bus*> initialVehicles_; //vehicles assigned to this controlcenter on input (should be preserved between resets)
	vector<pair<Bus*, Bustrip*>> completedVehicleTrips_; //used for bookeeping heap allocated buses and bustrips (similar to busvehicles and bustrips in network) for writing output and deleting between resets
};
#endif
