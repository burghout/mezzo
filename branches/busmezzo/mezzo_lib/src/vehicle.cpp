#include "vehicle.h"
#include "MMath.h"

Vehicle::Vehicle()
{
 id=-1;
 type=-1;
 route=NULL;
 odpair=NULL;
 start_time=0.0;
 length=0;
 exit_time=0.0;
 entered=false;
 switched=0;
 meters=0;
}

Vehicle::~Vehicle()
{}

Vehicle::Vehicle(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_): id(id_), route(route_), odpair(odpair_), start_time(time_) , type(type_), length(length_), exit_time(0.0)
{
	entered=false;	
	switched=0;
	meters=0;
}

void Vehicle::init (int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_)
{
 id=id_;
 type=type_;
 route=route_;
 odpair=odpair_;
 start_time=time_;
 length=length_;
 exit_time=0.0;
 entered=false;
 switched=0;
 meters=0;
}

void Vehicle::set_curr_link(Link* curr_link_)
{
	curr_link=curr_link_;
}

Link* Vehicle::get_curr_link()
{
	return curr_link;
}

Link* Vehicle::nextlink()
{
	if (entered)
		return route->nextlink(curr_link);
	else
		return route->firstlink();
}

const odval Vehicle::get_odids ()
{return odpair->odids();}

int Vehicle::get_oid()
{



	odval oid=odpair->odids() ;
	return oid.first;	
}

int Vehicle::get_did()
{
	odval did=odpair->odids() ;
	return did.second;	
}
 	

void Vehicle::report(double time)
{
  arrival_time=time;
  list <double> collector;
  collector.push_back(id);
  collector.push_back(start_time);
  collector.push_back(arrival_time);
  collector.push_back(arrival_time - start_time);
  collector.push_back(meters);
  collector.push_back(route->get_id());
  collector.push_back(switched);
  odpair->report(collector);
}

 // VehicleRecycler procedures

VehicleRecycler::	~VehicleRecycler()
{
 	/*
	for (list <Vehicle*>::iterator iter=recycled.begin();iter!=recycled.end();)
	{			
		delete (*iter); // calls automatically destructor
		iter=recycled.erase(iter);	
	}
	for (list <Bus*>::iterator iter1=recycled_busses.begin();iter1!=recycled_busses.end();)
	{			
		delete (*iter1); // calls automatically destructor
		iter1=recycled_busses.erase(iter1);	
	}
	*/

}

Bus::Bus(QObject* parent) : QObject(parent), Vehicle()
{
	occupancy = 0;
	on_trip = false;
	number_seats = 45;
	capacity = 70;
	type = 4;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	state_ = Null;
}
Bus::Bus(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_, QObject* parent) : QObject(parent),
	Vehicle(id_, type_, length_, route_, odpair_, time_)
{
	occupancy = 0;
	on_trip = false;
	number_seats = 50;
	capacity = 70;
	type = 4;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	state_ = Null;
};
Bus::Bus(int bv_id_, Bustype* bty, QObject* parent) : QObject(parent)
{
	bus_id = bv_id_;
	type = 4;
	occupancy = 0;
	on_trip = false;
	length = bty->get_length();
	number_seats = bty->get_number_seats();
	capacity = bty->get_capacity();
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	state_ = Null;
};

// ***** Special Bus Functions *****
void Bus::reset ()
{
	occupancy = 0;
	on_trip = true;
	type = 4;
	output_vehicle.clear();

	//ControlCenter
	disconnect(this, 0, 0, 0); //disconnect all signal slots (will reconnect to control center in Network::init)
	state_ = Null;
}

Bus::~Bus()
{}

Busvehicle_location::~Busvehicle_location()
{}

void Bus::set_bustype_attributes (Bustype* bty) 
// change the fields that are determined by the bustype
{
	type = 4;
	length = bty->get_length();
	number_seats = bty->get_number_seats();
	capacity = bty->get_capacity();
	bus_type = bty;
}

void Bus::advance_curr_trip (double time, Eventlist* eventlist) // progresses trip-pointer 
{
	vector <Start_trip*>::iterator trip1, next_trip; // find the pointer to the current and next trip
	for (vector <Start_trip*>::iterator trip = curr_trip->driving_roster.begin(); trip < curr_trip->driving_roster.end(); trip++)
	{
		if ((*trip)->first == curr_trip)
		{
			trip1 = trip;
			break;
		}
	}
	next_trip = trip1+1;
	on_trip = false;  // the bus is avaliable for its next trip
	if (next_trip != curr_trip->driving_roster.end()) // there are more trips for this bus
	{
		if ((*next_trip)->first->get_starttime() <= time) // if the bus is already late for the next trip
		{
			Busline* line = (*next_trip)->first->get_line();
			// then the trip is activated
			(*next_trip)->first->activate(time, line->get_busroute(), line->get_odpair(), eventlist);
		}
		// if the bus is early for the next trip, then it will be activated at the scheduled time from Busline
	}
}

void Bus::record_busvehicle_location (Bustrip* trip, Busstop* stop, double time)
{
	output_vehicle.push_back(Busvehicle_location(trip->get_line()->get_id(), trip->get_id() , stop->get_id(), bus_id , stop->get_link_id() , 1, time)); 
	output_vehicle.push_back(Busvehicle_location(trip->get_line()->get_id(), trip->get_id() , stop->get_id(), bus_id , stop->get_link_id() , 0, stop->get_exit_time())); 
}

void Bus::write_output(ostream & out)
{
	for (list <Busvehicle_location>::iterator iter = output_vehicle.begin(); iter!=output_vehicle.end();iter++)
	{
		iter->write(out);
	}
}

BusState Bus::calc_state(const bool bus_exiting_stop, const int occupancy) const
{
	assert(occupancy <= capacity && occupancy >= 0); //occupancy should be a valid load for this vehicle

	if (!bus_exiting_stop) //if the vehicle is idle/waiting at a stop
	{
		if (occupancy == 0) //bus is empty
			return IdleEmpty;
		if (occupancy > 0 && occupancy < capacity) //bus is partially full
			return IdlePartiallyFull;
		if (occupancy == capacity) //bus is full
			return IdleFull;
	}
	else //if the vehicle has begun to drive
	{
		if (occupancy == 0) 
			return DrivingEmpty;
		if (occupancy > 0 && occupancy < capacity)
			return DrivingPartiallyFull;
		if (occupancy == capacity) 
			return DrivingFull;
	}

	DEBUG_MSG_V("Null BusState returned by Bus::calc_state");
	return Null;
}

void Bus::set_state(const BusState newstate)
{
	if (state_ != newstate)
	{
		state_ = newstate;
		print_state();
		emit stateChanged(bus_id, state_);
	}
}

void Bus::print_state()
{
	cout << "Bus " << bus_id << " is ";
	switch (state_)
	{
	case IdleEmpty:
		cout << "IdleEmpty";
		break;
	case IdlePartiallyFull:
		cout << "IdlePartiallyFull";
		break;
	case IdleFull:
		cout << "IdleFull";
		break;
	case DrivingEmpty:
		cout << "DrivingEmpty";
		break;
	case DrivingPartiallyFull:
		cout << "DrivingPartiallyFull";
		break;
	case DrivingFull:
		cout << "DrivingFull";
		break;
	case Null:
		cout << "Null";
		break;
	default:
		DEBUG_MSG_V("Something went very wrong");
		abort();
	}
	cout << endl;
}

// ***** Bus-types functions *****
Bustype::Bustype ()
{
}

Bustype::Bustype (int type_id_, string bus_type_name_, double length_, int number_seats_, int capacity_, Dwell_time_function* dwell_time_function_):
	type_id(type_id_), bus_type_name(bus_type_name_), length(length_), number_seats(number_seats_), capacity(capacity_), dwell_time_function(dwell_time_function_)
{

}
Bustype::~Bustype ()
{
}

