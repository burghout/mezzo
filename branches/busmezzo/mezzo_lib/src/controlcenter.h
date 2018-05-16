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



/*Responsible generating trips without vehicles and adding these to TripSet as well as trips for corresponding Busline*/
class Bustrip;
class Busline;
class ITripGenerationStrategy;
class BustripGenerator
{
public:
	enum tgStrategyType { Null = 0, Naive }; //ids of trip generation strategies known to BustripGenerator
	BustripGenerator(ITripGenerationStrategy* generationStrategy = nullptr);
	~BustripGenerator();

	bool generateTrip(const RequestHandler& rh, double time);
	void setTripGenerationStrategy(int type);

	void reset(int tg_strategy_type);
	void addCandidateline(Busline* line);
	// 1. add trip to busline, now busline::execute will activate busline for its first Bustrip::activate call. if several trips are generated in a chain

private:
	set<Bustrip*> tripSet_;
	vector<Busline*> candidateLines_; //lines (i.e. routes and stops to visit along the route) that this BustripGenerator can create trips for (TODO: do other process classes do not need to know about this?)

	ITripGenerationStrategy* generationStrategy_;
};



/*Algorithms for making Bustrip generation decisions*/
class ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidatelines, const double time) const = 0; //returns true if a trip should be generated according to some strategy and false otherwise

protected:
	//supporting methods for generating trips with whatever ITripGenerationStrategy
	vector<Busline*> get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const; //returns buslines among lines given as input that run from a given originstop to a given destination stop (Note: assumes lines are uni-directional and that busline stops are ordered, which they currently are in BusMezzo)
};
/*Null strategy that always returns false*/
class NullTripGeneration : public ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidatelines, const double time) const { return false; }
};
/*Matches trip according to oldest unassigned request & first line found to serve this request*/
class NaiveTripGeneration : public ITripGenerationStrategy
{
public:
	virtual bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidatelines, const double time) const;
};

/*Responsible for assigning trips in TripQueue to transit Vehicles and adding these to matchedTripQueue*/
class IMatchingStrategy;
class BustripVehicleMatcher
{
public:
	explicit BustripVehicleMatcher(IMatchingStrategy* matchingStrategy = nullptr);
	~BustripVehicleMatcher();

	//find_candidate_vehicles

	void match_trip()
	{
		//matchingStrategy_->find_tripvehicle_match(TripQueue)
	}

	void setMatchingStrategy(IMatchingStrategy* matchingStrategy) 
	{
		if (!matchingStrategy)
		{
			DEBUG_MSG_V("setMatchingStrategy failed!");
			abort();
		}

		DEBUG_MSG("Changing matching strategy");
		matchingStrategy_ = matchingStrategy;
	}

private:
	vector<double> matchedTripQueue_;
	IMatchingStrategy* matchingStrategy_;
};

class IMatchingStrategy
{
public:
	virtual void find_tripvehicle_match() = 0;
    //static IMatchingStrategy* make_strategy

protected:
	//Reference to the TripQueue
	//Reference to Fleet attached to CC
	//Forecaster class?
};

class NaiveMatching : public IMatchingStrategy
{
public:
	virtual void find_tripvehicle_match() 
	{
		DEBUG_MSG("Matching Naively!");
	}
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
class Bus;
enum class BusState;
class ControlCenter : public QObject
{
	Q_OBJECT

public:
	explicit ControlCenter(
		int id = 0, 
		int tg_strategy = 0, 
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

	void addCandidateLine(Busline* line); //add line to BustripGenerator map of possible lines to create trips for

signals:
	void requestAccepted(double time);
	void requestRejected(double time);

	void fleetStateChanged(double time);

private slots:
	
	//request related
	void recieveRequest(Request req, double time); //time refers to when the request was recieved, may or may not be the same as the request time
	void removeRequest(int pass_id); //remove request with pass_id from requestSet in RequestHandler

	void on_requestAccepted(double time);
	void on_requestRejected(double time);

	//fleet related
	void updateFleetState(int bus_id, BusState newstate, double time);

	void generateTrip(double time); //delegates to BustripGenerator to generate a trip depending on whatever strategy it currently uses and the state of the RequestHandler

private:
	//OBS! remember to add all mutable members to reset method, including reset functions of process classes
	const int id_;	//id of control center
	const int tg_strategy_;	//initial trip generation strategy
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