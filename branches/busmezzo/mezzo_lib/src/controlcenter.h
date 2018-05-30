#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

/**************************

General Notes:

**************************/

/*! ControlCenter groups together objects that control or modify transit objects
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

//includes for bookkeeping maps in controlcenter (may remove in the future)
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
	Request(int pid, int oid, int did, int l, double t) : pass_id(pid), ostop_id(oid), dstop_id(did), load(l), time(t)
	{
		qRegisterMetaType<Request>(); //register Request as a metatype for QT signal arguments
	}

	bool operator == (const Request& rhs) const
	{
		return (pass_id == rhs.pass_id && ostop_id == rhs.ostop_id && dstop_id == rhs.dstop_id && load == rhs.load && time == rhs.time);
	}

	bool operator < (const Request& rhs) const // default less-than comparison of Requests in the order of smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id
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
};
Q_DECLARE_METATYPE(Request);


/*Responsible for adding Requests to requestSet as well as sorting and distributing the requestSet*/
class RequestHandler
{
public:
	RequestHandler();
	~RequestHandler();

	friend class BustripGenerator; //BustripGenerator recieves access to the requestSet as input to trip generation decisions

	void reset();

	bool addRequest(const Request req); //adds request vehRequest to the requestSet
	void removeRequest(const int pass_id); //removes requests with pass_id from the requestSet if it exists

private:
	set<Request> requestSet_; //set of recieved requests sorted by time
};



/*Responsible for generating planned trips without vehicles and adding these to a set of planned trips*/
class Bustrip;
class Busline;
class ITripGenerationStrategy;
class BustripGenerator
{
public:
	enum generationStrategyType { Null = 0, Naive }; //ids of trip generation strategies known to BustripGenerator

	explicit BustripGenerator(ITripGenerationStrategy* generationStrategy = nullptr);
	~BustripGenerator();

	friend class BustripVehicleMatcher; //give matcher class access to plannedTrips_. May remove trip from this set without destroying it if it has been matched

	bool requestTrip(const RequestHandler& rh, double time); //returns true if an unassigned trip has been generated and added to plannedTrips_ and false otherwise
	void setTripGenerationStrategy(int type);

	void reset(int generation_strategy_type);
	void addServiceRoute(Busline* line);
	
	void cancelPlannedTrip(Bustrip* trip); //destroy and remove trip from set of plannedTrips_
	

private:
	set<Bustrip*> plannedTrips_; //set of trips to be performed that have not been matched to vehicles yet
	vector<Busline*> serviceRoutes_; //lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes do not need to know about this?)

	ITripGenerationStrategy* generationStrategy_;
};

/*Algorithms for generating a Bustrip (unassigned to a vehicle)*/
class Busstop;
typedef pair<Busstop*, double> Visit_stop; 
class ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const = 0; //returns true if a trip was generated and added to tripSet container according to some strategy and false otherwise

protected:
	//supporting methods for generating trips with whatever ITripGenerationStrategy
	vector<Busline*> get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const; //returns buslines among lines given as input that run from a given originstop to a given destination stop (Note: assumes lines are uni-directional and that busline stops are ordered, which they currently are in BusMezzo)
	vector<Visit_stop*> create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const; //creates a vector of scheduled visits to stops starting from the dispatch time (given by simulation time)
	Bustrip* create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& schedule) const; //creates a Bustrip for a given line with a desired start time and a scheduled arrival to stops along this line (subject to the availablility of a vehicle to serve this trip)
};
/*Null strategy that always returns false*/
class NullTripGeneration : public ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const { return false; }
};
/*Matches trip according to oldest unassigned request & first line found to serve this request*/
class NaiveTripGeneration : public ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const double time, set<Bustrip*>& tripSet) const;
};



/*Responsible for assigning trips to transit vehicles and adding these to matchedTripSet as well as the trip's associated Busline*/
class Bus;
class IMatchingStrategy; //e.g. hungarian, k-opt, insertion
class BustripVehicleMatcher
{
public:
	enum matchingStrategyType { Null = 0, Naive };
	explicit BustripVehicleMatcher(IMatchingStrategy* matchingStrategy = nullptr);
	~BustripVehicleMatcher();

	void reset(int matching_strategy_type);

	//find_candidate_vehicles
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //add vehicle vector of vehicles assigned to serve the given line
	void setMatchingStrategy(int type);

	bool matchVehiclesToTrips(BustripGenerator& tg, double time);

private:
	set<Bustrip*> matchedTrips_; //set of trips that have been matched with a transit vehicle
	map<int, vector<Bus*>> candidateVehicles_per_SRoute_; //maps lineIDs among service routes for this control center to vector of candidate transit vehicles

	IMatchingStrategy* matchingStrategy_;
};

/*Algorithms for assigning a transit vehicle to a Bustrip*/
class IMatchingStrategy
{
public:
	virtual bool find_tripvehicle_match(
		set<Bustrip*>&					plannedTrips, 
		map<int, vector<Bus*>>&			candidateVehicles_per_SRoute, 
		const double					time, 
		set<Bustrip*>&					matchedTrips
	) = 0; //returns true if a trip from plannedTrips has been matched with a vehicle from candidateVehicles_per_SRoute and added to matchedTrips. The trip is in this case also removed from plannedTrips

protected:
	void assign_idlevehicle_to_trip(Bus* idletransitveh, Bustrip* trip, double starttime); //performs all operations (similar to Network::read_busvehicle) required in assigning a trip to an idle vehicle (TODO: assigning driving vehicles)

};
/*Null matching strategy that always returns false*/
class NullMatching : public IMatchingStrategy
{
public:
	virtual bool find_tripvehicle_match(set<Bustrip*>& plannedTrips, map<int, vector<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips) { return false; }
};

class NaiveMatching : public IMatchingStrategy
{
public:
	virtual bool find_tripvehicle_match(set<Bustrip*>& plannedTrips, map<int, vector<Bus*>>& veh_per_sroute, const double time, set<Bustrip*>& matchedTrips);
};

/*In charge of dispatching transit vehicle - trip pairs*/
class VehicleDispatcher
{
public:
	VehicleDispatcher();
	~VehicleDispatcher();

	//call Bustrip::activate somehow or Busline::execute
	//void reset()

private:

};

/*Groups togethers processes that control or modify transit Vehicles and provides an interface to Passenger*/
class Passenger;
enum class BusState;
class ControlCenter : public QObject
{
	Q_OBJECT

public:
	explicit ControlCenter(
		int id = 0,
		int tg_strategy = 0,
		int tvm_strategy = 0,
		Eventlist* eventlist = nullptr, 
		QObject* parent = nullptr
	);
	~ControlCenter();

	void reset();

private:
	void connectInternal(); //connects internal signal slots (e.g. requestAccepted with on_requestAccepted) that should always be connected for each controlcenter and between resets

public:
	//methods for connecting passengers, vehicles and lines
	void connectPassenger(Passenger* pass); //connects Passenger signals to recieveRequest slot
	void disconnectPassenger(Passenger* pass); //disconnects Passenger signals to recieveRequest slot
	
	void connectVehicle(Bus* transitveh); //connects Vehicle to CC
	void disconnectVehicle(Bus* transitveh); //disconnect Vehicle from CC

	void addServiceRoute(Busline* line); //add line to BustripGenerator map of possible lines to create trips for
	void addVehicleToServiceRoute(int line_id, Bus* transitveh); //add bus to vector of candidate vehicles that may be assigned trips for this line

signals:
	void requestAccepted(double time);
	void requestRejected(double time);

	void fleetStateChanged(double time);

	void tripGenerated(double time);

private slots:
	
	//request related
	void recieveRequest(Request req, double time); //time refers to when the request was recieved, may or may not be the same as the request time
	void removeRequest(int pass_id); //remove request with pass_id from requestSet in RequestHandler

	void on_requestAccepted(double time);
	void on_requestRejected(double time);

	void on_tripGenerated(double time);

	//fleet related
	void updateFleetState(int bus_id, BusState newstate, double time);
	void requestTrip(double time); //delegates to BustripGenerator to generate a trip depending on whatever strategy it currently uses and the state of the RequestHandler and add this to its list of trips

	void matchVehiclesToTrips(double time);

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_;	//id of control center
	const int tg_strategy_;	//initial trip generation strategy
	const int tvm_strategy_; //initial trip - vehicle matching strategy
	const Eventlist* eventlist_; //pointer to the eventlist to be passed to BustripGenerator or possibly FleetScheduler
	
	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //passengers currently connected to ControlCenter 
	map<int, Bus*> connectedVeh_; //transit vehicles currently connected to ControlCenter
	//map<BusState, vector<Bus*>> fleetState; //among transit vehicles connected to ControlCenter keeps track of which are in each possible bus vehicle (i.e. transit vehicle) state 
	
	RequestHandler rh_;
	BustripGenerator tg_;
	BustripVehicleMatcher tvm_;
	VehicleDispatcher vd_;
};
#endif