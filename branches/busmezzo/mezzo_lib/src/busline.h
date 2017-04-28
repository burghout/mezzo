#ifndef _BUSLINE
#define _BUSLINE


#include "parameters.h" 
#include "route.h"
#include "Random.h"
#include <algorithm>
#include "vehicle.h"
#include <fstream>
#include <string>
#include <vector>
#include "link.h"
#include "sdfunc.h"
#include "q.h"
#include "passenger.h"
#include "MMath.h" 
#include <cmath>
#include "od_stops.h"
#include <stddef.h>
#include <dlib/optimization.h>


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
class hist_set;
class hist_paths;
class hist_demand;

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
	double line_avg_headway;
	double line_avg_DT;
	double line_avg_abs_deviation;
	double line_avg_waiting_per_stop;
	double line_total_boarding;
	double line_sd_headway;		// averaged over stops (and not the SD for the whole dataset)
	double line_sd_DT;			// averaged over stops (and not the SD for the whole dataset)
	double line_on_time;
	double line_early;
	double line_late;
	double total_pass_riding_time;
	double total_pass_dwell_time;
	double total_pass_waiting_time;
	double total_pass_holding_time;
	double control_objective_function;
	double total_travel_time_crowding;
};

class Busline_assign // container object holding output data for trip assignments
{
public:
	Busline_assign (
		int line_id_, 
		int start_stop_id_, 
		int end_stop_id_,	
		int passenger_load_
		): line_id(line_id_), start_stop_id(start_stop_id_), end_stop_id(end_stop_id_), passenger_load(passenger_load_) {}

	Busline_assign();			//!< simple constructor
	virtual ~Busline_assign();  //!< destructor

	void write(ostream& out){
		out << line_id << '\t'
			<< start_stop_id << '\t'
			<< end_stop_id << '\t'
			<< passenger_load << '\t' 
			<< endl; 
	}

	void reset(){
		start_stop_id = 0;
		end_stop_id = 0;
		passenger_load = 0;
	}

	int line_id;
	int start_stop_id;
	int end_stop_id;
	int passenger_load;
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

typedef pair<Busstop*,double> Visit_stop;
typedef map<int,vector<Busstop*>> tr_line; //2016-06-23 Hend: contains the id of each transfer line and the transfer_stops

class Busline: public Action
{
public:
	Busline (
		int						id_,						//!< unique identification number
		int						opposite_id_,				//!< identification number of the line that indicates the opposite direction (relevant only when modelling passenger route choice)
		string					name_,						//!< a descriptive name
		Busroute*				busroute_,					//!< bus route
		vector <Busstop*>		stops_,						//!< stops on line
		Vtype* vtype_, ODpair*	odpair_,					//!< OD pair
		int						holding_strategy_,			//!< indicates the type of holding strategy used for line
		float					max_headway_holding_,		//!< threshold parameter relevant in case holding strategies 1 or 3 are chosen or max holding time in [sec] in case of holding strategy 6
		double					init_occup_per_stop_,		//!< average number of passengers that are on-board per prior upstream stops (scale of a Gamma distribution)
		int						nr_stops_init_occup			//!< number of prior upstream stops resulting with initial occupancy (shape of a Gamma distribution)
	); //!< Initialising constructor

	Busline ();			//!< simple constructor
	virtual ~Busline(); //!< destructor
	void reset ();
	void reset_curr_trip (); // initialize after reading bustrips

	// Gets and sets
	int		  get_id () {return id;}									//!< returns id, used in the compare  functions for find and find_if algorithms
	Busroute* get_busroute() {return busroute;}							//!< returns Busroute
	Vtype*	  get_vtype() {return vtype;}								//!< returns Vtype
	ODpair*	  get_odpair() {return odpair;}								//!< returns ODpair
	double	get_max_headway_holding() {return max_headway_holding;}	//!< returns ratio_headway_holding
	int		get_holding_strategy() {return holding_strategy;}			//!< returns the holding strategy
	double	get_init_occup_per_stop() {return init_occup_per_stop;}
	int		get_nr_stops_init_occup () {return nr_stops_init_occup;}
	int		get_opposite_id () {return opposite_id;}
	void	set_curr_trip(vector <Start_trip>::iterator curr_trip_) {curr_trip = curr_trip_;}
	//void set_opposite_line (Busline* line) {opposite_line = line;}
	//Busline* get_opposite_line () {return opposite_line;}
	Output_Summary_Line get_output_summary () {return output_summary;}
	vector <Start_trip>::iterator get_curr_trip() {return curr_trip;} 
	vector <Start_trip> get_trips () {return trips;}

	//transfer gets and sets
	int	get_tr_line_id() {return tr_line_id;}
	vector <Busstop*> get_tr_stops() {return tr_stops;}
	vector<Visit_stop> get_transfer_stops();

	bool check_stop_in_line_is_transfer(map<int, vector<Busstop*>> trnsfr_lines, Busline * line, Busstop * stop);//Hend 25/4/17 -return true if a stop in line is transfer line

	// initialization
	void add_timepoints (vector <Busstop*> tp) {line_timepoint = tp;}
	void add_trip(Bustrip* trip, double starttime){trips.push_back(Start_trip(trip,starttime));}
	void add_transferstops (vector <Busstop*> tr) {tr_stops = tr;} //!< Hend added 071016 add transfer stops
	void add_disruptions (Busstop* from_stop, Busstop* to_stop, double disruption_start_time, double disruption_end_time, double cap_reduction);

	//transfer initilization
	void add_tr_line_id (int id) {tr_line_id = id;}
	void add_tr_stops (vector <Busstop*> stops) {tr_stops = stops;}
	map<int,vector <Busstop*>> transfer_lines; //!< Hend added 070116 transfer lines
	map<int,vector <Busstop*>> get_transfer_lines (Busline*) {return transfer_lines;}
	void add_transfer_line (map<int,vector <Busstop*>> transfer_line ) { transfer_lines = transfer_line;} //!< Hend added 070116 add transfer line
	bool check_first_trip_in_handled(vector<Start_trip> trips_to_handle, Bustrip * trip); //Hend 26/4/17 to check if first trip for line in handled

	// checks
	bool check_last_stop (Busstop* stop);
	bool is_line_timepoint (Busstop* stop);											//!< returns true if stops is a time point for this busline, otherwise it returns false
	bool is_line_transfer_stop (Busstop* stop);									    //!< returns true if stops is a transfer stop for this busline, otherwise it returns false
	bool check_first_stop (Busstop* stop);											//!< returns true if the stop is the first stop on the bus line, otherwise it returns false 
	bool check_first_trip (Bustrip* trip);											//!< returns true if the trip is the first trip on the bus line, otherwise it returns false  
	bool check_last_trip (Bustrip* trip);											//!< returns true if the trip is the last trip on the bus line, otherwise it returns false  
//	double calc_next_scheduled_arrival_at_stop (Busstop* stop, double time);		//!< returns the remaining time till the next trip on this line is scheduled to arrive at a stop (according to schedule only) 
	double find_time_till_next_scheduled_trip_at_stop (Busstop* stop, double time); //!< returns the time till the next trip which is scheduled to arrive next at stop
	vector<Start_trip>::iterator find_next_expected_trip_at_stop (Busstop* stop);	//!< returns the next trip which is scheduled to arrive next at stop
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

protected:
	int id;						//!< line ID
	int opposite_id;			//!< the line ID of the opposite direction
	//Busline* opposite_line;
	string name;				//!< name of the busline "46 Sofia"
//	int vtype;					//!< vehicle type. There are usually multiple types of Busses
    Busroute* busroute;						//!< the route (in terms of links) that the busses follow

    ODpair* odpair;
	vector <Busstop*> line_timepoint;
	vector <Start_trip> trips;				//!< the trips that are to be made

	Vtype* vtype;							//!< the type of vehicle for the buses to be generated.

	float max_headway_holding;

	double desired_offset, planned_headway; //!< relevant in case headway control is of type 10
	int holding_strategy; 
	double init_occup_per_stop;
	int nr_stops_init_occup;

	//transfer attributes
	int	tr_line_id; //!< id of line 'this' line synchronizes transfers with, should be 0 if 'this' line is not synchronizing transfers
	vector <Busstop*> tr_stops;	//!< contains all transfer stops for line

    map <Busstop*,pair<Busstop*,pair<double,double> > > disruption_times; //!< contains the expected travel times between a pair of stops in case of disruption (does not affect actual travel time, only passenger information provision). Strat and end times
	map <Busstop*, double> disruption_cap_reduction;

	bool active;														//!< is true when the busline has started generating trips
	vector <Start_trip>::iterator curr_trip;							//!< indicates the next trip
	Output_Summary_Line output_summary;
	map <Busstop*, int> stop_pass;
	map <Busstop*, Busline_assign> output_line_assign;
	list <Busline_travel_times> output_travel_times;
};



class Bustrip_assign // container object holding output data for trip assignments
{
public:
	Bustrip_assign (
		int line_id_,
		int trip_id_,	
		int vehicle_id_, 
		int start_stop_id_, 
		int end_stop_id_,	
		int passenger_load_
	): line_id(line_id_),trip_id(trip_id_),vehicle_id(vehicle_id_), start_stop_id(start_stop_id_),end_stop_id(end_stop_id_),passenger_load(passenger_load_) {}

	virtual ~Bustrip_assign(); //!< destructor
	
	void write(ostream& out){
		out << line_id << '\t'
			<< trip_id << '\t'
			<< vehicle_id << '\t'
			<< start_stop_id << '\t'
			<< end_stop_id << '\t'
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
	int end_stop_id;
	int passenger_load;
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
	int get_id () {return id;}											  //!< returns id, used in the compare <..> functions for find and find_if algorithms
	Bus* get_busv () {return busv;}
	void set_busv (Bus* busv_) {busv = busv_;}
	void set_bustype (Bustype* btype_) {btype = btype_;}
	Bustype* get_bustype () {return btype;}
	void set_line (Busline* line_) {line = line_;}
	Busline* get_line () {return line;}
	double get_starttime () {return starttime;}
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
	map <Busstop*, passengers> get_passengers_on_board () {return passengers_on_board;}

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
	void convert_downstreamstops_vector_to_map(vector <Visit_stop*> down_stops);													//!< building stops_map
	vector <Visit_stop*> ::iterator get_target_stop(Bustrip *trip, vector<Visit_stop*> :: iterator& next_stop); //Hend added 190716 get target stop according to the stop horizon
	vector<Visit_stop*>::iterator  get_previous_stop(Bustrip *trip, vector <Visit_stop*> ::iterator& down_stp); // Hend added 190317 get the previous stop of the trip
	vector<Start_trip> Bustrip::find_trips_to_handle_same_line(vector<Start_trip> line_trips, Visit_stop *target_stop); //Hend added 23/4/17 to find trips to handle
	vector<Start_trip> Bustrip::find_trips_to_handle_transfer_lines(vector<Start_trip> trips_to_handle); //Hend added 23/4/17 to find trips to handle in the transfer lines
	vector <Visit_stop*> ::iterator get_transfer_target_stop(Bustrip *trip, vector<Visit_stop*> :: iterator& trnsfr_stop); //Hend added 171016 get target stop for transfer lines
	vector<Start_trip> calc_hist_prdct_arrivals(Start_trip trip, Visit_stop *target_stop, vector<Start_trip> trips_to_handle); ////Hend added 29/3/17: calc the predictions of the arrival times of trips
	
	vector<Start_trip> get_transfer_trips(Bustrip *trip, vector<Visit_stop*> transfer_in_downstream, vector<Start_trip> trips_to_handle);
	vector<Start_trip> calc_predict_times(Start_trip trip, Visit_stop *target_stop, vector<Start_trip> trips_to_handle); ////Hend added 130716: calc the predictions of the arrival times of trips
	vector<double> get_first_and_last_trips(vector<Start_trip> trips_to_handle,Visit_stop* transfer_stop);//Hend added 130716: get the first and last trip to enter transfer stop
	Busline* get_tr_line(Busstop* it_stp, int tr_id);//Hend added 130716: get busline of the transfer line
	vector<Visit_stop*> get_transfers_in_downstream(Bustrip* trip, vector<Visit_stop*> transfer_in_downstream, vector <Visit_stop> visit_transfers); //Hend added 120716: return a vector of transfer stops till the target stop
	bool is_trip_exist(vector<Start_trip> trips_in_transfer, Bustrip* transfer_trip); //Hend added 190716 checks if trips in transfer contain transfer trip
	double get_predict_time_to_trnsfr(Visit_stop *transfer_stop); //Hend added 200716 find the handled transfer and return the predict arrival time
	vector<Start_trip> get_trips_in_transfer_line(vector<Start_trip> transfer_trips, Visit_stop *transfer_stop ,vector<double> limited_tr_stop, vector<Start_trip> trips_in_transfer);//Hend added 190716 return transfer trips that in the horizon
	double find_crowding_coeff (Passenger* pass);										//!< returns the crowding coefficeint based on lod factor and pass. seating/standing
	static double find_crowding_coeff (bool sits, double load_factor);					//!< returns the crowding coefficeint based on lod factor and pass. seating/standing
	pair<double, double> crowding_dt_factor (double nr_boarding, double nr_alighting);
	vector <Busstop*> get_downstream_stops(); //!< return the remaining stops to be visited starting from 'next_stop', returns empty Busstop vector if there are none
	vector <Visit_stop*> get_downstream_stops_till_horizon(Bustrip *trip, Visit_stop* target_stop); //!< return the remaining stops to be visited starting from 'next_stop'
	bool is_stop_in_downstream(Visit_stop *target_stop);  //!< return true if the trip didnt get yet to the target stop
	bool check_first_stop_in_handled(vector<Visit_stop *> down_stops, Busstop* stop); //Hend added 24/4/17 to check if this stop is first in handled trip
// output-related functions
	void write_assign_segments_output(ostream & out);
	void record_passenger_loads (vector <Visit_stop*>::iterator start_stop); //!< creates a log-file for passenegr load assignment info

// public vectors
	vector <Visit_stop*> stops;						//!< contains all the busstops and the times that they are supposed to be served. NOTE: this can be a subset of the total nr of stops in the Busline (according to the schedule input file)
	map <Busstop*, double> stops_map;
	map <Busstop*, double> downstream_stops_map;
	vector <Start_trip*> driving_roster;			//!< trips assignment for each bus vehicle.
	map <Busstop*, passengers> passengers_on_board; //!< passenger on-board stored by their alighting stop (format 3)
	map <Busstop*, int> nr_expected_alighting;		//!< number of passengers expected to alight at the busline's stops (format 2)
	map <Busstop*, int> assign_segements;			//!< contains the number of pass. travelling between trip segments
	vector <Visit_stop*> down_stops;
	
protected:
	int id;										  //!< course nr
	Bus* busv;									  //!< pointer to the bus vehicle
	Bustype* btype;
	Busline* line;								  //!< pointer to the line it serves
	double init_occup_per_stop;					  //!< initial occupancy, usually 0
	int nr_stops_init_occup;
	bool complying_bustrip;						  //!< indicates whether this trip complies with the control strategy in place or not  
	double starttime;							  //!< when the trip is schedule to departure from the origin
	double actual_dispatching_time;
	vector <Visit_stop*> :: iterator next_stop; 
	Random* random;
	list <Bustrip_assign> output_passenger_load;  //!< contains the information on travelling on the segment starting at stop
	double enter_time;							  //!< the time it entered the most recently bus stop
	double last_stop_exit_time;					  //!< the time stamp of the exit time from the last stop that had been visited by this trip
	double last_stop_enter_time; 
	Busstop* last_stop_visited;
	bool holding_at_stop;						 //!< David added 2016-05-26: true if the trip is currently holding at a stop, false otherwise (used for progressing passengers in case of holding for demand format 3, should always be false for other formats)
	//	map <Busstop*,bool> trips_timepoint;	 //!< will be relevant only when time points are trip-specific. binary map with time point indicatons for stops on route only (according to the schedule input file)  
	Eventlist* eventlist;						 //!< for use by busstops etc to book themselves.
};

typedef pair<Busstop*, double> stop_rate;
typedef map <Busstop*, double> stops_rate;
typedef pair <Busline*, stops_rate> multi_rate;
typedef map <Busline*, stops_rate> multi_rates;
typedef map <Busstop*, ODstops*> ODs_for_stop;

class Busstop_Visit // container object holding output data for stop visits
{
public:
	Busstop_Visit (
		int		line_id_, 
		int		trip_id_,	
		int		vehicle_id_,	 
		int		stop_id_, 
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
	): line_id(line_id_),trip_id(trip_id_),vehicle_id(vehicle_id_), stop_id(stop_id_),entering_time(entering_time_),sched_arr_time(sched_arr_time_),dwell_time(dwell_time_),
	   lateness(lateness_), exit_time (exit_time_),riding_time (riding_time_), riding_pass_time (riding_pass_time_), crowded_pass_riding_time (crowded_pass_riding_time_), 
	   crowded_pass_dwell_time (crowded_pass_dwell_time_), crowded_pass_holding_time (crowded_pass_holding_time_), time_since_arr(time_since_arr_),time_since_dep(time_since_dep_),
	   nr_alighting(nr_alighting_),nr_boarding(nr_boarding_),occupancy(occupancy_),nr_waiting(nr_waiting_), total_waiting_time(total_waiting_time_),holding_time(holding_time_) {}

	virtual ~Busstop_Visit(); //!< destructor
	void write (ostream& out) { 
		out << line_id << '\t'
			<< trip_id << '\t'
			<< vehicle_id << '\t'
			<< stop_id<< '\t'
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

class LineIdStopId {
public:
	int line_id_;
	int stop_id_;
	LineIdStopId(int line_id, int stop_id) :
		line_id_(line_id), stop_id_(stop_id) {} 
};

struct CompareLineStop { 
	bool operator()(const LineIdStopId& left,const LineIdStopId& right ) const {
		if(left.line_id_ == right.line_id_ && 
			left.stop_id_ == right.stop_id_)
			return false;
		if (left.line_id_ > right.line_id_ ||
			(left.line_id_ == right.line_id_ &&left.stop_id_ > right.stop_id_))
			return false;
		return true;
	}
};

class TripIdtoTripId {
public:
	int from_trip_id1_;
	int to_trip_id2_;
	TripIdtoTripId(int from_trip_id1, int to_trip_id2) :
		from_trip_id1_(from_trip_id1), to_trip_id2_(to_trip_id2) {}
};

struct CompareTripIdtoTripId {
	bool operator()(const TripIdtoTripId& left, const TripIdtoTripId& right) const {
		if (left.from_trip_id1_ == right.from_trip_id1_ &&
			left.to_trip_id2_ == right.to_trip_id2_)
			return false;
		if (left.from_trip_id1_ > right.from_trip_id1_ ||
			(left.from_trip_id1_ == right.from_trip_id1_ &&left.to_trip_id2_ > right.to_trip_id2_))
			return false;
		return true;
	}
};

/*****************13-04-2017 new presentaion***********/
class BoardDemand {
public:
	int passengers_number_;
	int destination_;
	BoardDemand(int passengers_number, int destination) :
		passengers_number_(passengers_number), destination_(destination) {}

};
class AlightDemand {
public:
	int passengers_number_;
	int source_;
	AlightDemand(int passengers_number, int source) :
		passengers_number_(passengers_number), source_(source) {}

};
class BoardDemandTransfer {
public:
	int passengers_number_;
	int previous_line_;
	int previous_station_;

	BoardDemandTransfer(int passengers_number, int previous_line, int previous_station) :
		passengers_number_(passengers_number), 
		previous_line_(previous_line),
		previous_station_(previous_station){}

};
class AlightDemandTransfer {
public:
	int passengers_number_;
	int next_line_;
	int next_station_;

	AlightDemandTransfer(int passengers_number, int next_line, int next_station) :
		passengers_number_(passengers_number),
		next_line_(next_line),
		next_station_(next_station) {}

};
class LineStationData {
public:
	list<BoardDemand> boardings;
	list<AlightDemand> alightings;
	list<BoardDemandTransfer> boardingTransfers;
	list<AlightDemandTransfer> alightingTransfers;
};



/****************************/

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
	double stop_avg_headway;
	double stop_avg_DT;
	double stop_avg_abs_deviation;
	double stop_avg_waiting_per_stop;
	double stop_total_boarding;
	double stop_sd_headway;
	double stop_sd_DT;
	double stop_on_time;
	double stop_early;
	double stop_late;
	double total_stop_pass_riding_time;
	double total_stop_pass_dwell_time;
	double total_stop_pass_waiting_time;
	double total_stop_pass_holding_time;
	double stop_avg_holding_time;
	double total_stop_travel_time_crowding;
};

class Busstop : public Action
{
public:
	Busstop ();
	~Busstop ();
    Busstop (
		int		id_, 
		string	name_, 
		int		link_id_, 
		double	position_, 
		double	length_, 
		bool	has_bay_, 
		bool	can_overtake_, 
		double	min_DT_, 
		int		rti_
	);
	Busstop::Busstop (
		int		id_, 
		string	name_, 
		int		link_id_, 
		double	position_, 
		double	length_, 
		bool	has_bay_, 
		bool	can_overtake_, 
		double	min_DT_, 
		int		rti_,
		map<LineIdStopId, hist_set*, CompareLineStop>* history_summary_map_,
		map<LineIdStopId, hist_demand*, CompareLineStop>* hist_demand_map_,
		map<LineIdStopId, LineStationData*, CompareLineStop>* hist_dmnd_map_,
		map<LineIdStopId,list<pair<int,double>>, CompareLineStop>* transfer_to_my_line_
	);

	void reset (); 

// GETS & SETS:
	int get_id () {return id;} //!< returns id, used in the compare <..> functions for find and find_if algorithms
	int get_link_id() {return link_id;}
	string get_name() {return name;}
	int get_rti () {return rti;}
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
	map <Busline*, pair<Bustrip*, double> > get_last_departures () {return last_departures;}
	double get_last_departure (Busline* line) {return last_departures[line].second;}
	Bustrip* get_last_trip_departure (Busline* line) {return last_departures[line].first;}
	map<Busstop*,double> & get_walking_distances () {return distances;}
	bool get_had_been_visited ( Busline * line) {return had_been_visited[line];} 
	double get_walking_distance_stop (Busstop* stop) {return distances[stop];}
	void save_previous_arrival_rates () {previous_arrival_rates.swap(arrival_rates);}
	void save_previous_alighting_fractions () {previous_alighting_fractions.swap(alighting_fractions);}
	bool check_walkable_stop ( Busstop* const & stop);
	bool check_destination_stop (Busstop* stop); 
	
	//Hend added 29/3/17 to calculate the total passengers time for the optimal strategy 
	double total_passengers_time(vector<Start_trip> trips_to_handle, vector<double> ini_vars); // (const column_vector& m);
	void calc_expected_arrival_departue(vector<Start_trip> trips_to_handle, vector<double> ini_vars);
	void calc_direct_demand(vector<Start_trip> trips_to_handle, vector<double> ini_vars);
	void calc_transfer_demand(vector<Start_trip> trips_to_handle, vector<double> ini_vars);

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
	
// dwell-time calculation related functions	
	double calc_dwelltime (Bustrip* trip);								//!< calculates the dwelltime of each bus serving this stop. currently includes: passenger service times ,out of stop, bay/lane		
	double calc_holding_departure_time(Bustrip* trip, double time);		// David added 2016-04-01 calculates departure time from stop when holding is used, returns dwelltime + time if no holding is used
	bool check_out_of_stop (Bus* bus);									//!< returns TRUE if there is NO avaliable space for the bus at the stop (meaning the bus is out of the stop)
	void occupy_length (Bus* bus);										//!< update avaliable length when bus arrives
	void free_length (Bus* bus);										//!< update avaliable length when bus leaves
	void update_last_arrivals (Bustrip* trip, double time);				//!< everytime a bus ENTERS it updates the last_arrivals vector 
	void update_last_departures (Bustrip* trip, double time);			//!< everytime a bus EXITS it updates the last_departures vector 
	double get_time_since_arrival (Bustrip* trip, double time);			//!< calculates the headway (defined as the differnece in time between sequential arrivals) 
	double get_time_since_departure (Bustrip* trip, double time);		//!< calculates the headway (defined as the differnece in time between sequential departures) 
	double find_exit_time_bus_in_front ();								//!< returns the exit time of the bus vehicle that entered the bus stop before a certain bus (the bus in front)

// output-related functions
	void write_output(ostream & out);
	void record_busstop_visit (Bustrip* trip, double enter_time); //!< creates a log-file for stop-related info
	static double calc_crowded_travel_time (double travel_time, int nr_riders, int nr_seats);
	
	void calculate_sum_output_stop_per_line(int line_id); //!< calculates for a single line that visits the stop (identified by line_id)
	//void calculate_sum_output_stop_per_line_and_sched_at(int line_id, stop_sched_arrtime_T stop_sched);  //!< Hend added 301016 prepare a historical data
	int calc_total_nr_waiting ();

	vector<double> get_ini_val_vars(vector<Start_trip> trips_to_handle); // Hend added 30/3/17 to get the initial values for the opt
	vector<double> get_left_constraints(vector<Start_trip> trips_to_handle); // Hend added 30/3/17 to get the left constraints for the opt variables
	vector<double> get_right_constraints(vector<Start_trip> trips_to_handle); // Hend added 30/3/17 to get the right constraints for the opt variables
	map<LineIdStopId, hist_set*, CompareLineStop>* history_summary_map_;//Hend added 30/10/16 to save historical data
	map<LineIdStopId, hist_set*, CompareLineStop>* stops_map_handle;    //Hend added 20/3/17 to save data that enters the optimization
	map<LineIdStopId, hist_demand*, CompareLineStop>* hist_demand_map_; //Hend added 30/10/16 to save historical data
	map<LineIdStopId, LineStationData*, CompareLineStop>* hist_dmnd_map_; //Hend added 14/4/17 to save historical demand per stop and line.
	map<LineIdStopId, list<pair<int,double>>, CompareLineStop>* transfer_to_my_line_; //Hend added 25/4/17 to know if transfer from any line to my line is 1
	//map<LineStation, double>* get_line_station_demand(map<LineStation, LineStationData*, CompareLineStation>* hist_dmnd_map_); //Hend added 14/4/17 to get sum of the demands per stop and line

// relevant only for demand format 2
	multi_rates multi_arrival_rates; //!< parameter lambda that defines the poission proccess of passengers arriving at the stop for each sequential stop

   
	//control case 10: historical data for each stop 
	pair<Busstop*, double> dwell_time;      //Hend added 2401716: occupancy of buses on each trip on the horizon
	pair<Busstop*, double> trip_occupancy;      //Hend added 2401716: occupancy of buses on each trip on the horizon
	pair<Busstop*, double> riding_time;    //Hend added 05/02/17: riding time of buses on each trip on the horizon
	pair<Busstop*, double> trip_tmp_boardings;   //Hend added 05/02/17: number of boardings of buses on each trip on the horizon without considering capacity
	pair<Busstop*, double> trip_tmp_trnsfr_boardings;
	pair<Busstop*, double> trip_nr_boardings;   //Hend added 05/02/17: number of boardings of buses on each trip on the horizon
	pair<Busstop*, double> trip_nr_alightings;  //Hend added 05/02/17: number of alightings of buses on each trip on the horizon
	pair<Busstop*, double> trip_nr_denied;	  //Hend added 05/02/17: number of denied boardings of buses on each trip on the horizon
	pair<Busstop*, double> holding_time;	  //Hend added 30/03/17: number of denied boardings of buses on each trip on the horizon
	pair<Busstop*, double> expected_arrival_time;  //Hend added 30/03/17: expected arrival time
	pair<Busstop*, double> expected_departure_time;  //Hend added 11/04/17: expected departure time

protected:
	int id;						//!< stop id
	string name;				//!< name of the bus stop "T-centralen"
	int link_id;				//!< link it is on, maybe later a pointer to the respective link if needed
    double position;		    //!< relative position from the upstream node of the link (beteen 0 to 1)
    double length;				//!< length of the busstop, determines how many buses can be served at the same time
    bool has_bay;				//!< TRUE if it has a bay so it has an extra dwell time
	bool can_overtake;			//!< 0 - can't overtake, 1 - can overtake freely; TRUE if it is possible for a bus to overtake another bus that stops in front of it (if FALSE - dwell time is subject to the exit time of a blocking bus)
    double min_DT;
    int rti;					//!< indicates the level of real-time information at this stop: 0 - none; 1 - for all lines stoping at each stop; 2 - for all lines stoping at all connected stop; 3 - for the entire network.

	double avaliable_length;	//!< length of the busstop minus occupied length
	double exit_time;
	double dwelltime;			//!< standard dwell time

	int nr_boarding;			//!< pass. boarding
	int nr_alighting;			//!< pass alighting 
	Random* random;
	
	vector <Busline*> lines;
	map <double,Bus*> expected_arrivals;					//!< booked arrivals of buses on the link on their way to the stop
	vector<pair<Bustrip*,double> > expected_bus_arrivals;	//!< booked arrivals of buses on the link on their way to the stop
	map <double,Bus*> buses_at_stop;						//!< buses currently visiting stop
	vector<pair<Bustrip*,double> > buses_currently_at_stop;	//!< buses currently visiting stop
	map <Busline*, pair<Bustrip*, double> > last_arrivals;	//!< contains the arrival time of the last bus from each line that stops at the stop (can result headways)
	map <Busline*, pair<Bustrip*, double> > last_departures; //!< contains the departure time of the last bus from each line that stops at the stop (can result headways)
	map <Busline*,bool> had_been_visited;					//!< indicates if this stop had been visited by a given line till now

	// relevant only for demand format 1
	map <Busline*, double> arrival_rates;		//!< parameter lambda that defines the poission proccess of passengers arriving at the stop
	map <Busline*, double> alighting_fractions; //!< parameter that defines the poission process of the alighting passengers 

	// relevant only for demand format 1 TD (format 10)
	map <Busline*, double> previous_arrival_rates;
	map <Busline*, double> previous_alighting_fractions; 
	map <Busline*, vector<double> > update_rates_times;		//!< contains the information about when there is a change in rates (but not the actual change)
	
	// relevant only for demand format 2
	multi_rates multi_nr_waiting;			//!< for demant format is from type 2. 

	// relevant for demand formats 1 & 2
	map <Busline*, int> nr_waiting;			//!< number of waiting passengers for each of the bus lines that stops at the stop

	// relevant only for demand format 3
	ODs_for_stop stop_as_origin;			//!< a map of all the OD's that this busstop is their origin 
	ODs_for_stop stop_as_destination;		//!< a map of all the OD's that this busstop is their destination
	bool is_origin;							//!< indicates if this busstop serves as an origin for some passenger demand
	bool is_destination;					//!< indicates if this busstop serves as an destination for some passenger demand
	map <Busline*, bool> real_time_info;	//!< indicates for each line if it has real-time info. at this stop

	// walking distances between stops (relevant only for demand format 3 and 4)
	map<Busstop*,double> distances;			//!< contains the distances [meters] from other bus stops

	// transfer synchronization
	vector<pair<Bustrip*, int> > trips_awaiting_transfers;	//!< David added 2016-05-30: contains trips that are currently waiting to synchronize transfers with a connecting trip, paired with the line ID of the connecting trip

	// output structures
	list <Busstop_Visit> output_stop_visits;			//!< list of output data for buses visiting stops
	map<int,Output_Summary_Stop_Line> output_summary;	//!< int value is line_id

	
};

typedef pair<Busline*,double> TD_single_pair;
typedef map<Busstop*, map<Busline*,double>> TD_demand;
//typedef map<Busline*, map<Busstop*,double>> stop_sched_arrtime_T; //!<Hend added 301016: a map for each stop and the schedule arrival time

// In dlib, the general purpose solvers optimize functions that take a column
// vector as input and return a double.  So here we make a typedef for a
// variable length column vector of doubles.  This is the type we will use to
// represent the input to our objective functions which we will be minimizing.

//typedef matrix<double, 0, 1> column_vector;


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
	int function_id;
	int dwell_time_function_form; 
	// 11 - Linear function of boarding and alighting
    // 12 - Linear function of boarding and alighting + non-linear crowding effect (Weidmann) 
    // 13 - Max (boarding, alighting) + non-linear crowding effect (Weidmann) 
    // 21 - TCRP(max doors with crowding, boarding from front door, alighting from both doors) + bay + stop capacity
	// 22 - TCRP(max doors with crowding, boarding from x doors, alighting from y doors) + bay + stop capacity

	double dwell_constant; 
	double boarding_coefficient;
	double alighting_cofficient;
	double dwell_std_error;

	// only for TCRP functions
	int number_boarding_doors;
	int number_alighting_doors;
	double share_alighting_front_door;
	double crowdedness_binary_factor; 

	// extra delays
	double bay_coefficient;
	double over_stop_capacity_coefficient; 
};

class hist_set
{
 public:
	hist_set ();
	hist_set(int line_id_, int trip_id_, int vehicle_id_, int stop_id_, double entering_time_, double sched_arr_time_, double dwell_time_, double lateness_, double exit_time_, double riding_time_, double riding_pass_time_, double time_since_arr_, double time_since_dep_, double nr_alighting_, double nr_boarding_, double occupancy_, double nr_waiting_, double total_waiting_time_, double holding_time_);
	~hist_set ();
	//void reset();

	int line_id;
	int trip_id;
	int vehicle_id;
	int stop_id;
	double entering_time;
	double sched_arr_time;
	double dwell_time;
	double lateness;
	double exit_time;
	double riding_time;
	double riding_pass_time;
	double time_since_arr;
	double time_since_dep;
	double nr_alighting;
	double nr_boarding;
	double occupancy;
	double nr_waiting;
	double total_waiting_time;
	double holding_time;
};
class hist_paths
{
public:
	hist_paths();

	hist_paths(int passenger_ID_, int origin_ID_, int destination_ID_, double start_time_, double total_walking_time_, double total_waiting_time_, double total_waiting_time_denied_, double total_IVT_, double total_IVT_crowding_, double end_time_, vector<int> stops_, vector<int> trips_);
	~hist_paths();

	int passenger_ID;
	int origin_ID;
	int destination_ID;
	double start_time;
	double total_walking_time;
	double total_waiting_time;
	double total_waiting_time_denied;
	double total_IVT;
	double total_IVT_crowding;
	double end_time;
	vector<int> stops;
	vector<int> trips;
};

class hist_demand
{
public:
	hist_demand();

	hist_demand(int stop_id_, int line_id_, double origin_d_, double destination_d_, double transfer_board_d_, double transfer_alight_d_);
	~hist_demand();

	int stop_id;
	int line_id;
	double origin_d;
	double destination_d;
	double transfer_board_d;
	double transfer_alight_d;
};

#endif //_BUSLINE
