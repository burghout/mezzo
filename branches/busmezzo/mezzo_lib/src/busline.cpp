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
	id(id_), opposite_id(opposite_id_), name(name_), busroute(busroute_), stops(stops_), vtype(vtype_), odpair(odpair_), holding_strategy(holding_strategy_), max_headway_holding(max_headway_holding_), init_occup_per_stop(init_occup_per_stop_), nr_stops_init_occup(nr_stops_init_occup_)
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
		if (curr_trip->first->get_busv()->get_short_turn_counter() == 0) //if bus has short-turned earlier than it is no longer running according to scheduling constraints, if it has not then we wish to adhere to predefined schedule
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
bool Busline::is_st_startstop(Busstop * stop)
{
	if (this->st_map.count(stop) > 0)
	{
		return true;
	}
	return false;
}

vector<Busstop*> Busline::get_downstream_stops_line(Busstop* stop)
{
	vector <Busstop*> ds_stops;
	//find position of stop in vector
	vector<Busstop*>::iterator stop_pos;
	for (stop_pos = stops.begin(); stop_pos != stops.end(); stop_pos++)
	{
		if ((*stop_pos)->get_id() == stop->get_id())
			break;
	}

	if (stop_pos == stops.end()) //stop does not exist on this line
	{
		DEBUG_MSG("Busline::get_downstream_stops_line warning: Stop " << stop->get_id() << " does not exist on line " << this->get_id());
		return ds_stops; 
	}

	for (vector<Busstop*>::iterator ds_stop = stop_pos; ds_stop < stops.end(); ds_stop++)
	{
		ds_stops.push_back((*ds_stop));
	}
	return ds_stops;
}

vector<Busstop*> Busline::get_upstream_stops_line(Busstop* stop)
{
	vector<Busstop*> us_stops;
	//find position of stop in vector
	vector<Busstop*>::iterator stop_pos;
	for (stop_pos = stops.begin(); stop_pos != stops.end(); stop_pos++)
	{
		if ((*stop_pos)->get_id() == stop->get_id())
			break;
	}
	if (stop_pos == stops.end()) //stop does not exist on this line
	{
		DEBUG_MSG("Busline::get_downstream_stops_line warning: Stop " << stop->get_id() << " does not exist on line " << this->get_id());
		return us_stops;
	}
	for (vector<Busstop*>::iterator us_stop = stops.begin(); us_stop < stop_pos; us_stop++)
	{
		us_stops.push_back((*us_stop));
	}
	return us_stops;
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

	output_line_assign [stop1] = Busline_assign(id, stop1->get_id() , stop2->get_id()  ,output_line_assign[stop1].passenger_load + pass_assign); // accumulate pass. load on this segment
}

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
	short_turned = false;
	actual_dispatching_time = 0.0;
	for (vector <Visit_stop*>::iterator visit_stop_iter = stops.begin(); visit_stop_iter < stops.end(); visit_stop_iter++)
	{
		assign_segements[(*visit_stop_iter)->first] = 0;
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

Bustrip::Bustrip (int id_, double start_time_, Busline* line_): id(id_), starttime(start_time_), line(line_)
{
	init_occup_per_stop = line->get_init_occup_per_stop();
	nr_stops_init_occup = line->get_nr_stops_init_occup();
	random = new (Random);
	next_stop = stops.begin();
	actual_dispatching_time = 0.0;
	last_stop_enter_time = 0;
	last_stop_exit_time = 0;
	holding_at_stop = false;
	short_turned = false;
	for (vector<Visit_stop*>::iterator visit_stop_iter = stops.begin(); visit_stop_iter < stops.end(); visit_stop_iter++)
	{
		assign_segements[(*visit_stop_iter)->first] = 0;
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
	nr_expected_alighting.clear();
	passengers_on_board.clear();
	output_passenger_load.clear();
	last_stop_visited = stops.front()->first;
	holding_at_stop = false;
	short_turned = false;
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
		if (this->get_busv()->get_short_turn_counter() > 0) // if we have short-turned before we do not care about schedule adherence (starttime) anymore
		{
			actual_dispatching_time = time + min_recovery + theRandomizers[0]->lnrandom(mean_error_recovery, std_error_recovery);
		}
		else
		{
			actual_dispatching_time = Max(time + min_recovery, starttime) + theRandomizers[0]->lnrandom(mean_error_recovery, std_error_recovery);
		}
		return actual_dispatching_time; // error factor following log normal distribution;
	}
}

bool Bustrip::advance_next_stop (double time, Eventlist* eventlist)
{
	// TODO - vec.end is post last 
	if (busv->get_short_turning() && busv->get_on_trip() == true) //if bus is short-turning and has not been teleported to the last stop on its line yet 
	{
		//DEBUG_MSG( "Bustrip::advance_next_stop is setting next_stop to final stop of trip " << this->get_id() );
		next_stop = stops.end(); //short-turned bus has teleported to exit last stop for this trip
		return true;
	}

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

bool Bustrip::check_last_in_tripchain()
{
	if (this->get_id() == driving_roster.back()->first->get_id())
		return true;
	return false;
}

Bustrip* Bustrip::find_previous_in_tripchain()
{
	Bustrip* prev_trip;
	if (this->get_id() == driving_roster.front()->first->get_id()) //trip is first in chain
		return nullptr;

	for (vector<Start_trip*>::iterator trip_it = (driving_roster.begin()+1); trip_it != driving_roster.end(); trip_it++)
	{
		if (this->get_id() == (*trip_it)->first->get_id())
		{
			--trip_it;
			prev_trip = (*trip_it)->first;
			break;
		}
	}
	return prev_trip;
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
			return false;
		}
		busv->set_curr_trip(this);	
		first_dispatch_time = (*previous_trip)->first->get_last_stop_exit_time(); //Added by Jens 2014-07-03
		/*if (busv->get_route() != NULL)
			cout << "Warning, the route is changing!" << endl;*/
	}
	double dispatch_time;
	if (busv->get_short_turning()) {
		map<pair<int, int>, int> stpair_to_stfunc = (*previous_trip)->first->get_line()->get_stpair_to_stfunc();
		int st_func = stpair_to_stfunc[make_pair(this->get_last_stop_visited()->get_id(), this->get_busv()->get_end_stop_id())]; //used for calculating short-turning time (currently just a constant but can refer to the id of a function)
		dispatch_time = time + st_func; //dispatch immediately to start skipping stops to end stop
		actual_dispatching_time = time + st_func; //sets short-turning dispatch for this trip to end time of short-turn
	}
	else
		dispatch_time = calc_departure_time(first_dispatch_time);

	if (dispatch_time < time)
		cout << "Warning, dispatch time is before current time for bus trip " << id << endl;
	busv->init(busv->get_id(),4,busv->get_length(),route,odpair,time); // initialize with the trip specific details
	busv->set_occupancy(random->inverse_gamma(nr_stops_init_occup,init_occup_per_stop));
	if ( (odpair->get_origin())->insert_veh(busv, dispatch_time)) // insert the bus at the origin at the possible departure time
	{
  		busv->set_on_trip(true); // turn on indicator for bus on a trip
		ok = true;
	}
	else // if insert returned false (if inputqueue is full which should never happen)
  	{
		cout << "Bustrip::activate inserting bus at origin node " << odpair->get_origin()->get_id() << "failed for bus " << busv->get_bus_id() <<  endl;
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

void Bustrip::record_passenger_loads (vector <Visit_stop*>::iterator start_stop)
{	
	if(!holding_at_stop) 
	{
		output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id() , (*(start_stop+1))->first->get_id()  ,assign_segements[(*start_stop)->first]));
		this->get_line()->add_record_passenger_loads_line((*start_stop)->first, (*(start_stop+1))->first,assign_segements[(*start_stop)->first]);
	}
	else //David added 2016-05-26: overwrite previous record_passenger_loads if this is the second call to pass_activity_at_stop
	{
		--start_stop; //decrement start stop since we have already advanced 'next_stop'
		output_passenger_load.pop_back();
		output_passenger_load.push_back(Bustrip_assign(line->get_id(), id, busv->get_id(), (*start_stop)->first->get_id() , (*(start_stop+1))->first->get_id()  ,assign_segements[(*start_stop)->first]));
		this->get_line()->add_record_passenger_loads_line((*start_stop)->first, (*(start_stop+1))->first,assign_segements[(*start_stop)->first]);
	}
}

double Bustrip::find_crowding_coeff (Passenger* pass)
{
	// first - calculate load factor
	double load_factor = this->get_busv()->get_occupancy()/this->get_busv()->get_number_seats();
	
	// second - return value based on pass. standing/sitting
	bool sits = pass->get_pass_sitting();

	return find_crowding_coeff(sits, load_factor);
}

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

pair<double, double> Bustrip::crowding_dt_factor (double nr_boarding, double nr_alighting)
{
	pair<double, double> crowding_factor;
	if (busv->get_capacity() == busv->get_number_seats())
	{
		crowding_factor.first = 1;
		crowding_factor.second = 1;
	}
	else
	{
		double nr_standees_alighting = max(0.0, busv->get_occupancy() - (nr_boarding + nr_alighting) - busv->get_number_seats());
		double nr_standees_boarding = max(0.0, busv->get_occupancy() - (nr_boarding + nr_alighting)/2 - busv->get_number_seats());
		double crowdedness_ratio_alighting = nr_standees_alighting / (busv->get_capacity()- busv->get_number_seats());
		double crowdedness_ratio_boarding = nr_standees_boarding / (busv->get_capacity()- busv->get_number_seats());
		crowding_factor.second = 1 + 0.75 * pow(crowdedness_ratio_alighting, 2);
		crowding_factor.first = 1 + 0.75 * pow(crowdedness_ratio_boarding, 2);
	}
	return crowding_factor;
}

vector <Busstop*> Bustrip::get_downstream_stops()
{
	vector <Busstop*> ds_stops;
	for(vector <Visit_stop*> :: iterator stop = next_stop; stop < stops.end(); stop++)
	{
		ds_stops.push_back((*stop)->first);
	}
	return ds_stops;
}

vector<Busstop*> Bustrip::get_downstream_stops_from_stop(Busstop * first_stop)
{
	vector<Busstop*>::iterator start_stop;
	int first_stop_id = first_stop->get_id();
	start_stop = find_if(line->stops.begin(), line->stops.end(), compare <Busstop>(first_stop_id)); //find first stop on this trips route (Note: assuming that trip plans to visit all stops on its line)
	vector<Busstop*> ds_stops;
	for (vector<Busstop*>::iterator stop = start_stop; stop < line->stops.end(); stop++)
	{
		ds_stops.push_back((*stop));
	}
	return ds_stops;
}

vector<Busstop*> Bustrip::get_upstream_stops()
{
	vector<Busstop*> us_stops;
	for (vector<Visit_stop*>::iterator stop = stops.begin(); stop < next_stop; stop++)
	{
		us_stops.push_back((*stop)->first);
	}
	return us_stops;
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
	position = 0;
	has_bay = false;
	can_overtake = true;
	dwelltime = 0;
	rti = 0;
}

Busstop::Busstop (int id_, string name_, int link_id_, double position_, double length_, bool has_bay_, bool can_overtake_, double min_DT_, int rti_):
	id(id_), name(name_), link_id(link_id_), position (position_), length(length_), has_bay(has_bay_), can_overtake(can_overtake_), min_DT(min_DT_), rti (rti_)
{
	avaliable_length = length;
	nr_boarding = 0;
	nr_alighting = 0;
	dwelltime = 0;
	is_origin = false;
	is_destination = false;
	last_arrivals.clear();
	last_departures.clear();
	arrivals_per_line.clear();
	departures_per_line.clear();
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
	arrivals_per_line.clear();
	departures_per_line.clear();
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

	if (trip->get_busv()->get_short_turning()) //if the short-turned bus has just teleported to the beginning of its next trip we wish to schedule its arrival to the end stop instead of the first stop
	{
		vector<Visit_stop*>::iterator endstop_iter;
		Busstop* end_stop;
		for (endstop_iter = trip->stops.begin(); endstop_iter < trip->stops.end(); endstop_iter++) //find end stop 
		{
			if ((*endstop_iter)->first->get_id() == trip->get_busv()->get_end_stop_id()) {
				//DEBUG_MSG("Busstop::book_bus_arrival booking arrival to end stop " << (*endstop_iter)->first->get_id() << " for bus " << trip->get_busv()->get_bus_id() << " at time " << time);
				trip->set_next_stop(endstop_iter);
				end_stop = (*endstop_iter)->first;
				break;
			}	
		}
		if (endstop_iter == trip->stops.end())
			DEBUG_MSG("Busstop::bus_bus_arrival did not find end stop for short-turned bus " << trip->get_busv()->get_bus_id() << "!!!");

		end_stop->add_to_expected_bus_arrivals(bus_arrival_time);
		eventlist->add_event(time, end_stop);

		//update teleported bus' current link to the link the end stop is on
		Busroute* end_route; //!< the route that the short-turned bus ends up on
		vector<Link*> end_route_links;
		vector<Link*>::iterator link_iter;
		Link* end_link;
		int end_link_id = end_stop->get_link_id();
		
		end_route = trip->get_line()->get_busroute();
		end_route_links = end_route->get_links();
		for (link_iter = end_route_links.begin(); link_iter < end_route_links.end(); link_iter++) //find end stop link on route links
		{
			if ((*link_iter)->get_id() == end_link_id) {
				end_link = (*link_iter);
				break;
			}
		}
		if (link_iter == end_route_links.end())
			cout << "end stop link id " << end_link_id << "not found on route " << end_route->get_id() << endl;

		trip->get_busv()->set_curr_link(end_link);
	}
	else
	{
		expected_bus_arrivals.push_back(bus_arrival_time);
		eventlist->add_event(time, this);
	}
} 

void Busstop::short_turn_exit(Bustrip * st_trip, int target_stop_id, double time, Eventlist* eventlist)
{
	assert(theParameters->short_turn_control == true);
	assert(st_trip->get_busv()->get_short_turning() == true);

	Busline* opposite_line;
	vector<Busstop*>::iterator endstop_iter;
	Busstop* end_stop;

	opposite_line = st_trip->get_line()->get_opposite_line();
	endstop_iter = find_if(opposite_line->stops.begin(), opposite_line->stops.end(), compare <Busstop>(target_stop_id)); //find end stop on opposite line
	if (endstop_iter == opposite_line->stops.end())
	{
		cout << "End stop " << target_stop_id << " does not exist on opposite line " << opposite_line->get_id() << "!" << endl;
		cin.ignore();
	}
	end_stop = *endstop_iter;

	//short-turning version of bus_enter in Busstop::execute
	//dwelltime calculated in short_turn_force_alighting
	exit_time = time + dwelltime; // leave stop immediately after forced alighting dwelltime
	st_trip->set_enter_time(time);
	pair<Bustrip*, double> exiting_trip;
	exiting_trip.first = st_trip;
	exiting_trip.second = exit_time;
	//occupy_length(entering_trip->get_busv());
	
	//teleport bus to EXIT last stop
	Busstop* last_stop = st_trip->stops.back()->first;
	last_stop->add_to_buses_currently_at_stop(exiting_trip);
	eventlist->add_event(exit_time, last_stop); // book an event for the time it exits the last stop on this direction
	
	//update teleported bus' current link to the link the last busstop is on
	Busroute* orig_route; //!< the original route of the short-turned bus
	vector<Link*> orig_route_links;
	vector<Link*>::iterator link_iter;
	Link* last_link; //!< stop that last link is on
	int last_link_id = last_stop->get_link_id();
	
	orig_route = st_trip->get_line()->get_busroute();
	orig_route_links = orig_route->get_links();

	for (link_iter = orig_route_links.begin(); link_iter < orig_route_links.end(); link_iter++)
	{
		if ((*link_iter)->get_id() == last_link_id){
			last_link = (*link_iter);
			break;
		}
	}
	if (link_iter == orig_route_links.end())
		cout << "last stop link id " << last_link_id << "not found on route " << orig_route->get_id() << endl;
	
	//Link* last_link = orig_route->lastlink();
	st_trip->get_busv()->set_curr_link(last_link);

	//record_busstop_visit(entering_trip, entering_trip->get_enter_time()); // document stop-related info
																		  // done BEFORE update_last_arrivals in order to calc the headway and BEFORE set_last_stop_exit_time
	
	st_trip->get_busv()->record_busvehicle_location(st_trip, this, st_trip->get_enter_time());
	st_trip->set_last_stop_exit_time(exit_time);
	st_trip->set_last_stop_visited(this);
	update_last_arrivals(st_trip, st_trip->get_enter_time()); // in order to follow the arrival times (AFTER dwell time is calculated)
	update_last_departures(st_trip, exit_time); // in order to follow the departure times (AFTER the dwell time and time point stuff)
	update_arrivals_per_line(st_trip, st_trip->get_enter_time());
	update_departures_per_line(st_trip, exit_time);
	//st_trip->actual_departure_times[this] = exit_time;

	set_had_been_visited(st_trip->get_line(), true);
	st_trip->advance_next_stop(exit_time, eventlist);
	st_trip->set_entering_stop(false);
}

void Busstop::short_turn_enter(Bustrip * st_trip)
{
	st_trip->get_busv()->set_end_stop_id(0); //reset end stop id
	st_trip->get_busv()->set_short_turning(false); //bus is not short-turning anymore
}

int Busstop::calc_short_turning(Bustrip * trip, double time)
{
	if (!trip->get_line()->is_st_startstop(this)) // current stop is not a short-turning start stop in st_map
		return 0;

	if (trip->check_last_in_tripchain()) //if there is are no future trips assigned to this bus vehicle (according to driving roster in trip)
		return 0;

	if (trip->get_line()->check_first_trip(trip)) //do not short turn first trip for line
		return 0;

	if (!trip->get_line()->check_first_trip(trip))
	{
		if (trip->check_consecutive_short_turn(time)) //DEBUG testing purposes, do not short-turn consequtive trips
		{
			DEBUG_MSG(endl << "Trip " << trip->get_id() << " will not short-turn because previous trip " << trip->find_closest_preceding_arrival(time)->get_id() << " was short-turned");
			return 0;
		}
	}

	//Decision rule taking into account three passenger categories, set short-turn to true if costs to pass groups i (forced alighters) and ii (downstream boarders) are less than group iii (reverse downstream boarders)
	double beta_W = 2; //weight of perceived travel time for waiting passengers (same as Alexandra used)
	double beta_F = 10; //weight of percieved travel time for forced alighters (based off of what Alexandra used for denied boarding)
	double lambda_0; //arrival rate per hour at stop in direction before short-turn
	double lambda_1; //arrival rate per hour at stop in direction after short-turn
	int forced_alighters; //number of passengers that are forced to alight given a short turning decision (given APC)
	int ds_boarders; //estimated number of passengers that will be waiting downstream within the forward headway of trip (starting from start-stop)
	int reverse_ds_boarders; //estimated number of passengers that will be waiting downstream within the resulting forward headway of trip if short-turned (starting from end_stop)
	double expected_arrival_es; //expected arrival of short turned bus to end stop (time + dwelltime(alighters) + dwell constant + ?)

	double backward_headway;
	double forward_headway;
	double opp_last_arrival; //last arrival time to the end stop in opposite direction
	double opp_backward_headway; //backwards headway in opposite direction without short-turning
	double st_opp_backward_headway; //backwards headway in opposite direction with short-turning
	
	Busline* opposite_line;
	Bustrip* closest_trip; //closest trip on opposite direction of line (m1)
	vector<Busstop*>::iterator endstop_iter;
	Busstop* end_stop;
	multimap<Busstop*, Busstop*> st_map = trip->get_line()->get_st_map();
	int end_stop_id;
	end_stop_id = st_map.find(this)->second->get_id(); //currently assumes one-to-one start and end stops

	opposite_line = trip->get_line()->get_opposite_line();
	endstop_iter = find_if(opposite_line->stops.begin(), opposite_line->stops.end(), compare <Busstop>(end_stop_id)); //find end stop on opposite line
	if (endstop_iter == opposite_line->stops.end())
	{
		cout << "End stop " << end_stop_id << " does not exist on opposite line " << opposite_line->get_id() << "!" << endl;
		cin.ignore();
	}
	end_stop = *endstop_iter;

	if (end_stop->arrivals_per_line.empty()) //TODO: currently we decide not to short turn if no bus from the opposite line has arrived to its end stop yet
		return 0;
	
	//calculate passenger flows
	forced_alighters = trip->get_busv()->get_occupancy() - static_cast<int> (trip->passengers_on_board[this].size());
	
	map<pair<int, int>, int> stpair_to_stfunc = trip->get_line()->get_stpair_to_stfunc(); //get function/constant used to estimate the time taken to short-turn (independent of # of pass forced to alight)
	int st_func = stpair_to_stfunc[make_pair(this->get_id(), end_stop_id)]; //used for calculating short-turning time
	double dwelltime;

	nr_alighting = trip->get_busv()->get_occupancy();
	nr_boarding = 0;
	dwelltime = calc_dwelltime(trip); //estimate dwelltime due to forced alighting

	//calculate headways
	closest_trip = end_stop->arrivals_per_line[opposite_line].rbegin()->second;
	double layover = 4;
	expected_arrival_es = time + dwelltime + st_func + layover; //TODO: add something to compensate for the 'layover' time

	opp_last_arrival = end_stop->get_last_arrival(opposite_line);
	st_opp_backward_headway = expected_arrival_es - opp_last_arrival; //calculate forward headway on opposite line TODO: Currently it is possible for this to be negative

	backward_headway = trip->calc_backward_arrival_headway(time); //TODO: also this can be negative if scheduled arrival times are way off, returns 0 if the expected arrival is exactly the same as this trip (which is unlikely) or if no succeding trip was found
	if (backward_headway == 0) //no trip following this one was found
		return 0;
	forward_headway = trip->calc_forward_arrival_headway(time);
	if (forward_headway == 0) //not trip in front of this one was found
		return 0;

	opp_backward_headway = closest_trip->calc_backward_arrival_headway(closest_trip->get_enter_time());

	//passenger arrival rate for downstream stops in direction before short turn
	vector<Busstop*> ds_stops = trip->get_downstream_stops(); //TODO: check if this includes 'next_stop' currently (we would like it to)
	for (auto stop : ds_stops)
	{
		for (map<Busstop*, ODstops*>::iterator od_for_destination = stop->get_stop_as_origin().begin(); od_for_destination != stop->get_stop_as_origin().end(); od_for_destination++) //loop through destinations for this origin
		{
			lambda_0 += od_for_destination->second->get_arrivalrate(); 
		}
	}

	ds_boarders = lambda_0 * forward_headway / 3600; //lambda given in passengers per hour
	/*For validating lambda_0
	Hornstull = 989.91
	Fridhemsplan = 772.34
	Odenplan = 371.65
	�stra = 86.142
	*/

	vector <Start_trip*>::iterator next_trip; // pointer to next trip in this trips chain
	for (vector <Start_trip*>::iterator it_trip = trip->driving_roster.begin(); it_trip < trip->driving_roster.end(); it_trip++)
	{
		if ((*it_trip)->first == trip)
		{
			next_trip = it_trip+1;
			break;
		}
	}

	//passenger arrival rate for downstream stops in opposite direction
	vector<Busstop*> opp_ds_stops = (*next_trip)->first->get_downstream_stops_from_stop(end_stop);
	for (auto stop : opp_ds_stops)
	{
		for (map<Busstop*, ODstops*>::iterator od_for_destination = stop->get_stop_as_origin().begin(); od_for_destination != stop->get_stop_as_origin().end(); od_for_destination++) //loop through destinations for this origin
		{
			lambda_1 += od_for_destination->second->get_arrivalrate(); 
		}
	}

	reverse_ds_boarders = lambda_1 * st_opp_backward_headway / 3600;
	/*For validating lambda_1
	�stra = 1886.8
	Odenplan = 1458.99
	Fridhemsplan = 1124.29
	Hornstull = 645.202
	*/

	double utility_FA; //difference in utility of forced alighters
	double utility_DB; //difference in utility of downstream boarders
	double utility_RVB; //difference in utility of reverse downstream boarders

	double z; //total cost/benefit of short-turning decision

	if (backward_headway < 0) backward_headway = 0; //TODO: what happens if backward headway is negative? This implys that a following trip is very close which should benefit short-turning but by how much? Should we set it to zero?
	double cost_FA = forced_alighters * (beta_W * backward_headway + beta_F * backward_headway); 
	double cost_DB = ds_boarders * beta_W * backward_headway;
	double benefit_RVB = reverse_ds_boarders * beta_W * (opp_backward_headway - st_opp_backward_headway);

	z = benefit_RVB - cost_FA - cost_DB;
	DEBUG_MSG("z = " << z);
	if (z <= 0)
		return 0;

	/*DEBUG*/
	Bustrip* succ_m0 = trip->find_closest_following_trip();
	Bustrip* prec_m0 = trip->find_closest_preceding_arrival(time);
	DEBUG_MSG(endl << "-------Decision to short-turn bus " << trip->get_busv()->get_bus_id() << " has been made at " << this->get_name() << "------");
	DEBUG_MSG(endl << "Trip " << trip->get_id() << " and bus " << trip->get_busv()->get_bus_id() <<  " should short-turn from stop " << this->get_id() << " to stop " << end_stop_id);
	DEBUG_MSG("Starting occupancy             : " << trip->get_busv()->get_occupancy());
	int nr_reg_alighting = static_cast<int> (trip->passengers_on_board[this].size());
	DEBUG_MSG("Number of regular alighters    : " << nr_reg_alighting);
	DEBUG_MSG("Number of forced alighters     : " << forced_alighters);
	DEBUG_MSG("Arrival time                   : " << time);
	DEBUG_MSG("Dwelltime for forced alight    : " << dwelltime);
	DEBUG_MSG("Exit time                      : " << time + dwelltime);
	DEBUG_MSG("Estimated short-turn time      : " << st_func + layover);
	DEBUG_MSG("Total short-turn time          : " << dwelltime + st_func + layover);
	DEBUG_MSG("Following trip " << succ_m0->get_id() << " occupancy  : " << succ_m0->get_busv()->get_occupancy());
	DEBUG_MSG("Preceding trip " << prec_m0->get_id() << " occupancy  : " << prec_m0->get_busv()->get_occupancy());
	DEBUG_MSG(endl << "Passengers waiting at this stop BEFORE decision: " << this->calc_total_nr_waiting());


	return end_stop_id; //end_stop_id != 0 triggers perform_short_turn is where the bus first forces passengers to alight, abort its current trip and begin its next trip in the opposite direction but skipping the first stops until end-stop
}

int Busstop::alight_passengers(Eventlist* eventlist, Bustrip* st_trip, double time, passengers& alighting_passengers)
{
	assert(theParameters->demand_format == 3 && "Inside Busstop::alight_passengers with demand_format != 3");
	assert(theParameters->short_turn_control == true && "Inside Busstop::alight_passengers with short_turn_control != 1");

	int npass_alighted; //!< number of passengers that alight via this method
	npass_alighted = static_cast<int> (alighting_passengers.size());

	if (theParameters->demand_format == 3)   // demand is given in terms of arrival rate of individual passengers per OD of stops (future - route choice)
	{
		// * Process passengers that wish to alight at this stop anyways * 
		for (vector <Passenger*> ::iterator alighting_passenger = alighting_passengers.begin(); alighting_passenger != alighting_passengers.end(); alighting_passenger++)
		{
			pair<Busstop*, double> stop_time;
			stop_time.first = this;
			stop_time.second = time;
			(*alighting_passenger)->add_to_selected_path_stop(stop_time);
			// update experienced crowding on-board
			pair<double, double> riding_coeff;
			riding_coeff.first = time - st_trip->get_enter_time(); // refers to difference between departures
			riding_coeff.second = st_trip->find_crowding_coeff((*alighting_passenger));
			(*alighting_passenger)->add_to_experienced_crowding_levels(riding_coeff);
			//ODstops* od_stop = (*alighting_passenger)->get_OD_stop();
			ODstops* od_stop = (*alighting_passenger)->get_original_origin()->get_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination());
			od_stop->record_onboard_experience(*alighting_passenger, st_trip, this, riding_coeff);

			Busstop* next_stop = this;
			bool final_stop = false;
			//if this stop is not passenger's final destination then make a connection decision
			ODstops* od;
			if (check_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination()) == false) //check if this stop is already an origin to the destination of the passenger
			{
				od = new ODstops(next_stop, (*alighting_passenger)->get_OD_stop()->get_destination());
				add_odstops_as_origin((*alighting_passenger)->get_OD_stop()->get_destination(), od);
				(*alighting_passenger)->get_OD_stop()->get_destination()->add_odstops_as_destination(next_stop, od);
			}
			else
			{
				od = stop_as_origin[(*alighting_passenger)->get_OD_stop()->get_destination()];
			}
			(*alighting_passenger)->set_ODstop(od); // set the connected stop as passenger's new origin (new OD)
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
			}
			if (final_stop == false) //if this stop is not passenger's final destination then make a connection decision
			{
				next_stop = (*alighting_passenger)->make_connection_decision(time);
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
					if (next_stop->get_id() == this->get_id())  // pass stays at the same stop
					{
						passengers wait_pass = odstop->get_waiting_passengers(); // add passanger's to the waiting queue on the new OD
						wait_pass.push_back(*alighting_passenger);
						odstop->set_waiting_passengers(wait_pass);
						(*alighting_passenger)->set_arrival_time_at_stop(time);
						pair<Busstop*, double> stop_time;
						stop_time.first = this;
						stop_time.second = time;
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
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
					else  // pass walks to another stop
					{
						// booking an event to the arrival time at the new stop
						double arrival_time_connected_stop = time + distances[next_stop] * 60 / random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed / 4);
						//(*alighting_passenger)->execute(eventlist,arrival_time_connected_stop);
						eventlist->add_event(arrival_time_connected_stop, *alighting_passenger);
						pair<Busstop*, double> stop_time;
						stop_time.first = next_stop;
						stop_time.second = arrival_time_connected_stop;
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
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
		alighting_passengers.clear(); // clear passengers with this stop as their alighting stop
		st_trip->get_busv()->set_occupancy(st_trip->get_busv()->get_occupancy() - npass_alighted);	// update occupancy on bus
	} //demand format == 3

	return npass_alighted;
}

double Busstop::find_trip_arrival_time(Bustrip * trip)
{
	double arrival_time = 0; //arrival time of trip to this stop (if it exists) 
	map<double, Bustrip*> arrivals = arrivals_per_line[trip->get_line()];

	for (map<double, Bustrip*>::iterator arrival = arrivals.begin(); arrival != arrivals.end(); arrival++)
	{
		if (arrival->second->get_id() == trip->get_id())
		{
			arrival_time = arrival->first;
			break;
		}
	}

	return arrival_time;
}

void Busstop::short_turn_force_alighting(Eventlist* eventlist, Bustrip* st_trip, double time)
{
	/* List of output file updates in this method (and most likely in pass_activity_at_stop)
	Alighting passenger:
		selected_paths.dat
			- add_to_selected_path_stop
		passenger_on_board_experience.dat
			- add_to_experienced_crowding_levels
			- record_onboard_experience
		***passenger_alighting.dat
			- want to make this happen in this function for all forced alighters! This was never registered for them when they were boarding. 
				What was previously registered as an alighting decision for this passenger will now never actually occur.
		passenger_connection.dat
			- make_connection_decision
				record_passenger_connection_decision

	Boarding passenger:
		selected_paths.dat
			- add_to_selected_path_trips
		passenger_waiting_experience.dat
			- record_waiting_experience
		passenger_boarding.dat
			- make_boarding_decision
				record_passenger_boarding_decision
		passenger_alighting.dat
			- make_alighting_decision
				record_passenger_alighting_decision
	*/

	assert(theParameters->short_turn_control == true && "Inside Busstop::short_turn_force_alighting with short_turn_control != 1"); 
	nr_boarding = 0; //reset nr_boarding for stop
	nr_alighting = 0;
	
	int starting_occupancy; //bus crowdedness factor
	starting_occupancy = st_trip->get_busv()->get_occupancy();

	int nr_reg_alighting; // number of passengers that wish to alight at this stop anyways
	int nr_forced_alighting; // number of passengers that are forced to alight at this stop
		
	nr_reg_alighting = static_cast<int> (st_trip->passengers_on_board[this].size()); //passengers that wish to alight at this stop anyways
	nr_forced_alighting = starting_occupancy - nr_reg_alighting;
	
	// * Process passengers that wish to alight at this stop anyways * 
	nr_reg_alighting = alight_passengers(eventlist, st_trip, time, st_trip->passengers_on_board[this]);

	/* Process passengers that are forced to alight */
	nr_forced_alighting = 0;
	for (auto& pass_on_board : st_trip->passengers_on_board) //loop through all alighting stop -> passengers pairs for this trip
	{
		if (pass_on_board.second.empty())
			continue;

		for (Passenger* passenger : pass_on_board.second)
		{
			passenger->set_forced_alighting(true);
			passenger->forced_alighting_decision(st_trip, this, time); //record forced alighting output for each passenger
		}

		nr_forced_alighting += alight_passengers(eventlist, st_trip, time, pass_on_board.second); //force passengers to alight and make walking connection decisions
	} 

	assert(st_trip->get_busv()->get_occupancy() == 0 && "Busstop::short_turn_force_alighting occupancy of bus after forced alighting != 0");
	
	DEBUG_MSG("Passengers waiting at this stop AFTER forced alighting: " << this->calc_total_nr_waiting() << endl);

	nr_alighting = nr_reg_alighting + nr_forced_alighting;
	dwelltime = calc_dwelltime(st_trip); //used in short_turn_exit
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
		if(!exiting_trip->get_busv()->get_short_turning()) //bus does not occupy any space at stop when short-turning
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
				//if (exiting_trip->get_busv()->get_short_turning())
				//	DEBUG_MSG( "Short-turning bus " << exiting_trip->get_busv()->get_bus_id() << " is attempting to exit link " << veh->get_curr_link()->get_id() << " before its final stop " );
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
			//if (exiting_trip->get_busv()->get_short_turning())
			//	DEBUG_MSG("Bus " << exiting_trip->get_busv()->get_bus_id() << " has teleported to end of route at time " << time);
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
		entering_trip->set_entering_stop(true);
		entering_trip->visited_stop[this] = true;

		//Short-turning
		if (theParameters->short_turn_control)
		{
			if (!entering_trip->get_busv()->get_short_turning()) //if the bus isnt already short-turning
			{
				int target_stop_id;
				target_stop_id = calc_short_turning(entering_trip, time);
				if (target_stop_id) //if the decision to short-turn has been made
				{
					entering_trip->get_busv()->set_short_turning(true);
					entering_trip->get_busv()->set_end_stop_id(target_stop_id);
					entering_trip->set_short_turned(true); 
					short_turn_force_alighting(eventlist, entering_trip, time); //process voluntary alighters and force other passengers to alight, record corresponding output data
					expected_bus_arrivals.erase(iter_arrival);
					short_turn_exit(entering_trip, target_stop_id, time, eventlist); //initiate alternative exit_stop process
					return true; //TODO: check if returning true here effects anything else, now we've basically just created an event that this trip will instantly arrive at its last stop
				}
			}
			else  //if a entering trip is a short-turning bus
			{
				if (entering_trip->get_busv()->get_end_stop_id() == this->get_id()) //if this is its end stop
				{
					assert(entering_trip->get_busv()->get_short_turning());
					assert(time - entering_trip->get_actual_dispatching_time() > 0); //the time taken to short turn should never be negative
					short_turn_enter(entering_trip); //complete the short-turn and continue the bus enter process as usual
					/*DEBUG*/
					Busline* line_before_ST = entering_trip->get_line()->get_opposite_line();
					Busstop* stop_before_ST = entering_trip->get_last_stop_visited();
					Bustrip* prev_trip = entering_trip->find_previous_in_tripchain();
					vector<Busline*> lines = stop_before_ST->get_lines();
					for (vector<Busline*>::iterator line = lines.begin(); line < lines.end(); line++)
					{
						if ((*line)->get_id() == entering_trip->get_line()->get_opposite_id())
						{
							line_before_ST = (*line);
							break;
						}
					}
					double arrival_before_ST = entering_trip->get_last_stop_visited()->find_trip_arrival_time(prev_trip);
					DEBUG_MSG(endl << "-----Bus " << entering_trip->get_busv()->get_bus_id() << " completed short-turn to " << this->get_name() << "-----");
					DEBUG_MSG("Trip before short-turn : " << prev_trip->get_id());
					DEBUG_MSG("Trip after short-turn  : " << entering_trip->get_id());
					DEBUG_MSG("Start-stop             : " << entering_trip->get_last_stop_visited()->get_id() << " to stop ");
					DEBUG_MSG("End-stop               : " << this->get_id());
					DEBUG_MSG("End-stop nr waiting    : " << this->calc_total_nr_waiting());
					DEBUG_MSG("Total short-turn time  : " << time - arrival_before_ST);
				}
			}
		}

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
		update_arrivals_per_line(entering_trip, entering_trip->get_enter_time());
		update_departures_per_line(entering_trip, exit_time);

		set_had_been_visited (entering_trip->get_line(), true);
		entering_trip->advance_next_stop(exit_time, eventlist);
		entering_trip->set_entering_stop(false);
		expected_bus_arrivals.erase(iter_arrival);
	}
	
	return true;
}

void Busstop::passenger_activity_at_stop (Eventlist* eventlist, Bustrip* trip, double time) //!< progress passengers at stop: waiting, boarding and alighting
{
	nr_boarding = 0;
	nr_alighting = 0;
	stops_rate stops_rate_dwell, stops_rate_coming, stops_rate_waiting;
	int starting_occupancy; // bus crowdedness factor
	
	// find out bus occupancy when entering the stop
	//if (trip->get_next_stop() == trip->stops.begin()) // pass. on the bus when entring the first stop
	//{
	//	starting_occupancy = trip->get_init_occupancy();
	//}
	//else
	//{
	starting_occupancy = trip->get_busv()->get_occupancy(); 
	//}

	if (theParameters->demand_format == 1) // demand is given in terms of arrival rates and alighting fractions
	{
		// generate waiting pass. and alight passengers 
		nr_waiting [trip->get_line()] += random -> poisson (((get_arrival_rates (trip)) * get_time_since_arrival (trip, time)) / 3600.0 );
				//the arrival process follows a poisson distribution and the lambda is relative to the headway
				// with arrival time and the headway as the duration
		if (starting_occupancy > 0 && get_alighting_fractions (trip) > 0) 
		{
			set_nr_alighting (random -> binrandom (starting_occupancy, get_alighting_fractions (trip))); // the alighting process follows a binominal distribution 
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
			if ((iter_slices+1) == update_rates_times[trip->get_line()].end() && time > (*iter_slices))
				// it is the last slice
			{
				curr_slice_start = iter_slices;
				break;
			}
			if ((*iter_slices)< time && (*(iter_slices+1)) >= time)
			{
				curr_slice_start = iter_slices;
				break;
			}
		}
		if (simulation_start_slice == false && (time - get_time_since_arrival(trip,time) < (*curr_slice_start))) 
		// if the previous bus from this line arrives on the previous slice - then take into account previous rate
		{
			double time_previous_slice = ((*curr_slice_start) - (time - get_time_since_arrival(trip,time)));
			double time_current_slice = time - (*curr_slice_start);
			nr_waiting [trip->get_line()] += random -> poisson ((previous_arrival_rates[trip->get_line()] * time_previous_slice) / 3600.0 ) + random -> poisson (((get_arrival_rates (trip)) * time_current_slice) / 3600.0 );
			// sum of two poisson processes - pass. arriving during the leftover of the previous lsice plus pass. arriving during the begining of this slice
			double weighted_alighting_fraction = (get_alighting_fractions (trip) * time_current_slice + previous_alighting_fractions[trip->get_line()] * time_previous_slice) / (time_current_slice + time_previous_slice);
			if (starting_occupancy > 0 && weighted_alighting_fraction > 0)
			{
				set_nr_alighting (random -> binrandom (starting_occupancy, weighted_alighting_fraction)); // the alighting process follows a binominal distribution 
					// the number of trials is the number of passengers on board with the probability of the weighted alighting fraction
			}
		}
		else // take into account only current rates
		{
			nr_waiting [trip->get_line()] += random -> poisson (((get_arrival_rates (trip)) * get_time_since_arrival (trip, time)) / 3600.0 );
				//the arrival process follows a poisson distribution and the lambda is relative to the headway
				// with arrival time and the headway as the duration
			if (starting_occupancy > 0 && get_alighting_fractions (trip) > 0) 
			{
				set_nr_alighting (random -> binrandom (starting_occupancy, get_alighting_fractions (trip))); // the alighting process follows a binominal distribution 
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
			if (stops_rate_dwell[(*destination_stop)] != 0 )
			{
				//2014-07-03 Jens changed from poisson to poisson1, poisson does not give correct answers!
				//stops_rate_coming[(*destination_stop)] = random -> poisson (stops_rate_dwell[(*destination_stop)] * get_time_since_arrival (trip, time) / 3600.0 ); // randomized the number of new-comers to board that the destination stop
				stops_rate_coming[(*destination_stop)] = random -> poisson1 (stops_rate_dwell[(*destination_stop)], get_time_since_arrival (trip, time) / 3600.0 ); // randomized the number of new-comers to board that the destination stop
				//trip->nr_expected_alighting[(*destination_stop)] += int (stops_rate_coming[(*destination_stop)]);
				stops_rate_waiting[(*destination_stop)] += int(stops_rate_coming[(*destination_stop)]); // the total number of passengers waiting for the destination stop is updated by adding the new-comers
				nr_waiting [trip->get_line()] += int(stops_rate_coming[(*destination_stop)]);
			}
		}
		multi_nr_waiting[trip->get_line()] = stops_rate_waiting;
		set_nr_alighting (trip->nr_expected_alighting[this]); // passengers heading for this stop alight
		trip->nr_expected_alighting[this] = 0; 
	}
	if (theParameters->demand_format == 1 || theParameters->demand_format == 2 || theParameters->demand_format == 10) // in the case of non-individual passengers - boarding progress for waiting passengers (capacity constraints)
	{	
		if (trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()) <= 0) //Added by Jens 2014-08-12, if capacity is exceeded no boarding should be allowed
		{
			set_nr_boarding(0);
		}
		else if (trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()) < nr_waiting [trip->get_line()]) // if the capcity determines the boarding process
		{	
			if (theParameters->demand_format == 1 || theParameters->demand_format == 10)
			{
				set_nr_boarding(trip->get_busv()->get_capacity() - (starting_occupancy - get_nr_alighting()));
				nr_waiting [trip->get_line()] -= nr_boarding;
			} 
			if (theParameters->demand_format == 2)
			{
				double ratio = double(nr_waiting [trip->get_line()])/(trip->get_busv()->get_capacity() - (starting_occupancy + get_nr_boarding() - get_nr_alighting()));
				for (vector <Busstop*>::iterator destination_stop = trip->get_line()->stops.begin(); destination_stop < trip->get_line()->stops.end(); destination_stop++)
				 // allow only the ratio between supply and demand for boarding equally for all destination stops
				{
					if (stops_rate_dwell[(*destination_stop)] != 0 )
					{	
						int added_expected_passengers = Round(stops_rate_waiting[(*destination_stop)]/ratio);
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
			set_nr_boarding(nr_waiting [trip->get_line()]);
			nr_waiting [trip->get_line()] = 0;
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
		// * Alighting passengers *
		nr_alighting = static_cast<int> ( trip->passengers_on_board[this].size()); 
		for (vector <Passenger*> ::iterator alighting_passenger = trip->passengers_on_board[this].begin(); alighting_passenger != trip->passengers_on_board[this].end(); alighting_passenger++)
		{
			pair<Busstop*,double> stop_time;
			stop_time.first = this;
			stop_time.second = time;
			(*alighting_passenger)->add_to_selected_path_stop(stop_time);
			// update experienced crowding on-board
			pair<double,double> riding_coeff;
			riding_coeff.first = time - trip->get_enter_time(); // refers to difference between departures
			riding_coeff.second = trip->find_crowding_coeff((*alighting_passenger));
			(*alighting_passenger)->add_to_experienced_crowding_levels(riding_coeff);
			//ODstops* od_stop = (*alighting_passenger)->get_OD_stop();
			ODstops* od_stop = (*alighting_passenger)->get_original_origin()->get_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination());
			od_stop->record_onboard_experience(*alighting_passenger, trip, this, riding_coeff);

			//set current stop as origin for alighting passenger's OD stop pair
			Busstop* next_stop = this;
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
				pair<Busstop*,double> stop_time;
				stop_time.first = this;
				stop_time.second = time;
				(*alighting_passenger)->add_to_selected_path_stop(stop_time);
				(*alighting_passenger)->set_end_time(time);
				//pass_recycler.addPassenger(*alighting_passenger); // terminate passenger
			}		
			if (final_stop == false) //if this stop is not passenger's final destination then make a connection decision
			{
				next_stop =(*alighting_passenger)->make_connection_decision(time);
				// set connected_stop as the new origin
				ODstops* new_od;
				if (next_stop->check_stop_od_as_origin_per_stop((*alighting_passenger)->get_OD_stop()->get_destination()) == false)
				{
					new_od = new ODstops (next_stop,(*alighting_passenger)->get_OD_stop()->get_destination());
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
					if (next_stop->get_id() == this->get_id())  // pass stays at the same stop
					{
						passengers wait_pass = odstop->get_waiting_passengers(); // add passanger's to the waiting queue on the new OD
						wait_pass.push_back (*alighting_passenger);
						odstop->set_waiting_passengers(wait_pass);
						(*alighting_passenger)->set_arrival_time_at_stop(time);
						pair<Busstop*,double> stop_time;
						stop_time.first = this;
						stop_time.second = time;
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
						if ((*alighting_passenger)->get_pass_RTI_network_level()==true || this->get_rti() > 0)
						{
							vector<Busline*> lines_at_stop = this->get_lines();
							for (vector <Busline*>::iterator line_iter = lines_at_stop.begin(); line_iter < lines_at_stop.end(); line_iter++)
							{
								pair<Busstop*, Busline*> stopline;
								stopline.first = this;
								stopline.second = (*line_iter);
								(*alighting_passenger)->set_memory_projected_RTI(this,(*line_iter),(*line_iter)->time_till_next_arrival_at_stop_after_time(this,time));
								//(*alighting_passenger)->set_AWT_first_leg_boarding();
							}
						}
					}
					else  // pass walks to another stop
					{
						// booking an event to the arrival time at the new stop
						double arrival_time_connected_stop = time + distances[next_stop] * 60 / random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4);
						//(*alighting_passenger)->execute(eventlist,arrival_time_connected_stop);
						eventlist->add_event(arrival_time_connected_stop, *alighting_passenger);
						pair<Busstop*,double> stop_time;
						stop_time.first = next_stop;
						stop_time.second = arrival_time_connected_stop;
						(*alighting_passenger)->add_to_selected_path_stop(stop_time);
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
		trip->get_busv()->set_occupancy(trip->get_busv()->get_occupancy()-nr_alighting);	// update occupancy on bus

		// * Passengers on-board
		//int avialable_seats = trip->get_busv()->get_occupancy() - trip->get_busv()->get_number_seats();
		map <Busstop*, passengers> passengers_onboard = trip->get_passengers_on_board();
		bool next_stop = false;
		map <Busstop*, passengers>::iterator downstream_stops = passengers_onboard.end();
		downstream_stops--;
		while (next_stop == false)
		{
			for (vector <Passenger*> ::iterator onboard_passenger = trip->passengers_on_board[(*downstream_stops).first].begin(); onboard_passenger != trip->passengers_on_board[(*downstream_stops).first].end(); onboard_passenger++)
			{
				// update experienced crowding on-board
				pair<double,double> riding_coeff;
				riding_coeff.first = time - trip->get_enter_time(); // refers to difference between departures
				riding_coeff.second = trip->find_crowding_coeff((*onboard_passenger));
				(*onboard_passenger)->add_to_experienced_crowding_levels(riding_coeff);
				//ODstops* od_stop = (*onboard_passenger)->get_OD_stop();
				ODstops* od_stop = (*onboard_passenger)->get_original_origin()->get_stop_od_as_origin_per_stop((*onboard_passenger)->get_OD_stop()->get_destination());
				od_stop->record_onboard_experience(*onboard_passenger, trip, this, riding_coeff);
				// update sitting status - if a passenger stands and there is an available seat - allow sitting; sitting priority among pass. already on-board by remaning travel distance
				int available_seats = trip->get_busv()->get_occupancy() < trip->get_busv()->get_number_seats();
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
				while (check_pass < pass_waiting_od.end())
				{
					/*if ((*check_pass)->get_forced_alighting())
						DEBUG_MSG("Passenger that was forced to alight is making a boarding decision at stop " << this->get_id());*/
					// progress each waiting passenger  
					ODstops* this_od = this->get_stop_od_as_origin_per_stop((*check_pass)->get_OD_stop()->get_destination());
					(*check_pass)->set_ODstop(this_od);
					/*
					if ((*check_pass)->get_OD_stop()->get_origin()->get_id() != this->get_id())
					{
						break;
					}
					*/
					if ((*check_pass)->make_boarding_decision(trip, time) == true)
					{
						// if the passenger decided to board this bus
						if (trip->get_busv()->get_occupancy() < trip->get_busv()->get_capacity()) 
						{
							// if the bus is not completly full - then the passenger boards
							// currently - alighting decision is made when boarding
							(*check_pass)->record_waiting_experience(trip, time);
							nr_boarding++;
							pair<Bustrip*,double> trip_time;
							trip_time.first = trip;
							trip_time.second = time;
							(*check_pass)->add_to_selected_path_trips(trip_time);
							
							if (trip->get_busv()->get_occupancy() > trip->get_busv()->get_number_seats()) // the passenger stands
							{
								(*check_pass)->set_pass_sitting(false);
							}
							else // the passenger sits
							{
								(*check_pass)->set_pass_sitting(true);
							}
							if(theParameters->demand_format == 3) //this is here because previously there was a demand format 4 'else' case
							{
								trip->passengers_on_board[(*check_pass)->make_alighting_decision(trip, time)].push_back((*check_pass)); 
							}
							trip->get_busv()->set_occupancy(trip->get_busv()->get_occupancy()+1);
							if (check_pass < pass_waiting_od.end()-1)
							{
								check_pass++;
								next_pass = (*check_pass);
								pass_waiting_od.erase(check_pass-1);
								check_pass = find(pass_waiting_od.begin(),pass_waiting_od.end(),next_pass);
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
								pair<Busstop*,double> denied_boarding;
								denied_boarding.first = this;
								denied_boarding.second = time;
								(*check_pass)->add_to_denied_boarding(denied_boarding);
							}
							if (check_pass < pass_waiting_od.end()-1)
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
						if (check_pass < pass_waiting_od.end()-1)
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
			}
		}	
	}
	if (theParameters->demand_format!=3)
	{
		trip->get_busv()->set_occupancy(starting_occupancy + get_nr_boarding() - get_nr_alighting()); // updating the occupancy
	}
	if (id != trip->stops.back()->first->get_id()) // if it is not the last stop for this trip
	{
		trip->assign_segements[this] = trip->get_busv()->get_occupancy();
		trip->record_passenger_loads(trip->get_next_stop()); //next_stop still refers to 'this' stop here, has not been updated yet
	}
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

void Busstop::update_arrivals_per_line(Bustrip* trip, double arrival_time) //everytime a bus ENTERS a stop this should be update
{
	arrivals_per_line[trip->get_line()][arrival_time] = trip; //Note: we are now assuming that two trips do not arrive at EXACTLY the same time (will overwrite arrival entry)
}

void Busstop::update_departures_per_line(Bustrip* trip, double departure_time) //everytime a bus ENTERS a stop this should be update
{
	departures_per_line[trip->get_line()][departure_time] = trip; //Note: we are now assuming that two trips do not arrive at EXACTLY the same time (will overwrite arrival entry)
}

double Busstop::get_last_arrival(Busline* line)
{
	//assert(arrivals_per_line.count(line) != 0 && "Attempted to call Busstop::get_last_arrival when no trip has arrived to this stop from this line yet!");
	if (arrivals_per_line.count(line) == 0)
		return 0;
	return arrivals_per_line[line].rbegin()->first;
}

double Busstop::get_last_departure(Busline* line)
{
	//assert(departures_per_line.count(line) != 0 && "Attempted to call Busstop::get_last_departure when no trip has departed from this stop from this line yet!");
	if (departures_per_line.count(line) == 0)
		return 0;
	return departures_per_line[line].rbegin()->first;
}

double Busstop::get_time_since_arrival(Bustrip* trip, double time)
{
	if (arrivals_per_line.count(trip->get_line()) == 0) //if no trip from this trips line has arrived to this stop
		return trip->get_line()->calc_curr_line_headway();

	double time_since_arrival = time - get_last_arrival(trip->get_line());
	return time_since_arrival;
}

double Busstop::get_time_since_departure(Bustrip* trip, double time)
{
	if (departures_per_line.count(trip->get_line()) == 0) // if no trip from this trips line has departed from this stop
		return trip->get_line()->calc_curr_line_headway();

	double time_since_departure = time - get_last_departure(trip->get_line());
	return time_since_departure;
}

double Bustrip::calc_scheduled_travel_time_between_stops(Busstop * stop1, Busstop * stop2)
{
	double scheduled_tt = stops_map[stop1] - stops_map[stop2];
	if (scheduled_tt < 0) //if stop1 is earlier on line than stop2
		scheduled_tt = -scheduled_tt;
	assert(scheduled_tt > 0);
	return scheduled_tt;
}

bool Bustrip::check_consecutive_short_turn(double arrival_time)
{
	assert(theParameters->short_turn_control);
	assert(entering_stop); //should only be called when entering a stop and before next_stop is updated

	bool short_turned = false;
	Busstop* last_visited; //last stop visited by this trip

	last_visited = (*next_stop)->first; //if trip is currently entering a stop then last_stop_visited has not been updated yet
	assert(this->get_line()->is_st_startstop(last_visited)); //this method should only be called for a short-turning start-stop

	map<double, Bustrip*> last_stop_arrivals = last_visited->get_arrivals_per_line()[line];

	map<double, Bustrip*>::iterator it_closest_arrival = last_stop_arrivals.lower_bound(arrival_time); //find the latest arrival to last stop visited before this one
	if (it_closest_arrival == last_stop_arrivals.begin())//no trip has arrived to this stop before this one (or arrival map is empty)
		return short_turned;

	--it_closest_arrival;
	if (it_closest_arrival->second->get_short_turned())
	{
		short_turned = true;
	}

	return short_turned;
}

Bustrip* Bustrip::find_closest_preceding_arrival(double arrival_time)
{
	Busstop* last_visited; //last stop visited by this trip
	Bustrip* prec_trip = nullptr; //trip preceding this one Note: Regardless if this trip was short-turned or has overtaken!!
	if (entering_stop)
		last_visited = (*next_stop)->first; //if trip is currently entering a stop then last_stop_visited has not been updated yet
	else
		last_visited = last_stop_visited;

	map<double, Bustrip*> last_stop_arrivals = last_visited->get_arrivals_per_line()[line];

	map<double, Bustrip*>::iterator it_closest_arrival = last_stop_arrivals.lower_bound(arrival_time); //find the latest arrival to last stop visited before this one
	if (it_closest_arrival == last_stop_arrivals.begin())//no trip has arrived to this stop before this one (or arrival map is empty)
		return prec_trip;

	--it_closest_arrival;
	if (it_closest_arrival->second->get_id() == this->get_id())
	{
		if (it_closest_arrival != last_stop_arrivals.begin())
		{
			--it_closest_arrival;
			prec_trip = it_closest_arrival->second;
		}
		else
			return prec_trip;
	}
	else
	{
		prec_trip = it_closest_arrival->second;
	}

	return prec_trip;
}

double Bustrip::calc_forward_arrival_headway(double arrival_time)
{	
	double forward_arr_headway; //headway between this trip and the trip in front based on the last stop visited by this trip
	Busstop* last_visited; //last stop visited by this trip
	
	if (entering_stop)
		last_visited = (*next_stop)->first; //if trip is currently entering a stop then last_stop_visited has not been updated yet
	else
		last_visited = last_stop_visited;

	map<double, Bustrip*> last_stop_arrivals = last_visited->get_arrivals_per_line()[line];
	map<double, Bustrip*>::iterator it_closest_arrival = last_stop_arrivals.lower_bound(arrival_time); //find the latest arrival to last stop visited before this one
	double prec_arrival; //latest arrival time of a preceding trip that previously visited the last stop visited by this trip
	if (it_closest_arrival == last_stop_arrivals.begin()) //no trip has arrived to this stop before this one
		return 0;
	else
	{
		--it_closest_arrival;
next_arrival:
		if (theParameters->short_turn_control && it_closest_arrival->second->get_short_turned() && it_closest_arrival != last_stop_arrivals.begin()) //temporary solution, only solves some corner cases. 
				//Could possibly check each downstream stop for buses that may have skipped this one and then estimate this buses arrival to that stop (or the downstreams bus would be arrival to this one)
		{
			--it_closest_arrival;
			goto next_arrival;
		}
		if (it_closest_arrival->second->get_short_turned()) //we are at the beginning of the arrival list for this stop and that bus was short-turned
			return 0;

		assert(it_closest_arrival->second->get_id() != this->get_id()); //make sure that we are not finding the forward headway of a trip with itself
		prec_arrival = it_closest_arrival->first;
	}

	forward_arr_headway = arrival_time - prec_arrival;
	assert(forward_arr_headway >= 0);

	return forward_arr_headway;
}

double Bustrip::calc_backward_arrival_headway(double arrival_time)
{
	double backward_arr_headway; //headway between this trip and the first trip found behind based on arrivals at upstream stops
	Busstop* last_visited; //last stop visited by this trip
	double succ_arrival; //arrival time of closest succeeding trip
	double scheduled_tt; //scheduled travel time between last stop visited by succeding trip and last stop visited by this trip
	double expected_arrival; //expected arrival time of succeding trop to last stop visited by this trip

	vector<Busstop*> us_stops = get_upstream_stops();

	if (entering_stop)
		last_visited = (*next_stop)->first; //if trip is currently entering a stop then last_stop_visited has not been updated yet
	else
		last_visited = last_stop_visited;

	vector<Busstop*>::reverse_iterator stop_it;
	for (stop_it = us_stops.rbegin(); stop_it != us_stops.rend(); stop_it++) //loop through all upstream stops (starting from most downstream stop) until we find the closest following trip
	{
		map<double, Bustrip*> us_stop_arrivals = (*stop_it)->get_arrivals_per_line()[line]; //all arrivals for this upstream stop
		map<double, Bustrip*>::reverse_iterator rit_closest_arrival = us_stop_arrivals.rbegin(); //start from latest arrival time in arrivals map
		
		if (rit_closest_arrival != us_stop_arrivals.rend())//check if arrival list at previous stop is empty (i.e., skipped by this trip and has not been visited by any others)
		{
			if (rit_closest_arrival->second->get_id() == this->get_id()) //the latest arrival to the upstream stop is this trip
			{
				//check if the next latest arrival to the upstream stop has overtaken this trip (i.e. visited last_visited before this trip)
				bool found = false; //true if another arrival before this trip has been found that has not overtaken this trip
				for (++rit_closest_arrival; rit_closest_arrival != us_stop_arrivals.rend(); rit_closest_arrival++)//loop through other arrivals before this trip at this upstream stop
				{
					if (!rit_closest_arrival->second->has_visited(last_visited)) //true if this trip was not overtaken by the next closest arrival
					{
						if (theParameters->short_turn_control && rit_closest_arrival->second->get_short_turned()) //true if this trip has been overtaken via a short-turned trip
							continue;

						found = true;
						succ_arrival = rit_closest_arrival->first; //this trip has not been overtaken by the next latest arrival
						break;
					}
				}
				if (found)
					break;
			}
			else //there is another bus that has arrived at this upstream stop after this trip
			{
				if (!rit_closest_arrival->second->has_visited(last_visited))//true if this other bus has overtaken this trip
				{
					succ_arrival = rit_closest_arrival->first; //this trip has not been overtaken so this is the closest succeeding bus based on arrival
					break;
				}
			}
		}
	}
	if (stop_it == us_stops.rend()) //no other trips have arrived to any stops upstream that have not already overtaken this trip
bool Bustrip::is_behind(Bustrip* trip)
{
	assert(this->get_line()->get_id() == trip->get_line()->get_id());
	assert(this->get_id() != trip->get_id());

	int nr_downstream = this->get_downstream_stops().size();
	int nr_downstream2 = trip->get_downstream_stops().size();

	if (nr_downstream > nr_downstream2)
		return true; //this trip is behind the trip given as argument (i.e., has more stops downstream to visit)
	else
		return false;
}
		return 0;

	scheduled_tt = calc_scheduled_travel_time_between_stops(last_visited, (*stop_it));
	expected_arrival = succ_arrival + scheduled_tt;
	backward_arr_headway = expected_arrival - arrival_time;
	if (backward_arr_headway < 0)
		cout << "Bustrip::calc_backward_arrival_headway - warning: expected arrival time to last stop visited by trip has already passed" << endl;

	return backward_arr_headway;
}

//double Busstop::get_time_since_arrival (Bustrip* trip, double time) // calculates the headway (between arrivals)
//{  
//	if (last_arrivals.empty()) //Added by Jens 2014-07-03
//		return trip->get_line()->calc_curr_line_headway();
//	double time_since_arrival = time - last_arrivals[trip->get_line()].second;
//	// the headway is defined as the differnece in time between sequential arrivals
//	return time_since_arrival;
//}
//
//double Busstop::get_time_since_departure (Bustrip* trip, double time) // calculates the headway (between departures)
//{  
//	if (trip->get_line()->check_first_trip(trip)==true && trip->get_line()->check_first_stop(this)==true) // for the first stop on the first trip on that line - use the planned headway value
//	{
//		return trip->get_line()->calc_curr_line_headway();
//	}
//	double time_since_departure = time - last_departures[trip->get_line()].second;
//	// the headway is defined as the differnece in time between sequential departures
//	return time_since_departure;
//}

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
	int occupancy = trip->get_busv()->get_occupancy();
	int nr_riders = occupancy + nr_alighting - nr_boarding;
	double riding_time;
	double holdingtime = exit_time - enter_time - dwelltime;
	double crowded_pass_riding_time;
	double crowded_pass_dwell_time;
	double crowded_pass_holding_time;

	if (trip->get_line()->check_first_stop(this) == true)
	{
		riding_time = 0;
		crowded_pass_riding_time = 0;
		crowded_pass_dwell_time = 0;
		crowded_pass_holding_time = 0;
	}
	else
	{
		riding_time = enter_time - trip->get_last_stop_exit_time();
		int nr_seats = trip->get_busv()->get_number_seats();
		crowded_pass_riding_time = calc_crowded_travel_time(riding_time, nr_riders, nr_seats);
		crowded_pass_dwell_time = calc_crowded_travel_time(dwelltime, occupancy, nr_seats);
		crowded_pass_holding_time = calc_crowded_travel_time(holdingtime, occupancy, nr_seats);
	}

	if (trip->get_line()->check_first_trip(trip) == true)
	{
		arrival_headway = 0;
	}
	output_stop_visits.push_back(Busstop_Visit(trip->get_line()->get_id(), trip->get_id() , trip->get_busv()->get_bus_id() , get_id() , enter_time,
		trip->scheduled_arrival_time (this),dwelltime,(enter_time - trip->scheduled_arrival_time (this)), exit_time, riding_time, riding_time * nr_riders, crowded_pass_riding_time, crowded_pass_dwell_time, crowded_pass_holding_time,
		arrival_headway, get_time_since_departure (trip , exit_time), nr_alighting , nr_boarding , occupancy, calc_total_nr_waiting(), (arrival_headway * nr_boarding)/2, holdingtime)); 
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

bool Change_arrival_rate::execute(Eventlist* eventlist, double time)
{		
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
