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
*/

//#include "busline.h"
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

using namespace std;

//struct RequestQueue
//struct TripQueue


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

extern ControlCenter* CC;

#endif
