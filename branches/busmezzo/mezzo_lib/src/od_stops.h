#ifndef _ODSTOPS
#define _ODSTOPS

#include "parameters.h"
#include "od.h"
#include "busline.h"
#include "pass_route.h"
#include "passenger.h"
#include "Random.h"
#include "MMath.h" 

class Busline;
class Busstop;
class Bustrip;
class Passenger;
class Pass_path;
class ODzone;
enum class TransitModeType;

typedef vector <Passenger*> passengers;

class Pass_boarding_decision // container object holding output data for passenger boarding decision
{
public:
	Pass_boarding_decision(
		int pass_id_,
		int original_origin_,
		int destination_stop_,
		int line_id_,
		int trip_id_,
		int stop_id_,
		double time_,
		double generation_time_,
		double boarding_probability_,
		bool boarding_,
		double u_boarding_,
		double u_staying_
		): pass_id(pass_id_),original_origin(original_origin_),destination_stop(destination_stop_),line_id(line_id_),trip_id(trip_id_), stop_id(stop_id_),time(time_),generation_time(generation_time_),boarding_probability(boarding_probability_),boarding(boarding_),u_boarding(u_boarding_),u_staying(u_staying_){}

	virtual ~Pass_boarding_decision(); //!< destructor

	void write(ostream& out){
		out << std::fixed;
		out.precision(5);
		out << pass_id << '\t'
			<< original_origin << '\t'
			<< destination_stop << '\t'
			<< line_id << '\t'
			<< trip_id << '\t'
			<< stop_id<< '\t'
			<< time << '\t'
			<< generation_time << '\t'
			<< boarding_probability << '\t'
			<< boarding << '\t'
			<< u_boarding << '\t'
			<< u_staying <<'\t'
			<< endl;
	}

	void reset(){
		pass_id = 0; 
		original_origin = 0;
		destination_stop = 0;
		line_id = 0;
		trip_id = 0;
		stop_id = 0;
		time = 0;
		generation_time = 0;
		boarding = 0;
		u_boarding = 0;
		u_staying = 0;
	}

	int pass_id;
	int original_origin;
	int destination_stop;
	int line_id;
	int trip_id;
	int stop_id;
	double time;
	double generation_time;
	double boarding_probability;
	bool boarding;
	double u_boarding;
	double u_staying;
};

class Pass_alighting_decision // container object holding output data for passenger alighting decision
{
public:
	Pass_alighting_decision(
		int pass_id_,
		int original_origin_,
		int destination_stop_,
		int line_id_,
		int trip_id_,
		int stop_id_,
		double time_,
		double generation_time_,
		int chosen_alighting_stop_,
        map<Busstop*,pair<double,double> > alighting_MNL_
	): pass_id(pass_id_),original_origin(original_origin_),destination_stop(destination_stop_),line_id(line_id_),trip_id(trip_id_), stop_id(stop_id_),time(time_),generation_time(generation_time_),chosen_alighting_stop(chosen_alighting_stop_),alighting_MNL(alighting_MNL_){}

	virtual ~Pass_alighting_decision(); //!< destructor
	
	void write(ostream& out);
	
	void reset(){
		pass_id = 0;
		original_origin = 0;
		destination_stop = 0;
		line_id = 0;
		trip_id = 0;
		stop_id = 0;
		time = 0;
		generation_time = 0;
	}
	
	int pass_id;
	int original_origin;
	int destination_stop;
	int line_id;
	int trip_id;
	int stop_id;
	double time;
	double generation_time;
	int chosen_alighting_stop;
    map<Busstop*,pair<double,double> > alighting_MNL;
};

class Pass_transitmode_decision // container object holding output data for passenger connection decision
{
public:
	Pass_transitmode_decision(
		int pass_id_,
		int original_origin_,
		int destination_stop_,
		int pickupstop_id_,
		double time_,
		double generation_time_,
		TransitModeType chosen_transitmode_,
		map<TransitModeType, pair<double,double> > mode_MNL_
	) : pass_id(pass_id_), original_origin(original_origin_), destination_stop(destination_stop_), pickupstop_id(pickupstop_id_), time(time_), generation_time(generation_time_), chosen_transitmode(chosen_transitmode_), mode_MNL(mode_MNL_) {}

	virtual ~Pass_transitmode_decision() = default; //!< destructor

	void write(ostream& out);

	void reset() {
		pass_id = 0;
		original_origin = 0;
		destination_stop = 0;
		pickupstop_id = 0;
		time = 0;
		generation_time = 0;
	}

	int pass_id;
	int original_origin; //original origin
	int destination_stop; //final destination
	int pickupstop_id; //current stop, or connection/pickup stop that the mode choice decision was made for
	double time; //simtime of decision
	double generation_time; //generation time of pass
	TransitModeType chosen_transitmode; //chosen mode
	map < TransitModeType, pair<double, double> > mode_MNL; //mode alternative -> (utility, probability)
};

class Pass_dropoff_decision // container object holding output data for passenger connection decision
{
public:
	Pass_dropoff_decision(
		int pass_id_,
		int original_origin_,
		int destination_stop_,
		int pickupstop_id_,
		double time_,
		double generation_time_,
		int chosen_dropoff_stop_,
		map <Busstop*, pair<double, double> > dropoff_MNL_
	) : pass_id(pass_id_), original_origin(original_origin_), destination_stop(destination_stop_), pickupstop_id(pickupstop_id_), time(time_), generation_time(generation_time_), chosen_dropoff_stop(chosen_dropoff_stop_), dropoff_MNL(dropoff_MNL_) {}

	virtual ~Pass_dropoff_decision() = default; //!< destructor

	void write(ostream& out);

	void reset() {
		pass_id = 0;
		original_origin = 0;
		destination_stop = 0;
		pickupstop_id = 0;
		time = 0;
		generation_time = 0;
	}

	int pass_id;
	int original_origin; //original origin
	int destination_stop; //final destination
	int pickupstop_id; //current stop
	double time; //simtime of decision
	double generation_time; //generation time of pass
	int chosen_dropoff_stop; //chosen dropoff stop id
	map < Busstop*, pair<double, double> > dropoff_MNL; //dropoff stop alternative -> (utility, probability)
};

class Pass_connection_decision // container object holding output data for passenger connection decision
{
public:
	Pass_connection_decision(
		int pass_id_, 
		int original_origin_, 
		int destination_stop_, 
		int stop_id_, 
		double time_, 
		double generation_time_, 
		int chosen_connection_stop_, 
        map<Busstop*,pair<double,double> > connecting_MNL_
	):	pass_id(pass_id_), original_origin(original_origin_), destination_stop(destination_stop_), stop_id(stop_id_),time(time_), generation_time(generation_time_), chosen_connection_stop(chosen_connection_stop_), connecting_MNL(connecting_MNL_) {}
	
	virtual ~Pass_connection_decision(); //!< destructor
	
	void write (ostream& out);
	
	void reset(){
		pass_id = 0; 
		original_origin = 0; 
		destination_stop = 0; 
		stop_id = 0; 
		time = 0;
		generation_time = 0;
	}
	
	int pass_id;
	int original_origin;
	int destination_stop;
	int stop_id;
	double time;
	double generation_time;
	int chosen_connection_stop;
    map<Busstop*,pair<double,double> > connecting_MNL;
};

class Pass_waiting_experience // container object holding output data for passenger waiting experience
{
public:
    Pass_waiting_experience(
        int pass_id_,
        int original_origin_,
        int destination_stop_,
        int line_id_,
        int trip_id_,
        int stop_id_,
        double time_,
        double generation_time_,
        double wt_pk_,
        bool RTI_level_available_,
        double wt_rti_,
        double wt_exp_,
        double wt_acc_exp_,
        double wt_anticip_,
        double alpha_pk_,
        double alpha_rti_,
        double alpha_exp_,
        int nr_missed_
    ) : pass_id(pass_id_), original_origin(original_origin_), destination_stop(destination_stop_),
        line_id(line_id_), trip_id(trip_id_), stop_id(stop_id_), time(time_), generation_time(generation_time_),
        wt_pk(wt_pk_), RTI_level_available(RTI_level_available_), wt_rti(wt_rti_), wt_exp(wt_exp_), wt_acc_exp(wt_acc_exp_), wt_anticip(wt_anticip_),
        alpha_pk(alpha_pk_), alpha_rti(alpha_rti_), alpha_exp(alpha_exp_), nr_missed(nr_missed_) {}
    virtual ~Pass_waiting_experience(); //!< destructor
	
	void write(ostream& out){
		out << std::fixed;
		out.precision(5);
		out << pass_id << '\t' 
            << original_origin << '\t'
            << destination_stop << '\t'
            << line_id << '\t'
            << trip_id << '\t'
            << stop_id << '\t'
            << time << '\t'
            << generation_time << '\t'
            << wt_pk << '\t'
            << RTI_level_available << '\t'
            << wt_rti << '\t'
            << wt_exp << '\t'
            << wt_acc_exp << '\t'
            << wt_anticip << '\t'
            << alpha_pk << '\t'
            << alpha_rti << '\t'
            << alpha_exp << '\t'
            << nr_missed << '\t'
            << endl;
	}
	
    void reset() {
        pass_id = 0;
        original_origin = 0;
        destination_stop = 0;
        line_id = 0;
        trip_id = 0;
        stop_id = 0;
        time = 0.0;
        generation_time = 0.0;
        wt_pk = 0.0;
        RTI_level_available = false;
        wt_rti = 0.0;
        wt_exp = 0.0;
        wt_acc_exp = 0.0;
        wt_anticip = 0.0;
        alpha_pk = 0.0;
        alpha_rti = 0.0;
        alpha_exp = 0.0;
        nr_missed = 0;
    }
	
	int pass_id;
	int original_origin;
	int destination_stop;
	int line_id;
	int trip_id;
	int stop_id;
	double time;
	double generation_time;
	double wt_pk; // anticipated waiting time based on prior knowledge (static information source e.g. schedule or exploration)
	bool RTI_level_available;
	double wt_rti; // anticipated waiting time based on real-time information
	double wt_exp; // actual waiting time experienced
	double wt_acc_exp; // anticipated waiting time based on accumulated experience
	double wt_anticip; // anticipated waiting time based on anticip_exp, rti, and pk information sources combined with credibility weights
	double alpha_pk; // credibility coeff of prior knowledge
	double alpha_rti; // credibility coeff of real-time information
	double alpha_exp; // credibility coeff of accumulated experience
	int nr_missed; //Nr of missed buses due to overcrowding
};

class Pass_onboard_experience
{
public:
	Pass_onboard_experience(
		int pass_id_, 
		int original_origin_, 
		int destination_stop_, 
		int line_id_, 
		int trip_id_, 
		int stop_id_, 
        int leg_id_,
        double ivt_pk_,
        pair<double, double> ivt_exp_,
        double ivt_acc_exp_,
        double ivt_anticip_,
        double ivt_alpha_exp_crowding_,
        double ivt_alpha_exp_,
        double ivt_alpha_pk_
	): pass_id(pass_id_),original_origin(original_origin_),destination_stop(destination_stop_),line_id(line_id_), trip_id(trip_id_), stop_id(stop_id_), leg_id(leg_id_),
    ivt_pk(ivt_pk_), ivt_exp(ivt_exp_), ivt_acc_exp(ivt_acc_exp_), ivt_anticip(ivt_anticip_),
    ivt_alpha_exp_crowding(ivt_alpha_exp_crowding_), ivt_alpha_exp(ivt_alpha_exp_), ivt_alpha_pk(ivt_alpha_pk_) {}
	
	void write (ostream& out){
		out << std::fixed;
		out.precision(5);
		out << pass_id << '\t' 
			<< original_origin << '\t' 
			<< destination_stop << '\t' 
			<< line_id << '\t'
			<< trip_id << '\t' 
			<< stop_id << '\t' 
			<< leg_id << '\t'
	        << ivt_pk << '\t' 
			<< ivt_exp.first << '\t'
		    << ivt_exp.first*ivt_exp.second << '\t';
		out.precision(2);
		out	<< ivt_exp.second << '\t'
		    << ivt_alpha_exp_crowding << '\t';
		out.precision(5);
		out << ivt_acc_exp << '\t'
		    << ivt_anticip << '\t'
		    << ivt_alpha_exp << '\t'
		    << ivt_alpha_pk
			<< endl;
	}
	
	int pass_id;
	int original_origin;
	int destination_stop;
	int line_id;
	int trip_id;
	int stop_id;
	int leg_id;
	double ivt_pk; // ivt based on static prior knowledge (e.g. schedules/exploration)
	pair<double, double> ivt_exp; // first = experienced ivt, second = crowding coefficient
	double ivt_acc_exp;
	double ivt_anticip;
    double ivt_alpha_exp_crowding;
    double ivt_alpha_exp;
	double ivt_alpha_pk;
	
	double crowding = 0;
};

struct SLL
{
	Busstop* stop;
	Busline* line;
	Busstop* leg;

	bool operator < (const SLL& rhs) const
	{
		if (stop != rhs.stop)
			return stop < rhs.stop;
		else if (line != rhs.line)
			return line < rhs.line;
		else
			return leg < rhs.leg;
	}
};

class ODstops : public Action
{
public:
	ODstops ();
	ODstops (Busstop* origin_stop_, Busstop* destination_stop_);
	ODstops (Busstop* origin_stop_, Busstop* destination_stop_, double arrival_rate_);
	~ODstops ();
	void reset ();
	void delete_passengers();
	
	//Gets and Sets:
	Busstop* get_origin() const {return origin_stop;}
	Busstop* get_destination () const {return destination_stop;}
	void set_arrival_rate (double rate_) {arrival_rate= rate_;}
	vector <Pass_path*> get_path_set () {return path_set;}
	void set_path_set (vector <Pass_path*> path_set_) {path_set = path_set_;}
	bool check_path_set ();

	bool is_active() { return active; } //!< check if ODstop has been initialized
	bool has_empirical_arrivals() { return empirical_arrivals; } //!< check if ODstop has empirical passenger arrivals being generated for it
	void set_empirical_arrivals(bool empirical_arrivals_) { empirical_arrivals = empirical_arrivals_; }

	/** @ingroup DRT
	 	@{
	 */
	// methods for filtering the path set of this OD stop pair
	vector<Pass_path*> get_flex_first_paths(); //!< returns all paths for this OD that have a flexible first transit leg (empty if none exist)
	vector<Pass_path*> get_fix_first_paths(); //!< returns all paths for this OD that have a fixed first transit leg (empty if none exist)
	vector<Pass_path*> get_nonflex_first_paths(); //!< returns all paths for this OD that do NOT have a flexible first transit leg (similar to get_fix_first_paths() but also includes walking only paths)
	vector<Pass_path*> get_nonflex_paths(); //!< returns all paths for this OD that do not contain a flexible transit leg (including walk only paths)
	/**@}*/

	double get_arrivalrate () {return arrival_rate;}
	vector<Passenger*> get_waiting_passengers() {return waiting_passengers;}
	int get_min_transfers () {return min_transfers;}
	void set_waiting_passengers(passengers passengers_) {waiting_passengers = passengers_;}
	void set_origin (Busstop* origin_stop_) {origin_stop=origin_stop_;}
	void set_destination (Busstop* destination_stop_) {destination_stop=destination_stop_;}
	void set_min_transfers (int min_transfers_) {min_transfers = min_transfers_;}
    map <Passenger*, list<Pass_boarding_decision>, ptr_less<Passenger*> > get_boarding_output() { return output_pass_boarding_decision; }
    map <Passenger*, list<Pass_alighting_decision>, ptr_less<Passenger*> > get_alighting_output() { return output_pass_alighting_decision; }
    map <Passenger*, list<Pass_connection_decision>, ptr_less<Passenger*> > get_connection_output() { return output_pass_connection_decision; }
    map <Passenger*, list<Pass_waiting_experience>, ptr_less<Passenger*> > get_waiting_output() { return output_pass_waiting_experience; }
    map <Passenger*, list<Pass_onboard_experience>, ptr_less<Passenger*> > get_onboard_output() { return output_pass_onboard_experience; }
	vector <Passenger*> get_passengers_during_simulation () const {return passengers_during_simulation;}
	
	void add_pass_waiting(Passenger* add_pass); //!< add passenger to the queue of this OD, called when passenger has arrived to the origin stop of this OD
	void add_passenger_to_odstop(Passenger* pass) { passengers_during_simulation.push_back(pass); } // used for reading empirical pass arrivals
	
	// Passengers processes
	void book_next_passenger (double curr_time);
	bool execute(Eventlist* eventlist, double time);

	// Path set
	void add_paths (Pass_path* pass_path_) {path_set.push_back(pass_path_);}
	double calc_boarding_probability (Busline* arriving_bus, double time, Passenger* pass);
	double calc_binary_logit (double utility_i, double utility_j);
	double calc_multinomial_logit (double utility_i, double utility_sum);
    double calc_path_size_logit (map<Pass_path*,pair<bool,double> > set_utilities, double utliity_i, double utliity_j);
	map<Pass_path*,double> calc_path_size_factor_nr_stops (map<Pass_path*,double> cluster_set_utilities);
	double calc_path_size_factor_between_clusters (Pass_path* path, map<Pass_path*,double> cluster_probs);
	double calc_combined_set_utility_for_alighting (Passenger* pass, Bustrip* bus_on_board, double time); // the trip that the pass. is currently on-board when calc. utility from downstream stop
	double calc_combined_set_utility_for_alighting_zone (Passenger* pass, Bustrip* bus_on_board, double time); 
	double calc_combined_set_utility_for_connection (double walking_distance, double time, Passenger* pass);
	double calc_combined_set_utility_for_connection_zone (Passenger* pass, double walking_distance, double time);
	bool check_if_path_is_dominated (Pass_path* considered_path, vector<Pass_path*> arriving_paths);

	// output-related functions
	void record_passenger_boarding_decision (Passenger* pass, Bustrip* trip, double time, double boarding_probability, bool boarding_decision); //!< creates a log-file for boarding decision related info
    void record_passenger_alighting_decision (Passenger* pass, Bustrip* trip, double time, Busstop* chosen_alighting_stop, map<Busstop*,pair<double,double> > alighting_MNL); // !< creates a log-file for alighting decision related info
    void record_passenger_connection_decision (Passenger* pass, double time, Busstop* chosen_alighting_stop, map<Busstop*,pair<double,double> > connecting_MNL_);
	void record_passenger_transitmode_decision(Passenger* pass, double time, Busstop* pickup_stop, TransitModeType chosen_mode, map<TransitModeType, pair<double, double>> mode_MNL_);
	void record_passenger_dropoff_decision(Passenger* pass, double time, Busstop* pickup_stop, Busstop* chosen_dropoff_stop, map<Busstop*, pair<double, double>> dropoff_MNL_);
	void record_waiting_experience (Passenger* pass, Bustrip* trip, double time, double wt_exp, int level_of_rti_upon_decision, double wt_rti, double wt_acc_exp, int nr_missed); // !< creates a log-file for a decision and related waiting time components
	void record_onboard_experience(Passenger* pass, Bustrip* trip, Busstop* stop, pair<double,double> ivt_exp);
    void write_boarding_output(ostream& out, Passenger* pass);
    void write_alighting_output(ostream& out, Passenger* pass);
    void write_connection_output(ostream& out, Passenger* pass);
    void write_transitmode_output(ostream& out, Passenger* pass, int day);
    void write_dropoff_output(ostream& out, Passenger* pass, int day);
    void write_waiting_exp_output(ostream& out, Passenger* pass, int day);
    void write_onboard_exp_output(ostream& out, Passenger* pass, int day);
    void write_od_summary(ostream& out);
    void write_od_summary_without_paths(ostream& out);
    void calc_pass_measures();

	//Day2Day
	void set_anticipated_waiting_time (Busstop* stop, Busline* line, double anticipated_WT);
	void set_anticipated_ivtt (Busstop* stop, Busline* line, Busstop* leg, double anticipated_IVTT);
	void set_alpha_RTI (Busstop* stop, Busline* line, double alpha); 
	void set_alpha_exp (Busstop* stop, Busline* line, double alpha); 
	void set_ivtt_alpha_exp (Busstop* stop, Busline* line, Busstop* leg, double alpha);
    void set_ivtt_alpha_exp_crowding(Busstop* stop, Busline* line, Busstop* leg, double alpha);
    void set_ivtt_acc_exp(Busstop* stop, Busline* line, Busstop* leg, double ivt_acc_exp);
    map<pair<Busstop*, Busline*>,double, pair_less<pair <Busstop*, Busline*> >> get_anticipated_waiting_time () {return anticipated_waiting_time;}
    map<pair<Busstop*, Busline*>,double, pair_less<pair <Busstop*, Busline*> >> get_alpha_RTI () {return alpha_RTI;}
    map<pair<Busstop*, Busline*>,double, pair_less<pair <Busstop*, Busline*> >> get_alpha_exp () {return alpha_exp;}
    map<SLL, double> get_anticipated_ivtt() { return anticipated_ivtt; }
    map<SLL, double> get_ivtt_alpha_exp() { return ivtt_alpha_exp; }
    map<SLL, double> get_ivtt_alpha_exp_crowding() { return ivtt_alpha_exp_crowding; }
    map<SLL, double> get_ivtt_acc_exp() { return ivtt_acc_exp; }

protected:
	Busstop* origin_stop = nullptr;
	Busstop* destination_stop = nullptr;
	double arrival_rate = 0.0; 
	bool empirical_arrivals = false; //!< flags whether or not empirical passenger arrivals are being generated for this ODstop pair
	passengers waiting_passengers; // a list of passengers with this OD that wait at the origin
	int min_transfers = 0; // the minimum number of trnasfers possible for getting from O to D
	int nr_pass_completed = 0;
	double avg_tt = 0.0;
	double avg_nr_boardings = 0.0;
    
	vector <Pass_path*> path_set;
	double boarding_utility = 0.0;
	double staying_utility = 0.0;

/** @ingroup DRT
*	@note used purely for setting default values for boarding and staying utilities for recording boarding output in case of flexible servies (and there is no boarding decision since passenger is commited one deciding to used flexible transit)
	@{
 */
public:
	void set_boarding_utility(double boarding_utility_) { boarding_utility = boarding_utility_; }
	void set_staying_utility(double staying_utility_) { staying_utility = staying_utility_; }
	
	//mostly for debugging
	int get_nr_pass_completed() { return nr_pass_completed; } //!< Getter added for testing purposes currently. Disclaimer: nr_pass_completed calculated after call to this->calc_pass_measures.
	Passenger* first_passenger_start = nullptr; //!< @todo For debugging purposes only, will contain the first passenger arrival at each ODstop pair (set at the beginning of Passenger::start)

	list<Pass_transitmode_decision> get_pass_transitmode_decisions(Passenger* pass); //!< returns the list, if any, of transitmode decisions the a passenger has made for this OD
	list<Pass_dropoff_decision> get_pass_dropoff_decisions(Passenger* pass); //!< returns the list, if any, of dropoff decisions the a passenger has made for this OD
	list<Pass_connection_decision> get_pass_connection_decisions(Passenger* pass); //!< returns the list, if any, of connection decisions that a passenger has made for this OD
/** @} */

protected:
	// output structures and measures (all output stored by origin zone)
    map <Passenger*,list<Pass_boarding_decision>, ptr_less<Passenger*> > output_pass_boarding_decision;
    map <Passenger*,list<Pass_alighting_decision>, ptr_less<Passenger*> > output_pass_alighting_decision;
    map <Passenger*,list<Pass_connection_decision>, ptr_less<Passenger*> > output_pass_connection_decision;

/** @ingroup DRT
	@{
*/
	map <Passenger*, list<Pass_transitmode_decision>, ptr_less<Passenger*> > output_pass_transitmode_decision;
	map <Passenger*, list<Pass_dropoff_decision>, ptr_less<Passenger*> > output_pass_dropoff_decision;
/** @} */

    map <Passenger*,list<Pass_waiting_experience>, ptr_less<Passenger*> > output_pass_waiting_experience;
    map <Passenger*,list<Pass_onboard_experience>, ptr_less<Passenger*> > output_pass_onboard_experience;
	vector <Passenger*> passengers_during_simulation;
	
    vector <pair<vector<Busstop*>, pair <int,double> > > paths_tt;

	Random* random = nullptr;
	Eventlist* eventlist = nullptr; //!< to book passenger generation events
	bool active = false; // indicator for non-initialization call

	// relevant only in case of day2day procedures
    map<pair<Busstop*, Busline*>,double,pair_less<pair <Busstop*, Busline*> >> anticipated_waiting_time;
    map<pair<Busstop*, Busline*>,double,pair_less<pair <Busstop*, Busline*> >> alpha_RTI;
    map<pair<Busstop*, Busline*>,double,pair_less<pair <Busstop*, Busline*> >> alpha_exp;
	map<SLL,double> anticipated_ivtt;
	map<SLL,double> ivtt_alpha_exp;
	map<SLL,double> ivtt_alpha_exp_crowding;
	map<SLL,double> ivtt_acc_exp;

};

class Pass_boarding_decision_zone // container object holding output data for passenger boarding decision
{
public:
	Pass_boarding_decision_zone (int pass_id_, int origin_zone_, int destination_zone_, int line_id_, int trip_id_, int stop_id_, double time_,	double generation_time_,
							double boarding_probability_, bool boarding_,double u_boarding_, double u_staying_):
							pass_id(pass_id_),origin_zone(origin_zone_),destination_zone(destination_zone_),line_id(line_id_),trip_id(trip_id_), stop_id(stop_id_),time(time_),generation_time(generation_time_),boarding_probability(boarding_probability_),boarding(boarding_),
							u_boarding(u_boarding_),u_staying(u_staying_) {}
	virtual ~Pass_boarding_decision_zone(); //!< destructor
	void write (ostream& out) { out << pass_id << '\t' << origin_zone << '\t' << destination_zone << '\t' << line_id << '\t'<< trip_id << '\t'<< stop_id<< '\t'<< time << '\t'<< generation_time << '\t' << boarding_probability << '\t'
		<< boarding << '\t'<< u_boarding << '\t'<< u_staying <<'\t'<< endl; }
	void reset () { pass_id = 0; origin_zone = 0; destination_zone = 0; line_id = 0; trip_id = 0; stop_id = 0; time = 0;
	generation_time = 0; boarding = 0; u_boarding = 0; u_staying = 0; }
	int pass_id;
	int origin_zone;
	int destination_zone;
	int line_id;
	int trip_id;
	int stop_id;
	double time;
	double generation_time;
	double boarding_probability;
	bool boarding;
	double u_boarding;
	double u_staying;
};

class Pass_alighting_decision_zone // container object holding output data for passenger alighting decision
{
public:
    Pass_alighting_decision_zone (int pass_id_, int origin_zone_, int destination_zone_, int line_id_, int trip_id_, int stop_id_, double time_,
                                  double generation_time_, int chosen_alighting_stop_,
                            map<Busstop*,pair<double,double>,ptr_less<Busstop*> > alighting_MNL_):
                            pass_id(pass_id_),origin_zone(origin_zone_),destination_zone(destination_zone_),
                            line_id(line_id_),trip_id(trip_id_), stop_id(stop_id_),time(time_),generation_time(generation_time_),
                            chosen_alighting_stop(chosen_alighting_stop_),alighting_MNL(alighting_MNL_) {}
	virtual ~Pass_alighting_decision_zone(); //!< destructor
	void write (ostream& out);
	void reset () { pass_id = 0; origin_zone = 0; destination_zone = 0; line_id = 0; trip_id = 0; stop_id = 0; time = 0;
	generation_time = 0; }
	int pass_id;
	int origin_zone;
	int destination_zone;
	int line_id;
	int trip_id;
	int stop_id;
	double time;
	double generation_time;
	int chosen_alighting_stop;
    map<Busstop*,pair<double,double>,ptr_less<Busstop*> > alighting_MNL;
};

class ODzone : public Action
{
public:
	ODzone (int zone_id);
	~ODzone ();
	int get_id () {return id;}
	void reset (); 
    map <Busstop*,pair<double,double> , ptr_less<Busstop*> > get_stop_distances() {return stops_distances;}
	void set_boarding_u (double boarding_utility_) {boarding_utility = boarding_utility_;}
	void set_staying_u (double staying_utility_) {staying_utility = staying_utility_;}
	
	// initialize
	void add_stop_distance (Busstop* stop, double mean_distance, double sd_distance);
	void add_arrival_rates (ODzone* d_zone, double arrival_rate); 

	// demand generation
	bool execute(Eventlist* eventlist, double curr_time);

	// output-related functions
	void record_passenger_boarding_decision_zone (Passenger* pass, Bustrip* trip, double time, double boarding_probability, bool boarding_decision); //!< creates a log-file for boarding decision related info
    void record_passenger_alighting_decision_zone (Passenger* pass, Bustrip* trip, double time, Busstop* chosen_alighting_stop, map<Busstop *, pair<double, double>, ptr_less<Busstop *> > alighting_MNL); // !< creates a log-file for alighting decision related info
	void write_boarding_output_zone(ostream & out, Passenger* pass);
	void write_alighting_output_zone(ostream & out, Passenger* pass);
    map <Passenger*,list<Pass_boarding_decision_zone>, ptr_less<Passenger*> > get_boarding_output_zone () {return output_pass_boarding_decision_zone;}
    map <Passenger*,list<Pass_alighting_decision_zone>,ptr_less<Passenger*> > get_alighting_output_zone () {return output_pass_alighting_decision_zone;}

protected:
	int id = -1;
	double boarding_utility = 0.0;
	double staying_utility = 0.0;
    map <Busstop*,pair<double,double>, ptr_less<Busstop*> > stops_distances; // the mean and SD of the distances to busstops included in the zone
	map <ODzone*,double> arrival_rates; // hourly arrival rate from this origin zone to the specified destination zone 
	Random* random = nullptr;
	Eventlist* eventlist = nullptr; //!< to book passenger generation events
	bool active = false; // indicator for non-initialization call

	
	// output structures and measures
    map <Passenger*,list<Pass_boarding_decision_zone>,ptr_less<Passenger*> > output_pass_boarding_decision_zone;
    map <Passenger*,list<Pass_alighting_decision_zone>,ptr_less<Passenger*> > output_pass_alighting_decision_zone;
	vector <Passenger*> passengers_during_simulation;
	int nr_pass_completed = 0;
	double avg_tt = 0.0;
	double avg_nr_boardings = 0.0;
    vector <pair<vector<Busstop*>, pair <int,double> > > paths_tt;
};

#endif // OD
