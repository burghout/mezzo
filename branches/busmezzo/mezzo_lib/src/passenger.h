#ifndef _PASSENGER
#define _PASSENGER

#include "parameters.h"
#include "busline.h"
#include "od_stops.h"
#include "Random.h"
#include <qobject.h> //for signals and slots in passenger/control center connection
#include "controlstrategies.h" 

class Bustrip;
class Busline;
class Busstop;
class ODstops;
class ODzone;
struct SLL;
struct Request;
class Pass_path;

enum class TransitModeType { Null = 0, Fixed, Flexible }; //!< used mainly for transitmode decision
ostream& operator << (ostream& os, const TransitModeType& obj);
QString TransitModeType_to_QString(TransitModeType mode); //!< helper print function for debug output


enum class PassengerState
{
    Null = 0, //Null before the passenger has started their journey, and after their journey is completed
    Walking, //In the process of walking in between stops
    ArrivedToStop, //After Passenger::start (journey start) and after passenger_activity_at_stop (alighting at stop that is not final destination), but before connection decision
    WaitingForFixed, //After connection/mode decision waiting for fixed vehicles
    WaitingForFlex, //After connection/mode/dropoff decision waiting for flex
    WaitingForFixedDenied, //if waiting after experiencing at least one denied boarding
    WaitingForFlexDenied,
    OnBoard //After boarding a bus
};
QString PassengerState_to_QString(PassengerState state);
class Passenger : public QObject, public Action
{
    Q_OBJECT

public:
    Passenger(
        int		   pass_id,
        double	   start_time_,
        ODstops* OD_stop_,
        QObject* parent = nullptr
    );
    Passenger();
    virtual ~Passenger();
    void init();
    void init_zone(int pass_id, double start_time_, ODzone* origin_, ODzone* destination_);
    void reset();

    // Gets and sets:
    int			get_id() { return passenger_id; }
    double		get_start_time() { return start_time; }
    void		set_end_time(double end_time_) { end_time = end_time_; }
    double		get_end_time() { return end_time; }
    ODstops* get_OD_stop() { return OD_stop; }
    ODzone* get_o_zone() { return o_zone; }
    ODzone* get_d_zone() { return d_zone; }

    void		set_origin_walking_distances(map<Busstop*, double, ptr_less<Busstop*>> origin_walking_distances_) { origin_walking_distances = origin_walking_distances_; }
    void		set_destination_walking_distances(map<Busstop*, double, ptr_less<Busstop*>> destination_walking_distances_) { destination_walking_distances = destination_walking_distances_; }
    double		get_origin_walking_distance(Busstop* stop) { return origin_walking_distances[stop]; }
    double		get_destination_walking_distance(Busstop* stop) { return destination_walking_distances[stop]; }

    Busstop* get_original_origin() { return original_origin; }
    int			get_nr_boardings() { return nr_boardings; }
    vector <pair<Busstop*, double> > get_chosen_path_stops() { return selected_path_stops; }
    void		set_ODstop(ODstops* ODstop_) { OD_stop = ODstop_; }
    // bool get_already_walked () {return already_walked;}
    // void set_already_walked (bool already_walked_) {already_walked = already_walked_;}
    bool	get_this_is_the_last_stop() { return this_is_the_last_stop; }
    bool	get_pass_RTI_network_level() { return RTI_network_level; }
    void	set_arrival_time_at_stop(double arrival_time) { arrival_time_at_stop = arrival_time; }
    double	get_arrival_time_at_stop() { return arrival_time_at_stop; }
    void	set_memory_projected_RTI(Busstop* stop, Busline* line, double projected_RTI);
    double	get_memory_projected_RTI(Busstop* stop, Busline* line);
    void	set_pass_sitting(bool sits) { sitting = sits; }
    bool	get_pass_sitting() { return sitting; }
    double	get_latest_boarding_time() { return (selected_path_trips.back().second); }
    vector <pair<Busstop*, double> > get_selected_path_stops() { return selected_path_stops; }

    bool execute(Eventlist* eventlist, double time); //!< called every time passengers choose to walk to another stop (origin/transfer), puts the passenger at the waiting list at the right timing
    void walk(double time);
    void start(Eventlist* eventlist, double time);

    // Passenger decision processes
    bool	 make_boarding_decision(Bustrip* arriving_bus, double time);	//!< boarding decision making 
    Busstop* make_alighting_decision(Bustrip* boarding_bus, double time);	//!< alighting decision making 
    Busstop* make_connection_decision(double time);						//!< connection link decision (walking between stops)

/** @ingroup DRT
    @{
*/
    TransitModeType make_transitmode_decision(Busstop* pickup_stop, double time); //!< choice of fixed or flexible mode for the nextmost transit leg of a trip. Follows immediately after a connection decision
    Busstop* make_dropoff_decision(Busstop* pickup_stop, double time); //!< if chosen mode is flexible, choice of which drop-off stop (transfer or final destination) to send a request for. Follows immediately after transitmode decision
/** @} */

    // Demand in terms of zones
    map<Busstop*, double, ptr_less<Busstop*>> sample_walking_distances(ODzone* zone);
    Busstop* make_first_stop_decision(double time); // deciding at which stop to initiate the trip
    double calc_boarding_probability_zone(Busline* arriving_bus, Busstop* o_stop, double time);
    Busstop* make_alighting_decision_zone(Bustrip* boarding_bus, double time);
    Busstop* make_connection_decision_zone(double time);
    bool stop_is_in_d_zone(Busstop* stop);

    // output-related 
    void write_selected_path(ostream& out);
    void write_passenger_trajectory(ostream& out);

    void add_to_selected_path_trips(pair<Bustrip*, double> trip_time) { selected_path_trips.push_back(trip_time); }
    Bustrip* get_last_selected_path_trip() const; //!< returns the latest trip boarded by the passenger, returns nullptr if none exists
    void add_to_selected_path_stop(pair<Busstop*, double> stop_time) { selected_path_stops.push_back(stop_time); }
    void add_to_experienced_crowding_levels(pair<double, double> riding_coeff) { experienced_crowding_levels.push_back(riding_coeff); }
    void add_to_denied_boarding(pair<Busstop*, double> denied_time) { waiting_time_due_denied_boarding.push_back(denied_time); }
    void increment_nr_denied_boardings(); //!< increment nr_denied_boardings counter
    int get_nr_denied_boardings(); //!< returns the number of times a passenger missed a bus due to denied boarding
    bool check_selected_path_trips_empty() { return selected_path_trips.empty(); }
    int get_selected_path_last_line_id();
    int get_last_denied_boarding_stop_id();
    double get_GTC() { return total_GTC; }
    void set_GTC(double pass_GTC) { total_GTC = pass_GTC; }
    bool empty_denied_boarding();
    void remove_last_trip_selected_path_trips() { selected_path_trips.pop_back(); }
    void record_waiting_experience(Bustrip* arriving_bus, double time);
    //void set_left_behind(double time);

    //Day2Day
    void set_anticipated_waiting_time(Busstop* stop, Busline* line, double anticipated_WT);
    void set_alpha_RTI(Busstop* stop, Busline* line, double alpha);
    void set_alpha_exp(Busstop* stop, Busline* line, double alpha);
    double get_anticipated_waiting_time(Busstop* stop, Busline* line);
    double get_alpha_RTI(Busstop* stop, Busline* line);
    double get_alpha_exp(Busstop* stop, Busline* line);
    bool any_previous_exp_ODSL(Busstop* stop, Busline* line);
    void set_anticipated_ivtt(Busstop* stop, Busline* line, Busstop* leg, double anticipated_ivt);
    double get_anticipated_ivtt(Busstop* stop, Busline* line, Busstop* leg);
    void set_ivtt_alpha_exp(Busstop* stop, Busline* line, Busstop* leg, double alpha);
    double get_ivtt_alpha_exp(Busstop* stop, Busline* line, Busstop* leg);
    void set_ivtt_alpha_exp_crowding(Busstop* stop, Busline* line, Busstop* leg, double alpha);
    double get_ivtt_alpha_exp_crowding(Busstop* stop, Busline* line, Busstop* leg);
    void set_ivtt_acc_exp(Busstop* stop, Busline* line, Busstop* leg, double ivt_acc_exp);
    double get_ivtt_acc_exp(Busstop* stop, Busline* line, Busstop* leg);
    bool any_previous_exp_ivtt(Busstop* stop, Busline* line, Busstop* leg);

    double calc_total_waiting_time();
    double calc_total_IVT();
    double calc_total_walking_time();
    double calc_IVT_crowding();
    double calc_total_waiting_time_due_to_denied_boarding();
    int get_nr_transfers();
    bool line_is_rejected(int id); //If the passenger has rejected line with id the function returns true

    //walking time
    double get_walking_time(Busstop* target_stop, double curr_time);

    /** @ingroup DRT
        @{
    */
    //Controlcenter
    void set_state(PassengerState newstate, double time);
    PassengerState get_state() const { return state_; }

    void print_state();
    Request* createRequest(Busstop* origin_stop, Busstop* dest_stop, int load = 1, double desired_departure_time = -1, double current_time = -1); /*!< creates a request for this passenger between origin stop and destination stop
                                                                                                   with a given load and a given time. If origin stop and destination stop are not within the same service area
                                                                                                   then attempts to create a request to travel to the first transfer stop found within the service area instead.
                                                                                                   Returns TRUE and the request if successful, FALSE and an invalid request otherwise (Note: uses protected members of Passenger) */

protected:
    TransitModeType chosen_mode_ = TransitModeType::Null; /**!< Null if no choice has been made yet, otherwise the result of a mode choice decision. Travelers do not currently re-make a choice of fixed or flexible mode and are commited to this mode for the next leg of their trip once this choice is made. */
    Request* curr_request_ = nullptr; //!< should point to the current request of the traveler, nullptr if none is active
    int total_vehicle_meters_traveled = 0; //!< total meters traveled using a transit vehicle (so basically everything but walking right now)
    int total_flexvehicle_meters_traveled = 0; //!< total meters traveled using a transit vehicle that was dynamically scheduled and/or routed
    int total_fixvehicle_meters_traveled = 0; //!< total meters traveled using a transit vehicle that followed a fixed schedule and route
    PassengerState state_ = PassengerState::Null;

    int num_drt_mode_choice = 0;
    int num_fix_mode_choice = 0;
    int num_null_mode_choice = 0;

public:
    map<ODstops*, map<Pass_path*, double> > temp_connection_path_utilities; //!< cached exp(path utilities) calculated for a given connection/transitmode/dropoff decision. Cleared at the beginning of each make_connection_decision call and filled via calls from this method
    void set_chosen_mode(TransitModeType chosen_mode) { chosen_mode_ = chosen_mode; }
    TransitModeType get_chosen_mode() { return chosen_mode_; }
    void set_curr_request(Request* curr_request) { curr_request_ = curr_request; }
    Request* get_curr_request() { return curr_request_; }
    bool is_flexible_user() { return chosen_mode_ == TransitModeType::Flexible; }
    vector<Pass_path*> get_first_leg_flexible_paths(const vector<Pass_path*>& path_set) const; //!< returns all paths in path_set that have a flexible first transit leg (that a traveler would need to send a request for to ride with)
    vector<Pass_path*> get_first_leg_fixed_paths(const vector<Pass_path*>& path_set) const; //!< returns all paths in path_set that have a fixed first transit leg
    void update_vehicle_meters_traveled(int meters, bool is_flextrip); //!< updates total, flex and fixed vehicle meters traveled to calculate PKT in output
    double get_total_vkt();
    double get_total_drt_vkt();
    double get_total_fix_vkt();

    int get_num_drt_mode_choice(); //!< number of times passenger chose 'drt' in a mode choice
    int get_num_fix_mode_choice(); //!< number of times passenger chose 'fix' in a mode choice
    void increment_mode_choice_counter(TransitModeType chosen_mode);
signals:
    void sendRequest(Request* req, double time); //!< signal to send Request message to Controlcenter along with time in which signal is sent
    void stateChanged(Passenger* pass, PassengerState oldstate, PassengerState newstate, double time);
/**@}*/

protected:
    int passenger_id = -1;
    double start_time = 0;
    double end_time = 0;
    double total_waiting_time = 0;
    double toal_IVT = 0;
    double total_IVT_crowding = 0;
    double total_walking_time = 0;
    double total_GTC = 0;
    Busstop* original_origin = nullptr;
    ODstops* OD_stop = nullptr;
    bool boarding_decision = false;
    Random* random = nullptr;
    bool already_walked = false;
    bool sitting = false;		//!< 0 - sits; 1 - stands
    int nr_boardings = 0;	//!< counts the number of times pass boarded a vehicle @note actually this seems to be more like a counter for each time a make_boarding_decision() call returned true, if e.g. denied boarding nr_boardings increases regardless...
    int nr_denied_boardings = 0; //!< counts the number of times this passenger has been denied boarding a vehicle
    vector <pair<Busstop*, double> > selected_path_stops;				 //!< stops and corresponding arrival times
    vector <pair<Bustrip*, double> > selected_path_trips;				 //!< trips and corresponding boarding times
    vector <pair<double, double> > experienced_crowding_levels;		 //!< IVT and corresponding crowding levels (route segment level)
    vector <pair<Busstop*, double> > waiting_time_due_denied_boarding; //!< stops at which the pass. experienced denied boarding and the corresponding time at which it was experienced
    vector<int> rejected_lines; //!< To keep track of the lines that the passenger chose not to board earlier, these should not be regarded next time
    bool RTI_network_level = false;
    double arrival_time_at_stop = 0;
    //double first_bus_arrival_time; //Used to calculate weighted waiting time in case the first bus is full
    //bool left_behind_before;
    map<pair<Busstop*, Busline*>, double, pair_less<pair <Busstop*, Busline*> >> memory_projected_RTI;
    double AWT_first_leg_boarding = 0;

    // relevant only in case of day2day procedures
    map<pair<Busstop*, Busline*>, double, pair_less<pair <Busstop*, Busline*> >> anticipated_waiting_time;
    map<pair<Busstop*, Busline*>, double, pair_less<pair <Busstop*, Busline*> >> alpha_RTI;
    map<pair<Busstop*, Busline*>, double, pair_less<pair <Busstop*, Busline*> >> alpha_exp;
    map<SLL, double> anticipated_ivtt;
    map<SLL, double> ivtt_alpha_exp;
    map<SLL, double> ivtt_alpha_exp_crowding;
    map<SLL, double> ivtt_acc_exp;

    // relevant only for OD in terms od zones
    ODzone* o_zone = nullptr;
    ODzone* d_zone = nullptr;
    map<Busstop*, double, ptr_less<Busstop*>> origin_walking_distances;
    map<Busstop*, double, ptr_less<Busstop*>> destination_walking_distances;
    bool this_is_the_last_stop = false;
};

class PassengerRecycler
{
public:
    ~PassengerRecycler();
    Passenger* newPassenger()
    {
        if (pass_recycled.empty())
            return new Passenger();
        else
        {
            Passenger* pass = pass_recycled.front();
            pass_recycled.pop_front();
            return pass;
        }
    }

    void addPassenger(Passenger* pass) { pass_recycled.push_back(pass); }

private:
    list <Passenger*> pass_recycled;
};
//static PassengerRecycler recycler;
extern PassengerRecycler pass_recycler;

#endif //_Passenger
