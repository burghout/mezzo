///! busline.cpp: implementation of the busline class.

#include "busline.h"
#include <math.h>
#include "MMath.h"
#include <sstream>
#include <stddef.h>

template<class T>
struct compare
{
	compare(int id_):id(id_) {}
	bool operator () (T* thing)

	{
		return (thing->get_id()==id);
	}

	int id;
};

PassengerRecycler pass_recycler; // Global passenger recycler

template<class T>
struct compare_pair
{
 compare_pair(int id_):id(id_) {}
 bool operator () (T thing)

 	{
 	 return (thing->first->get_id()==id);
 	}

 int id;
};

// ***** Busline functions *****

Busline::Busline ()
{
	active = false;
	output_line_assign.clear();
}

Busline_assign::Busline_assign ()
{
	passenger_load = 0;
}

Busline_assign::~Busline_assign ()
{}

Output_Summary_Line::~Output_Summary_Line ()
{}

Busline_travel_times::~Busline_travel_times ()
{}

Busline::Busline (int id_, int opposite_id_, string name_, Busroute* busroute_, vector<Busstop*> stops_, Vtype* vtype_, ODpair* odpair_, int holding_strategy_, float max_headway_holding_, double init_occup_per_stop_, int nr_stops_init_occup_):
	stops(stops_), id(id_), opposite_id(opposite_id_), name(name_), busroute(busroute_), odpair(odpair_), vtype(vtype_), max_headway_holding(max_headway_holding_), holding_strategy(holding_strategy_), init_occup_per_stop(init_occup_per_stop_), nr_stops_init_occup(nr_stops_init_occup_)
{
	active=false;
}

Busline::~Busline()
{}

void Busline::reset ()
{
	active = false;
	curr_trip = trips.begin();
	stop_pass.clear();
	output_summary.reset();
	output_line_assign.clear();
	output_travel_times.clear();
}

void Busline::reset_curr_trip ()
{
	curr_trip = trips.begin();
}

bool Busline::execute(Eventlist* eventlist, double time)
{
	if (!active) // first time this function is called. no active trips yet
	{
		if (trips.size() == 0)
		{
			return true; // if no trips, return 0
		}
		else
		{
			curr_trip = trips.begin();
			double next_time = curr_trip->second;
			eventlist->add_event(next_time, this); // add itself to the eventlist, with the time the next trip is starting
			active = true; // now the Busline is active, there is a trip that will be activated at t=next_time
			return true;
		}
	}
	else // if the Busline is active
	{
		curr_trip->first->activate(time, busroute, odpair, eventlist); // activates the trip, generates bus etc.
		curr_trip++; // now points to next trip
		if (curr_trip < trips.end()) // if there exists a next trip
		{
			double next_time = curr_trip->second;
			eventlist->add_event(next_time, this); // add itself to the eventlist, with the time the next trip is starting
			return true;
		}
	}
	return true;
}

vector<Busstop*>::iterator Busline::get_stop_iter (Busstop* stop)
{
	for (vector<Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == stop->get_id())
		{
			return stop_iter;
		}
	}
	return stops.end();
}

void Busline::add_disruptions (Busstop* from_stop, Busstop* to_stop, double disruption_start_time, double disruption_end_time, double cap_reduction)
{
	pair<Busstop*,pair<double,double> > pair_dis;
	pair_dis.first = to_stop;
	pair_dis.second.first = disruption_start_time;
	pair_dis.second.second = disruption_end_time;
	disruption_times[from_stop] = pair_dis;
	disruption_cap_reduction [from_stop] = cap_reduction;
}

bool Busline::is_line_timepoint (Busstop* stop)
{
	for (vector <Busstop*>::iterator tp = line_timepoint.begin(); tp < line_timepoint.end(); tp++ )
	{
		if (stop == *(tp))
		{
			return true;
		}
	}
return false;
}

bool Busline::check_first_stop (Busstop* stop)
{
	if (stop==*(stops.begin()))
	{
		return true;
	}
return false;
}

bool Busline::check_last_stop (Busstop* stop)
{
	if (stop== stops.back())
	{
		return true;
	}
	return false;
}

bool Busline::check_first_trip (Bustrip* trip)
{
	if (trip == trips.begin()->first)
	{
		return true;
	}
return false;
}

bool Busline::check_last_trip (Bustrip* trip)
{
	if (trip == (trips.end()-1)->first)
	{
		return true;
	}
return false;
}

/*
double Busline::calc_next_scheduled_arrival_at_stop (Busstop* stop, double time)
{
	vector <Visit_stop*> line_stops;
	if (curr_trip == trips.begin())
	{
		line_stops = (*curr_trip).first->stops;
	}
	else
	{
		line_stops = (*(curr_trip-1)).first->stops;
	}
	vector <Visit_stop*>::iterator decision_stop;
	for (vector <Visit_stop*>::iterator stop_iter = line_stops.begin(); stop_iter < line_stops.end(); stop_iter++)
	{
		Busstop* check_stop = (*stop_iter)->first;
		if (stop->get_id() == check_stop->get_id())
		{
			decision_stop = stop_iter;
			break;
		}
	}
	return (*decision_stop)->second - time;
}
*/

double Busline::find_time_till_next_scheduled_trip_at_stop (Busstop* stop, double time)
{
	for (vector <Start_trip>::iterator trip_iter = trips.begin(); trip_iter < trips.end(); trip_iter++)
	{
		map <Busstop*, double> stop_time = (*trip_iter).first->stops_map;
		if (stop_time[stop] > time)
		// assuming that trips are stored according to their chronological order
		{
			return ((*trip_iter).first->stops_map[stop] - time);
		}
	}
	// currently - in case that there is no additional trip scheduled - return the time till simulation end
	return theParameters->running_time - time;
}

vector<Start_trip>::iterator Busline::find_next_expected_trip_at_stop (Busstop* stop)
{
	if (stop->get_had_been_visited(this) == false)
	{
		return trips.begin(); // if no trip had visited the stop yet then the first trip is the expected next arrival
	}
	Bustrip* next_trip = stop->get_last_trip_departure(this);
	if (next_trip->get_id() == trips.back().first->get_id())
	{
		return trips.end()-1;
	}
	for (vector <Start_trip>::iterator trip_iter = trips.begin(); trip_iter < trips.end(); trip_iter++)
	{
		if ((*trip_iter).first->get_id() == next_trip->get_id())
		{
			return (trip_iter+1); // the trip following the most recent arrival is expected to arrive next
		}
	}
	return trips.end()-1;
}

double Busline::time_till_next_arrival_at_stop_after_time (Busstop* stop, double time)
{
	double time_till_next_visit;
	if (stops.front()->get_had_been_visited(this) == false)
	// in case no trip started yet - according to time table of the first trip
	{
		time_till_next_visit = trips.front().first->stops_map[stop] - time;
		return time_till_next_visit;
	}
	// find the iterator for this pass stop
	vector<Busstop*>::iterator this_stop;
	for (vector <Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == stop->get_id())
		{
			this_stop = stop_iter;
			break;
		}
	}
	vector <Start_trip>::iterator last_trip = find_next_expected_trip_at_stop(stop);
	Busstop* last_stop_visited = (*last_trip).first->get_last_stop_visited();
	double time_last_stop_visited = (*last_trip).first->get_last_stop_exit_time();
	if (check_first_stop(last_stop_visited) == true && time_last_stop_visited == 0) // next trip has not started yet
	{
		time_till_next_visit = find_time_till_next_scheduled_trip_at_stop(stop,time); // time till starting time plus time to stop
	}
	else
	{
		time_till_next_visit = (*last_trip).first->stops_map[stop] - (*last_trip).first->stops_map[last_stop_visited]; // additional scheduled time
	}
	int min_display = Round((time_till_next_visit + check_subline_disruption(last_stop_visited, stop, time))/60);
	return  max(min_display*60,0);
}


double Busline::extra_disruption_on_segment (Busstop* next_stop, double time)
{
	if (disruption_times.count(next_stop) > 0 && time > disruption_times[next_stop].second.first && time < disruption_times[next_stop].second.second) // if disruption starts from this stop and within the time window
	{
		if (disruption_cap_reduction[next_stop] == 1.0)
		{
			return disruption_times[next_stop].second.second-time;
		}
		else
		{
			double expected_headway = time - next_stop->get_last_departure(this);
			double min_headway = calc_curr_line_headway() / (1-disruption_cap_reduction[next_stop]); // due to partial capacity reduction
			return max(0.0, min_headway-expected_headway); // if needed to satisfy capacity limitation
		}
	}
	return 0.0;
}
double Busline::calc_curr_line_headway ()
{
	if (curr_trip == trips.end())
	{
		return ((*(curr_trip-1)).second - (*(curr_trip-2)).second);
	}
	if (curr_trip == trips.begin())
	{
		return ((*(curr_trip+1)).second - (*curr_trip).second);
	}
	else
	{
		return ((*curr_trip).second - (*(curr_trip-1)).second);
	}
}

double Busline::calc_curr_line_headway_forward ()
{
	if (curr_trip == trips.end())
	{
		return ((*(curr_trip-1)).second - (*(curr_trip-2)).second);
	}
	if (curr_trip == trips.begin())
	{
		return ((*(curr_trip+1)).second - (*curr_trip).second);
	}
	if (curr_trip == trips.end()-1)
	{
		return ((*curr_trip).second - (*(curr_trip-1)).second);
	}
	else
	{
		return ((*(curr_trip+1)).second - (*curr_trip).second);
	}
}

double Busline:: calc_max_headway ()
{
	double max_headway = 0.0;
	for (vector <Start_trip>::iterator start_trip_iter = trips.begin(); start_trip_iter < trips.end()-1; start_trip_iter++)
	{
		max_headway = max (max_headway, (*(start_trip_iter+1)).second - (*(start_trip_iter)).second);
	}
	return max_headway;
}

Bustrip* Busline::get_next_trip (Bustrip* reference_trip) //!< returns the trip after the reference trip on the trips vector
{
	for (vector <Start_trip>::iterator trips_iter = trips.begin(); trips_iter < trips.end(); trips_iter++)
	{
		if ((*trips_iter).first->get_id() == reference_trip->get_id())
		{
			return ((*(trips_iter+1)).first);
		}
	}
	return trips.back().first;
}

Bustrip* Busline::get_previous_trip (Bustrip* reference_trip) //!< returns the trip before the reference trip on the trips vector
{
	for (vector <Start_trip>::iterator trips_iter = trips.begin(); trips_iter < trips.end(); trips_iter++)
	{
		if ((*trips_iter).first->get_id() == reference_trip->get_id())
		{
			return ((*(trips_iter-1)).first);
		}
	}
	return trips.front().first;
}

double Busline::calc_curr_line_ivt (Busstop* start_stop, Busstop* end_stop, int rti, double time)
{
	double extra_travel_time = 0.0;
	if (rti == 3)
	{
		extra_travel_time = check_subline_disruption(start_stop, end_stop, time);
	}
	vector<Visit_stop*>::iterator board_stop;
	vector<Visit_stop*>::iterator alight_stop;
	bool found_board = false;
	bool found_alight = false;
	vector <Start_trip>::iterator check_trip;
	if	(curr_trip == trips.end())
	{
		check_trip = curr_trip-1;
	}
	else
	{
		check_trip = curr_trip;
	}
 	for (vector<Visit_stop*>::iterator stop = (*check_trip).first->stops.begin(); stop <(*check_trip).first->stops.end(); stop++)
	{
			if ((*stop)->first->get_id() == start_stop->get_id() || (*stop)->first->get_name() == start_stop->get_name())
			{
				board_stop = stop;
				found_board = true;
			}
			if ((*stop)->first->get_id() == end_stop->get_id() || (*stop)->first->get_name() == end_stop->get_name())
			{
				alight_stop = stop;
				found_alight = true;
				break;
			}
	}
	if (found_board == false || found_alight == false)
	{
		return 10000; // default in case no matching
	}
	return ((*alight_stop)->second - (*board_stop)->second) + extra_travel_time; // in seconds
}

double Busline::check_subline_disruption (Busstop* last_visited_stop, Busstop* pass_stop, double time)
{
	if (disruption_times.empty() == true)
	{
		return 0.0;
	}
	// find the iterator for the starting stop of the disruption
	Busstop* first_dis_stop = (*disruption_times.begin()).first;
	vector<Busstop*>::iterator first_dis_stop_iter;
	for (vector <Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == first_dis_stop->get_id())
		{
			first_dis_stop_iter = stop_iter;
			break;
		}
	}
	// find the iterator for the last stop of the disruption
	Busstop* last_dis_stop = (*disruption_times.begin()).second.first;
	vector<Busstop*>::iterator last_dis_stop_iter;
	for (vector <Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == last_dis_stop->get_id())
		{
			last_dis_stop_iter = stop_iter;
			break;
		}
	}
	// find the iterator for the pass stop
	vector<Busstop*>::iterator pass_stop_iter;
	for (vector <Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == pass_stop->get_id())
		{
			pass_stop_iter = stop_iter;
			break;
		}
	}
	// find the iterator for the last visited stop
	vector<Busstop*>::iterator last_visited_stop_iter;
	for (vector <Busstop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		if ((*stop_iter)->get_id() == last_visited_stop->get_id())
		{
			last_visited_stop_iter = stop_iter;
			break;
		}
	}
	if (time < disruption_times[first_dis_stop].second.first || time > disruption_times[first_dis_stop].second.second || last_visited_stop_iter > last_dis_stop_iter || (last_visited_stop_iter < first_dis_stop_iter && pass_stop_iter < first_dis_stop_iter)) // out of the relevant time frame or disruption part
	{
		return 0.0;
	}
	return disruption_times[first_dis_stop].second.second - time; // the remaining time for the disruption
}

void Busline::calculate_sum_output_line()
{
	// initialize all the measures
	output_summary.line_avg_headway = 0;
	output_summary.line_avg_DT = 0;
	output_summary.line_avg_abs_deviation = 0;
	output_summary.line_avg_waiting_per_stop = 0;
	output_summary.line_total_boarding = 0;
	output_summary.line_sd_headway = 0; // average standard deviation over line's stops
	output_summary.line_sd_DT = 0; // average standard deviation over line's stops
	output_summary.line_on_time = 0;
	output_summary.line_early = 0;
	output_summary.line_late = 0;
	output_summary.total_pass_riding_time = 0;
	output_summary.total_pass_dwell_time = 0;
	output_summary.total_pass_waiting_time = 0;
	output_summary.total_pass_holding_time = 0;
	output_summary.control_objective_function = 0;
	output_summary.total_travel_time_crowding = 0;

	// accumulating the measures over line's stops
	for (vector<Busstop*>::iterator stop = stops.begin(); stop < stops.end(); stop++)
	{
		output_summary.line_avg_headway += (*stop)->get_output_summary(id).stop_avg_headway;
		output_summary.line_avg_DT += (*stop)->get_output_summary(id).stop_avg_DT;
		output_summary.line_avg_abs_deviation += (*stop)->get_output_summary(id).stop_avg_abs_deviation;
		output_summary.line_avg_waiting_per_stop += (*stop)->get_output_summary(id).stop_avg_waiting_per_stop;
		output_summary.line_total_boarding += (*stop)->get_output_summary(id).stop_total_boarding;
		output_summary.line_sd_headway += (*stop)->get_output_summary(id).stop_sd_headway;
		output_summary.line_sd_DT += (*stop)->get_output_summary(id).stop_sd_DT;
		output_summary.line_on_time += (*stop)->get_output_summary(id).stop_on_time;
		output_summary.line_early += (*stop)->get_output_summary(id).stop_early;
		output_summary.line_late += (*stop)->get_output_summary(id).stop_late;

		output_summary.total_pass_riding_time += (*stop)->get_output_summary(id).total_stop_pass_riding_time;
		output_summary.total_pass_dwell_time += (*stop)->get_output_summary(id).total_stop_pass_dwell_time;
		output_summary.total_pass_waiting_time += (*stop)->get_output_summary(id).total_stop_pass_waiting_time;
		output_summary.total_pass_holding_time += (*stop)->get_output_summary(id).total_stop_pass_holding_time;
		output_summary.total_travel_time_crowding += (*stop)->get_output_summary(id).total_stop_travel_time_crowding;
	}
	// dividing by the number of stops for average measures
	output_summary.line_avg_headway = output_summary.line_avg_headway / stops.size();
	output_summary.line_avg_DT = output_summary.line_avg_DT / stops.size();
	output_summary.line_avg_abs_deviation = output_summary.line_avg_abs_deviation / stops.size();
	output_summary.line_avg_waiting_per_stop = output_summary.line_avg_waiting_per_stop / stops.size();
	output_summary.line_sd_headway = output_summary.line_sd_headway / stops.size();
	output_summary.line_sd_DT = output_summary.line_sd_DT / stops.size();
	output_summary.line_on_time = output_summary.line_on_time / stops.size();
	output_summary.line_early = output_summary.line_early / stops.size();
	output_summary.line_late = output_summary.line_late / stops.size();

	// calculate weighted objective function
	output_summary.control_objective_function += theParameters->riding_time_weight * output_summary.total_pass_riding_time + theParameters->dwell_time_weight * output_summary.total_pass_dwell_time + theParameters->waiting_time_weight * output_summary.total_pass_waiting_time + theParameters->holding_time_weight * output_summary.total_pass_holding_time;
}

/*
void Busline::calc_line_assignment()
{
	for (vector <Busstop*>::iterator stop_nr = stops.begin(); stop_nr < stops.end() - 1; stop_nr++) // initialize
	{
		stop_pass [(*stop_nr)] = 0;
	}
	for (vector <Start_trip>::iterator trip_iter = trips.begin(); trip_iter < trips.end(); trip_iter++) // calculating
	{
		vector <Busstop*>::iterator line_stop = stops.begin();
		list <Bustrip_assign> list_ass = (*trip_iter).first->get_output_passenger_load();
		for (list <Bustrip_assign>::iterator stop_iter = list_ass.begin(); stop_iter != list_ass.end(); stop_iter++)
		{
			stop_pass [(*line_stop)] += (*stop_iter).passenger_load;
			line_stop++;
		}
	}
	for (vector <Busstop*>::iterator stop_nr = stops.begin(); stop_nr < stops.end() - 1; stop_nr++) // recording
	{
		add_record_passenger_loads_line((*stop_nr),(*stop_nr)+1, stop_pass[(*stop_nr)]);
	}
}
*/


void Busline::add_record_passenger_loads_line (Busstop* stop1, Busstop* stop2, int pass_assign)
{

	output_line_assign[stop1] = Busline_assign(id, stop1->get_id(), stop1->get_name(), stop2->get_id(), stop2->get_name(), output_line_assign[stop1].passenger_load + pass_assign); // accumulate pass. load on this segment
}

// Erik 18-09-16
//void Busline::add_record_passenger_loads_line(Busstop* stop1, Busstop* stop2, int pass_assign, map<int,int> car_pass_assign)
//{
//	cout << "add_record_passenger_loads_line" << endl;
//	int a;
//	//int b;
//	for (map<int, int>::iterator car_it = car_pass_assign.begin(); car_it != car_pass_assign.end(); ++car_it)
//	{
//		a = car_it->first;
//		car_pass_assign[a] = output_line_assign[stop1].car_passenger_load[a] + car_pass_assign[a];
//		cout << "a = " << a << ", car_pass_assign[a] = " << car_pass_assign[a] << endl;
//	}
//
//	output_line_assign[stop1] = Busline_assign(
//		id, 
//		stop1->get_id(), 
//		stop1->get_name(), 
//		stop2->get_id(), 
//		stop2->get_name(), 
//		output_line_assign[stop1].passenger_load + pass_assign,
//		car_pass_assign); // accumulate pass. load on this segment
//}

void Busline::write_assign_output(ostream & out)
{
	for (map <Busstop*,Busline_assign>::iterator iter = output_line_assign.begin(); iter != output_line_assign.end(); iter++)
	{
		(*iter).second.write(out);
	}
}

void Busline::write_ttt_output(ostream & out)
{
	for (list <Busline_travel_times>::iterator iter = output_travel_times.begin(); iter != output_travel_times.end(); iter++)
	{
		(*iter).write(out);
	}
}

void Busline::update_total_travel_time (Bustrip* trip, double time)
{
	output_travel_times.push_back(Busline_travel_times(trip->get_id(),time - trip->get_actual_dispatching_time()));
}

// ***** Bustrip Functions *****

Bustrip::Bustrip ()
{
	init_occup_per_stop = line->get_init_occup_per_stop();
	nr_stops_init_occup = line->get_nr_stops_init_occup();
	random = new (Random);
	next_stop = stops.begin();
	last_stop_exit_time = 0;
	last_stop_enter_time = 0;
	last_stop_visited = stops.front()->first;
	holding_at_stop = false;
	actual_dispatching_time = 0.0;
	// Erik 18-09-24
	//map<int, int> zeros;
	//for (int car_id = 1; car_id <= busv->get_number_cars(); ++car_id) {
	//	zeros[car_id] = 0;
	//}
	for (vector <Visit_stop*>::iterator visit_stop_iter = stops.begin(); visit_stop_iter < stops.end(); visit_stop_iter++)
	{
		assign_segements[(*visit_stop_iter)->first] = 0;
		//assign_car_segments[(*visit_stop_iter)->first] = zeros;
	}
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}
}

Bustrip::Bustrip(int id_, double start_time_, Busline* line_) : id(id_), line(line_), starttime(start_time_)
{
	init_occup_per_stop = line->get_init_occup_per_stop();
	nr_stops_init_occup = line->get_nr_stops_init_occup();
	random = new (Random);
	next_stop = stops.begin();
	actual_dispatching_time = 0.0;
	last_stop_enter_time = 0;
	last_stop_exit_time = 0;
	holding_at_stop = false;
	// Erik 18-09-24
	//map<int, int> zeros;
	//for (int car_id = 1; car_id <= busv->get_number_cars(); ++car_id) {
	//	zeros[car_id] = 0;
	//}
	for (vector <Visit_stop*>::iterator visit_stop_iter = stops.begin(); visit_stop_iter < stops.end(); visit_stop_iter++)
	{
		assign_segements[(*visit_stop_iter)->first] = 0;
		//assign_car_segments[(*visit_stop_iter)->first] = zeros;
	}
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}
	/*  will be relevant only when time points will be trip-specific
	for (map<Busstop*,bool>::iterator tp = trips_timepoint.begin(); tp != trips_timepoint.end(); tp++)
	{
		tp->second = false;
	}
	*/
}

Bustrip::~Bustrip ()
{}

Bustrip_assign::~Bustrip_assign()
{}

void Bustrip::reset ()
{
	init_occup_per_stop = line->get_init_occup_per_stop();
	nr_stops_init_occup = line->get_nr_stops_init_occup();
	passengers_on_board.clear();
	enter_time = 0;
	last_stop_enter_time = 0;
	actual_dispatching_time = 0.0;
	last_stop_exit_time = 0;
	next_stop = stops.begin();
	assign_segements.clear();
	assign_car_segments.clear(); // Erik 18-09-24
	nr_expected_alighting.clear();
	//passengers_on_board.clear();
	output_passenger_load.clear();
	last_stop_visited = stops.front()->first;
	holding_at_stop = false;
}

void Bustrip::convert_stops_vector_to_map ()
{
	for (vector <Visit_stop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		stops_map[(*stop_iter)->first] =(*stop_iter)->second;
	}
}

double Bustrip::calc_departure_time (double time) // calculates departure time from origin according to arrival time and schedule (including layover effect)
{
	double min_recovery = 30.00;
	double mean_error_recovery = 10.00;
	double std_error_recovery = 10.00;
	// These three parameters should be used from the parameters input file
	vector <Start_trip*>::iterator curr_trip; // find the pointer to the current trip
	for (vector <Start_trip*>::iterator trip = driving_roster.begin(); trip < driving_roster.end(); trip++)
	{
		if ((*trip)->first == this)
		{
			curr_trip = trip;
			break;
		}
	}
	if (curr_trip == driving_roster.begin()) // if it is the first trip for this bus
	{
		actual_dispatching_time = starttime;
		return actual_dispatching_time;  // first dispatching is cuurently assumed to follow the schedule
	}
	else
	// if it is not the first trip for this bus then:
	// if the scheduled time is after arrival+recovery, it determines departure time.
	// otherwise (bus arrived behind schedule) - delay at origin.
	// in any case - there is error factor.
	{
		if (line->get_holding_strategy() == 6 || line->get_holding_strategy() == 7)
		{
			if (stops.front()->first->get_time_since_departure(this,time) < line->calc_curr_line_headway())
			{
				actual_dispatching_time = time + min_recovery + random->lnrandom (mean_error_recovery, std_error_recovery);
				return actual_dispatching_time;
			}
		}
		actual_dispatching_time = Max (time + min_recovery , starttime) + theRandomizers[0]->lnrandom (mean_error_recovery, std_error_recovery);
		return actual_dispatching_time; // error factor following log normal distribution;
	}
}

bool Bustrip::advance_next_stop (double time, Eventlist* eventlist)
{
	// TODO - vec.end is post last
	if (busv->get_on_trip()== true && next_stop < stops.end()) // progress to the next stop, unless it is the last stop for this trip
	{
		next_stop++;
		return true;
	}
	if (busv->get_on_trip()== false && next_stop == stops.end()) // if it was the last stop for this trip
	{
		vector <Start_trip*>::iterator curr_trip, next_trip; // find the pointer to the current and next trip
		for (vector <Start_trip*>::iterator trip = driving_roster.begin(); trip < driving_roster.end(); trip++)
		{
			if ((*trip)->first == this)
			{
				curr_trip = trip;
				break;
			}

		}
		next_trip = curr_trip +1;
		vector <Start_trip*>::iterator last_trip = driving_roster.end()-1;
		if (busv->get_curr_trip() != (*last_trip)->first) // if there are more trips for this vehicle
		{
			vid++;
			//Moved to read_busvehicle by Jens 2014-09-05. This placement caused problems when buses were very late, then a trip could be activated too early and not updated
			//Bus* new_bus=recycler.newBus(); // then generate a new (chained) vehicle
			//new_bus->set_bustype_attributes ((*next_trip)->first->get_bustype());
			//new_bus->set_bus_id(busv->get_bus_id());
			//(*next_trip)->first->set_busv (new_bus);
			//new_bus->set_curr_trip((*next_trip)->first);
		}
		busv->advance_curr_trip(time, eventlist); // progress the roster for the vehicle
		//int pass_id = busv->get_id();
		//recycler.addBus(busv);

		return false;
	}
	else
	{
		return true;
	}
}

// Erik 18-09-24
void Bustrip::add_stops(vector <Visit_stop*>  st)
{
	stops = st; next_stop = stops.begin();
	int num_stops = st.size();
	if (num_stops > 0)
	{
		vector <Visit_stop*>::iterator visit_stop_iter = stops.begin();
		Busstop* bstop = (*visit_stop_iter)->first;
		int num_cars = bstop->get_num_sections();
		assign_car_segments.clear();
		map<int, int> zeros;
		for (int car_id = 1; car_id <= num_cars; ++car_id) {
			zeros[car_id] = 0;
		}
		for (vector <Visit_stop*>::iterator visit_stop_iter = stops.begin(); visit_stop_iter < stops.end(); visit_stop_iter++)
		{
			assign_car_segments[(*visit_stop_iter)->first] = zeros;
		}
	}
}

double Bustrip::calc_forward_headway()
{

	return 0;
}

double Bustrip::calc_backward_headway()
{

	return 0;
}

bool Bustrip::activate (double time, Route* route, ODpair* odpair, Eventlist* eventlist_)
{
	// inserts the bus at the origin of the route
	// if the assigned bus isn't avaliable at the scheduled time, then the trip is activated by Bus::advance_curr_trip as soon as it is done with the previous trip
	double first_dispatch_time = time;
	eventlist = eventlist_;
	next_stop = stops.begin();
	complying_bustrip = random->brandom(theParameters->compliance_rate);
	bool ok = false; // flag to check if all goes ok
	vector <Start_trip*>::iterator curr_trip, previous_trip; // find the pointer to the current and previous trip
	if (driving_roster.empty()) cout << "Error: Driving roster empty for trip nr " << id << endl;
	for (vector <Start_trip*>::iterator trip = driving_roster.begin(); trip < driving_roster.end(); trip++)
	{
		if ((*trip)->first == this)
		{
			curr_trip = trip;
			break;
		}
	}
	if (curr_trip == driving_roster.begin()) // if it is the first trip for this chain
	{
		//vid++;
		//Bus* new_bus=recycler.newBus(); // then generate a new vehicle
		//new_bus->set_bustype_attributes (btype);
		//busv =new_bus;
		busv->set_curr_trip(this);
	}
	else // if it isn't the first trip for this chain
	{
		previous_trip = curr_trip-1;
		if ((*previous_trip)->first->busv->get_on_trip() == true) // if the assigned bus isn't avaliable
		{
			ok=false;
			return ok;
		}
		busv->set_curr_trip(this);
		first_dispatch_time = (*previous_trip)->first->get_last_stop_exit_time(); //Added by Jens 2014-07-03
		/*if (busv->get_route() != NULL)
			cout << "Warning, the route is changing!" << endl;*/
	}
	double dispatch_time = calc_departure_time(first_dispatch_time);
	if (dispatch_time < time)
		cout << "Warning, dispatch time is before current time for bus trip " << id << endl;
	busv->init(busv->get_id(),4,busv->get_length(),route,odpair,time); // initialize with the trip specific details
	busv->set_occupancy(random->inverse_gamma(nr_stops_init_occup,init_occup_per_stop));
	if ( (odpair->get_origin())->insert_veh(busv, dispatch_time)) // insert the bus at the origin at the possible departure time
	{
  		busv->set_on_trip(true); // turn on indicator for bus on a trip
		ok = true;
	}
	else // if insert returned false
  	{
  		ok = false;
  	}
	return ok;
}

void Bustrip::book_stop_visit (double time)
{
	// books an event for the arrival of a bus at a bus stop
	((*next_stop)->first)->book_bus_arrival(eventlist,time,this);
}


bool Bustrip::check_end_trip ()
{
	// indicates if the trip doesn't have anymore stops on its route
	if (next_stop == stops.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}


double Bustrip::scheduled_arrival_time (Busstop* stop) // finds the scheduled arrival time for a given bus stop
{
	for (vector<Visit_stop*>::iterator scheduled_time = stops.begin();scheduled_time < stops.end(); scheduled_time++)
	{
		if ((*scheduled_time)->first == stop)
		{
			return (*scheduled_time)->second;
		}
	}
	return 0; // if bus stop isn't on the trip's route
}

void Bustrip::write_assign_segments_output(ostream & out)
{
	for (list <Bustrip_assign>::iterator iter = output_passenger_load.begin(); iter != output_passenger_load.end(); iter++)
	{
		(*iter).write(out);
	}
}

/*
void Bustrip::record_passenger_loads (vector <Visit_stop*>::iterator start_stop)
{
	if(!holding_at_stop)
	{
		output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id(), (*start_stop)->first->get_name(), (*(start_stop + 1))->first->get_id(), (*(start_stop + 1))->first->get_name(), assign_segements[(*start_stop)->first]));
		this->get_line()->add_record_passenger_loads_line((*start_stop)->first, (*(start_stop+1))->first,assign_segements[(*start_stop)->first]);
	}
	else //David added 2016-05-26: overwrite previous record_passenger_loads if this is the second call to pass_activity_at_stop
	{
		--start_stop; //decrement start stop since we have already advanced 'next_stop'
		output_passenger_load.pop_back();
		output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id() , (*start_stop)->first->get_name(), (*(start_stop+1))->first->get_id(), (*(start_stop + 1))->first->get_name(), assign_segements[(*start_stop)->first]));
		this->get_line()->add_record_passenger_loads_line((*start_stop)->first, (*(start_stop+1))->first,assign_segements[(*start_stop)->first]);
	}
}
*/

// Erik 18-09-16
void Bustrip::record_passenger_loads(vector <Visit_stop*>::iterator start_stop)
{
	if (!holding_at_stop)
	{
		//output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id(), (*start_stop)->first->get_name(), (*(start_stop + 1))->first->get_id(), (*(start_stop + 1))->first->get_name(), assign_segements[(*start_stop)->first]));
		// Erik 18-09-16
		output_passenger_load.push_back(Bustrip_assign(
			line->get_id(), 
			id, 
			busv->get_id(), 
			(*start_stop)->first->get_id(), 
			(*start_stop)->first->get_name(), 
			(*(start_stop + 1))->first->get_id(), 
			(*(start_stop + 1))->first->get_name(), 
			assign_segements[(*start_stop)->first],
			assign_car_segments[(*start_stop)->first]
		));
		this->get_line()->add_record_passenger_loads_line(
			(*start_stop)->first, 
			(*(start_stop + 1))->first, 
			assign_segements[(*start_stop)->first] //,
			//assign_car_segments[(*start_stop)->first]
		);
	}
	else //David added 2016-05-26: overwrite previous record_passenger_loads if this is the second call to pass_activity_at_stop
	{
		--start_stop; //decrement start stop since we have already advanced 'next_stop'
		output_passenger_load.pop_back();
		//output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id(), (*start_stop)->first->get_name(), (*(start_stop + 1))->first->get_id(), (*(start_stop + 1))->first->get_name(), assign_segements[(*start_stop)->first]));
		// Erik 18-09-16
		output_passenger_load.push_back(Bustrip_assign(
			line->get_id(), 
			id, 
			busv->get_id(), 
			(*start_stop)->first->get_id(), 
			(*start_stop)->first->get_name(), 
			(*(start_stop + 1))->first->get_id(), 
			(*(start_stop + 1))->first->get_name(), 
			assign_segements[(*start_stop)->first],
			assign_car_segments[(*start_stop)->first]
		));
		this->get_line()->add_record_passenger_loads_line(
			(*start_stop)->first, 
			(*(start_stop + 1))->first, 
			assign_segements[(*start_stop)->first]//,
			//assign_car_segments[(*start_stop)->first]
		);
	}
}

//double Bustrip::find_crowding_coeff (Passenger* pass)
//{
//
//	// first - calculate load factor
//	double load_factor = this->get_busv()->get_occupancy()/this->get_busv()->get_number_seats();
//
//	// second - return value based on pass. standing/sitting
//	bool sits = pass->get_pass_sitting();
//
//	return find_crowding_coeff(sits, load_factor);
//
//}

// Erik 18-10-30
double Bustrip::find_crowding_coeff(Passenger* pass)
{
	//map<int, int> car_occupancy = this->get_busv()->get_car_occupancy();
	// first - calculate load factor
	double load_factor = this->get_busv()->get_car_occupancy(pass->get_pass_car()) / this->get_busv()->get_car_number_seats();

	// second - return value based on pass. standing/sitting
	bool sits = pass->get_pass_sitting();

	return find_crowding_coeff(sits, load_factor);

}


//double Bustrip::find_crowding_coeff(Passenger* pass)
//{
//	//std::cout << "i am here 1" << std::endl;
//	// first - calculate load factor
//	for (std::map<int, int>::iterator it = this->get_busv()->get_car_occupancy().begin(); it != this->get_busv()->get_car_occupancy().end(); ++it)
//	{
//		//std::cout << "Map first Value:" << it->first << std::endl;
//		//std::cout << "Map second Value:" << it->second << std::endl;
//		std::cout << "Iterator:" << it << std::endl;
//
//		double load_factor = it->second / this->get_busv()->get_number_seats();
//
//	}
//		// second - return value based on pass. standing/sitting
//		bool sits = pass->get_pass_sitting();
//
//	return find_crowding_coeff(sits, load_factor);
//}

double Bustrip::find_crowding_coeff (bool sits, double load_factor)
{

	if (load_factor < 0.75)
	{
		return 0.95;
	}
	else if (load_factor < 1.00)
	{
		return 1.05;
	}
	else if (load_factor < 1.25)
	{
		if (sits == true)
		{
			return 1.16;
		}
		else
		{
			return 1.78;
		}
	}
	else if (load_factor < 1.50)
	{
		if (sits == true)
		{
			return 1.28;
		}
		else
		{
			return 1.97;
		}
	}
	else if (load_factor < 1.75)
	{
		if (sits == true)
		{
			return 1.40;
		}
		else
		{
			return 2.19;
		}
	}
	else if (load_factor < 2.00)
	{
		if (sits == true)
		{
			return 1.55;
		}
		else
		{
			return 2.42;
		}
	}
	else
	{
		if (sits == true)
		{
			return 1.71;
		}
		else
		{
			return 2.69;
		}
	}
}


pair<int, int> Bustrip::crowding_dt_factor(int nr_boarding, int nr_alighting)
{
	pair<double, double> crowding_factor;
	//if (busv->get_car_capacity() == busv->get_number_seats())
	if (busv->get_capacity() == busv->get_number_seats())
	{
		crowding_factor.first = 1;
		crowding_factor.second = 1;
	}
	else
	{
		int nr_standees_alighting = max(0, busv->get_occupancy() - (nr_boarding + nr_alighting) - busv->get_number_seats());
		int nr_standees_boarding = max(0, busv->get_occupancy() - (nr_boarding + nr_alighting) / 2 - busv->get_number_seats());
		double crowdedness_ratio_alighting = nr_standees_alighting / (busv->get_capacity() - busv->get_number_seats());
		double crowdedness_ratio_boarding = nr_standees_boarding / (busv->get_capacity() - busv->get_number_seats());
		/*double nr_standees_alighting = max(0.0, busv->get_occupancy() - (nr_boarding + nr_alighting) - busv->get_number_seats());
		double nr_standees_boarding = max(0.0, busv->get_occupancy() - (nr_boarding + nr_alighting) / 2 - busv->get_number_seats());
		double crowdedness_ratio_alighting = nr_standees_alighting / (busv->get_capacity() - busv->get_number_seats());
		double crowdedness_ratio_boarding = nr_standees_boarding / (busv->get_capacity() - busv->get_number_seats());*/
		crowding_factor.second = 1 + 0.75 * pow(crowdedness_ratio_alighting, 2);
		crowding_factor.first = 1 + 0.75 * pow(crowdedness_ratio_boarding, 2);
	}
	return crowding_factor;
}

vector <Busstop*> Bustrip::get_downstream_stops()
{
	vector <Busstop*> remaining_stops;
	for(vector <Visit_stop*> :: iterator stop = next_stop; stop < stops.end(); stop++)
	{
		remaining_stops.push_back((*stop)->first);
	}
	return remaining_stops;
}

vector <Visit_stop*> Bustrip::get_downstream_stops_till_horizon(Visit_stop *target_stop)
{
	vector <Visit_stop*> remaining_stops;
	for(vector <Visit_stop*> :: iterator stop = next_stop; stop < stops.end(); stop++)
	{
		if ((*stop)->first != (target_stop)->first)
		{
			remaining_stops.push_back(*stop);
		}
		else
		{
			remaining_stops.push_back(*stop);
			break;
		}
	}
	return remaining_stops;
}
/* will be relevant only when time points will be trip-specific
bool Bustrip::is_trip_timepoint (Busstop* stop)
{
	 if (trips_timepoint.count(stop) > 0)
		return (int)trips_timepoint[stop];
	 else
		return -1;
}
*/


// ***** Busstop functions *****

Busstop::Busstop()
{
	length = 20;
	num_sections = 1;
	position = 0;
	has_bay = false;
	can_overtake = true;
	dwelltime = 0;
	rti = 0;
}

// Erik 18-09-15
Busstop::Busstop (int id_, string name_, int link_id_, double position_, double length_, int num_sections_, bool has_bay_, bool can_overtake_, double min_DT_, int rti_, bool gate_flag_):
id(id_), name(name_), link_id(link_id_), position (position_), length(length_), num_sections(num_sections_), has_bay(has_bay_), can_overtake(can_overtake_), min_DT(min_DT_), rti (rti_), gate_flag (gate_flag_)
{
	avaliable_length = length;
	nr_boarding = 0;
	nr_alighting = 0;
	dwelltime = 0;
	is_origin = false;
	is_destination = false;
	last_arrivals.clear();
	last_departures.clear();
	random = new (Random);
	if (randseed != 0)
		{
		random->seed(randseed);
		}
	else
	{
		random->randomize();
	}
}

Busstop::~Busstop ()
{
	delete random;
}

void Busstop::reset()
{
	avaliable_length = length;
	nr_boarding = 0;
	nr_alighting = 0;
	is_origin = false;
	is_destination = false;
	dwelltime = 0;
	exit_time = 0;
	expected_arrivals.clear();
	expected_bus_arrivals.clear();
	buses_at_stop.clear();
	last_arrivals.clear();
	last_departures.clear();
	had_been_visited.clear();
	multi_nr_waiting.clear();
	nr_waiting.clear();
	output_stop_visits.clear();
	output_summary.clear();
}

Busstop_Visit::~Busstop_Visit()
{}

Output_Summary_Stop_Line::~Output_Summary_Stop_Line ()
{}

Change_arrival_rate::~Change_arrival_rate()
{}

Dwell_time_function::~Dwell_time_function()
{}

void Busstop::book_bus_arrival(Eventlist* eventlist, double time, Bustrip* trip)
{
	// books an event for the arrival of a bus at a bus stop by adding it to the expected arrivals at the stop
	pair<Bustrip*,double> bus_arrival_time;
	bus_arrival_time.first = trip;
	bus_arrival_time.second = time;
	expected_bus_arrivals.push_back(bus_arrival_time);
	eventlist->add_event(time,this);
}


void Busstop::add_walking_time_quantiles(Busstop* dest_stop_ptr, vector<double> quantiles, vector<double> quantile_values, int num_quantiles, double interval_start, double interval_end){

    Walking_time_dist* time_dist = new Walking_time_dist(dest_stop_ptr, quantiles, quantile_values, num_quantiles, interval_start, interval_end);

    if ( walking_time_distribution_map.find(dest_stop_ptr) == walking_time_distribution_map.end() ) {
        //not found
        vector<Walking_time_dist*> dist_vector = {time_dist};
        walking_time_distribution_map[dest_stop_ptr] = dist_vector;

    }
    else {
        // found
        walking_time_distribution_map.at(dest_stop_ptr).push_back(time_dist);

    }

}


double Busstop::estimate_walking_time_from_quantiles(Busstop* dest_stop_ptr, double curr_time){

    if ( walking_time_distribution_map.find(dest_stop_ptr) == walking_time_distribution_map.end() ) {
        //not found

    } else {
        // found


        for(auto const& time_dist: walking_time_distribution_map.at(dest_stop_ptr) ) {

            if (time_dist->time_is_in_range(curr_time)){

                //found the right distribution: need to draw from distribution and return obtained walking time

                int num_quantiles = time_dist->get_num_quantiles();

                double random_draw = random->urandom(0.0, 100);

                double* quantiles = time_dist->get_quantiles();
                double* quantile_values = time_dist->get_quantile_values();

                double lower_quantile = 0.0;
                double upper_quantile = 100;

                double lower_quantile_value, upper_quantile_value;

                double curr_quantile, curr_quantile_value;

                for (int i=0;i<num_quantiles;i++){
                    curr_quantile = quantiles[i];
                    curr_quantile_value = quantile_values[i];

                    if (curr_quantile <= random_draw){
                        lower_quantile = curr_quantile;
                        lower_quantile_value = curr_quantile_value;
                    } else {
                        break;
                    }

                }



                for (int i=num_quantiles-1;i>=0;i--){
                    curr_quantile = quantiles[i];
                    curr_quantile_value = quantile_values[i];

                    if (curr_quantile >= random_draw){
                        upper_quantile = curr_quantile;
                        upper_quantile_value = curr_quantile_value;
                    } else {
                        break;
                    }

                }

                //linear interpolation between current quantiles:
                double frac = (random_draw-lower_quantile)/(upper_quantile-lower_quantile);

                double walking_time = (1-frac)*lower_quantile_value + frac*upper_quantile_value;

                return walking_time;
            }
        }


    }
    return -1;
}


bool Busstop::execute(Eventlist* eventlist, double time) // is executed by the eventlist and means a bus needs to be processed
{
  	// progress the vehicle when entering or exiting a stop
	bool bus_exit = false;
	Bustrip* exiting_trip;
	vector<pair<Bustrip*,double> >::iterator iter_departure;
	for (iter_departure = buses_currently_at_stop.begin(); iter_departure<buses_currently_at_stop.end(); iter_departure++)
	{
		if ((*iter_departure).second == time)
		{
			bus_exit = true;
			exiting_trip = (*iter_departure).first;
			break;
		}
	}
	if (bus_exit == true)
	// if there is an exiting bus
	{
		if(exiting_trip->get_holding_at_stop()) //David added 2016-05-26: the exiting trip is holding and needs to account for additional passengers that board during the holding period
		{
			passenger_activity_at_stop(eventlist,exiting_trip,time);
			exiting_trip->set_holding_at_stop(false); //exiting trip is not holding anymore
		}

		Vehicle* veh =  (Vehicle*)(exiting_trip->get_busv()); // so we can do vehicle operations
		free_length (exiting_trip->get_busv());
		double relative_length;
		// calculate the updated exit time from the link
		double ro =0.0;
		#ifdef _RUNNING     // if running segment is seperate density is calculated on that part only
			ro=bus->get_curr_link()->density_running(time);
		#else
			#ifdef _RUNNING_ONLY
				ro = veh->get_curr_link()->density_running_only(time);
			#else
			ro=bus->get_curr_link()->density();
			#endif	//_RUNNING_ONLY
		#endif  //_RUNNING

		double speed = veh->get_curr_link()->get_sdfunc()->speed(ro);
		double link_total_travel_time = veh->get_curr_link()->get_length()/speed ;

		#ifdef _USE_EXPECTED_DELAY
			double exp_delay = 1.44 * (queue->queue(time)) / bus->get_curr_link()->get_nr_lanes();
			exit_time = exit_time + exp_delay;
		#endif //_USE_EXPECTED_DELAY
		if (exiting_trip->check_end_trip() == false) // if there are more stops on the bus's route
		{
			Visit_stop* next_stop1 = *(exiting_trip->get_next_stop());
			if (veh->get_curr_link()->get_id() == (next_stop1->first->get_link_id())) // the next stop IS on this link
			{
				double stop_position = (next_stop1->first)->get_position();
				relative_length = (stop_position - position)/(veh->get_curr_link()->get_length()); // calculated for the interval between the two sequential stops
				double time_to_stop = time + link_total_travel_time * relative_length;
				exiting_trip->book_stop_visit (time_to_stop); // book  stop visit
			}
			else // the next stop is NOT on this link
			{
				relative_length = (veh->get_curr_link()->get_length()-position)/ veh->get_curr_link()->get_length(); // calculated for the remaining part of the link
				double exit_time = time + link_total_travel_time * relative_length;
				veh->set_exit_time(exit_time);
				if (veh->get_curr_link()->get_queue()->full() == false)
				{
					veh->get_curr_link()->get_queue()->enter_veh(veh);
				}
				else
				{
					cout << " Origin:: insertveh inputqueue->enterveh doesnt work " << endl;
	    			return false;
				}
			}
		}
		else // there are no more stops on this route
		{
			exiting_trip->get_line()->update_total_travel_time (exiting_trip, time);
			relative_length = (veh->get_curr_link()->get_length()-position)/ veh->get_curr_link()->get_length(); // calculated for the remaining part of the link
			double exit_time = time + link_total_travel_time * relative_length;
			veh->set_exit_time(exit_time);
			veh->get_curr_link()->get_queue()->enter_veh(veh);
			exiting_trip->get_busv()->set_on_trip(false); // indicate that there are no more stops on this route
			exiting_trip->advance_next_stop(exit_time, eventlist);
		}
		buses_currently_at_stop.erase(iter_departure);
		return true;
	}

	bool bus_enter = false;
	Bustrip* entering_trip;
	vector<pair<Bustrip*,double> >::iterator iter_arrival;
	for (iter_arrival = expected_bus_arrivals.begin(); iter_arrival<expected_bus_arrivals.end(); iter_arrival++)
	{
		if ((*iter_arrival).second == time)
		{
			bus_enter = true;
			entering_trip = (*iter_arrival).first;
			break;
		}
	}
	// if this is for a bus ENTERING the stop:
	if (bus_enter == true)
	{
		exit_time = calc_exiting_time(eventlist, entering_trip, time); // get the expected exit time according to dwell time calculations and time point considerations
		entering_trip->set_enter_time (time);
		pair<Bustrip*,double> exiting_trip;
		exiting_trip.first = entering_trip;
		exiting_trip.second = exit_time;
		occupy_length (entering_trip->get_busv());
		buses_currently_at_stop.push_back(exiting_trip);
		eventlist->add_event (exit_time, this); // book an event for the time it exits the stop
		record_busstop_visit (entering_trip, entering_trip->get_enter_time()); // document stop-related info
								// done BEFORE update_last_arrivals in order to calc the headway and BEFORE set_last_stop_exit_time
		entering_trip->get_busv()->record_busvehicle_location (entering_trip, this, entering_trip->get_enter_time());
		entering_trip->set_last_stop_exit_time(exit_time);
		entering_trip->set_last_stop_visited(this);
		update_last_arrivals (entering_trip, entering_trip->get_enter_time()); // in order to follow the arrival times (AFTER dwell time is calculated)
		update_last_departures (entering_trip, exit_time); // in order to follow the departure times (AFTER the dwell time and time point stuff)
		set_had_been_visited (entering_trip->get_line(), true);
		entering_trip->advance_next_stop(exit_time, eventlist);
		expected_bus_arrivals.erase(iter_arrival);
	}

	return true;
}

void Busstop::passenger_activity_at_stop(Eventlist* eventlist, Bustrip* trip, double time) //!< progress passengers at stop: waiting, boarding and alighting
{
	nr_boarding = 0;
	//for (int car_id = 1; car_id <= trip->get_busv->get_number_cars; car_id++) { nr_boarding.insert(std::pair<int, int>(car_id, 0)); }
	nr_alighting = 0;
	stops_rate stops_rate_dwell, stops_rate_coming, stops_rate_waiting;
	int starting_occupancy; // bus crowdedness factor
	starting_occupancy = trip->get_busv()->get_occupancy();

	if (theParameters->demand_format == 1) // demand is given in terms of arrival rates and alighting fractions
	{
		// generate waiting pass. and alight passengers
		nr_waiting[trip->get_line()] += random->poisson(((get_arrival_rates(trip)) * get_time_since_arrival(trip, time)) / 3600.0);
		//the arrival process follows a poisson distribution and the lambda is relative to the headway
		// with arrival time and the headway as the duration
		if (starting_occupancy > 0 && get_alighting_fractions(trip) > 0)
		{
			set_nr_alighting(random->binrandom(starting_occupancy, get_alighting_fractions(trip))); // the alighting process follows a binominal distribution
					// the number of trials is the number of passengers on board with the probability of the alighting fraction
		}
	}
	if (theParameters->demand_format == 10) // format 1 with time-dependent demand slices
	{
		// find the time that this slice started
		vector<double>::iterator curr_slice_start;
		bool simulation_start_slice = true;
		for (vector<double>::iterator iter_slices = update_rates_times[trip->get_line()].begin(); iter_slices != update_rates_times[trip->get_line()].end(); iter_slices++)
		{
			if (iter_slices == update_rates_times[trip->get_line()].begin() && time > (*iter_slices))
				// it is before the first slice
			{
				simulation_start_slice = false;
			}
			if ((iter_slices + 1) == update_rates_times[trip->get_line()].end() && time > (*iter_slices))
				// it is the last slice
			{
				curr_slice_start = iter_slices;
				break;
			}
			if ((*iter_slices) < time && (*(iter_slices + 1)) >= time)
			{
				curr_slice_start = iter_slices;
				break;
			}
		}
		if (simulation_start_slice == false && (time - get_time_since_arrival(trip, time) < (*curr_slice_start)))
			// if the previous bus from this line arrives on the previous slice - then take into account previous rate
		{
			double time_previous_slice = ((*curr_slice_start) - (time - get_time_since_arrival(trip, time)));
			double time_current_slice = time - (*curr_slice_start);
			nr_waiting[trip->get_line()] += random->poisson((previous_arrival_rates[trip->get_line()] * time_previous_slice) / 3600.0) + random->poisson(((get_arrival_rates(trip)) * time_current_slice) / 3600.0);
			// sum of two poisson processes - pass. arriving during the leftover of the previous lsice plus pass. arriving during the begining of this slice
			double weighted_alighting_fraction = (get_alighting_fractions(trip) * time_current_slice + previous_alighting_fractions[trip->get_line()] * time_previous_slice) / (time_current_slice + time_previous_slice);
			if (starting_occupancy > 0 && weighted_alighting_fraction > 0)
			{
				set_nr_alighting(random->binrandom(starting_occupancy, weighted_alighting_fraction)); // the alighting process follows a binominal distribution
					// the number of trials is the number of passengers on board with the probability of the weighted alighting fraction
			}
		}
		else // take into account only current rates
		{
			nr_waiting[trip->get_line()] += random->poisson(((get_arrival_rates(trip)) * get_time_since_arrival(trip, time)) / 3600.0);
			//the arrival process follows a poisson distribution and the lambda is relative to the headway
			// with arrival time and the headway as the duration
			if (starting_occupancy > 0 && get_alighting_fractions(trip) > 0)
			{
				set_nr_alighting(random->binrandom(starting_occupancy, get_alighting_fractions(trip))); // the alighting process follows a binominal distribution
					// the number of trials is the number of passengers on board with the probability of the alighting fraction
			}
		}
	}
	if (theParameters->demand_format == 2) // demand is given in terms of arrival rates per a pair of stops (no individual pass.)
	{
		// generate waiting pass. per pre-determined destination stop and alight passngers headed for this stop
		stops_rate_dwell = multi_arrival_rates[trip->get_line()];
		stops_rate_waiting = multi_nr_waiting[trip->get_line()];
		for (vector <Busstop*>::iterator destination_stop = trip->get_line()->stops.begin(); destination_stop < trip->get_line()->stops.end(); destination_stop++)
		{
			if (stops_rate_dwell[(*destination_stop)] != 0)
			{
				//2014-07-03 Jens changed from poisson to poisson1, poisson does not give correct answers!
				//stops_rate_coming[(*destination_stop)] = random -> poisson (stops_rate_dwell[(*destination_stop)] * get_time_since_arrival (trip, time) / 3600.0 ); // randomized the number of new-comers to board that the destination stop
				stops_rate_coming[(*destination_stop)] = random->poisson1(stops_rate_dwell[(*destination_stop)], get_time_since_arrival(trip, time) / 3600.0); // randomized the number of new-comers to board that the destination stop
				//trip->nr_expected_alighting[(*destination_stop)] += int (stops_rate_coming[(*destination_stop)]);
				stops_rate_waiting[(*destination_stop)] += int(stops_rate_coming[(*destination_stop)]); // the total number of passengers waiting for the destination stop is updated by adding the new-comers
				nr_waiting[trip->get_line()] += int(stops_rate_coming[(*destination_stop)]);
			}
		}
		multi_nr_waiting[trip->get_line()] = stops_rate_waiting;
		set_nr_alighting(trip->nr_expected_alighting[this]); // passengers heading for this stop alight
		trip->nr_expected_alighting[this] = 0;
	}
	if (theParameters->demand_format == 1 || theParameters->demand_format == 2 || theParameters->demand_format == 10) // in the case of non-individual passengers - boarding progress for waiting passengers (capacity constraints)
	{
		if (trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()) <= 0) //Added by Jens 2014-08-12, if capacity is exceeded no boarding should be allowed
		{
			set_nr_boarding(0);
		}
		else if (trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()) < nr_waiting[trip->get_line()]) // if the capcity determines the boarding process
		{
			if (theParameters->demand_format == 1 || theParameters->demand_format == 10)
			{
				set_nr_boarding(trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()));
				nr_waiting[trip->get_line()] -= nr_boarding;
			}
			if (theParameters->demand_format == 2)
			{
				double ratio = double(nr_waiting[trip->get_line()]) / (trip->get_busv()->get_capacity() - (starting_occupancy + get_nr_boarding() - get_nr_alighting()));
				for (vector <Busstop*>::iterator destination_stop = trip->get_line()->stops.begin(); destination_stop < trip->get_line()->stops.end(); destination_stop++)
					// allow only the ratio between supply and demand for boarding equally for all destination stops
				{
					if (stops_rate_dwell[(*destination_stop)] != 0)
					{
						int added_expected_passengers = Round(stops_rate_waiting[(*destination_stop)] / ratio);
						set_nr_boarding(get_nr_boarding() + added_expected_passengers);
						if (nr_boarding < 0)
						{
							cout << "Error! Nr of boarding passengers below zero for stop " << get_id();
						}
						trip->nr_expected_alighting[(*destination_stop)] += added_expected_passengers;
						//trip->nr_expected_alighting[(*destination_stop)] -= int(stops_rate_coming[(*destination_stop)]);
						stops_rate_waiting[(*destination_stop)] -= added_expected_passengers;
						nr_waiting[trip->get_line()] -= added_expected_passengers;
					}
				}
				multi_nr_waiting[trip->get_line()] = stops_rate_waiting;
			}
		}
		else // all waiting passengers for this busline can board it
		{
			set_nr_boarding(nr_waiting[trip->get_line()]);
			nr_waiting[trip->get_line()] = 0;
			if (theParameters->demand_format == 2)
			{
				// keep track of boarding and waiting passengers by destination
				for (vector <Busstop*>::iterator destination_stop = trip->get_line()->stops.begin(); destination_stop < trip->get_line()->stops.end(); destination_stop++)
				{
					trip->nr_expected_alighting[(*destination_stop)] += int(stops_rate_waiting[(*destination_stop)]);
					//trip->nr_expected_alighting[(*destination_stop)] -= int(stops_rate_coming[(*destination_stop)]);
					stops_rate_waiting[(*destination_stop)] = 0;
				}
				multi_nr_waiting[trip->get_line()] = stops_rate_waiting;
			}
		}
	}
	
	if (theParameters->demand_format == 3)   // demand is given in terms of arrival rate of individual passengers per OD of stops (future - route choice)
	{
		// Erik 18-09-16
		//int num_cars = trip->get_busv()->get_number_cars();
		//int nr_alighting = 0;
		//map<int, passengers> car_passengers = trip->passengers_on_board_car[this];
		//map<int, int> nr_alighting_car;
		
		for (int car_id = 1; car_id <= num_sections; ++car_id)
		{
			//nr_alighting += static_cast<int> (car_passengers[car_id].size());
			//nr_alighting_car[car_id] = static_cast<int> (trip->passengers_on_board_car[this][car_id].size());
			nr_alighting_section[car_id] = 0;

			//for (vector<passenger*>::iterator alight_it = car_passengers[car_id].begin(); alight_it != car_passengers[car_id].end(); ++alight_it)
			//{

			//}
		}
		// * Alighting passengers *


		nr_alighting = static_cast<int> (trip->passengers_on_board[this].size());
		int pass_car;

		for (vector <Passenger*> ::iterator alighting_passenger = trip->passengers_on_board[this].begin(); alighting_passenger != trip->passengers_on_board[this].end(); alighting_passenger++)
		{
			// Erik 18-09-16
			pass_car = (*alighting_passenger)->get_pass_car();
			//cout << "alight pass_car = " << pass_car << '\t';
			nr_alighting_section[pass_car] += 1;
			pair<Busstop*, double> stop_time;
			stop_time.first = this;
			stop_time.second = time;
			(*alighting_passenger)->add_to_selected_path_stop(stop_time);
			// Erik 18-09-16
			pair<int, double> section_time;
			section_time.first = pass_car; // Passenger alights to same platform as car
			section_time.second = time;			
			(*alighting_passenger)->add_to_selected_path_sections(section_time);
			// update experienced crowding on-board
			pair<double, double> riding_coeff;
			riding_coeff.first = time - trip->get_enter_time(); // refers to difference between departures
			riding_coeff.second = trip->find_crowding_coeff((*alighting_passenger));
			(*alighting_passenger)->add_to_experienced_crowding_levels(riding_coeff);
			//ODstops* od_stop = (*alighting_passenger)->get_OD_stop();
			ODstops* od_stop = (*alighting_passenger)->get_original_origin()->get_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination());
			od_stop->record_onboard_experience(*alighting_passenger, trip, this, riding_coeff);
			//set current stop as origin for alighting passenger's OD stop pair
			Busstop* next_stop = this;
			int next_section = pass_car;
			bool final_stop = false;
			ODstops* od;
			if (check_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination()) == false) //check if OD stop pair exists with this stop as origin to the destination of the passenger
			{
				od = new ODstops(next_stop, (*alighting_passenger)->get_OD_stop()->get_destination());
				add_odstops_as_origin((*alighting_passenger)->get_OD_stop()->get_destination(), od);
				(*alighting_passenger)->get_OD_stop()->get_destination()->add_odstops_as_destination(next_stop, od);
			}
			else
			{
				od = stop_as_origin[(*alighting_passenger)->get_OD_stop()->get_destination()];
			}
			(*alighting_passenger)->set_ODstop(od); // set this stop as passenger's new origin
			if (id == (*alighting_passenger)->get_OD_stop()->get_destination()->get_id() || (*alighting_passenger)->get_OD_stop()->check_path_set() == false) // if this stop is passenger's destination
			{
				// passenger has no further conection choice
				next_stop = this;
				final_stop = true;
				pair<Busstop*, double> stop_time;
				stop_time.first = this;
				stop_time.second = time;
				(*alighting_passenger)->add_to_selected_path_stop(stop_time);
				(*alighting_passenger)->set_end_time(time);
				//pass_recycler.addPassenger(*alighting_passenger); // terminate passenger
				// Erik 18-09-16: Not sure this is needed
				(*alighting_passenger)->add_to_selected_path_sections(section_time);
				(*alighting_passenger)->set_pass_section(pass_car);
			}
			if (final_stop == false)
			{
				// Erik 18-09-16: Connection decision should also return platform section
				pair<Busstop*, int> stop_section;
				stop_section = (*alighting_passenger)->make_connection_decision_2(time); // Erik 18-11-30: Make new connection decision!
				next_stop = stop_section.first;
				next_section = stop_section.second;
				// set connected_stop as the new origin
				ODstops* new_od;
				if (next_stop->check_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination()) == false)
				{
					new_od = new ODstops(next_stop, (*alighting_passenger)->get_OD_stop()->get_destination());
					next_stop->add_odstops_as_origin((*alighting_passenger)->get_OD_stop()->get_destination(), new_od);
					(*alighting_passenger)->get_OD_stop()->get_destination()->add_odstops_as_destination(next_stop, new_od);
				}
				else
				{
					new_od = next_stop->get_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination());
				}
				(*alighting_passenger)->set_ODstop(new_od); // set the connected stop as passenger's new origin (new OD)
				ODstops* odstop = (*alighting_passenger)->get_OD_stop();
				//if (odstop->get_waiting_passengers().size() != 0) //Why was it like this??
				if (true)  //Changed by Jens 2014-06-23
				{
					if (next_stop->get_id() == this->get_id() && pass_car == next_section)  // pass stays at the same stop and section
					{
						passengers wait_pass = odstop->get_waiting_passengers(); // add passanger's to the waiting queue on the new OD
						wait_pass.push_back(*alighting_passenger);
						odstop->set_waiting_passengers(wait_pass);
						(*alighting_passenger)->set_arrival_time_at_stop(time);
						pair<Busstop*, double> stop_time;
						stop_time.first = this;
						stop_time.second = time;
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
						// Erik 18-09-16
						(*alighting_passenger)->add_to_selected_path_sections(section_time);

						if ((*alighting_passenger)->get_pass_RTI_network_level() == true || this->get_rti() > 0)
						{
							vector<Busline*> lines_at_stop = this->get_lines();
							for (vector <Busline*>::iterator line_iter = lines_at_stop.begin(); line_iter < lines_at_stop.end(); line_iter++)
							{
								pair<Busstop*, Busline*> stopline;
								stopline.first = this;
								stopline.second = (*line_iter);
								(*alighting_passenger)->set_memory_projected_RTI(this, (*line_iter), (*line_iter)->time_till_next_arrival_at_stop_after_time(this, time));
								//(*alighting_passenger)->set_AWT_first_leg_boarding();
							}
						}
					}
					else  // pass walks to another stop and/or section Erik 18-11-27
					{
						// booking an event to the arrival time at the new stop

						double arrival_time_connected_stop_section = time + get_walking_time(pass_car, next_stop, next_section, time); // Erik 18-11-30
						//(*alighting_passenger)->execute(eventlist,arrival_time_connected_stop);
						eventlist->add_event(arrival_time_connected_stop_section, *alighting_passenger);
						pair<Busstop*, double> stop_time;
						stop_time.first = next_stop;
						stop_time.second = arrival_time_connected_stop_section;
						pair <int, double> section_time; // Erik 18-11-30
						section_time.first = next_section; // Erik 18-11-30
						section_time.second = arrival_time_connected_stop_section; // Erik 18-11-30
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
						(*alighting_passenger)->add_to_selected_path_sections(section_time); // Erik 18-11-30

					}
				}
				else
				{
					passengers wait_pass;
					wait_pass.push_back(*alighting_passenger);
					odstop->set_waiting_passengers(wait_pass);
					(*alighting_passenger)->add_to_selected_path_stop(stop_time);
				}
			}
		}
		trip->passengers_on_board[this].clear(); // clear passengers with this stop as their alighting stop

		// Erik 18-09-16
		//map<int, int> old_car_occupancy = trip->get_busv()->get_car_occupancy();
		//map<int, int> new_car_occupancy;

		for (int car_id = 1; car_id <= num_sections; ++car_id)
		{
			//nr_alighting += static_cast<int> (car_passengers[car_id].size());
			//trip->passengers_on_board_car[this][car_id].clear();

			//new_car_occupancy[car_id] = old_car_occupancy[car_id] - nr_alighting_car[car_id];
			trip->get_busv()->set_car_occupancy(car_id, trip->get_busv()->get_car_occupancy(car_id) - nr_alighting_section[car_id]);
			
			//cout << nr_alighting_section[car_id] << '\t';
		}
		//cout << endl;
		//trip->get_busv()->set_car_occupancy(new_car_occupancy);	// update occupancy on bus

//		trip->get_busv()->set_occupancy(trip->get_busv()->get_occupancy() - nr_alighting);	// update occupancy on bus

		// * Passengers on-board

		//int avialable_seats = trip->get_busv()->get_occupancy() - trip->get_busv()->get_number_seats();
		map <Busstop*, passengers> passengers_onboard = trip->get_passengers_on_board();
		//map <pair <Busstop*, int>, passengers> passengers_onboard = trip->get_passengers_on_board();
		bool next_stop = false;
		map <Busstop*, passengers>::iterator downstream_stops = passengers_onboard.end();
		//map <pair <Busstop*,int>, passengers>::iterator downstream_stops = passengers_onboard.end();
		downstream_stops--;
		//int pass_car; // Erik 18-09-26
		while (next_stop == false)
		{
			for (vector <Passenger*> ::iterator onboard_passenger = trip->passengers_on_board[(*downstream_stops).first].begin(); onboard_passenger != trip->passengers_on_board[(*downstream_stops).first].end(); onboard_passenger++)
			{
				// Erik 18-09-16
				pass_car = (*onboard_passenger)->get_pass_car();
				//cout << "passenger car: " << pass_car << endl;
				// update experienced crowding on-board
				pair<double, double> riding_coeff;
				riding_coeff.first = time - trip->get_enter_time(); // refers to difference between departures
				riding_coeff.second = trip->find_crowding_coeff((*onboard_passenger));
				(*onboard_passenger)->add_to_experienced_crowding_levels(riding_coeff);
				//ODstops* od_stop = (*onboard_passenger)->get_OD_stop();
				ODstops* od_stop = (*onboard_passenger)->get_original_origin()->get_stop_od_as_origin_per_stop((*onboard_passenger)->get_OD_stop()->get_destination());
				od_stop->record_onboard_experience(*onboard_passenger, trip, this, riding_coeff);
				// update sitting status - if a passenger stands and there is an available seat - allow sitting; sitting priority among pass. already on-board by remaning travel distance
				// Erik 18-09-16
				int available_seats = trip->get_busv()->get_car_occupancy(pass_car) < trip->get_busv()->get_car_number_seats();
				if (available_seats > 0 && (*onboard_passenger)->get_pass_sitting() == false)
				{
					(*onboard_passenger)->set_pass_sitting(true);
					available_seats--;
				}
			}
			if (downstream_stops == passengers_onboard.begin())
			{
				next_stop = true;
			}
			else
			{
				downstream_stops--;
			}
		}
		// * Boarding passengers *

		for (map <Busstop*, ODstops*>::iterator destination_stop = stop_as_origin.begin(); destination_stop != stop_as_origin.end(); destination_stop++)
		{
			// going through all the stops that this stop is their origin on a given OD pair
			passengers pass_waiting_od = (*destination_stop).second->get_waiting_passengers();
			if (pass_waiting_od.empty() == false) // if there are waiting passengers with this destination
			{
				passengers::iterator check_pass = pass_waiting_od.begin();
				Passenger* next_pass;
				bool last_waiting_pass = false;
				// Erik 18-09-16
				int pass_car;
				while (check_pass < pass_waiting_od.end())
				{
					pass_car = (*check_pass)->get_pass_section();
					//cout << "pass_car = " << pass_car << '\t';
					// progress each waiting passenger
					ODstops* this_od = this->get_stop_od_as_origin_per_stop((*check_pass)->get_OD_stop()->get_destination());
					(*check_pass)->set_ODstop(this_od);
					/*
					if ((*check_pass)->get_OD_stop()->get_origin()->get_id() != this->get_id())
					{
						break;
					}
					*/
					// Erik 18-09-17: Check: does function need modifying?
					if ((*check_pass)->make_boarding_decision(trip, time) == true)
					{
						// if the passenger decided to board this bus
						//if (trip->get_busv()->get_occupancy() < trip->get_busv()->get_capacity())
						// Erik 18-09-16: Modify this to consider nearby cars if current is full
						if (trip->get_busv()->get_car_occupancy(pass_car) < trip->get_busv()->get_car_capacity())
						{
							// if the bus is not completly full - then the passenger boards
							// currently - alighting decision is made when boarding
							(*check_pass)->record_waiting_experience(trip, time);
							nr_boarding++;
							//Erik 18-09-16
							nr_boarding_section[pass_car] += 1;
							pair<Bustrip*, double> trip_time;
							trip_time.first = trip;
							trip_time.second = time;
							(*check_pass)->add_to_selected_path_trips(trip_time);

							// Erik 18-09-16
							pair<int, double> car_time;
							car_time.first = pass_car;
							car_time.second = time;
							(*check_pass)->add_to_selected_path_cars(car_time);
							(*check_pass)->set_pass_car(pass_car);

							if (trip->get_busv()->get_car_occupancy(pass_car) > trip->get_busv()->get_car_number_seats()) // the passenger stands
							{
								(*check_pass)->set_pass_sitting(false);
							}
							else // the passenger sits
							{
								(*check_pass)->set_pass_sitting(true);
							}
							if (theParameters->demand_format == 3)
							{
								trip->passengers_on_board[(*check_pass)->make_alighting_decision(trip, time)].push_back((*check_pass));
							}

							// Sets the car_id to the passenger. Added by Melina
							//(*check_pass)->add_to_selected_car(make_pair(trip, trip->get_busv()->get_car_id()));

							//trip->get_busv()->set_occupancy(trip->get_busv()->get_occupancy() + 1);
							// Erik 18-09-16
							trip->get_busv()->set_car_occupancy(pass_car,trip->get_busv()->get_car_occupancy(pass_car) + 1);
							if (check_pass < pass_waiting_od.end() - 1)
							{
								check_pass++;
								next_pass = (*check_pass);
								pass_waiting_od.erase(check_pass - 1);
								check_pass = find(pass_waiting_od.begin(), pass_waiting_od.end(), next_pass);
							}
							else
							{
								last_waiting_pass = true;
								pass_waiting_od.erase(check_pass);
								break;
							}
						}
						else
						{
							// if the passenger CAN NOT board
							if ((*check_pass)->empty_denied_boarding() == true || this->get_id() != (*check_pass)->get_last_denied_boarding_stop_id()) // no double registration
							{
								pair<Busstop*, double> denied_boarding;
								denied_boarding.first = this;
								denied_boarding.second = time;
								(*check_pass)->add_to_denied_boarding(denied_boarding);
							}
							if (check_pass < pass_waiting_od.end() - 1)
							{
								check_pass++;
							}
							else
							{
								last_waiting_pass = true;
								break;
							}
						}
					}
					else
					{
						// if the passenger decides he DOES NOT WANT to board this bus
						if (check_pass < pass_waiting_od.end() - 1)
						{
							check_pass++;
						}
						else
						{
							last_waiting_pass = true;
							break;
						}
					}
				}
				(*destination_stop).second->set_waiting_passengers(pass_waiting_od); // updating the waiting list at the ODstops object (deleting boarding pass.)
				//cout << endl;
			}
		}
}
		if (theParameters->demand_format != 3)
		{
			trip->get_busv()->set_occupancy(starting_occupancy + get_nr_boarding() - get_nr_alighting()); // updating the occupancy
		}
		if (id != trip->stops.back()->first->get_id()) // if it is not the last stop for this trip
		{
			// Erik 18-09-16: Change to car-specific loads
			/*cout << trip->get_busv()->get_occupancy() << endl;
			for (int car_id = 1; car_id <= trip->get_busv()->get_number_cars(); ++car_id) {
				cout << trip->get_busv()->get_car_occupancy(car_id) << '\t';
			}
			cout << endl;*/
			trip->assign_car_segments[this] = trip->get_busv()->get_car_occupancy();
			trip->assign_segements[this] = trip->get_busv()->get_occupancy();
			trip->record_passenger_loads(trip->get_next_stop());
		}
}


double Busstop::get_walking_time(Busstop* next_stop, double curr_time)
{

    double walking_time;

    //get walking time from exogenous distribution, returns negative value if not available
    walking_time = estimate_walking_time_from_quantiles(next_stop,curr_time);

    //otherwise, infer walking time from walking distance
    if (walking_time < 0){
        walking_time = distances[next_stop] * 60 / random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4);
    }

    return walking_time;
}

// Erik 18-09-27
double Busstop::get_walking_time(int curr_section, Busstop* next_stop, int next_section, double curr_time)
{

	double walking_time;


	//get walking time from exogenous distribution, returns negative value if not available
	walking_time = estimate_walking_time_from_quantiles(next_stop, curr_time);

	//otherwise, infer walking time from walking distance
	if (walking_time < 0) {

		walking_time = get_walking_distance_stop_section(curr_section, next_stop, next_section) 
			* 60 / random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed / 4);

	}

	return walking_time;
}


// Erik 18-09-27
double Busstop::get_walking_distance_stop_section(int curr_section, Busstop* next_stop, int next_section)
{
	double walking_distance = 0.0;
	double curr_section_length = length / (double)num_sections;

	if (this->get_id() != next_stop->get_id())
	{
		double curr_section_diff = abs(shortest_walks[next_stop].first - curr_section);
		double next_section_diff = abs(shortest_walks[next_stop].second - next_section);
		double next_section_length = next_stop->get_length() / next_stop->get_num_sections();

		walking_distance = distances[next_stop];
		//cout << " from = " << this->get_id() << " curr_section = " << curr_section << " to = " << next_stop->get_id() << " next_section = " << next_section;
		//cout << " walking_distance = " << walking_distance << " sw1 = " << shortest_walks[next_stop].first << " sw2 " << shortest_walks[next_stop].second << endl;
		// Adds walking distance along each platform
		walking_distance += (curr_section_diff * curr_section_length + next_section_diff * next_section_length);// / 1000.0;
	}
	else 
	{
		double curr_section_diff = abs(next_section - curr_section);
		walking_distance = curr_section_diff * curr_section_length;// / 1000.0;
	}

	return walking_distance;
}


double Busstop::calc_dwelltime (Bustrip* trip)  //!< calculates the dwelltime of each bus serving this stop. currently includes: passenger service times ,out of stop, bay/lane
{
	// double crowdedness_ratio = 0;
	pair<double, double> crowding_factor;
	double dwell_constant, alighting_time, boarding_time;
	double boarding_front_door;
	bool crowded = 0;
	double time_front_door, time_rear_door, time_per_other_doors;
	/* Lin & Wilson version of dwell time function
	// calculating standees
	if (trip->get_busv()->get_occupancy() > trip->get_busv()->get_number_seats())	// Calculating alighting standees
	{
		alighting_standees = trip->get_busv()->get_occupancy() - trip->get_busv()->get_number_seats();
	}
	else
	{
		alighting_standees = 0;
	}
	if (trip->get_busv()->get_occupancy() > trip->get_busv()->get_number_seats()) // Calculating the boarding standess
	{
		boarding_standees = trip->get_busv()->get_occupancy() - trip->get_busv()->get_number_seats();
	}
	else
	{
		boarding_standees = 0;
	}
	loadfactor = get_nr_boarding() * alighting_standees + get_nr_alighting() * boarding_standees;
	dwelltime = (dwell_constant + boarding_coefficient*get_nr_boarding() + alighting_coefficient*get_nr_alighting()
		+ crowdedness_coefficient*loadfactor + get_bay() * bay_coefficient + out_of_stop_coefficient*out_of_stop); // Lin&Wilson (1992) + out of stop effect.

	// with these values
	double dwell_constant = 12.5; // Value of the constant component in the dwell time function.
	double boarding_coefficient = 0.55;	// Should be read as an input parameter. Would be different for low floor for example.
	double alighting_coefficient = 0.23;
	double crowdedness_coefficient = 0.0078;
	double out_of_stop_coefficient = 3.0; // Taking in consideration the increasing dwell time when bus stops out of the stop
	double bay_coefficient = 4.0;
	*/
    Dwell_time_function* dt_func = trip->get_bustype()->get_dt_function();
	dwell_constant = has_bay * dt_func->bay_coefficient + dt_func->over_stop_capacity_coefficient * check_out_of_stop(trip->get_busv()) + dt_func->dwell_constant;

	switch (dt_func->dwell_time_function_form)
	{
		case 11:
			dwelltime = dwell_constant + dt_func->boarding_coefficient * nr_boarding + dt_func->alighting_cofficient * nr_alighting;
			break;
		case 12:
			crowding_factor = trip->crowding_dt_factor(nr_boarding, nr_alighting);
			dwelltime = dwell_constant + dt_func->boarding_coefficient * crowding_factor.first * nr_boarding + dt_func->alighting_cofficient * crowding_factor.second * nr_alighting;
			break;
		case 13:
			dwelltime = dwell_constant + max(dt_func->boarding_coefficient * nr_boarding, dt_func->alighting_cofficient * nr_alighting);
			break;
		case 14:
			crowding_factor = trip->crowding_dt_factor(nr_boarding, nr_alighting);
			dwelltime = dwell_constant + max(dt_func->boarding_coefficient * crowding_factor.first * nr_boarding, dt_func->alighting_cofficient * crowding_factor.second * nr_alighting);
			break;
		case 15:
			crowding_factor = trip->crowding_dt_factor(nr_boarding, nr_alighting);
			alighting_time = dt_func->alighting_cofficient * crowding_factor.second * nr_alighting;
			boarding_time = dt_func->boarding_coefficient * crowding_factor.first * nr_boarding;
			if (alighting_time >= boarding_time)
			{
				dwelltime = dwell_constant + alighting_time;
			}
			else
			{
				boarding_front_door = alighting_time / (dt_func->boarding_coefficient * crowding_factor.first);
				dwelltime = dwell_constant + alighting_time + random->urandom(0.6, 1.0) * dt_func->boarding_coefficient * crowding_factor.first * (nr_boarding - boarding_front_door);
			}
			break;
		case 21:
			if (trip->get_busv()->get_occupancy() > trip->get_busv()->get_number_seats())
			{
				crowded = 1;
			}
			time_front_door = dt_func->boarding_coefficient * nr_boarding + dt_func->alighting_cofficient * dt_func->share_alighting_front_door * nr_alighting + dt_func->crowdedness_binary_factor * crowded * nr_boarding;
			time_rear_door = dt_func->alighting_cofficient * (1-dt_func->share_alighting_front_door) * nr_alighting;
			dwelltime = has_bay * dt_func->bay_coefficient + dt_func->over_stop_capacity_coefficient * check_out_of_stop(trip->get_busv()) + max(time_front_door, time_rear_door) + dt_func->dwell_constant;
			break;
		case 22:
			if (trip->get_busv()->get_occupancy() > trip->get_busv()->get_number_seats())
			{
				crowded = 1;
			}
			time_front_door = (dt_func->boarding_coefficient * nr_boarding)/dt_func->number_boarding_doors + dt_func->alighting_cofficient * dt_func->share_alighting_front_door * nr_alighting + dt_func->crowdedness_binary_factor * crowded * nr_boarding;
			time_per_other_doors = (dt_func->boarding_coefficient * nr_boarding)/dt_func->number_boarding_doors + (dt_func->alighting_cofficient * (1-dt_func->share_alighting_front_door) * nr_alighting)/dt_func->number_alighting_doors;
			dwelltime = has_bay * dt_func->bay_coefficient + dt_func->over_stop_capacity_coefficient * check_out_of_stop(trip->get_busv()) + max(time_front_door, time_per_other_doors) + dt_func->dwell_constant;
			break;
	}
	dwelltime = max (dwelltime + random->nrandom(0, dt_func->dwell_std_error), dt_func->dwell_constant + min_DT + random->nrandom(0, dt_func->dwell_std_error));
	return dwelltime;
}

double Busstop::find_exit_time_bus_in_front () //Fixed by Jens 2014-08-26
{
	if (buses_currently_at_stop.empty() == true)
	{
		return 0;
	}
	else
	{
		vector<pair<Bustrip*,double> >::iterator iter_bus = buses_currently_at_stop.end();
		iter_bus--;
		return iter_bus->second;
	}
}

void Busstop::occupy_length (Bus* bus) // a bus arrived - decrease the left space at the stop
{
	double space_between_buses = 0.5; // the reasonable space between stoping buses, input parameter - IMPLEMENT: shouldn't be for first bus at the stop
	set_avaliable_length (get_avaliable_length() - bus->get_length() - space_between_buses);
}

void Busstop::free_length (Bus* bus) // a bus left - increase the left space at the stop
{
	double space_between_buses = 0.5; // the reasonable space between stoping buses
	set_avaliable_length  (get_avaliable_length() + bus->get_length() + space_between_buses);
}

bool Busstop::check_out_of_stop (Bus* bus) // checks if there is any space left for the bus at the stop
{
	if (bus->get_length() > get_avaliable_length())
	{
		return true; // no left space for the bus at the stop. IMPLEMENT: generate incidence (capacity reduction)
	}
	else
	{
		return false; // there is left space for the bus at the stop
	}
}

void Busstop::update_last_arrivals (Bustrip* trip, double time) // everytime a bus ENTERS a stop it should be updated,
// in order to keep an updated vector of the last arrivals from each line.
// the time paramater which is sent is the enter_time, cause headways are defined as the differnece in time between sequential arrivals
{
	pair <Bustrip*, double> trip_time;
	trip_time.first = trip;
	trip_time.second = time;
	last_arrivals [trip->get_line()] = trip_time;
}

void Busstop::update_last_departures (Bustrip* trip, double time) // everytime a bus EXITS a stop it should be updated,
// in order to keep an updated vector of the last deparures from each line.
// the time paramater which is sent is the exit_time, cause headways are defined as the differnece in time between sequential arrivals
{
	pair <Bustrip*, double> trip_time;
	trip_time.first = trip;
	trip_time.second = time;
	last_departures [trip->get_line()] = trip_time;
}

double Busstop::get_time_since_arrival (Bustrip* trip, double time) // calculates the headway (between arrivals)
{
	if (last_arrivals.empty()) //Added by Jens 2014-07-03
		return trip->get_line()->calc_curr_line_headway();
	double time_since_arrival = time - last_arrivals[trip->get_line()].second;
	// the headway is defined as the differnece in time between sequential arrivals
	return time_since_arrival;
}

double Busstop::get_time_since_departure (Bustrip* trip, double time) // calculates the headway (between departures)
{
	if (trip->get_line()->check_first_trip(trip)==true && trip->get_line()->check_first_stop(this)==true) // for the first stop on the first trip on that line - use the planned headway value
	{
		return trip->get_line()->calc_curr_line_headway();
	}
	double time_since_departure = time - last_departures[trip->get_line()].second;
	// the headway is defined as the differnece in time between sequential departures
	return time_since_departure;
}

double Busstop::calc_exiting_time (Eventlist* eventlist, Bustrip* trip, double time)
{
	passenger_activity_at_stop (eventlist, trip,time); //progress passengers at stop (waiting, boarding, alighting)
	dwelltime = calc_dwelltime(trip);

	if (can_overtake == false) // if the buses can't overtake at this stop - then it depends on the bus in front
	{
		dwelltime = max(dwelltime, find_exit_time_bus_in_front() - time + 1.0);
	}
	double holding_departure_time = 0;
	//check if holding strategy is used for this trip
	if (trip->get_complying() == true)
	{
		holding_departure_time = calc_holding_departure_time(trip, time); //David added 2016-04-01
	}
	double ready_to_depart = max(time + dwelltime, holding_departure_time);

	if(theParameters->demand_format == 1 || theParameters->demand_format == 2 || theParameters->demand_format == 10)
	{
		if(ready_to_depart > time + dwelltime)
		{
			// account for additional passengers that board while the bus is held at the time point
			double holding_time = last_departures[trip->get_line()].second - time - dwelltime;
			int additional_boarding = random -> poisson ((get_arrival_rates (trip)) * holding_time / 3600.0 );
			nr_boarding += additional_boarding;
			int curr_occupancy = trip->get_busv()->get_occupancy();
			trip->get_busv()->set_occupancy(curr_occupancy + additional_boarding); // Updating the occupancy
		}
	}

	if(theParameters->demand_format == 3)
	{
		if(ready_to_depart > time + dwelltime)
		{
			trip->set_holding_at_stop(true); //David added 2016-05-26: set indicator that additional passengers that board whil bus is held at time point need to be accounting for at the bus's exit time
		}
	}

	return ready_to_depart;
}

double Busstop::calc_holding_departure_time (Bustrip* trip, double time)
{
	switch (trip->get_line()->get_holding_strategy())
	{
		// for no control:
		case 0:
			return time + dwelltime; // since it isn't a time-point stop, it will simply exit after dwell time
		// for headway based (looking on headway from preceding bus):
		case 1:
			if (trip->get_line()->is_line_timepoint(this) == true) // if it is a time point
			{
				double holding_departure_time = last_departures[trip->get_line()].second + (trip->get_line()->calc_curr_line_headway() * trip->get_line()->get_max_headway_holding());

				return holding_departure_time;
			}
			break;
		// for schedule based:
		case 2:
			if (trip->get_line()->is_line_timepoint(this) == true) // if it is a time point
			{
				double scheduled_departure_time = time + (trip->scheduled_arrival_time(this)-time)* trip->get_line()->get_max_headway_holding();

				return scheduled_departure_time;
			}
			break;
		// for headway based (looking on headway from proceeding bus)
		case 3:
			if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false) // if it is a time point and it is not the last trip
			{
				Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
				vector <Visit_stop*> :: iterator& next_trip_next_stop = next_trip->get_next_stop();
				if ((*next_trip_next_stop)->first != trip->get_line()->stops.front()) // in case the next trip already started wise- no holding (otherwise - no base for holding)
				{
					vector <Visit_stop*> :: iterator curr_stop; // hold the scheduled time for this trip at this stop
					for (vector <Visit_stop*> :: iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
					{
						if ((*trip_stops)->first->get_id() == this->get_id())
						{
							curr_stop = trip_stops;
							break;
						}
					}
					double expected_next_arrival = (*(next_trip_next_stop-1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop-1))->second; // time at last stop + scheduled travel time between stops
					double holding_departure_time = expected_next_arrival - (trip->get_line()->calc_curr_line_headway_forward() * (2 - trip->get_line()->get_max_headway_holding())); // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio

					return holding_departure_time;
				}
			}
			break;
			// for headway-based (looking both backward and forward and averaging)
			case 4:
				if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false && trip->get_line()->check_first_trip(trip) == false) // if it is a time point and it is not the first or last trip
				{
						Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
						Bustrip* previous_trip = trip->get_line()->get_previous_trip(trip);
						vector <Visit_stop*> :: iterator& next_trip_next_stop = next_trip->get_next_stop();
						if (next_trip->check_end_trip() == false && (*next_trip_next_stop)->first != trip->get_line()->stops.front()&& previous_trip->check_end_trip() == false) // in case the next trip already started and the previous trip hadn't finished yet(otherwise - no base for holding)
						{
							vector <Visit_stop*> :: iterator curr_stop; // hold the scheduled time for this trip at this stop
							for (vector <Visit_stop*> :: iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
							{
								if ((*trip_stops)->first->get_id() == this->get_id())
								{
									curr_stop = trip_stops;
									break;
								}
							}
							double expected_next_arrival = (*(next_trip_next_stop-1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop-1))->second; // time at last stop + scheduled travel time between stops
							double average_curr_headway = ((expected_next_arrival - time) + (time - last_departures[trip->get_line()].second))/2; // average of the headway in front and behind
							// double average_planned_headway = (trip->get_line()->calc_curr_line_headway_forward() + trip->get_line()->calc_curr_line_headway())/2;
							double holding_departure_time = last_departures[trip->get_line()].second + average_curr_headway; // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio

							return holding_departure_time;
						}
				}
				break;
			// for headway-based (looking both backward and forward and averaging) with max. according to planned headway
			case 5:
				if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false && trip->get_line()->check_first_trip(trip) == false) // if it is a time point and it is not the first or last trip
				{
						Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
						Bustrip* previous_trip = trip->get_line()->get_previous_trip(trip);
						vector <Visit_stop*> :: iterator& next_trip_next_stop = next_trip->get_next_stop();
						if (next_trip->check_end_trip() == false && (*next_trip_next_stop)->first != trip->get_line()->stops.front()&& previous_trip->check_end_trip() == false) // in case the next trip already started and the previous trip hadn't finished yet(otherwise - no base for holding)
						{
							vector <Visit_stop*> :: iterator curr_stop; // hold the scheduled time for this trip at this stop
							for (vector <Visit_stop*> :: iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
							{
								if ((*trip_stops)->first->get_id() == this->get_id())
								{
									curr_stop = trip_stops;
									break;
								}
							}
							double expected_next_arrival = (*(next_trip_next_stop-1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop-1))->second; // time at last stop + scheduled travel time between stops
							double average_curr_headway = ((expected_next_arrival - time) + (time - last_departures[trip->get_line()].second))/2; // average of the headway in front and behind
							// double average_planned_headway = (trip->get_line()->calc_curr_line_headway_forward() + trip->get_line()->calc_curr_line_headway())/2;
							double holding_departure_time = min(last_departures[trip->get_line()].second + average_curr_headway, time + (trip->get_line()->calc_curr_line_headway() * trip->get_line()->get_max_headway_holding())); // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio

							return holding_departure_time;
					}
				}
				break;
			// for headway-based (looking both backward and forward and averaging) with max. according to pre-specified max holding time [sec]
			case 6:
				if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false && trip->get_line()->check_first_trip(trip) == false) // if it is a time point and it is not the first or last trip
				{
						Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
						Bustrip* previous_trip = trip->get_line()->get_previous_trip(trip);
						vector <Visit_stop*> :: iterator& next_trip_next_stop = next_trip->get_next_stop();
						if (next_trip->check_end_trip() == false && (*next_trip_next_stop)->first != trip->get_line()->stops.front()&& previous_trip->check_end_trip() == false) // in case the next trip already started and the previous trip hadn't finished yet(otherwise - no base for holding)
						{
							vector <Visit_stop*> :: iterator curr_stop; // hold the scheduled time for this trip at this stop
							for (vector <Visit_stop*> :: iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
							{
								if ((*trip_stops)->first->get_id() == this->get_id())
								{
									curr_stop = trip_stops;
									break;
								}
							}
							double expected_next_arrival = (*(next_trip_next_stop-1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop-1))->second; // time at last stop + scheduled travel time between stops
							double average_curr_headway = ((expected_next_arrival - time) + (time - last_departures[trip->get_line()].second))/2; // average of the headway in front and behind
							double holding_departure_time = min(last_departures[trip->get_line()].second + average_curr_headway, time + trip->get_line()->get_max_headway_holding()); // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio
						return holding_departure_time;
						}
				}
				break;
			// Rule-based headway-based control with passenger load to boarding ratio (based on Erik's formulation)
			// basically copying case 5 and then substracting the passenegr ratio term
			case 9:
				if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false && trip->get_line()->check_first_trip(trip) == false) // if it is a time point and it is not the first or last trip
				{
					Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
					Bustrip* previous_trip = trip->get_line()->get_previous_trip(trip);
					vector <Visit_stop*> :: iterator& next_trip_next_stop = next_trip->get_next_stop();
					if (next_trip->check_end_trip() == false && (*next_trip_next_stop)->first != trip->get_line()->stops.front()&& previous_trip->check_end_trip() == false) // in case the next trip already started and the previous trip hadn't finished yet(otherwise - no base for holding)
					{
						vector <Visit_stop*> :: iterator curr_stop; // hold the scheduled time for this trip at this stop
						for (vector <Visit_stop*> :: iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
						{
							if ((*trip_stops)->first->get_id() == this->get_id())
							{
								curr_stop = trip_stops;
								break;
							}
						}
						double expected_next_arrival = (*(next_trip_next_stop-1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop-1))->second; // time at last stop + scheduled travel time between stops
						double average_curr_headway = ((expected_next_arrival - time) + (time - last_departures[trip->get_line()].second))/2; // average of the headway in front and behind
						// double average_planned_headway = (trip->get_line()->calc_curr_line_headway_forward() + trip->get_line()->calc_curr_line_headway())/2;
						double pass_ratio;

						// vector <Visit_stop*> :: iterator& next_stop = trip->get_next_stop(); // finding the arrival rate (lambda) at the next stop along this line
						double sum_arrival_rate_next_stops = 0.0;
						for (vector <Visit_stop*> :: iterator trip_stops = trip->get_next_stop(); trip_stops < trip->stops.end(); trip_stops++)
						{
							sum_arrival_rate_next_stops += (*trip_stops)->first->get_arrival_rates(trip);
						}
						if (sum_arrival_rate_next_stops == 0)
						{
							pass_ratio = 0.0;
						}
						else
						{
							sum_arrival_rate_next_stops = sum_arrival_rate_next_stops / 3600;
							pass_ratio = (trip->get_busv()->get_occupancy() - nr_alighting + nr_boarding) / (2 * 2 * sum_arrival_rate_next_stops);
						}
						double holding_departure_time = min(last_departures[trip->get_line()].second + average_curr_headway - (pass_ratio), last_departures[trip->get_line()].second + (trip->get_line()->calc_curr_line_headway() * trip->get_line()->get_max_headway_holding())); // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio

						return holding_departure_time;
					}
				}
				break;
			case 20: // case of holding and speed adjustment; holding itself is the same as Case 6
				if (trip->get_line()->is_line_timepoint(this) == true && trip->get_line()->check_last_trip(trip) == false && trip->get_line()->check_first_trip(trip) == false) // if it is a time point and it is not the first or last trip
				{
					Bustrip* next_trip = trip->get_line()->get_next_trip(trip);
					Bustrip* previous_trip = trip->get_line()->get_previous_trip(trip);
					vector <Visit_stop*> ::iterator& next_trip_next_stop = next_trip->get_next_stop();
					if (next_trip->check_end_trip() == false && (*next_trip_next_stop)->first != trip->get_line()->stops.front() && previous_trip->check_end_trip() == false) // in case the next trip already started and the previous trip hadn't finished yet(otherwise - no base for holding)
					{
						vector <Visit_stop*> ::iterator curr_stop; // hold the scheduled time for this trip at this stop
						for (vector <Visit_stop*> ::iterator trip_stops = next_trip->stops.begin(); trip_stops < next_trip->stops.end(); trip_stops++)
						{
							if ((*trip_stops)->first->get_id() == this->get_id())
							{
								curr_stop = trip_stops;
								break;
							}
						}
						double expected_next_arrival = (*(next_trip_next_stop - 1))->first->get_last_departure(trip->get_line()) + (*curr_stop)->second - (*(next_trip_next_stop - 1))->second; // time at last stop + scheduled travel time between stops
						double average_curr_headway = ((expected_next_arrival - time) + (time - last_departures[trip->get_line()].second)) / 2; // average of the headway in front and behind
						double holding_departure_time = min(last_departures[trip->get_line()].second + average_curr_headway, time + trip->get_line()->get_max_headway_holding()); // headway ratio means here how tolerant we are to exceed the gap (1+(1-ratio)) -> 2-ratio
						return holding_departure_time;
					}
				}
				break;
		default:
			return time + dwelltime;
	}
	return time + dwelltime;
}

int Busstop::calc_total_nr_waiting ()
{
	int total_nr_waiting = 0;
	// formats 1 and 2
	if (theParameters->demand_format == 1 || theParameters->demand_format == 2)
	{
		for (vector<Busline*>::iterator lines_at_stop = lines.begin(); lines_at_stop < lines.end(); lines_at_stop++)
		{
			total_nr_waiting += nr_waiting[(*lines_at_stop)];
		}
	}
	// format 3
	else if (theParameters->demand_format == 3)
	{
		for (map <Busstop*, ODstops*>::iterator destination_stop = stop_as_origin.begin(); destination_stop != stop_as_origin.end(); destination_stop++)
				// going through all the stops that this stop is their origin on a given OD pair
		{
			total_nr_waiting += static_cast<int>((*destination_stop).second->get_waiting_passengers().size());
		}
	}
return total_nr_waiting;
}

void Busstop::record_busstop_visit (Bustrip* trip, double enter_time)  // creates a log-file for stop-related info
{
	double arrival_headway = get_time_since_arrival (trip , enter_time);
	//int starting_occupancy;
	int occupancy = trip->get_busv()->get_occupancy();
	map<int, int> car_occupancy = trip->get_busv()->get_car_occupancy(); // Erik 18-09-23
	int nr_riders = occupancy + nr_alighting - nr_boarding;
	map<int, int> nr_car_riders;
	double riding_time;
	double holdingtime = exit_time - enter_time - dwelltime;
	double crowded_pass_riding_time;
	double crowded_pass_dwell_time;
	double crowded_pass_holding_time;

	// Erik 18-09-23
	for (int car_id = 1; car_id <= trip->get_busv()->get_number_cars(); ++car_id) 
	{
		nr_car_riders[car_id] = car_occupancy[car_id] + nr_alighting_section[car_id] - nr_boarding_section[car_id];
	}

	if (trip->get_line()->check_first_stop(this) == true)
	{
		riding_time = 0;
		crowded_pass_riding_time = 0;
		crowded_pass_dwell_time = 0;
		crowded_pass_holding_time = 0;
	}
	else
	{
		// Erik 18-11-26: Updated with car-specific crowding
		riding_time = enter_time - trip->get_last_stop_exit_time();
		int nr_car_seats = trip->get_busv()->get_car_number_seats();
		//int nr_seats = trip->get_busv()->get_number_seats();
		crowded_pass_riding_time = 0;
		crowded_pass_dwell_time = 0;
		crowded_pass_holding_time = 0;
		for (int car_id = 1; car_id <= trip->get_busv()->get_number_cars(); ++car_id)
		{
			crowded_pass_riding_time  += calc_crowded_travel_time(riding_time, nr_car_riders[car_id], nr_car_seats);
			crowded_pass_dwell_time   += calc_crowded_travel_time(dwelltime, car_occupancy[car_id], nr_car_seats);
			crowded_pass_holding_time += calc_crowded_travel_time(holdingtime, car_occupancy[car_id], nr_car_seats);
		}
		//crowded_pass_riding_time = calc_crowded_travel_time(riding_time, nr_riders, nr_car_seats);
		//crowded_pass_dwell_time = calc_crowded_travel_time(dwelltime, occupancy, nr_car_seats);
		//crowded_pass_dwell_time = calc_crowded_travel_time(dwelltime, trip->get_busv()->get_occupancy(), nr_seats);
		//crowded_pass_holding_time = calc_crowded_travel_time(holdingtime, occupancy, nr_car_seats);
		//crowded_pass_holding_time = calc_crowded_travel_time(holdingtime, trip->get_busv()->get_occupancy(), nr_seats);
	}

	if (trip->get_line()->check_first_trip(trip) == true)
	{
		arrival_headway = 0;
	}
	output_stop_visits.push_back(Busstop_Visit(trip->get_line()->get_id(), trip->get_id() , trip->get_busv()->get_bus_id() , get_id() , get_name(), enter_time,
		trip->scheduled_arrival_time (this),dwelltime,(enter_time - trip->scheduled_arrival_time (this)), exit_time, riding_time, riding_time * nr_riders, crowded_pass_riding_time, crowded_pass_dwell_time, crowded_pass_holding_time,
		arrival_headway, get_time_since_departure (trip , exit_time), nr_alighting , nr_boarding , occupancy, car_occupancy, calc_total_nr_waiting(), (arrival_headway * nr_boarding)/2, holdingtime));
}

double Busstop::calc_crowded_travel_time (double travel_time, int nr_riders, int nr_seats) //Returns the sum of the travel time weighted by the crowding factors
{
	double crowded_travel_time;
	double load_factor = nr_riders / nr_seats;

	if (load_factor < 1) //if everyone had a seat
	{
		crowded_travel_time = travel_time * nr_riders * Bustrip::find_crowding_coeff(true, load_factor);
	}
	else
	{
		int nr_standees = nr_riders - nr_seats;
		crowded_travel_time = travel_time * (nr_seats * Bustrip::find_crowding_coeff(true, load_factor) + nr_standees * Bustrip::find_crowding_coeff(false, load_factor));
	}

	return crowded_travel_time;
}

void Busstop::write_output(ostream & out)
{
	for (list <Busstop_Visit>::iterator iter = output_stop_visits.begin(); iter!=output_stop_visits.end();iter++)
	{
		iter->write(out);
	}
}

void Busstop::calculate_sum_output_stop_per_line(int line_id)
{
	int counter = 0;
	// initialize all output measures
	Busline* bl=(*(find_if(lines.begin(), lines.end(), compare <Busline> (line_id) )));
	vector<Start_trip> trips = bl->get_trips();
	output_summary[line_id].stop_avg_headway = 0;
	output_summary[line_id].stop_avg_DT = 0;
	output_summary[line_id].stop_avg_abs_deviation = 0;
	output_summary[line_id].stop_total_boarding = 0;
	output_summary[line_id].stop_avg_waiting_per_stop = 0;
	output_summary[line_id].stop_sd_headway = 0;
	output_summary[line_id].stop_sd_DT = 0;
	output_summary[line_id].stop_on_time = 0;
	output_summary[line_id].stop_early = 0;
	output_summary[line_id].stop_late = 0;
	output_summary[line_id].total_stop_pass_riding_time = 0;
	output_summary[line_id].total_stop_pass_dwell_time = 0;
	output_summary[line_id].total_stop_pass_waiting_time = 0;
	output_summary[line_id].total_stop_pass_holding_time = 0;
	output_summary[line_id].stop_avg_holding_time = 0;
	output_summary[line_id].total_stop_travel_time_crowding = 0;

	for (list <Busstop_Visit>::iterator iter1 = output_stop_visits.begin(); iter1!=output_stop_visits.end();iter1++)
	{
		if ((*iter1).line_id == line_id)
			{
				// accumulating all the measures
				counter++; // should equal the total number of trips for this bus line passing at this bus stop
				if (trips.size()>2)
				{
					vector<Start_trip>::iterator iter = trips.begin();
					if ((*iter1).trip_id != (*iter).first->get_id())
					{
						iter++;
						if ((*iter1).trip_id != (*iter).first->get_id())
						{
							output_summary[line_id].stop_avg_headway += (*iter1).time_since_dep;
						}
					}
				}
				else
				{
					output_summary[line_id].stop_avg_headway += (*iter1).time_since_dep;
				}
				output_summary[line_id].stop_avg_DT += (*iter1).dwell_time;
				output_summary[line_id].stop_avg_abs_deviation += abs((*iter1).lateness);
				output_summary[line_id].stop_total_boarding += (*iter1).nr_boarding;
				output_summary[line_id].stop_avg_waiting_per_stop += (*iter1).nr_waiting;
				output_summary[line_id].total_stop_pass_riding_time += (*iter1).riding_pass_time;
				output_summary[line_id].total_stop_pass_dwell_time += (*iter1).dwell_time * (*iter1).occupancy;
				output_summary[line_id].total_stop_pass_waiting_time += ((*iter1).time_since_arr * (*iter1).nr_boarding) / 2;
				output_summary[line_id].total_stop_pass_holding_time += (*iter1).holding_time * (*iter1).occupancy;
				//output_summary[line_id].total_stop_pass_holding_time += (*iter1).holding_time;
				output_summary[line_id].total_stop_travel_time_crowding += (*iter1).crowded_pass_riding_time + (*iter1).crowded_pass_dwell_time + (*iter1).crowded_pass_holding_time;
				if ((*iter1).lateness > 300)
				{
					output_summary[line_id].stop_late ++;
				}
				else if ((*iter1).lateness < -60)
				{
					output_summary[line_id].stop_early ++;
				}
				else
				{
					output_summary[line_id].stop_on_time ++;
				}
			}
	}
	// dividing all the average measures by the number of records
	if (trips.size()>2)
	{
		output_summary[line_id].stop_avg_headway = output_summary[line_id].stop_avg_headway/(counter-2);
	}
	else
	{
		output_summary[line_id].stop_avg_headway = output_summary[line_id].stop_avg_headway/(counter);
	}
	output_summary[line_id].stop_avg_DT = output_summary[line_id].stop_avg_DT/counter;
	output_summary[line_id].stop_avg_abs_deviation = output_summary[line_id].stop_avg_abs_deviation/counter;
	output_summary[line_id].stop_total_boarding = output_summary[line_id].stop_total_boarding/counter;
	output_summary[line_id].stop_avg_waiting_per_stop = output_summary[line_id].stop_avg_waiting_per_stop/counter;
	output_summary[line_id].stop_on_time = output_summary[line_id].stop_on_time/counter;
	output_summary[line_id].stop_early = output_summary[line_id].stop_early/counter;
	output_summary[line_id].stop_late = output_summary[line_id].stop_late/counter;
	output_summary[line_id].stop_avg_holding_time = output_summary[line_id].stop_avg_holding_time/counter;

	// now go over again for SD calculations
	for (list <Busstop_Visit>::iterator iter1 = output_stop_visits.begin(); iter1!=output_stop_visits.end();iter1++)
	{
		if ((*iter1).line_id == line_id)
		{
			vector<Start_trip>::iterator iter = trips.begin();
			if (trips.size()>2)
			{
				if ((*iter1).trip_id != (*iter).first->get_id())
				{
					iter++;
					if ((*iter1).trip_id != (*iter).first->get_id())
					{
						output_summary[line_id].stop_sd_headway += pow ((*iter1).time_since_dep - output_summary[line_id].stop_avg_headway, 2);
					}
				}
			}
			else
			{
				output_summary[line_id].stop_sd_headway += pow ((*iter1).time_since_dep - output_summary[line_id].stop_avg_headway, 2);
			}
			output_summary[line_id].stop_sd_DT += pow ((*iter1).dwell_time - output_summary[line_id].stop_avg_DT, 2);
		}
	}
	// finish calculating all the SD measures
	if (trips.size()>2)
	{
		output_summary[line_id].stop_sd_headway = sqrt(output_summary[line_id].stop_sd_headway/(counter-3));
	}
	else
	{
		output_summary[line_id].stop_sd_headway = sqrt(output_summary[line_id].stop_sd_headway/(counter-1));
	}
	output_summary[line_id].stop_sd_DT = sqrt(output_summary[line_id].stop_sd_DT/(counter-1));
}

bool Busstop::check_walkable_stop ( Busstop* const & stop)
{
	if (distances.count(stop) > 0)
	{
		return true;
	}
	return false;
}

bool Busstop::check_destination_stop (Busstop* stop)
{
	if (stop_as_origin.count(stop) > 0)
	{
		return true;
	}
	return false;
}

bool Busstop::is_awaiting_transfers(Bustrip* trip)
{
	int trip_id = trip->get_id();
	for(auto& trip_awaiting_transfer : trips_awaiting_transfers)
	{
		if(trip_awaiting_transfer.first->get_id() == trip_id)
		{
			return true;
		}
	}
	return false;
}


Change_arrival_rate::Change_arrival_rate(double time)
{
	loadtime = time;
}

void Change_arrival_rate::book_update_arrival_rates (Eventlist* eventlist, double time)
{
	eventlist->add_event(time,this);
}

bool Change_arrival_rate::execute(Eventlist* eventlist, double time) //variables are not used, but kept for auto-completion
{
    Q_UNUSED (eventlist);
    Q_UNUSED (time);
	for (TD_demand::iterator stop_iter = arrival_rates_TD.begin(); stop_iter != arrival_rates_TD.end(); stop_iter++)
	{
		map<Busline*,double> td_line = (*stop_iter).second;
		(*stop_iter).first->save_previous_arrival_rates ();
		for (map<Busline*,double>::iterator line_iter = td_line.begin(); line_iter != td_line.end(); line_iter++)
		{
			(*stop_iter).first->add_line_nr_boarding((*line_iter).first,(*line_iter).second);
		}
	}
	for (TD_demand::iterator stop_iter = alighting_fractions_TD.begin(); stop_iter != alighting_fractions_TD.end(); stop_iter++)
	{
		map<Busline*,double> td_line = (*stop_iter).second;
		(*stop_iter).first->save_previous_alighting_fractions ();
		for (map<Busline*,double>::iterator line_iter = td_line.begin(); line_iter != td_line.end(); line_iter++)
		{
			(*stop_iter).first->add_line_nr_alighting((*line_iter).first,(*line_iter).second);
		}
	}
	return true;
}

Walking_time_dist::Walking_time_dist(Busstop* dest_stop_, vector<double> quantiles_, vector<double> quantile_values_, int num_quantiles_, double time_start_, double time_end_){

    dest_stop = dest_stop_;
    num_quantiles = num_quantiles_;
    time_start = time_start_;
    time_end = time_end_;

    quantiles = new double[num_quantiles];
    quantile_values = new double[num_quantiles];

    for (int i=0; i<num_quantiles; i++){
        quantiles[i] = quantiles_[i];
        quantile_values[i] = quantile_values_[i];
    }

}

double* Walking_time_dist::get_quantiles() {
    return quantiles;
}

bool Walking_time_dist::time_is_in_range(double curr_time) {

    if ((time_start <= curr_time) && (curr_time < time_end)){
        return true;
    }
    else {
        return false;
    }

}
