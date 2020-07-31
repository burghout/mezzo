#ifndef PASSENGER_ROUTE
#define PASSENGER_ROUTE

#include "busline.h"
#include "parameters.h" 
#include "od_stops.h"
#include "Random.h"
#include "passenger.h"

class Busline;
class Busstop;
class ODstops;
class Passenger;

class Pass_path
{
	public:
	Pass_path ();
    Pass_path (int path_id, vector<vector<Busline*> > alt_lines_);
    Pass_path (int path_id, vector<vector<Busline*> > alt_lines_, vector <vector <Busstop*> > alt_transfer_stops_);
    Pass_path (int path_id, vector<vector<Busline*> > alt_lines_, vector <vector <Busstop*> > alt_transfer_stops_, vector<double> walking_distances_);
	~Pass_path ();
	void reset();

	// Gets and sets:
	int get_id () {return p_id;}
    vector <vector <Busline*> > get_alt_lines () const {return alt_lines;}
    vector <vector <Busstop*> > get_alt_transfer_stops () {return alt_transfer_stops;}
    vector <double> get_walking_distances () {return walking_distances;}
	int get_number_of_transfers () const {return number_of_transfers;}
    void set_alt_transfer_stops (vector <vector <Busstop*> > trans_stops) {alt_transfer_stops = trans_stops;}
	bool get_arriving_bus_rellevant () {return arriving_bus_rellevant;}
	void set_arriving_bus_rellevant (bool arriving_bus_rellevant_) {arriving_bus_rellevant = arriving_bus_rellevant_;}

	// Attributes of path alternative
	int find_number_of_transfers ();
    double calc_total_scheduled_in_vehicle_time (double time); //!< @note resets and sets IVT attribute used in other methods, used both for static dominancy rules in CSGM as well as dynamic dominancy rules in boarding decision (ODstops::check_if_path_is_dominated)
    double calc_total_in_vehicle_time (double time, Passenger* pass); //!< @note resets and sets IVT attribute used in other methods
	double calc_total_walking_distance ();
    double calc_total_waiting_time (double time, bool without_first_waiting, bool alighting_decision, double avg_walking_speed, Passenger* pass);
//	double calc_total_scheduled_waiting_time (double time, bool without_first_waiting);
    double calc_curr_leg_headway (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double time);
//	double calc_curr_leg_waiting_schedule (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double arriving_time);
    double calc_curr_leg_waiting_RTI (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double arriving_time);

	double calc_arriving_utility (double time, Passenger* pass);
    double calc_waiting_utility (vector <vector <Busstop*> >::iterator stop_iter, double time, bool alighting_decision, Passenger* pass);
    map<Busline*, bool> check_maybe_worthwhile_to_wait (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, bool dynamic_indicator); // returns false for lines which are not worthwhile to wait for in any case


    /** @ingroup DRT
        @{
    */
    
    bool check_all_flexible_lines(const vector<Busline*>& line_vec) const; //!< returns true if all lines in vector are flagged as flexible (i.e. dynamically scheduled or routed) and is non-empty
    bool check_any_flexible_lines() const; //!< returns true if ANY transit leg of this path is flexible, will e.g. return false if all line legs are fixed, and for walking only paths
    bool check_all_fixed_lines(const vector<Busline*>& line_vec) const; //!< returns true if all lines in vector are not flagged as flexible and is non-empty
    bool is_first_transit_leg_fixed() const; //!< returns true if the first set of lines in alt_lines are all fixed
    bool is_first_transit_leg_flexible() const; //!< returns true if the first set of lines in alt_lines are all flexible
    Busstop* get_first_transfer_stop() const; //!< returns the end stop of the first transit leg for this path if there are transfers. Returns false if there are no transfers in this path
    
    /**@}*/
protected:
    int p_id = -1;
    Random* random = nullptr;
    vector <vector <Busline*> > alt_lines;
    vector <double> IVT;
    vector <vector <Busstop*> > alt_transfer_stops;
    vector <double> walking_distances;
    int number_of_transfers = 0;
    double scheduled_in_vehicle_time = 0;
    double scheduled_headway = 0;
    bool arriving_bus_rellevant = false;
};

#endif
