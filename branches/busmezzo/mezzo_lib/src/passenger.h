#ifndef _PASSENGER
#define _PASSENGER

#include "parameters.h"
#include "busline.h"
#include "od_stops.h"
#include "Random.h"

class Bustrip;
class Busline;
class Busstop;
class ODstops;
class ODzone;
struct SLL;
struct SLLC;

class Passenger : public Action
{
public:
	Passenger (
		int		   pass_id,
		double	   start_time_,
		ODstops*   OD_stop_
	);
	Passenger(
		int		   pass_id,
		double	   start_time_,
		ODstops*   OD_stop_,
		int		   orig_section_,
		int		   dest_section_
	);
	Passenger ();
    virtual ~Passenger ();
	void init ();
	void init_zone (int pass_id, double start_time_, ODzone* origin_, ODzone* destination_);
	void reset();
	
	// Gets and sets:
	int			get_id () {return passenger_id;}
	double		get_start_time () {return start_time;}
	void		set_end_time (double end_time_) {end_time = end_time_;}
	double		get_end_time () {return end_time;}
	ODstops*	get_OD_stop () {return OD_stop;}
	ODzone*		get_o_zone () {return o_zone;}
	ODzone*		get_d_zone () {return d_zone;}
	
	//Melina
	void		set_origin_walking_distances (map<Busstop*,double> origin_walking_distances_) {origin_walking_distances = origin_walking_distances_;}
	//void		set_origin_walking_distances (map<pair<Busstop*,int>, double> origin_walking_distances_) {origin_walking_distances = origin_walking_distances_;}
	void		set_destination_walking_distances (map<Busstop*,double> destination_walking_distances_) {destination_walking_distances = destination_walking_distances_;}
	double		get_origin_walking_distance (Busstop* stop) {return origin_walking_distances[stop];}
	double		get_destination_walking_distance (Busstop* stop) {return destination_walking_distances[stop];}
	
	Busstop*	get_original_origin () {return original_origin;}
	// Erik 18-09-25: 
	// int get_original_section() {return_original_section;}
	int			get_nr_boardings () {return nr_boardings;}
    vector <pair<Busstop*,double> > get_chosen_path_stops () {return selected_path_stops;}
	void		set_ODstop (ODstops* ODstop_) {OD_stop = ODstop_;}
	// bool get_already_walked () {return already_walked;}
	// void set_already_walked (bool already_walked_) {already_walked = already_walked_;}
	bool	get_this_is_the_last_stop () {return this_is_the_last_stop;}
	bool	get_pass_RTI_network_level () {return RTI_network_level;}
	void	set_arrival_time_at_stop (double arrival_time) {arrival_time_at_stop = arrival_time;}
	double	get_arrival_time_at_stop () {return arrival_time_at_stop;}
	void	set_memory_projected_RTI (Busstop* stop, Busline* line, double projected_RTI);
	double	get_memory_projected_RTI (Busstop* stop, Busline* line);
	void	set_pass_sitting (bool sits) {sitting = sits;}
	bool	get_pass_sitting () {return sitting;}
	// /* Erik 18-09-15
	int		get_pass_car () {return car;}	
	void	set_pass_car(int car_) { car = car_; }
	int		get_pass_section() { return section; }
	void	set_pass_section(int section_) { section = section_; }
	// */
	double	get_latest_boarding_time () {return (selected_path_trips.back().second);}
    vector <pair<Busstop*,double> > get_selected_path_stops() {return selected_path_stops;}
	//vector <pair<Busstop*, int> > get_selected_car() { return selected_car; } //Melina

	bool execute(Eventlist* eventlist, double time); //!< called every time passengers choose to walk to another stop (origin/transfer), puts the passenger at the waiting list at the right timing
	void walk(double time);
	void start(Eventlist* eventlist);

	// Passenger decision processes
	bool	 make_boarding_decision (Bustrip* arriving_bus, double time);	//!< boarding decision making 
	Busstop* make_alighting_decision (Bustrip* boarding_bus, double time);	//!< alighting decision making 
	//Busstop* make_connection_decision (double time);						//!< connection link decision (walking between stops)
	pair<Busstop*,int> make_connection_decision_2(double time);						// Erik 18-09-27

	// Demand in terms of zones
	map<Busstop*,double> sample_walking_distances (ODzone* zone);
	Busstop* make_first_stop_decision (double time); // deciding at which stop to initiate the trip
	double calc_boarding_probability_zone (Busline* arriving_bus, Busstop* o_stop, double time);
	Busstop* make_alighting_decision_zone (Bustrip* boarding_bus, double time); 
	Busstop* make_connection_decision_zone (double time);
	bool stop_is_in_d_zone (Busstop* stop);
	
	// output-related 
	void write_selected_path(ostream& out);
	void write_passenger_trajectory(ostream& out);
	void add_to_selected_path_trips (pair<Bustrip*,double> trip_time) {selected_path_trips.push_back(trip_time);}
	void add_to_selected_path_stop (pair<Busstop*,double> stop_time) {selected_path_stops.push_back(stop_time);}
	//Erik 18-09-16
	void add_to_selected_path_sections(pair<int, double> stop_section) { selected_path_sections.push_back(stop_section); } //Erik 18-09-16
	void add_to_selected_path_cars(pair<int, double> stop_car) { selected_path_cars.push_back(stop_car); } //Erik 18-09-16
	void add_to_selected_car (pair<Bustrip*, int> stop_car) { selected_car.push_back(stop_car); } //Melina
	void add_to_experienced_crowding_levels(pair<double,double> riding_coeff) {experienced_crowding_levels.push_back(riding_coeff);};
	void add_to_denied_boarding (pair<Busstop*,double> denied_time) {waiting_time_due_denied_boarding.push_back(denied_time);}
	bool check_selected_path_trips_empty () {return selected_path_trips.empty();}
	int get_selected_path_last_line_id ();
	int get_last_denied_boarding_stop_id ();
	double get_GTC() { return total_GTC; }
	void set_GTC (double pass_GTC) { total_GTC = pass_GTC; }
	bool empty_denied_boarding ();
	void remove_last_trip_selected_path_trips () {selected_path_trips.pop_back();}
	void record_waiting_experience(Bustrip* arriving_bus, double time);
	//void set_left_behind(double time);

	//Day2Day
	void set_anticipated_waiting_time (Busstop* stop, Busline* line, double anticipated_WT);
	void set_alpha_RTI (Busstop* stop, Busline* line, double alpha); 
	void set_alpha_exp (Busstop* stop, Busline* line, double alpha); 
	double get_anticipated_waiting_time (Busstop* stop, Busline* line);
	double get_alpha_RTI (Busstop* stop, Busline* line);
	double get_alpha_exp (Busstop* stop, Busline* line);
	bool any_previous_exp_ODSL (Busstop* stop, Busline* line);
	//void set_anticipated_ivtt (Busstop* stop, Busline* line, Busstop* leg, double anticipated_ivt);
	//double get_anticipated_ivtt (Busstop* stop, Busline* line, Busstop* leg);
	void set_anticipated_ivtt(Busstop* stop, Busline* line, Busstop* leg, int car, double anticipated_ivt);
	double get_anticipated_ivtt(Busstop* stop, Busline* line, Busstop* leg, int car);
	//double get_ivtt_alpha_exp (Busstop* stop, Busline* line, Busstop* leg);
	//bool any_previous_exp_ivtt (Busstop* stop, Busline* line, Busstop* leg);
	double get_ivtt_alpha_exp(Busstop* stop, Busline* line, Busstop* leg, int car);
	bool any_previous_exp_ivtt(Busstop* stop, Busline* line, Busstop* leg, int car);
	void set_AWT_first_leg_boarding(Busstop* stop, Busline* line);

	double calc_total_waiting_time();
	double calc_total_IVT();
	double calc_total_walking_time();
	double calc_IVT_crowding();
	double calc_total_waiting_time_due_to_denied_boarding();
	bool line_is_rejected(int id); //If the passenger has rejected line with id the function returns true
    
    
    //walking time
    double get_walking_time(Busstop*,double);
	double get_walking_time(Busstop*, int, double); // Erik 18-10-08

	double get_walking_distance(Busstop*, int);

protected:
	int passenger_id;
	double start_time;
	double end_time;
	double total_waiting_time;
	double toal_IVT;
	double total_IVT_crowding;
	double total_walking_time;
	double total_GTC;
	Busstop* original_origin;
	Busstop* stop;
	Bustrip* trip;
	ODstops* OD_stop;
	// /* Erik 18-09-26
	int orig_section;
	int dest_section;
	// */
	bool boarding_decision;
	Random* random;
	bool already_walked;
	bool sitting;		//!< 0 - sits; 1 - stands
	int car; // chosen vehicle car Erik 18-09-16
	int section; // chosen platform section Erik 18-09-16
	int nr_boardings;	//!< counts the number of times pass boarded a vehicle
    vector <pair<Busstop*,double> > selected_path_stops;				//!< stops and corresponding arrival times
    vector <pair<Bustrip*,double> > selected_path_trips;				//!< trips and corresponding boarding times
	vector <pair<int, double> > selected_path_sections;					//!< platform sections and corresponding arrival times
	vector <pair<int, double> > selected_path_cars;						//!< vehicle cars and corresponding boarding times //Erik 18-09-25
	vector <pair <Bustrip*, int> > selected_car;						//!< trips and corresponding car units //Melina
    vector <pair<double,double> > experienced_crowding_levels;			//!< IVT and corresponding crowding levels (route segment level)
	//map<pair<Busstop*, int>, passengers > passengers_on_board;
    vector <pair<Busstop*,double> > waiting_time_due_denied_boarding; //!< stops at which the pass. experienced denied boarding and the corresponding time at which it was experienced
	vector<int> rejected_lines; //!< To keep track of the lines that the passenger chose not to board earlier, these should not be regarded next time
	bool RTI_network_level;
	double arrival_time_at_stop;
	//double first_bus_arrival_time; //Used to calculate weighted waiting time in case the first bus is full
	//bool left_behind_before;
	map<pair<Busstop*, Busline*>,double> memory_projected_RTI; 
	double AWT_first_leg_boarding;
	
	// relevant only in case of day2day procedures
	map<pair<Busstop*, Busline*>,double> anticipated_waiting_time;
	map<pair<Busstop*, Busline*>,double> alpha_RTI;
	map<pair<Busstop*, Busline*>,double> alpha_exp;
	//map<SLL, double> anticipated_ivtt;
	//map<SLL, double> ivtt_alpha_exp;
	map<SLLC, double> anticipated_ivtt; // Erik 18-11-26
	map<SLLC, double> ivtt_alpha_exp; // Erik 18-11-26

	// relevant only for OD in terms od zones
	ODzone* o_zone;
	ODzone* d_zone;
	map<Busstop*,double> origin_walking_distances;
	//map<pair<Busstop*,int>, double> origin_walking_distances; //Melina
	map<Busstop*,double> destination_walking_distances;
	//map<pair<Busstop*,int>, double> destination_walking_distances;
	bool this_is_the_last_stop;
};

class PassengerRecycler
{
 public:
 	~PassengerRecycler();
	Passenger* newPassenger() {	 	if (pass_recycled.empty())
     								return new Passenger();
     								else
     								{
     									Passenger* pass=pass_recycled.front();
     									pass_recycled.pop_front();
     									return pass;
     								}	
     							}
							
     void addPassenger(Passenger* pass){pass_recycled.push_back(pass);}
 
private:
	list <Passenger*> pass_recycled;
};
//static PassengerRecycler recycler;
extern PassengerRecycler pass_recycler;

#endif //_Passenger
