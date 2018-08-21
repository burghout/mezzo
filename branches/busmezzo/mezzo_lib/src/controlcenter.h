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
Offers interface between connected vehicles and connected passengers at connected stops

*/

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
class DispatchingStrategy;
class Eventlist;
class Passenger;

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
class BustripGenerator
{
	enum generationStrategyType { Null = 0, Naive }; //ids of passenger trip generation strategies known to BustripGenerator
	enum emptyVehicleStrategyType {	EVNull = 0, NearestLongestQueue }; //ids of empty-vehicle redistribution strategies known to BustripGenerator
	friend class BustripVehicleMatcher; //give matcher class access to unmatchedTrips_. May remove trip from this set without destroying it if it has been matched

public:
	explicit BustripGenerator(TripGenerationStrategy* generationStrategy = nullptr, TripGenerationStrategy* emptyVehicleStrategy = nullptr);
	~BustripGenerator();

	bool requestTrip(const RequestHandler& rh, const map<BusState, set<Bus*>>& fleetState, double time); //returns true if an unassigned trip has been generated and added to unmatchedTrips_ and false otherwise
	bool requestRebalancingTrip(const RequestHandler& rh, const map<BusState,set<Bus*>>& fleetState, double time); //returns true if an unassigned rebalancing trip has been generated and added to unmatchedTripsRebalancing_ and false otherwise

	void setTripGenerationStrategy(int type);
	void setEmptyVehicleStrategy(int type);

	void reset(int generation_strategy_type, int empty_vehicle_strategy_type);
	void addServiceRoute(Busline* line);
	
	void cancelUnmatchedTrip(Bustrip* trip); //destroy and remove trip from set of unmatchedTrips_
	void cancelRebalancingTrip(Bustrip* trip); //destroy and remove rebalancing trip from set of unmatchedTripsRebalancing_

private:
	set<Bustrip*> unmatchedTrips_; //set of planned passenger carrying trips to be performed that have not been matched to vehicles yet
	set<Bustrip*> unmatchedTripsRebalancing_; //set of planned empty-vehicle rebalancing trips to be performed that have not been matched to vehicles yet
	vector<Busline*> serviceRoutes_; //lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes do not need to know about this?)

	TripGenerationStrategy* generationStrategy_; //strategy for passenger carrying trips
	TripGenerationStrategy* emptyVehicleStrategy_; //strategy for supply re-balancing trips
};

/*Responsible for assigning trips to transit vehicles and adding these to matchedTripSet as well as the trip's associated Busline*/
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

/*In charge of dispatching transit vehicle - trip pairs*/
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
	Eventlist* eventlist_; //currently the dispatching strategy books a bus line event (vehicle - trip dispatching event)

	DispatchingStrategy* dispatchingStrategy_; //strategy that is used to determine the start time (for dispatch) of flexible transit vehicle trips
};

/*Groups together processes that control or modify transit Vehicles and provides an interface to Passenger*/
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
		int ev_strategy = 0,
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
	void updateFleetState(Bus* bus, BusState oldstate, BusState newstate, double time);
	
	void requestTrip(double time); //delegates to BustripGenerator to generate a trip depending on whatever strategy it currently uses and the state of the RequestHandler and add this to its list of trips
	void matchVehiclesToTrips(double time);
	void dispatchMatchedTrips(double time);

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_;	//id of control center
	const int tg_strategy_;	//initial trip generation strategy
	const int ev_strategy_; //initial empty vehicle strategy
	const int tvm_strategy_; //initial trip - vehicle matching strategy
	const int vd_strategy_; //initial vehicle dispatching strategy
	
	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //passengers currently connected to Controlcenter 
	map<int, Bus*> connectedVeh_; //transit vehicles currently connected to Controlcenter
	map<BusState, set<Bus*>> fleetState_; //among transit vehicles connected to Controlcenter keeps track of which are in each possible bus vehicle (i.e. transit vehicle) state 

	RequestHandler rh_;
	BustripGenerator tg_;
	BustripVehicleMatcher tvm_;
	VehicleDispatcher vd_;

	set<Bus*> initialVehicles_; //vehicles assigned to this control center on input (should be preserved between resets)
	vector<pair<Bus*, Bustrip*>> completedVehicleTrips_; //used for bookkeeping dynamically generated buses and bustrips (similar to busvehicles and bustrips in network) for writing output and deleting between resets
};
#endif
