/*
	Mezzo Mesoscopic Traffic Simulation 
	Copyright (C) 2008  Wilco Burghout

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VEHICLE_HH
#define VEHICLE_HH
#include "route.h"
#include "od.h"
//#include "PVM.h"
#include "signature.h"
#include <list>
#include <set>
#include "vtypes.h"
#include "busline.h"
#include "Random.h"

#include <qobject.h>

class ODpair;
class Route;
class Link;
//class PVM;
class Bustype;
class Bustrip;
class Busstop;
class Busvehicle_location;
class Dwell_time_function;

class Vehicle
{
public:
	Vehicle();
	virtual ~Vehicle(); //!< destructor
	Vehicle(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_);
	void init(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_);

	double get_length() { return length; }
	void set_length(double length_) { length = length_; }
	double get_exit_time() { return exit_time; }
	double get_start_time() { return start_time; }
	const odval get_odids();
	void set_exit_time(double time) { exit_time = time; }
	void set_entry_time(double time) { entry_time = time; }
	void set_route(Route* route_) { route = route_; switched = 1; }
	void set_switched(int i) { switched = i; }
	double get_entry_time() { return entry_time; }
	void set_curr_link(Link* curr_link_);
	Link* get_curr_link();
	Route* get_route() { return route; }
	Link* nextlink();
	int get_id() { return id; }
	int get_type() { return type; }
	int get_oid();
	int get_did();
	void set_entered() { entered = true; }
	void add_meters(int meters_) { meters += meters_; }
	void set_meters(int meters_) { meters = meters_; }
	int get_meters() { return meters; }
	void report(double time);
  
protected:
	int id;
	Route* route;
	ODpair * odpair;
	double start_time;
	int type;
	double length;
 	double entry_time;
	double exit_time; 	
	double arrival_time;
	Link* curr_link;
	bool entered;
	int switched;
	int meters;
};
#ifdef _BUSES
class Bustrip;
typedef pair<Bustrip*,double> Start_trip;

class Bustype 
{
public:
	Bustype ();
	Bustype (int type_id_, string bus_type_name, double length_, int number_seats_, int capacity_, Dwell_time_function* dwell_time_function_);
	~Bustype ();
	double get_length () {return length;}
	int get_number_seats () {return number_seats;}
	int get_capacity () {return capacity;}
	Dwell_time_function* get_dt_function() {return dwell_time_function;}
	int get_id () {return type_id;}
protected:
	int type_id;	// bus type id
	string bus_type_name;
	double length;
	int number_seats;
	int capacity;
	Dwell_time_function* dwell_time_function;
};

enum class BusState //used by controlcenter to keep track of fleet state
{ 
	Null = 0, 
	IdleEmpty, 
	IdlePartiallyFull, 
	IdleFull, 
	DrivingEmpty, 
	DrivingPartiallyFull, 
	DrivingFull
	//Loading/Unloading
	//Refeuling
	//OnCall
}; 
class Bus : public QObject, public Vehicle
{
	Q_OBJECT

public:
	Bus(QObject* parent = nullptr);
	Bus(
		int id_,
		int type_,
		double length_,
		Route* route_,
		ODpair* odpair_,
		double time_,
		bool flex_vehicle = false,
		QObject* parent = nullptr
	);
	
	Bus(
		int bv_id_, 
		Bustype* bty,
		bool flex_vehicle = false,
		QObject* parent = nullptr
	);

	virtual ~Bus(); //!< destructor
	void reset ();
// GETS and SETS
	int get_bus_id () const {return bus_id;}
	void set_bus_id (int bus_id_) {bus_id = bus_id_;}
	int get_occupancy() {return occupancy;}
	void set_occupancy (const int occup) {occupancy=occup;}
	int get_number_seats () {return number_seats;}
	int get_capacity () {return capacity;}
	bool get_on_trip () {return on_trip;}
	void set_on_trip (bool on_trip_) {on_trip=on_trip_;}
	Bustype* get_bus_type () {return bus_type;}
	void set_curr_trip (Bustrip* curr_trip_) {curr_trip = curr_trip_;}
	Bustrip* get_curr_trip () {return curr_trip;}
	
// other functions:	
	void set_bustype_attributes (Bustype* bty); // change the fields that are determined by the bustype
	void advance_curr_trip (double time, Eventlist* eventlist); // progresses trip-pointer for statically scheduled vehicles and dynamically scheduled vehicles. If a dynamically scheduled vehicle has no trips remaining in its trip-chain then it is re-initialized as unassigned for the last stop it visited

// output-related functions
	void record_busvehicle_location (Bustrip* trip,  Busstop* stop, double time);
	void write_output(ostream & out);

//Control Center
	BusState get_state() const { return state_; }
	BusState calc_state(const bool bus_exiting_stop, const int occupancy) const; //returns the BusState of bus depending whether a bus has just entered or exited a stop, and the occupancy of the bus
	void set_state(const BusState newstate, const double time); //sets state_ to newstate and emits stateChanged
	void print_state(); //prints current BusState for debugging purposes (TODO: remove later)

	bool is_idle() const;	//returns true if bus is idle/waiting at a stop
	bool is_driving() const; //returns true if bus is driving between stops

	Busstop* get_last_stop_visited() const { return last_stop_visited_; }
	void set_last_stop_visited(Busstop* last_stop_visited) { last_stop_visited_ = last_stop_visited; } 

	void set_flex_vehicle(bool flex_vehicle) { flex_vehicle_ = flex_vehicle; }
	bool is_flex_vehicle() const { return flex_vehicle_; }
	void add_sroute_id(int sroute_id) { sroute_ids_.insert(sroute_id); }
	void remove_sroute_id(int sroute_id) { if (sroute_ids_.count(sroute_id) != 0) { sroute_ids_.erase(sroute_id); } }

signals:
	void stateChanged(int bus_id, BusState state, double time); // Signal informing a change of BusState

protected:
	int	bus_id;
	Bustype* bus_type;
	Random* random;
	int number_seats; // Two added variables for LOS satistics and for dwell time calculations
	int capacity; // In the future will be determined according to the bus type
	int occupancy;
	Bustrip* curr_trip;

	bool on_trip; // is true when bus is on a trip and false when waiting for the next trip
	list <Busvehicle_location> output_vehicle; //!< list of output data for buses visiting stops
	
	Busstop* last_stop_visited_; //The last busstop (if no stop has been visited then initialized to nullptr) that this transit vehicle has entered
	BusState state_; //state of the vehicle used for DRT service with controlcenter
	bool flex_vehicle_; //true if vehicle can be assigned trips dynamically, false otherwise
	set<int> sroute_ids_; //ids of service routes (buslines) that this bus can be assigned dynamically generated trips for
};

class Busvehicle_location // container object holding output data for stop visits
{
public:
	Busvehicle_location (
		int line_id_,
		int trip_id_,
		int stop_id_,
		int vehicle_id_,
		int link_id_,
		bool entering_stop_,
		double time_
		): line_id(line_id_),trip_id(trip_id_), stop_id(stop_id_), vehicle_id(vehicle_id_), link_id(link_id_), entering_stop(entering_stop_), time (time_){}
	
	virtual ~Busvehicle_location(); //!< destructor
	
	void write(ostream& out){ 
		out << line_id << '\t'
			<< trip_id << '\t'
			<< stop_id << '\t'
			<< vehicle_id << '\t'
			<< link_id << '\t'
			<< entering_stop << '\t'
			<< time << '\t'
			<< endl;
	}
	
	void reset(){
		line_id = 0;
		trip_id = 0;
		vehicle_id = 0;
		stop_id = 0;
		link_id = 0;
		entering_stop = 0;
		time = 0;
	}
	
	int line_id;
	int trip_id;
    int stop_id;
	int vehicle_id;
	int link_id;
	bool entering_stop;
	double time;
};

#endif// BUSES

class VehicleRecycler
{
 public:
 	~VehicleRecycler();
	Vehicle* newVehicle() {	 	if (recycled.empty())
     								return new Vehicle();
     							else
     							{
     								Vehicle* veh=recycled.front();
     								recycled.pop_front();
     								return veh;
     							}	
     						}
							
     void addVehicle(Vehicle* veh){recycled.push_back( veh);}
#ifdef _BUSES	 
	 Bus* newBus() {	 	if (recycled_busses.empty())
     								return new Bus();
     							else
     							{
     								Bus* bus=recycled_busses.front();
     								recycled_busses.pop_front();
     								return bus;
     							}	
     						}
							
     void addBus(Bus* bus){recycled_busses.push_back( bus);}
#endif
 private:
	list <Vehicle*> recycled;
#ifdef _BUSES
	list <Bus*> recycled_busses;
#endif
};

//static VehicleRecycler recycler;
extern VehicleRecycler recycler;

#endif

