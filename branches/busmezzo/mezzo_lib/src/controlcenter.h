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

#include <vector>
#include <map>
#include <qobject.h>
#include "passenger.h"

/*structure that corresponds to a request from a passenger for a vehicle to travel between an origin stop and a destination stop*/
struct Request
{
	int pass_id;		//id of passenger that created request
	int ostop_id;		//id of origin stop
	int dstop_id;		//id of destination stop
	int load;			//number of passengers in request
	double time;		//time request was generated

	Request(int pid, int oid, int did, int l, double t) : pass_id(pid), ostop_id(oid), dstop_id(did), load(l), time(t)
	{
		qRegisterMetaType<Request>(); //register Request as a metatype for QT signal arguments
	}

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
Q_DECLARE_METATYPE(Request);

/*less-than comparison of Requests in the order of smallest time, smallest load, smallest origin stop id, smallest destination stop id and finally smallest passenger id*/
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

/*Responsible for adding Requests to requestSet as well as sorting and distributing the requestSet*/
class RequestHandler
{
public:
	RequestHandler();
	~RequestHandler();

	void reset();

	bool addRequest(Request vehRequest);

private:
	vector<Request> requestSet;
};


/*Responsible generating trips and adding these to TripSet*/
class TripGenerator
{
public:
	TripGenerator();
	~TripGenerator();

	void reset();

private:
	vector<double> tripSet;
};

/*Responsible for assigning trips in TripQueue to transit Vehicles and adding these to matchedTripQueue*/
class IMatchingStrategy;
class TripMatcher
{
public:
	explicit TripMatcher(IMatchingStrategy* matchingstrategy = nullptr);
	~TripMatcher();

	void match_trip()
	{
		//matchingStrategy_->find_tripvehicle_match(TripQueue)
	}

	void setMatchingStrategy(IMatchingStrategy* matchingstrategy) 
	{
		if (!matchingstrategy)
		{
			DEBUG_MSG_V("setMatchingStrategy failed!");
			abort();
		}

		DEBUG_MSG("Changing matching strategy");
		matchingStrategy_ = matchingstrategy;
	}

private:
	vector<double> matchedTripQueue;
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
};

class NaiveMatching : public IMatchingStrategy
{
public:
	virtual void find_tripvehicle_match() 
	{
		DEBUG_MSG("Matching Naively!");
	}
};


/*Groups togethers processes that control or modify transit Vehicles and provides an interface to Passenger*/
class Passenger;
class ControlCenter : public QObject
{
	Q_OBJECT

public:
	explicit ControlCenter(int id = 0, QObject* parent = nullptr);
	~ControlCenter();

	void reset();

	void connectPassenger(Passenger* pass); //connects Passenger signals to RequestHandler slot
	void disconnectPassenger(Passenger* pass); //disconnects Passenger signals to RequestHandler slot
	//void connectVehicle(); //connects Vehicle to CC

signals:
	void requestAccepted();
	void requestRejected();

private slots:
	void recieveRequest(Request req);

	void on_requestAccepted();
	void on_requestRejected();

private:
	//OBS! remember to add all mutable members to reset method
	const int id_;
	
	//maps for bookkeeping connected passengers and vehicles
	map<int, Passenger*> connectedPass_; //passengers currently connected to ControlCenter 
	map<int, Vehicle*> connectedVeh_; //vehicles currently connected to ControlCenter
	RequestHandler rh_;
	TripGenerator tg_;
	TripMatcher tm_;
};
#endif