#ifndef _BUSLINE
#define _BUSLINE


#include "parameters.h" 
#include "route.h"
#include "Random.h"
#include <algorithm>
#include "vehicle.h"
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include "link.h"
#include "sdfunc.h"
#include "q.h"
#include "passenger.h"
#include "MMath.h" 
#include <cmath>
#include "od_stops.h"
#include <cstddef>
#include <iterator>
#include <list>

class Busroute;
class Busstop;
class Bustrip;
class ODpair;
class Bus;
class Bustype;
class Passenger;
class ODstops;
class Change_arrival_rate;
class Bustrip_assign;
class Dwell_time_function;
class Walking_time_dist;
class Controlcenter;

typedef pair<Bustrip*,double> Start_trip;
typedef vector <Passenger*> passengers;

class Output_Summary_Line // container object holding output data for busline
{
public:
	virtual ~Output_Summary_Line(); //!< destructor
	void write (ostream& out, int line_id) { 
		out << line_id << '\t'
			<< line_avg_headway << '\t'
			<< line_avg_DT << '\t'
			<< line_avg_abs_deviation << '\t'
			<< line_avg_waiting_per_stop << '\t'
			<< line_total_boarding << '\t'
			<< line_sd_headway << '\t'
			<< line_sd_DT << '\t'
			<< line_on_time << '\t'
			<< line_early << '\t'
			<< line_late << '\t'
			<< total_pass_riding_time << '\t'
			<< total_pass_dwell_time << '\t'
			<< total_pass_waiting_time << '\t'
			<< total_pass_holding_time << '\t' 
			<< control_objective_function << '\t' 
			<< total_travel_time_crowding << '\t' 
			<< endl; 
	}

	void reset () { 
		line_avg_headway = 0;
		line_avg_DT = 0;
		line_avg_abs_deviation = 0;
		line_avg_waiting_per_stop = 0;
		line_total_boarding = 0;
		line_sd_headway = 0;
		line_sd_DT = 0;
		line_on_time = 0;
		line_early = 0;
		line_late = 0;
		total_pass_riding_time = 0;
		total_pass_dwell_time = 0;
		total_pass_waiting_time = 0;
		total_pass_holding_time = 0;
		control_objective_function = 0;
		total_travel_time_crowding = 0;
	}

	// line summary measures
	double line_avg_headway = 0;
	double line_avg_DT = 0;
	double line_avg_abs_deviation = 0;
	double line_avg_waiting_per_stop = 0;
	double line_total_boarding = 0;
	double line_sd_headway = 0;		// averaged over stops (and not the SD for the whole dataset)
	double line_sd_DT = 0;			// averaged over stops (and not the SD for the whole dataset)
	double line_on_time = 0;
	double line_early = 0;
	double line_late = 0;
	double total_pass_riding_time = 0;
	double total_pass_dwell_time = 0;
	double total_pass_waiting_time = 0;
	double total_pass_holding_time = 0;
	double control_objective_function = 0;
	double total_travel_time_crowding = 0;
};

class Busline_assign // container object holding output data for trip assignments
{
public:
	Busline_assign (
		int line_id_, 
		int start_stop_id_, 
		string start_stop_name_,
		int end_stop_id_,	
		string end_stop_name_,
		int passenger_load_
        ): line_id(line_id_), start_stop_id(start_stop_id_), end_stop_id(end_stop_id_), start_stop_name(start_stop_name_), end_stop_name(end_stop_name_),passenger_load(passenger_load_) {}

	Busline_assign();			//!< simple constructor
	virtual ~Busline_assign();  //!< destructor

	void write(ostream& out){
		out << line_id << '\t'
			<< start_stop_id << '\t'
			<< start_stop_name << '\t'
			<< end_stop_id << '\t'
			<< end_stop_name << '\t'
			<< passenger_load << '\t' 
			<< endl; 
	}

	void reset(){
		start_stop_id = 0;
		end_stop_id = 0;
		passenger_load = 0;
	}

    int line_id = -1;
    int start_stop_id = 0;
    int end_stop_id = 0;
    string start_stop_name;
    string end_stop_name;
    int passenger_load = 0;
};

class Busline_travel_times // container that holds the total travel time experienced by line's trips
{
public:
	Busline_travel_times(
		int		trip_id_, 
		double	total_travel_time_
	):	trip_id(trip_id_), total_travel_time(total_travel_time_) {}

	virtual ~Busline_travel_times(); //!< destructor

	void write(ostream& out){ 
		out << trip_id << '\t'
			<< total_travel_time << '\t'
			<< endl; 
	}

	void reset(){ total_travel_time = 0; }

	int trip_id;
	double total_travel_time;
};

class Busline : public Action
{
public:
    Busline(
        int                     id_,                    //!< unique identification number
        int                     opposite_id_,           //!< identification number of the line that indicates the opposite direction (relevant only when modeling passenger route choice)
        string                  name_,                  //!< a descriptive name
        Busroute*               busroute_,              //!< bus route
        vector <Busstop*>       stops_,                 //!< stops on line
        Vtype*                  vtype_,                 //!< Vehicle type of this line (TODO 2018-10-23: Currently completely unusued but removing will invalidate all current network inputs)
        ODpair*                 odpair_,                //!< OD pair
        int                     holding_strategy_,      //!< indicates the type of holding strategy used for line
        double					max_headway_holding_,   //!< threshold parameter relevant in case holding strategies 1 or 3 are chosen or max holding time in [sec] in case of holding strategy 6
        double                  init_occup_per_stop_,   //!< average number of passengers that are on-board per prior upstream stops (scale of a Gamma distribution)
        int                     nr_stops_init_occup_,   //!< number of prior upstream stops resulting with initial occupancy (shape of a Gamma distribution)
        bool                    flex_line_ = false      //!< true if this line allows for dynamically scheduled trips
    ); //!< Initializing constructor

	Busline ();			//!< simple constructor
	virtual ~Busline(); //!< destructor
	void reset ();
	void reset_curr_trip (); // initialize after reading bustrips

	// Gets and sets
	int		  get_id () const {return id;}									//!< returns id, used in the compare  functions for find and find_if algorithms
	Busroute* get_busroute() {return busroute;}							//!< returns Busroute
	Vtype*	  get_vtype() {return vtype;}								//!< returns Vtype
	ODpair*	  get_odpair() {return odpair;}								//!< returns ODpair
	double	get_max_headway_holding() {return max_headway_holding;}	//!< returns ratio_headway_holding
	int		get_holding_strategy() {return holding_strategy;}			//!< returns the holding strategy
	double	get_init_occup_per_stop() {return init_occup_per_stop;}
	int		get_nr_stops_init_occup () {return nr_stops_init_occup;}
	int		get_opposite_id () {return opposite_id;}
	bool	set_curr_trip(const Bustrip* trip); //!< sets curr_trip to location of trip in trips list. Returns false if trip does not exist in trips list
	//void set_opposite_line (Busline* line) {opposite_line = line;}
	//Busline* get_opposite_line () {return opposite_line;}
	Output_Summary_Line get_output_summary () {return output_summary;}
	list <Start_trip>::iterator get_curr_trip() {return curr_trip;} 
	list <Start_trip> get_trips () {return trips;}
    string get_name(){return name;}

	//transfer gets and sets
	int	get_tr_line_id() {return tr_line_id;}
	vector <Busstop*> get_tr_stops() {return tr_stops;}

	// initialization
	void add_timepoints (vector <Busstop*> tp) {line_timepoint = tp;}
	void add_trip(Bustrip* trip, double starttime); 
	void add_disruptions (Busstop* from_stop, Busstop* to_stop, double disruption_start_time, double disruption_end_time, double cap_reduction);

	//transfer initialization
	void add_tr_line_id (int id) {tr_line_id = id;}
	void add_tr_stops (vector <Busstop*> stops) {tr_stops = stops;}
	
	// checks
	bool check_last_stop (Busstop* stop);
	bool is_line_timepoint (Busstop* stop);											//!< returns true if stops is a time point for this busline, otherwise it returns false
	bool check_first_stop (Busstop* stop);											//!< returns true if the stop is the first stop on the bus line, otherwise it returns false 
	bool check_first_trip (Bustrip* trip);											//!< returns true if the trip is the first trip on the bus line, otherwise it returns false  
	bool check_last_trip (Bustrip* trip);											//!< returns true if the trip is the last trip on the bus line, otherwise it returns false  
//	double calc_next_scheduled_arrival_at_stop (Busstop* stop, double time);		//!< returns the remaining time till the next trip on this line is scheduled to arrive at a stop (according to schedule only) 
	double find_time_till_next_scheduled_trip_at_stop (Busstop* stop, double time); //!< returns the time till the next trip which is scheduled to arrive next at stop
	Bustrip* find_next_expected_trip_at_stop (Busstop* stop);	//!< returns the next trip which is scheduled to arrive next at stop
	double time_till_next_arrival_at_stop_after_time (Busstop* stop, double time);	//!< returns the time left till next trip is expected to arrive at the stop (real-time calculation)
	Bustrip* get_next_trip (Bustrip* reference_trip);								//!< returns the trip after the reference trip on the trips vector	
	Bustrip* get_previous_trip (Bustrip* reference_trip);							//!< returns the trip before the reference trip on the trips vector
	Bustrip* get_last_trip () {return trips.back().first;}
	vector<Busstop*>::iterator get_stop_iter (Busstop* stop);										//!< returns the location of stop on the stops sequence for this line
	double check_subline_disruption (Busstop* last_visited_stop, Busstop* pass_stop, double time);	//!< check if this pair of stops is included in the disruption area and return extra time due to disrupution
	double extra_disruption_on_segment (Busstop* next_stop, double time);
	
	bool execute(Eventlist* eventlist, double time); //!< re-implemented from virtual function in Action this function does the real work. It initiates the current Bustrip and books the next one
	
	// calc attributes (for pass_paths)
	double calc_curr_line_headway ();
	double calc_curr_line_headway_forward ();
	double calc_max_headway ();
	double calc_curr_line_ivt (Busstop* start_stop, Busstop* end_stop, int rti, double time);
	
	// output-related functions
	void calculate_sum_output_line(); 
	// void calc_line_assignment();
	void add_record_passenger_loads_line (Busstop* stop1, Busstop* stop2, int pass_assign); //!< creates a log-file for passenegr load assignment info
	void write_assign_output(ostream & out);
	void update_total_travel_time (Bustrip* trip, double time);
	void write_ttt_output(ostream & out);

	vector <Busstop*>  stops; //!< contains all the stops on this line

    /** @ingroup DRT
        @{
    */
	//DRT implementation
	bool is_flex_line() const { return flex_line; } //!< returns true if trips can be dynamically generated by a control center for this line
	void add_flex_trip(Bustrip* trip); //!< adds trip to set of dynamically generated trips for this line
	void remove_flex_trip(Bustrip* trip); //!< remove trip from set of dynamically generated trips for this line, currently used when the trip has been completed
	void add_stop_delta(Busstop* stop, double delta_from_preceding_stop) { delta_at_stops.emplace_back(stop, delta_from_preceding_stop); } //!< add a scheduled/expected travel time between stops on this line (starting from 0 for the first stop)
	void set_delta_at_stops(vector<pair<Busstop*, double> > delta_at_stops_) { delta_at_stops = delta_at_stops_; } //!< set entire scheduled/expected inter-stop travel times for this line
	vector<pair<Busstop*,double>> get_delta_at_stops() const { return delta_at_stops; } //!< return vector of expected IVTs between stops starting at 0 from the first stop on this line
	int generate_new_trip_id() { trip_count++; return id*100 + trip_count; } //!< generates a new tripid for this line and increments the trip counter for this line, TODO: add condition if trips for this line > 100 to avoid ID conflicts
	bool is_unique_tripid(int tripid); //!< returns true if no trip in flex_trips or trips for this line has a matching tripid

	void set_static_trips(const list <Start_trip>& static_trips_) { static_trips = static_trips_; } //!< ugly solution, sole purpose is to save the initial (non-dynamically generated) trips vector between resets
    void set_planned_headway(double planned_headway_) { planned_headway = planned_headway_; }
    double get_planned_headway() const { return planned_headway; }
	void add_CC(Controlcenter* CC) { CC_ = CC; }
	Controlcenter* get_CC() { return CC_; }

	//Additional outputs for testing, perhaps redundant but can remove later
	// int count_total_boarded_passengers() const; // returns the total number of passengers that boarded this line at any stop along this line @note commented out wasnt really working for some odd reason
	void add_to_total_boarded_pass(int nr_boarding); 
	int get_total_boarded_pass() { return total_boarded_pass; }
	/**@}*/

protected:
    int id = -1;						//!< line ID
    int opposite_id = -1;			//!< the line ID of the opposite direction
    //Busline* opposite_line;
    string name;				//!< name of the busline "46 Sofia"
//	int vtype;					//!< vehicle type. There are usually multiple types of Busses
    Busroute* busroute = nullptr;						//!< the route (in terms of links) that the busses follow

    ODpair* odpair = nullptr;
    vector <Busstop*> line_timepoint;
    list <Start_trip> trips;				//!< the trips that are to be made

    Vtype* vtype = nullptr;							//!< the type of vehicle for the buses to be generated.

    double max_headway_holding = 0;

    int holding_strategy = 0;
    double init_occup_per_stop = 0;
    int nr_stops_init_occup = 0;

    //transfer attributes
    int	tr_line_id = 0; //!< id of line 'this' line synchronizes transfers with, should be 0 if 'this' line is not synchronizing transfers
    vector <Busstop*> tr_stops;	//!< contains all transfer stops for line

    /** @ingroup DRT
        @{
    */
    //drt related attributes
    bool flex_line = false;	//!< true if trips can be created dynamically for this line
    set<Bustrip*> flex_trips; //!< trips that were created dynamically for this line via a controlcenter
    vector<pair<Busstop*, double> > delta_at_stops; //!< expected ivt between departures for all stops on this line (defined on input when reading bustrips for this line) with first stop departure at time = 0
    int trip_count = 0; //!< the number of trips created for this line
    list <Start_trip> static_trips; //!< trips that were created from input files (i.e. were not created dynamically for this line), to be saved between resets
    double planned_headway = 0; //!< planned headway in seconds of this line, set to zero by default in constructor. If this is a DRT line then the planned headway is not based on pre-scheduled trips but on expectations given service design (@todo DRT currently only valid with trip format 3 now!)
	Controlcenter* CC_ = nullptr; //!< controlcenter that can dynamically generate trips for this line

	// additional output collection for FWF implementation/testing
	int total_boarded_pass = 0; //!< total number of passengers that boarded this line at any stop along this line
    /**@}*/

    map <Busstop*, pair<Busstop*, pair<double, double> > > disruption_times; //!< contains the expected travel times between a pair of stops in case of disruption (does not affect actual travel time, only passenger information provision). Strat and end times
    map <Busstop*, double, ptr_less<Busstop*>> disruption_cap_reduction;

    bool active = false;														//!< is true when the busline has started generating trips
    list <Start_trip>::iterator curr_trip;							//!< indicates the next trip
    list <Start_trip>::iterator next_trip; //!< indicates the next trip
    Output_Summary_Line output_summary;
    map <Busstop*, int, ptr_less<Busstop*>> stop_pass;
    map <Busstop*, Busline_assign, ptr_less<Busstop*>> output_line_assign;
    list <Busline_travel_times> output_travel_times;
};

typedef pair<Busstop*,double> Visit_stop;

class Bustrip_assign // container object holding output data for trip assignments
{
public:
	Bustrip_assign (
		int line_id_,
		int trip_id_,	
		int vehicle_id_, 
		int start_stop_id_,
		string start_stop_name_,
		int end_stop_id_,	
		string end_stop_name_,
		int passenger_load_
	): line_id(line_id_),trip_id(trip_id_),vehicle_id(vehicle_id_), start_stop_id(start_stop_id_), start_stop_name(start_stop_name_), end_stop_id(end_stop_id_),end_stop_name(end_stop_name_),passenger_load(passenger_load_) {}

	virtual ~Bustrip_assign(); //!< destructor
	
	void write(ostream& out){
		out << line_id << '\t'
			<< trip_id << '\t'
			<< vehicle_id << '\t'
			<< start_stop_id << '\t'
			<< start_stop_name << '\t'
			<< end_stop_id << '\t'
			<< end_stop_name << '\t'
			<< passenger_load << '\t' 
			<< endl;
	}

	void reset(){
		line_id = 0;
		trip_id = 0;
		vehicle_id = 0;
		start_stop_id = 0;
		end_stop_id = 0;
		passenger_load = 0;
	}

	int line_id;
	int trip_id;
	int vehicle_id;
	int start_stop_id;
	string start_stop_name;
	int end_stop_id;
	string end_stop_name;
	int passenger_load;
};

//!< @brief Categories of Bustrip, used mainly in the drt assignment pipeline used mainly if the BusTrip is a flex trip @todo maybe theres an easier way of bookeeping these, feels a bit redundant at the moment with all the 'flag' attributes
enum class BustripStatus
{
    Null = 0, // default status, if not a flex trip this should always be Null
	Unmatched, // preliminary trip consisting of a route and sequence of stops (Busline), not associated with any vehicle yet
	Matched, // trip has been matched to a vehicle (i.e. the vehicle has been assigned to the trip somehow, either directly or as part of a trip-chain)
	Scheduled, // trip has been scheduled for dispatch (i.e. a Busline event has been added for it)
	Activated, // trip has been activated (dispatched), in other words a vehicle has started to perform this trip
	Completed, // trip is finished (i.e. added to completed trips of a ControlCenter)
};
class Bustrip 
{
public:
	Bustrip (
		int			id_, 
		double		start_time_, 
		Busline*	line_
	);
	Bustrip ();
	~Bustrip ();
	void reset ();

// GETS & SETS
	int get_id () const {return id;}											  //!< returns id, used in the compare <..> functions for find and find_if algorithms
	Bus* get_busv () {return busv;}
    void set_busv(Bus* busv_);
	void set_bustype (Bustype* btype_) {btype = btype_;}
	Bustype* get_bustype () {return btype;}
	void set_line (Busline* line_) {line = line_;}
	Busline* get_line () {return line;}
	double get_starttime () const {return starttime;}
	double get_init_occup_per_stop () {return init_occup_per_stop;}
	int get_nr_stops_init_occup () {return nr_stops_init_occup;}
	vector <Visit_stop*> :: iterator& get_next_stop() {return next_stop;} //!< returns pointer to next stop
	void set_enter_time (double enter_time_) {enter_time = enter_time_;}
	double get_enter_time () {return enter_time;}
	list <Bustrip_assign> get_output_passenger_load() {return output_passenger_load;}
	double get_last_stop_exit_time() {return last_stop_exit_time;}
	void set_last_stop_exit_time (double last_stop_exit_time_) {last_stop_exit_time = last_stop_exit_time_;}
	void set_last_stop_visited (Busstop* last_stop_visited_) {last_stop_visited = last_stop_visited_;}
	Busstop* get_last_stop_visited() {return last_stop_visited;}
	double get_actual_dispatching_time () {return actual_dispatching_time;}
    map <Busstop*, passengers,ptr_less<Busstop*>> get_passengers_on_board () {return passengers_on_board;}

	void set_holding_at_stop(bool holding_at_stop_){holding_at_stop = holding_at_stop_;} //David added 2016-05-26
	bool get_holding_at_stop(){return holding_at_stop;} //David added 2016-05-26
	bool get_complying(){return complying_bustrip;}

// other functions:	
//	bool is_trip_timepoint(Busstop* stop); //!< returns 1 if true, 0 if false, -1 if busstop not found
	bool activate (double time, Route* route, ODpair* odpair, Eventlist* eventlist_);	//!< activates the trip. Generates the bus and inserts in net.
	bool advance_next_stop (double time, Eventlist* eventlist_);						//!< advances the pointer to the next stop (checking bounds)
	void add_stops (vector <Visit_stop*>  st) {stops = st; next_stop = stops.begin();}
	void add_trips (vector <Start_trip*>  st) {driving_roster = st;} 
	double scheduled_arrival_time (Busstop* stop);										//!< finds the scheduled arrival time for a given bus stop
	void book_stop_visit (double time);													//!< books a visit to the stop
	bool check_end_trip ();																//!< returns 1 if true, 0 if false
	double calc_departure_time (double time);											//!< calculates departure time from origin according to arrival time and schedule (including layover effect)
	void convert_stops_vector_to_map();													//!< building stops_map
	double find_crowding_coeff (Passenger* pass);										//!< returns the crowding coefficient based on load factor and pass. seating/standing
	static double find_crowding_coeff (bool sits, double load_factor);					//!< returns the crowding coefficient based on load factor and pass. seating/standing
	pair<double, double> crowding_dt_factor (double nr_boarding, double nr_alighting);
	vector <Busstop*> get_downstream_stops(); //!< return the remaining stops to be visited starting from 'next_stop', returns empty Busstop vector if there are none
	vector <Visit_stop*> get_downstream_stops_till_horizon(Visit_stop* target_stop); //!< return the remaining stops to be visited starting from 'next_stop'

// output-related functions
	void write_assign_segments_output(ostream & out);
	void record_passenger_loads (vector <Visit_stop*>::iterator start_stop); //!< creates a log-file for passenger load assignment info

// public vectors
	vector <Visit_stop*> stops;						//!< contains all the busstops and the times that they are supposed to be served. NOTE: this can be a subset of the total nr of stops in the Busline (according to the schedule input file)
	map <Busstop*, double> stops_map;
	vector <Start_trip*> driving_roster;			//!< trips assignment for each bus vehicle.
	bool deleted_driving_roster = false;			//!< false if the driving roster for a chain of trips that this trip is a part of has been deleted already, true otherwise
    map <Busstop*, passengers,ptr_less<Busstop*>> passengers_on_board; //!< passenger on-board stored by their alighting stop (format 3)
    map <Busstop*, int, ptr_less<Busstop*>> nr_expected_alighting;		//!< number of passengers expected to alight at the busline's stops (format 2)
    map <Busstop*, int, ptr_less<Busstop*>> assign_segements;			//!< contains the number of pass. traveling between trip segments

/** @ingroup DRT
    @{
*/
    double get_max_wait_requests(double cur_time) const; //!< returns the waiting time of longest waiting request for this trip
    double get_cumulative_wait_requests(double cur_time) const; //!< returns sum of all the waiting times for requests for this trip

    void set_starttime(double starttime_) { starttime = starttime_; }
    void set_scheduled_for_dispatch(bool scheduled_for_dispatch_);
	bool is_scheduled_for_dispatch() const { return scheduled_for_dispatch; }
    void set_activated(bool activated_);
	bool is_activated() const { return activated; }
	void set_flex_trip(bool flex_trip_) { flex_trip = flex_trip_; }
	bool is_flex_trip() const { return flex_trip; }
    vector <Request*> get_requests() const { return scheduled_requests;}
    void add_request (Request* req) { scheduled_requests.push_back((req));}
	bool remove_request(const Request* req); //!< removes request from scheduled requests if it exists, returns true if successful, false otherwise

	void update_total_boardings(int n_boarding) { total_boarding += n_boarding; }
	void update_total_alightings(int n_alighting) { total_alighting += n_alighting; }
	int get_total_boarding() const { return total_boarding; }
	int get_total_alighting() const { return total_alighting; }

	bool is_rebalancing_trip() const; //!< check that the trip purpose is not a pickup-trip or a passenger-carrying one, i.e. not assigned to any requests and not part of any trip-chain
	bool is_empty_pickup_trip() const; //!< check if the trip purpose is to pick-up travelers at the destination of the trip, i.e. not assigned to any requests, is part of a trip-chain, and the following trip is assigned to requests

	Bustrip* get_next_trip_in_chain() const; //!< returns the Bustrip that follows this one in the driving roster, nullptr if no Bustrip follows this one

	void set_status(BustripStatus newstatus);
    BustripStatus get_status()const { return status_; }
/**@}*/

protected:
    int id = -1;									//!< course nr
    Bus* busv = nullptr;							//!< pointer to the bus vehicle
    Bustype* btype = nullptr;
    Busline* line = nullptr;						//!< pointer to the line it serves
    double init_occup_per_stop = 0.0;				//!< initial occupancy, usually 0
    int nr_stops_init_occup = 0;
    bool complying_bustrip = true;					//!< indicates whether this trip complies with the control strategy in place or not  
    double starttime = 0.0;							//!< when the trip is schedule to departure from the origin
    double actual_dispatching_time = 0.0;
    vector <Visit_stop*> ::iterator next_stop;
    Random* random = nullptr;
    list <Bustrip_assign> output_passenger_load;	//!< contains the information on traveling on the segment starting at stop
    double enter_time = 0.0;						//!< the time it entered the most recently bus stop
    double last_stop_exit_time = 0.0;				//!< the time stamp of the exit time from the last stop that had been visited by this trip
    double last_stop_enter_time = 0.0;
    Busstop* last_stop_visited = nullptr;
    bool holding_at_stop = false;					//!< David added 2016-05-26: true if the trip is currently holding at a stop, false otherwise (used for progressing passengers in case of holding for demand format 3, should always be false for other formats)
    //	map <Busstop*,bool> trips_timepoint;		//!< will be relevant only when time points are trip-specific. binary map with time point indicatons for stops on route only (according to the schedule input file)  
    Eventlist* eventlist = nullptr;					//!< for use by busstops etc to book themselves.
	
	/** @ingroup DRT
        @{
    */
    vector <Request*> scheduled_requests;
	bool scheduled_for_dispatch = false; //!< true if this trip has been scheduled for dispatch (i.e. a busline event has been created with for the starttime of this trip) for its respective line, false otherwise
	bool activated = false; //!< true if this trip has been successfully activated (i.e. a bus has started this trip), false otherwise
	bool flex_trip = false; //!< true if this trip was generated dynamically
	BustripStatus status_ = BustripStatus::Null;
	int total_boarding = 0;
	int total_alighting = 0;
    /**@}*/
};

typedef pair<Busstop*, double> stop_rate;
typedef map <Busstop*, double, ptr_less<Busstop*>> stops_rate;
typedef pair <Busline*, stops_rate> multi_rate;
typedef map <Busline*, stops_rate, ptr_less<Busline*>> multi_rates;
typedef map <Busstop*, ODstops*, ptr_less<Busstop*>> ODs_for_stop;

class Busstop_Visit // container object holding output data for stop visits
{
public:
	Busstop_Visit(
		int		line_id_,
		int		trip_id_,
		int		vehicle_id_,
		int		stop_id_,
		string	stop_name_,
		double	entering_time_,	
		double	sched_arr_time_,	
		double	dwell_time_,	
		double	lateness_,					
		double	exit_time_,
		double	riding_time_, 
		double	riding_pass_time_, 
		double	crowded_pass_riding_time_, 
		double	crowded_pass_dwell_time_, 
		double	crowded_pass_holding_time_,					
		double	time_since_arr_, 
		double	time_since_dep_,
		int		nr_alighting_,	
		int		nr_boarding_,	
		int		occupancy_,	
		int		nr_waiting_, 
		double	total_waiting_time_, 
		double	holding_time_
	): line_id(line_id_),trip_id(trip_id_),vehicle_id(vehicle_id_), stop_id(stop_id_), stop_name(stop_name_), entering_time(entering_time_),sched_arr_time(sched_arr_time_),dwell_time(dwell_time_),
	   lateness(lateness_), exit_time (exit_time_),riding_time (riding_time_), riding_pass_time (riding_pass_time_), crowded_pass_riding_time (crowded_pass_riding_time_), 
	   crowded_pass_dwell_time (crowded_pass_dwell_time_), crowded_pass_holding_time (crowded_pass_holding_time_), time_since_arr(time_since_arr_),time_since_dep(time_since_dep_),
	   nr_alighting(nr_alighting_),nr_boarding(nr_boarding_),occupancy(occupancy_),nr_waiting(nr_waiting_), total_waiting_time(total_waiting_time_),holding_time(holding_time_) {}

	virtual ~Busstop_Visit(); //!< destructor
	void write (ostream& out) { 
		out << line_id << '\t'
			<< trip_id << '\t'
			<< vehicle_id << '\t'
			<< stop_id << '\t'
			<< stop_name << '\t'
			<< entering_time << '\t'
			<< sched_arr_time << '\t'
			<< dwell_time << '\t'
			<< lateness << '\t'
			<< exit_time <<'\t'
			<< riding_time << '\t' 
			<< riding_pass_time << '\t'
			<< time_since_arr << '\t'
			<< time_since_dep << '\t'
			<< nr_alighting << '\t'
			<< nr_boarding << '\t'
			<< occupancy << '\t'
			<< nr_waiting << '\t'
			<< total_waiting_time << '\t' 
			<< holding_time << '\t'	<< endl; 
	}
	void reset () {
		line_id = 0; 
		trip_id = 0; 
		vehicle_id = 0; 
		stop_id = 0; 
		entering_time = 0; 
		sched_arr_time = 0; 
		dwell_time = 0; 
		lateness = 0; 
		exit_time = 0; 
		riding_time = 0; 
		riding_pass_time = 0; 
		time_since_arr = 0; 
		time_since_dep = 0; 
		nr_alighting = 0; 
		nr_boarding = 0; 
		occupancy = 0; 
		nr_waiting = 0; 
		total_waiting_time = 0; 
		holding_time = 0; 
	}
	int line_id;
	int trip_id;
	int vehicle_id;
	int stop_id;
	string stop_name;
	double entering_time;
	double sched_arr_time;
	double dwell_time;
	double lateness;
	double exit_time;
	double riding_time;
	double riding_pass_time;
	double crowded_pass_riding_time;
	double crowded_pass_dwell_time;
	double crowded_pass_holding_time;
	double time_since_arr;
	double time_since_dep;
	int nr_alighting;
	int nr_boarding;
	int occupancy;
	int nr_waiting;
	double total_waiting_time;
	double holding_time;
};

class Output_Summary_Stop_Line // container object holding output data for stop visits
{
public:
	virtual ~Output_Summary_Stop_Line(); //!< destructor
	void write (ostream& out, int stop_id, int line_id, string name) { 
		out << stop_id <<  '\t' 
			<< name << '\t' 
			<< line_id << '\t'
			<< stop_avg_headway << '\t'
			<< stop_avg_DT << '\t'
			<< stop_avg_abs_deviation << '\t'
			<< stop_avg_waiting_per_stop<< '\t'
			<< stop_total_boarding << '\t'
			<< stop_sd_headway << '\t'
			<< stop_sd_DT << '\t'
			<< stop_on_time << '\t'
			<< stop_early <<'\t'
			<< stop_late << '\t' 
			<< total_stop_pass_riding_time << '\t' 
			<< total_stop_pass_dwell_time << '\t' 
			<< total_stop_pass_waiting_time << '\t' 
			<< total_stop_pass_holding_time << '\t' 
			<< stop_avg_holding_time << '\t' 
			<< total_stop_travel_time_crowding << '\t'
			<< endl; 
	}
	void reset () { 
		stop_avg_headway = 0; 
		stop_avg_DT = 0; 
		stop_avg_abs_deviation = 0; 
		stop_avg_waiting_per_stop = 0; 
		stop_total_boarding = 0;
		stop_sd_headway = 0; 
		stop_sd_DT = 0; 
		stop_on_time = 0; 
		stop_early = 0; 
		stop_late = 0; 
		total_stop_pass_riding_time = 0; 
		total_stop_pass_dwell_time = 0; 
		total_stop_pass_waiting_time = 0; 
		total_stop_pass_holding_time = 0;
		stop_avg_holding_time = 0;
		total_stop_travel_time_crowding = 0;
	}
	double stop_avg_headway = 0;
	double stop_avg_DT = 0;
	double stop_avg_abs_deviation = 0;
	double stop_avg_waiting_per_stop = 0;
	double stop_total_boarding = 0;
	double stop_sd_headway = 0;
	double stop_sd_DT = 0;
	double stop_on_time = 0;
	double stop_early = 0;
	double stop_late = 0;
	double total_stop_pass_riding_time = 0;
	double total_stop_pass_dwell_time = 0;
	double total_stop_pass_waiting_time = 0;
	double total_stop_pass_holding_time = 0;
	double stop_avg_holding_time = 0;
	double total_stop_travel_time_crowding = 0;
};

class Busstop : public Action
{
public:
	Busstop ();
	~Busstop ();
	Busstop(
		int		id_,
		string	name_,
		int		link_id_,
		double	position_,
		double	length_,
		bool	has_bay_,
		bool	can_overtake_,
		double	min_DT_,
		int		rti_,
		bool    non_random_pass_generation_,
		Controlcenter* CC_ = nullptr,
		Origin* origin_node_ = nullptr,
		Destination* dest_node_ = nullptr
	);

	void reset (); 

// GETS & SETS:
	int get_id () const {return id;} //!< returns id, used in the compare <..> functions for find and find_if algorithms
	int get_link_id() const {return link_id;}
	string get_name() const {return name;}
	int get_rti () const {return rti;}
	double get_arrival_rates (Bustrip* trip) {return arrival_rates[trip->get_line()];}
	double get_alighting_fractions (Bustrip* trip) {return alighting_fractions[trip->get_line()];}
	const ODs_for_stop & get_stop_as_origin () {return stop_as_origin;}
	ODstops* get_stop_od_as_origin_per_stop (Busstop* stop) {return stop_as_origin[stop];}
	bool check_stop_od_as_origin_per_stop (Busstop* stop) {if (stop_as_origin.count(stop)==0) return false; else return true;}
	double get_length () {return length;}
	double get_avaliable_length () {return avaliable_length;}
	void set_avaliable_length (double avaliable_length_) {avaliable_length = avaliable_length_;}
	bool get_bay () {return has_bay;}
	int get_nr_boarding () {return nr_boarding;}
	void set_nr_boarding (int nr_boarding_) {nr_boarding = nr_boarding_;}
	void set_nr_alighting (int nr_alighting_) {nr_alighting = nr_alighting_;}	
	int get_nr_alighting () {return nr_alighting;}
	int get_nr_waiting (Bustrip* trip) {return nr_waiting[trip->get_line()];}
	double get_position () { return position;}
	double get_exit_time() { return exit_time;}
	vector<Busline*> get_lines () {return lines;}
	void set_position (double position_ ) {position = position_;}
    map <Busline*, pair<Bustrip*, double>, ptr_less<Busline*> > get_last_departures () {return last_departures;}
	double get_last_departure (Busline* line) {return last_departures[line].second;}
	Bustrip* get_last_trip_departure (Busline* line) {return last_departures[line].first;}
    map<Busstop*,double, ptr_less<Busstop*>> & get_walking_distances () {return distances;}
	bool get_had_been_visited ( Busline * line) {return had_been_visited[line];} 
	double get_walking_distance_stop (Busstop* stop) {return distances[stop];}
	void save_previous_arrival_rates () {previous_arrival_rates.swap(arrival_rates);}
	void save_previous_alighting_fractions () {previous_alighting_fractions.swap(alighting_fractions);}
	bool check_walkable_stop ( Busstop* const & stop);
	bool check_destination_stop (Busstop* stop);
    bool get_gate_flag () {return gate_flag;}

	//transfer related checks
	bool is_awaiting_transfers(Bustrip* trip); //David added 2016-05-30: returns true if trip is currently awaiting transfers at stop

	Output_Summary_Stop_Line get_output_summary (int line_id) {return output_summary[line_id];}

// functions for initializing lines-specific vectors at the busstop level
	void add_lines (Busline*  line) {lines.push_back(line);}
	void add_line_nr_waiting(Busline* line, int value){nr_waiting[line] = value;}
	void add_line_nr_boarding(Busline* line, double value){arrival_rates[line] = value;}
	void add_line_nr_alighting(Busline* line, double value){alighting_fractions[line]= value;}
	void add_line_update_rate_time(Busline* line, double time){update_rates_times[line].push_back(time);}
	void add_real_time_info (Busline* line, bool info) {real_time_info[line] = info;}
	void set_had_been_visited (Busline* line, bool visited) {had_been_visited[line] = visited;}

// functions for initializing stop-specific input (relevant for demand format 3 only)	
	void add_odstops_as_origin(Busstop* destination_stop, ODstops* od_stop){stop_as_origin[destination_stop]= od_stop; is_origin = true;}
	void add_odstops_as_destination(Busstop* origin_stop, ODstops* od_stop){stop_as_destination[origin_stop]= od_stop; is_destination = true;}
	void add_distance_between_stops (Busstop* stop, double distance) {distances[stop] = distance;}

	void clear_odstops_as_origin(Busstop* destination_stop){stop_as_origin.erase(destination_stop);}
	void clear_odstops_as_destination(Busstop* origin_stop){stop_as_destination.erase(origin_stop);}

//	Action for visits to stop
	bool execute(Eventlist* eventlist, double time);									  //!< is executed by the eventlist and means a bus needs to be processed
	void passenger_activity_at_stop (Eventlist* eventlist, Bustrip* trip, double time);	  //!< progress passengers at stop: waiting, boarding and alighting
	void book_bus_arrival(Eventlist* eventlist, double time, Bustrip* trip);			  //!< add to expected arrivals
	double calc_exiting_time (Eventlist* eventlist, Bustrip* trip, double time);		  //!< To be implemented when time-points will work
    
    double get_walking_time(Busstop*,double);
	
// dwell-time calculation related functions	
	double calc_dwelltime (Bustrip* trip);								//!< calculates the dwelltime of each bus serving this stop. currently includes: passenger service times ,out of stop, bay/lane		
	double calc_holding_departure_time(Bustrip* trip, double time);		// David added 2016-04-01 calculates departure time from stop when holding is used, returns dwelltime + time if no holding is used
	bool check_out_of_stop (Bus* bus);									//!< returns TRUE if there is NO available space for the bus at the stop (meaning the bus is out of the stop)
	void occupy_length (Bus* bus);										//!< update available length when bus arrives
	void free_length (Bus* bus);										//!< update available length when bus leaves
	void update_last_arrivals (Bustrip* trip, double time);				//!< every time a bus ENTERS it updates the last_arrivals vector 
	void update_last_departures (Bustrip* trip, double time);			//!< every time a bus EXITS it updates the last_departures vector 
	double get_time_since_arrival (Bustrip* trip, double time);			//!< calculates the headway (defined as the difference in time between sequential arrivals) 
	double get_time_since_departure (Bustrip* trip, double time);		//!< calculates the headway (defined as the difference in time between sequential departures) 
	double find_exit_time_bus_in_front ();								//!< returns the exit time of the bus vehicle that entered the bus stop before a certain bus (the bus in front)

// output-related functions
	void write_output(ostream & out);
	void record_busstop_visit (Bustrip* trip, double enter_time); //!< creates a log-file for stop-related info
	static double calc_crowded_travel_time (double travel_time, int nr_riders, int nr_seats);
	
	void calculate_sum_output_stop_per_line(int line_id); //!< calculates for a single line that visits the stop (identified by line_id)
	int calc_total_nr_waiting ();

/**@ingroup DRT 
   @{
*/
    //control center related functions
	Controlcenter* get_CC() { return CC; } //!< returns the Controlcenter associated with this stop @todo figure out what to do if this stop has multiple Controlcenters associated with each Busstop
    void add_CC(Controlcenter* CC_) { CC = CC_; } //!< adds a Controlcenter to this stop, used for facilitating communication between Passenger and Controlcenter via sigslots
    void book_unassigned_bus_arrival(Eventlist* eventlist, Bus* bus, double expected_arrival_time); //!< add bus to vector of unassigned (i.e. no trip and no busline) bus vehicles arrivals, sorted by expected arrival time and add a Busstop event scheduled for the init_time of vehicle to switch BusState to OnCall from Null
	void add_unassigned_bus(Bus* bus, double arrival_time); //!< add bus to vector of unassigned buses at this stop sorted by actual arrival time, sets BusState to "OnCall"
	bool remove_unassigned_bus(Bus* bus, const double time); //!< remove bus from vector of unassigned buses at stop and sets its BusState from OnCall to IdleEmpty, returns false if bus does not exist
	vector<pair<Bus*, double>> get_unassigned_buses_at_stop() { return unassigned_buses_at_stop; }

	//bunch of accessors and modifiers related to buses ending a trip and being reinitialized at an opposing stop
	bool is_line_end() const { return dest_node != nullptr; } //!< true if this stop is at the end of a transit route/line (i.e. has a destination node associated with it)
	bool is_line_begin() const { return origin_node != nullptr; } //!< true if this stop is at the beginning of a transit route/line (i.e. has an origin node associated with it)
	Origin* get_origin_node() const { return origin_node; } 
	Destination* get_dest_node() const { return dest_node; } 
	void set_origin_node(Origin* origin_node_) { origin_node = origin_node_; }
	void set_dest_node(Destination* dest_node_) { dest_node = dest_node_; }

	bool is_within_walking_distance_of(Busstop* target_stop); //!< check to see which stops are connected by walking links from this stop
/**@}*/

// relevant only for demand format 2
	multi_rates multi_arrival_rates; //!< parameter lambda that defines the poisson process of passengers arriving at the stop for each sequential stop
    
    //methods related to exogenous walking times
    void add_walking_time_quantiles(Busstop*, vector<double>, vector<double>, int, double, double);
    double estimate_walking_time_from_quantiles(Busstop*, double);

protected:
    int id = -1;					//!< stop id
    string name;				//!< name of the bus stop "T-centralen"
    int link_id = -1;				//!< link it is on, maybe later a pointer to the respective link if needed
    double position = 0;		    //!< relative position from the upstream node of the link (between 0 to 1)
    double length = 20;			//!< length of the busstop, determines how many buses can be served at the same time
    bool has_bay = false;			//!< TRUE if it has a bay so it has an extra dwell time
    bool can_overtake = true;		//!< 0 - can't overtake, 1 - can overtake freely; TRUE if it is possible for a bus to overtake another bus that stops in front of it (if FALSE - dwell time is subject to the exit time of a blocking bus)
    double min_DT = 1;
    int rti = 0;					//!< indicates the level of real-time information at this stop: 0 - none; 1 - for all lines stoping at each stop; 2 - for all lines stoping at all connected stop; 3 - for the entire network.

    bool gate_flag = false;		//!< gate flag. If set true, passenger generation subject to timetable of transport services

    double avaliable_length = 20;	//!< length of the busstop minus occupied length
    double exit_time = 0;
    double dwelltime = 0;			//!< standard dwell time

    int nr_boarding = 0;			//!< pass. boarding
    int nr_alighting = 0;			//!< pass alighting

    list<double> exogenous_arrivals; //!< unordered list of arrival times of exogenous trains
    
	Random* random=nullptr;
	
	vector <Busline*> lines;
	map <double,Bus*> expected_arrivals;					//!< booked arrivals of buses on the link on their way to the stop
	vector<pair<Bustrip*,double> > expected_bus_arrivals;	//!< booked arrivals of buses on the link on their way to the stop
	map <double,Bus*> buses_at_stop;						//!< buses currently visiting stop
	vector<pair<Bustrip*,double> > buses_currently_at_stop;	//!< buses currently visiting stop
    map <Busline*, pair<Bustrip*, double>, ptr_less<Busline*> > last_arrivals;	//!< contains the arrival time of the last bus from each line that stops at the stop (can result headways)
    map <Busline*, pair<Bustrip*, double>,ptr_less<Busline*> > last_departures; //!< contains the departure time of the last bus from each line that stops at the stop (can result headways)
    map <Busline*,bool, ptr_less<Busline*>> had_been_visited;					//!< indicates if this stop had been visited by a given line till now

	// relevant only for demand format 1
    map <Busline*, double, ptr_less<Busline*>> arrival_rates;		//!< parameter lambda that defines the poisson process of passengers arriving at the stop
    map <Busline*, double, ptr_less<Busline*>> alighting_fractions; //!< parameter that defines the poisson process of the alighting passengers

	// relevant only for demand format 1 TD (format 10)
    map <Busline*, double, ptr_less<Busline*> > previous_arrival_rates;
    map <Busline*, double, ptr_less<Busline*> > previous_alighting_fractions;
    map <Busline*, vector<double>, ptr_less<Busline*> > update_rates_times;		//!< contains the information about when there is a change in rates (but not the actual change)
	
	// relevant only for demand format 2
	multi_rates multi_nr_waiting;			//!< for demand format is from type 2. 

	// relevant for demand formats 1 & 2
	map <Busline*, int> nr_waiting;			//!< number of waiting passengers for each of the bus lines that stops at the stop

	// relevant only for demand format 3
	ODs_for_stop stop_as_origin;			//!< a map of all the OD's that this busstop is their origin 
	ODs_for_stop stop_as_destination;		//!< a map of all the OD's that this busstop is their destination
	bool is_origin=false;							//!< indicates if this busstop serves as an origin for some passenger demand
	bool is_destination=false;					//!< indicates if this busstop serves as an destination for some passenger demand
    map <Busline*, bool,ptr_less<Busline*> > real_time_info;	//!< indicates for each line if it has real-time info. at this stop

	// walking distances between stops (relevant only for demand format 3 and 4)
    map<Busstop*,double, ptr_less<Busstop*> > distances;			//!< contains the distances [meters] from other bus stops
    
    // walking times between steps
    map<Busstop*, vector<Walking_time_dist*>,ptr_less<Busstop*> > walking_time_distribution_map; //!< contains set of distributions for a given destination node

    /** @ingroup DRT 
        @{
    */
	//drt implementation
    Controlcenter* CC = nullptr; //!< control center that this stop is associated with @todo change to a collection of possible Controlcenters, currently only assuming one available
    vector<pair<Bus*, double>> unassigned_bus_arrivals; //!< expected arrivals of transit vehicles to stop that are not assigned to any trip
    vector<pair<Bus*, double>> unassigned_buses_at_stop; //!< unassigned buses currently at stop along with the time they arrived/were initialized to this stop
    Origin* origin_node = nullptr; //!< origin node for generating buses when a trip begins at this stop. Is equal to nullptr if no trip can begin at this stop
    Destination* dest_node = nullptr; //!< destination node for processing buses that end a trip at this stop. Is equal to nullptr if no trip can end at this stop
    /**@}*/


	// transfer synchronization
	vector<pair<Bustrip*, int> > trips_awaiting_transfers;	//!< David added 2016-05-30: contains trips that are currently waiting to synchronize transfers with a connecting trip, paired with the line ID of the connecting trip

	// output structures
	list <Busstop_Visit> output_stop_visits;			//!< list of output data for buses visiting stops
	map<int,Output_Summary_Stop_Line> output_summary;	//!< int value is line_id
};

typedef pair<Busline*,double> TD_single_pair;
typedef map<Busstop*, map<Busline*,double>,ptr_less<Busstop*> > TD_demand;

class Change_arrival_rate : public Action
{
public:
	Change_arrival_rate(double time); 
	virtual ~Change_arrival_rate(); //!< destructor
	void book_update_arrival_rates (Eventlist* eventlist, double time);
	bool execute(Eventlist* eventlist, double time);
	void add_line_nr_boarding_TD(Busstop* stop, Busline* line, double value){TD_single_pair TD; TD.first = line; TD.second = value; arrival_rates_TD[stop].insert(TD);}
	void add_line_nr_alighting_TD(Busstop* stop, Busline* line, double value){TD_single_pair TD; TD.first = line; TD.second = value; alighting_fractions_TD[stop].insert(TD);}
	
protected:
	double loadtime;
	// relevant only for time-dependent demand format 1
	TD_demand arrival_rates_TD;			//!< parameter lambda that defines the poission proccess of passengers arriving at the stop
	TD_demand alighting_fractions_TD;	//!< parameter that defines the poission process of the alighting passengers 

};

class Walking_time_dist {
public:
    Walking_time_dist (Busstop* dest_stop_, vector<double> quantiles_, vector<double> quantile_values_, int num_quantiles_, double time_start_, double time_end_);
    
    virtual ~Walking_time_dist(){}
    
    bool time_is_in_range(double);
    int get_num_quantiles() {return num_quantiles;}
    double* get_quantiles();
    double* get_quantile_values() {return quantile_values;}
    
protected:
    Busstop* dest_stop;
    double* quantiles;
    double* quantile_values;
    int num_quantiles;
    double time_start;
    double time_end;
};


class Dwell_time_function // container that holds the total travel time experienced by line's trips
{
public:
	Dwell_time_function (
		int function_id_, 
		int dwell_time_function_form_, 
		double dwell_constant_, 
		double boarding_coefficient_, 
		double alighting_cofficient_, 
		double dwell_std_error_, 
		int number_boarding_doors_, 
		int number_alighting_doors_,
		double share_alighting_front_door_, 
		double crowdedness_binary_factor_, 
		double bay_coefficient_, 
		double over_stop_capacity_coefficient_
	): function_id(function_id_), dwell_time_function_form (dwell_time_function_form_), dwell_constant(dwell_constant_), boarding_coefficient(boarding_coefficient_), alighting_cofficient(alighting_cofficient_), dwell_std_error(dwell_std_error_),number_boarding_doors(number_boarding_doors_),number_alighting_doors(number_alighting_doors_),share_alighting_front_door(share_alighting_front_door_),crowdedness_binary_factor(crowdedness_binary_factor_),bay_coefficient(bay_coefficient_),over_stop_capacity_coefficient(over_stop_capacity_coefficient_) {}
	
	Dwell_time_function (
		int function_id_, 
		int dwell_time_function_form_, 
		double dwell_constant_, 
		double boarding_coefficient_, 
		double alighting_cofficient_, 
		double dwell_std_error_, 
		double share_alighting_front_door_, 
		double crowdedness_binary_factor_, 
		double bay_coefficient_, 
		double over_stop_capacity_coefficient_
	): function_id(function_id_), dwell_time_function_form (dwell_time_function_form_), dwell_constant(dwell_constant_), boarding_coefficient(boarding_coefficient_), alighting_cofficient(alighting_cofficient_), dwell_std_error(dwell_std_error_), share_alighting_front_door(share_alighting_front_door_),crowdedness_binary_factor(crowdedness_binary_factor_),bay_coefficient(bay_coefficient_),over_stop_capacity_coefficient(over_stop_capacity_coefficient_) {}
	
	Dwell_time_function (
		int function_id_, 
		int dwell_time_function_form_, 
		double dwell_constant_, 
		double boarding_coefficient_, 
		double alighting_cofficient_, 
		double dwell_std_error_, 
		double bay_coefficient_, 
		double over_stop_capacity_coefficient_
	): function_id(function_id_), dwell_time_function_form (dwell_time_function_form_), dwell_constant(dwell_constant_), boarding_coefficient(boarding_coefficient_), alighting_cofficient(alighting_cofficient_), dwell_std_error(dwell_std_error_),bay_coefficient(bay_coefficient_),over_stop_capacity_coefficient(over_stop_capacity_coefficient_) {}
	
	virtual ~Dwell_time_function(); //!< destructor
	int get_id () {return function_id;}

	// dwell time parameters
	int function_id=-1;
	int dwell_time_function_form=-1; 
	// 11 - Linear function of boarding and alighting
    // 12 - Linear function of boarding and alighting + non-linear crowding effect (Weidmann) 
    // 13 - Max (boarding, alighting) + non-linear crowding effect (Weidmann) 
    // 21 - TCRP(max doors with crowding, boarding from front door, alighting from both doors) + bay + stop capacity
	// 22 - TCRP(max doors with crowding, boarding from x doors, alighting from y doors) + bay + stop capacity

	double dwell_constant=1; 
	double boarding_coefficient=1;
	double alighting_cofficient=1;
	double dwell_std_error=0;

	// only for TCRP functions
	int number_boarding_doors=1;
	int number_alighting_doors=1;
	double share_alighting_front_door=1;
	double crowdedness_binary_factor=0; 

	// extra delays
	double bay_coefficient=0;
	double over_stop_capacity_coefficient=0; 
};

#endif //_BUSLINE
