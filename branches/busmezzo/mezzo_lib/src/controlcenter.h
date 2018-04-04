#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

/**************************

General Notes:

**************************/

/*! ControlCenter groups together objects that control or modify transit objects
- RequestHandler
- TripGenerator
- TripMatcher
- FleetScheduler

Registers (unregisters) service vehicles
Offers interface to connected vehicles as well as connected passengers

*/

//#include "busline.h"
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

using namespace std;

//struct RequestQueue
//struct TripQueue

//structure that corresponds to a request from a passenger for a vehicle to travel between an origin stop and a destination stop
struct Request
{
	int pass_id;		//id of passenger that created request
	int ostop_id;		//id of origin stop
	int dstop_id;		//id of destination stop
	int load;			//number of passengers in request
	double time;		//time request was generated

	Request(int pid, int oid, int did, int l, double t) : pass_id(pid), ostop_id(oid), dstop_id(did), load(l), time(t) {}

	bool operator == (const Request& rhs) const
	{
		return (pass_id == rhs.pass_id && ostop_id == rhs.ostop_id && dstop_id == rhs.dstop_id && load == rhs.load && time == rhs.time);
	}

	bool operator < (const Request& rhs) const  //default comparator sorts by order of attributes
	{
		if (pass_id != rhs.pass_id)
			return pass_id < rhs.pass_id;
		else if (ostop_id != rhs.ostop_id)
			return ostop_id < rhs.ostop_id;
		else if (dstop_id != rhs.dstop_id)
			return dstop_id < rhs.dstop_id;
		else if (load != rhs.load)
			return load < rhs.load;
		else
			return time < rhs.time;
	}
};

//compare in the order of smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id
struct compareRequestByTime
{
	inline bool operator() (const Request& req1, const Request& req2)
	{
		if (req1.time != req2.time)
			return req1.time < req2.time;
		else if (req1.load != req2.load)
			return req1.load < req2.load;
		else if (req1.ostop_id != req2.ostop_id)
			return req1.ostop_id < req2.ostop_id;
		else if (req1.dstop_id != req2.dstop_id)
			return req1.dstop_id < req2.dstop_id;
		else
			return req1.pass_id < req2.pass_id;
	}
};

/*Responsible for generating requests and adding these to RequestQueue*/
class RequestHandler
{
public:
	RequestHandler();
	~RequestHandler();

	void generate_request(const string passengerData, const double time);

private:
	vector<double> requestQueue;
};


/*Responsible generating trips and adding these to TripQueue*/
class TripGenerator
{
public:
	TripGenerator();
	~TripGenerator();

	void generate_trip(const string requestQueue, const  double time);

private:
	vector<double> tripQueue;
};

/*Responsible for assigning trips in TripQueue to transit Vehicles and adding these to matchedTripQueue*/
class IMatchingStrategy;

class TripMatcher
{
public:
	TripMatcher();
	~TripMatcher();

	void match_trip()
	{
		//matchingStrategy_->find_tripvehicle_match(TripQueue)
	}

	void setMatchingStrategy(IMatchingStrategy* ms) 
	{
		cout << "Changing matching strategy" << endl;
		matchingStrategy_ = ms;
	}

private:
	vector<double> matchedTripQueue;
	IMatchingStrategy* matchingStrategy_;
};

class IMatchingStrategy
{
public:
	virtual void find_tripvehicle_match() = 0;

protected:
	//Reference to the TripQueue
	//Reference to Fleet attached to CC
};

class NaiveMatching : public IMatchingStrategy
{
public:
	virtual void find_tripvehicle_match() 
	{
		cout << "Matching Naively!" << endl;
	}
};


/*Groups togethers processes that control or modify transit Vehicles and provides an interface to Passenger*/
class ControlCenter
{
public:
	ControlCenter();
	~ControlCenter();

	void generate_trip(const string requestQueue, const double time)
	{
		tg_.generate_trip(requestQueue, time);
	}
	void generate_request(const string passengerData, const double time)
	{
		rh_.generate_request(passengerData, time);
	}

	//process_request()

private:
	RequestHandler rh_;
	TripGenerator tg_;
	TripMatcher tm_;
};


#endif
