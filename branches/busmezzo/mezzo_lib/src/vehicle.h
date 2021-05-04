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

class Controlcenter;

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
	/** @ingroup DRT
		Messy bunch of methods used to keep track of cycles (repeating links) in routes
		@{
	*/
	void advance_curr_link_route_idx() { ++curr_link_route_idx; }
	void set_curr_link_route_idx(size_t link_idx) { curr_link_route_idx = link_idx; }
	size_t get_curr_link_route_idx() const { return curr_link_route_idx; }
	/**@}*/
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
    int id = -1;
    Route* route = nullptr;
    ODpair* odpair = nullptr;
    double start_time = 0.0;
    int type = -1;
    double length = 0;
    double entry_time = 0.0;
    double exit_time = 0.0;
    double arrival_time = 0.0;
    Link* curr_link = nullptr;
	/** @ingroup DRT
	@{
	*/
    size_t curr_link_route_idx = 0; //!< index position of curr_link on route->links, initialized to 0
	/**@}*/
    bool entered = false;
    int switched = 0;
    int meters = 0;
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
	int type_id = -1;	// bus type id
	string bus_type_name;
	double length = 0.0;
	int number_seats = 0;
	int capacity = 0;
	Dwell_time_function* dwell_time_function = nullptr;
};

/** @ingroup DRT
    @brief Used by Controlcenter to keep track of fleet state of connected transit vehicles
*/
enum class BusState
{ 
	Null = 0, //!< vehicles should be null before they are available, and null when they finish a trip and are copied (e.g. when they are driving on a dummy-link)
	IdleEmpty, //!< 'idle' in this context refers to the vehicle being on a trip but not driving (e.g. boarding/alighting/dwelling)
	IdlePartiallyFull, 
	IdleFull, 
	DrivingEmpty, //!< 'driving' here refers to when the vehicle is moving between two stops
	DrivingPartiallyFull, 
	DrivingFull,
	//Loading/Unloading
	//Refeuling
	OnCall //!< 'oncall' refers to if the vehicle is not currently assigned any trip and is standing still
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
        Controlcenter* CC = nullptr,
		QObject* parent = nullptr
	);
	
	Bus(
		int bv_id_, 
		Bustype* bty,
		bool flex_vehicle = false,
        Controlcenter* CC = nullptr,
		QObject* parent = nullptr
	);

	virtual ~Bus(); //!< destructor
	void reset ();
// GETS and SETS
	int get_bus_id () const {return bus_id;}
	void set_bus_id (int bus_id_) {bus_id = bus_id_;}
	int get_occupancy() {return occupancy;}
	void set_occupancy(int occup);
	int get_number_seats () {return number_seats;}
	int get_capacity () {return capacity;}
	bool get_on_trip () const {return on_trip;}
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

/** @ingroup DRT
    @{
*/
//Control Center
	BusState get_state() const { return state_; }
	BusState calc_state(const bool assigned_to_trip, const bool bus_exiting_stop, const int occupancy) const; //!< returns the BusState of bus depending whether a bus is assigned to a trip, has just entered or exited a stop, and the occupancy of the bus. Other states for when a bus is not on a trip (e.g. onCall are set elsewhere)
	double get_total_time_in_state(BusState state) const;
	double get_total_time_empty();
	double get_total_time_occupied();
	double get_total_time_driving();
	double get_total_time_idle();
	double get_total_time_oncall();
	void update_meters_traveled(int meters_, bool is_empty);
	double get_total_vkt() const;
	double get_total_empty_vkt() const;
	double get_total_occupied_vkt() const;
	void set_state(const BusState newstate, const double time); //!< sets current state_ to newstate and emits stateChanged if newstate differs from current state_
	void print_state(); //!< prints current BusState for debugging purposes

	bool is_idle() const;	//!< returns true if bus is idle/waiting at a stop
	bool is_driving() const; //!< returns true if bus is driving between stops
	bool is_oncall() const; //!< returns true if bus is unassigned to any trip and is available for assignment
	bool is_empty() const; //!< returns true if bus occupancy is currently 0 and false otherwise
	bool is_null() const;

	Busstop* get_last_stop_visited() const { return last_stop_visited_; }
	void set_last_stop_visited(Busstop* last_stop_visited) { last_stop_visited_ = last_stop_visited; } 
	Busstop* get_next_stop() const; //!< returns the next stop of the current trip for this vehicle. If no trip is assigned to this vehicle then returns nullptr
	Bustrip* get_next_trip(); //!< returns the next trip this bus is scheduled to visit according to it's driving_roster. Returns nullptr if there is no next trip (curr_trip)

	Bus* progressFlexVehicle(Bus* oldbus, double time); //!< clones the oldbus into newbus, sets oldbus to null, disconnects it, removes it from service routes. Newbus inherits state from oldbus (DrivingEmpty), and is connected to cc, with same service routes as oldbus. Returns the new bus
	void assignBusToTrip(Bus* bus, Bustrip* trip); //!< performs operations necessary to connect a bus with no trip with an 'unassigned' bustrip (for example the next trip of the driving_roster)

    double get_init_time() const { return init_time; }
    void set_init_time(double time) { init_time = time; }
	void set_flex_vehicle(bool flex_vehicle) { flex_vehicle_ = flex_vehicle; }
	bool is_flex_vehicle() const { return flex_vehicle_; }
	void add_sroute_id(int sroute_id) { sroute_ids_.insert(sroute_id); } 
	void remove_sroute_id(int sroute_id) { if (sroute_ids_.count(sroute_id) != 0) { sroute_ids_.erase(sroute_id); } }
    void set_control_center(Controlcenter* CC) { CC_ = CC; }
	Controlcenter* get_CC() { return CC_; }

signals:
	void stateChanged(Bus* bus, BusState oldstate, BusState newstate, double time); //!< signal informing a change in this vehicle's BusState
/**@}*/

protected:
	int	bus_id=-1;
	Bustype* bus_type = nullptr;
	Random* random = nullptr;
	int number_seats = 0; // Two added variables for LOS satistics and for dwell time calculations
	int capacity = 0; // In the future will be determined according to the bus type
	int occupancy = 0;
	Bustrip* curr_trip = nullptr;

	bool on_trip = false; // is true when bus is on a trip and false when waiting for the next trip
	list <Busvehicle_location> output_vehicle; //!< list of output data for buses visiting stops
	
/** @ingroup DRT
    @{
*/
	Busstop* last_stop_visited_ = nullptr; //!< the last busstop (if no stop has been visited then initialized to nullptr) that this transit vehicle has entered (or exited)
	BusState state_ = BusState::Null; //!< current BusState of the transit vehicle
	bool flex_vehicle_ = false; //!< true if vehicle can be assigned trips dynamically, false otherwise
	set<int> sroute_ids_; //!< ids of service routes (buslines) that this bus can be assigned dynamically generated trips for @todo remove, vehicle now knows of its CC if flex vehicle anyways
    Controlcenter* CC_ = nullptr; //!< control center that this vehicle is currently connected to. nullptr if not connected to any control center

	// output attributes
	map<BusState, double> total_time_spent_in_state; //!< maps each busstate to the total time spent in this busstate by this vehicle for each simulation replication. Currently used for output
	map<BusState, double> time_state_last_entered; //!< maps each busstate (if entered previously) to the latest time this vehicle entered this state
	int total_meters_traveled = 0; //!< cumulative meters traveled (excluding dummy links)
	int total_empty_meters_traveled = 0; //!< cumulative meters traveled without any passengers onboard
	int total_occupied_meters_traveled = 0; //!< cumulative meters traveled with at least one passenger onboard

	double init_time = 0.0; //!< time the vehicle is generated and made available, i.e. the time in which this vehicle was initially set to 'on-call' if a flex vehicle, set to zero by default if vehicle is not flex when read on input
/**@}*/
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
	
	int line_id = 0;
	int trip_id = 0;
    int stop_id = 0;
	int vehicle_id = 0;
	int link_id = 0;
	bool entering_stop = false;
	double time = 0.0;
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

