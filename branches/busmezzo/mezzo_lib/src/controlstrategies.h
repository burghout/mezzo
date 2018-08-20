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

/*Request structure that corresponds to a request from a passenger for a vehicle to travel between an origin stop and a destination stop*/
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

//Trip Generation Strategies
/*Algorithms for generating a Bustrip (unassigned to a vehicle)*/
class TripGenerationStrategy
{
public:
	virtual ~TripGenerationStrategy() {}
	virtual bool calc_trip_generation(
		const set<Request>&		requestSet,				//set of requests that motivate the potential generation of a trip
		const vector<Busline*>& candidateServiceRoutes, //service routes available to potentially serve the requests
		const map<BusState,set<Bus*>>& fleetState,		//state of all vehicles that are potentially available to serve the requests
		const double			time,					//time for which calc_trip_generation is called
		set<Bustrip*>&			tripSet					//set of trips that have been generated to serve requests that have not been assigned to a vehicle yet
	) const = 0; //returns true if a trip was generated and added to tripSet container according to some strategy and false otherwise

protected:
	//supporting methods for generating trips with whatever TripGenerationStrategy
	map<pair<int, int>, int> countRequestsPerOD(const set<Request>& requestSet) const; //counts the number of requests with a particular od
	bool line_exists_in_tripset(const set<Bustrip*>& tripSet, const Busline* line) const; //returns true if a trip exists in trip set for the given bus line
	vector<Busline*> get_lines_between_stops(const vector<Busline*>& lines, const int ostop_id, const int dstop_id) const; //returns buslines among lines given as input that run from a given origin stop to a given destination stop (Note: assumes lines are unidirectional and that bus line stops are ordered, which they currently are in BusMezzo)
	vector<Visit_stop*> create_schedule(double init_dispatch_time, const vector<pair<Busstop*, double>>& time_between_stops) const; //creates a vector of scheduled visits to stops starting from the dispatch time (given by simulation time)
	Bustrip* create_unassigned_trip(Busline* line, double desired_dispatch_time, const vector<Visit_stop*>& schedule) const; //creates a Bustrip for a given line with a desired start time and a scheduled arrival to stops along this line (subject to the availability of a vehicle to serve this trip)
};
/*Null strategy that always returns false*/
class NullTripGeneration : public TripGenerationStrategy
{
public:
	~NullTripGeneration() override {}
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& tripSet) const override;
};
/*Matches trip according to oldest unassigned request & first line found to serve this request*/
class NaiveTripGeneration : public TripGenerationStrategy
{
public:
	~NaiveTripGeneration() override {}
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& tripSet) const override;
};

/*Algorithms for generating empty vehicle trips*/
/*Reactive empty vehicle repositioning strategy that attempts to generate a trip going from a source of supply at an adjacent stop to a source of demand*/
class NearestLongestQueueEVTripGeneration : public TripGenerationStrategy
{
	~NearestLongestQueueEVTripGeneration() override {}
	bool calc_trip_generation(const set<Request>& requestSet, const vector<Busline*>& candidateServiceRoutes, const map<BusState, set<Bus*>>& fleetState, const double time, set<Bustrip*>& tripSet) const override;
};

//Matching Strategies
/*Algorithms for assigning a transit vehicle to a Bustrip*/
//e.g. Hungarian, k-opt, insertion
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

//Dispatching Strategy
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

#endif // ifndef CONTROLSTRATEGIES_H
