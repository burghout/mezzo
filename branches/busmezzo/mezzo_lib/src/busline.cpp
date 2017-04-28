#include <sstream>
#include <stddef.h>
#include <math.h>
#include "busline.h"
#include "MMath.h"
#include <iostream>


using namespace std;



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

bool Busline::is_line_transfer_stop (Busstop* stop)
{
	for (vector <Busstop*>::iterator tr = tr_stops.begin(); tr < tr_stops.end(); tr++ )
	{
		if (stop == *(tr))
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

vector<Visit_stop> Busline::get_transfer_stops()
{
	vector<Visit_stop> visit_transfers;
	for(size_t i=0; i<tr_stops.size(); ++i)
	{
		visit_transfers.push_back(make_pair(tr_stops[i],0.0));
	}
	return (visit_transfers);
}

bool Busline::check_stop_in_line_is_transfer(map<int, vector <Busstop*>> trnsfr_lines,Busline* line, Busstop* stop)
{
	for (map<int, vector <Busstop*>>::iterator tr_it = trnsfr_lines.begin(); tr_it != trnsfr_lines.end(); ++tr_it)
	{
		for (vector <Busstop*> ::iterator stop_it = tr_it->second.begin(); stop_it != tr_it->second.end(); ++stop_it)
		{
			if (tr_it->first == line->get_id() & (*stop_it)->get_id() == stop->get_id())
			{
				return true;
			}
		}
	}
	return false;
}

vector<double> Busstop:: get_ini_val_vars(vector<Start_trip> trips_to_handle)
{
	vector<double> opt_var;
	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), trips_to_handle[tr_it].first->down_stops[st_it]->first->get_id());
			hist_set* tmp = (*trips_to_handle[tr_it].first->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];
			trips_to_handle[tr_it].first->down_stops[st_it]->first->riding_time = make_pair(trips_to_handle[tr_it].first->down_stops[st_it]->first, tmp->riding_time);
			
			opt_var.push_back(tmp->riding_time);
		}
	}
	//for the holding time-Zero as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			opt_var.push_back(0.0);
		}
	}
	return opt_var;
}

vector<double> Busstop::get_left_constraints(vector<Start_trip> trips_to_handle)
{
	vector<double> left_cnstrnts;
	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), trips_to_handle[tr_it].first->down_stops[st_it]->first->get_id());
			hist_set* tmp = (*trips_to_handle[tr_it].first->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];
			
			Vehicle* veh = (Vehicle*)(trips_to_handle[tr_it].first->get_busv());
			int st_link_id = trips_to_handle[tr_it].first->down_stops[st_it]->first->get_link_id();
			vector<Link*> links = veh->get_route()->get_links();
			Link* link;
			for (vector<Link*>::iterator iter_links = links.begin(); iter_links < links.end(); ++iter_links)
			{
				if ((*iter_links)->get_id() == st_link_id)
					 link = *iter_links;
			}
			/* in case we get the speed from speed density
			double ro = link->density();
			double speed = link->get_sdfunc()->speed(ro);
			*/
			double speed = link->get_length() / tmp->riding_time;
			//assume we can increase speed in 2 km/hr (0.55556 m/sec)
			double left_cons = link->get_length() / (speed + 0.555555556);
			left_cnstrnts.push_back(left_cons);
		}
	}
	//for the holding time-Zero as left_constraints
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			left_cnstrnts.push_back(0.0);
		}
	}
	return left_cnstrnts;
}

vector<double> Busstop::get_right_constraints(vector<Start_trip> trips_to_handle)
{
	vector<double> right_cnstrnts;

	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), trips_to_handle[tr_it].first->down_stops[st_it]->first->get_id());
			hist_set* tmp = (*trips_to_handle[tr_it].first->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];

			Vehicle* veh = (Vehicle*)(trips_to_handle[tr_it].first->get_busv());
			int st_link_id = trips_to_handle[tr_it].first->down_stops[st_it]->first->get_link_id();
			vector<Link*> links = veh->get_route()->get_links();
			Link* link;
			for (vector<Link*>::iterator iter_links = links.begin(); iter_links < links.end(); ++iter_links)
			{
				if ((*iter_links)->get_id() == st_link_id)
					link = *iter_links;
			}
			/* in case we get the speed from speed density
			double ro = link->density();
			double speed = link->get_sdfunc()->speed(ro);
			*/
			double speed = link->get_length() / tmp->riding_time;
			//assume we can decrease speed in 5 km/hr
			double right_cons = link->get_length() / (speed - 1.38888889); 
			right_cnstrnts.push_back(right_cons);
		}
	}
	//for the holding time-0.7 * headway as right_constraints
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			double line_headway = trips_to_handle[tr_it].first->get_line()->calc_curr_line_headway();
			right_cnstrnts.push_back(0.7*line_headway);
		}
	}
	return right_cnstrnts;
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

bool Busline::check_first_trip_in_handled(vector<Start_trip> trips_to_handle,Bustrip* trip)
{
	for (vector<Start_trip>::iterator tr = trips_to_handle.begin(); tr != trips_to_handle.end(); ++tr)
	{
		if (tr->first->get_line() == trip->get_line())
		{
			if (tr->first == trip){
				return true;
			}
			else {
				return false;
			}
		}
		
	}
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
}

void Bustrip::convert_stops_vector_to_map ()
{
	for (vector <Visit_stop*>::iterator stop_iter = stops.begin(); stop_iter < stops.end(); stop_iter++)
	{
		stops_map[(*stop_iter)->first] =(*stop_iter)->second;
	}
}

void Bustrip::convert_downstreamstops_vector_to_map(vector <Visit_stop*> down_stops)
{
	for (vector <Visit_stop*>::iterator stop_iter = down_stops.begin(); stop_iter < down_stops.end(); stop_iter++)
	{
		downstream_stops_map[(*stop_iter)->first] = (*stop_iter)->second;
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

vector<Visit_stop*>::iterator Bustrip:: get_target_stop(Bustrip *trip, vector <Visit_stop*> :: iterator& next_stop)
{
	//TODO: Find a way to move target_stop without moving the next_stop!!!!
	vector <Visit_stop*>::iterator& target_stop = trip->stops.begin();
	for(vector <Visit_stop*> :: iterator trip_stops = trip->stops.begin(); trip_stops<trip->stops.end(); ++trip_stops)
	{
		if((*trip_stops)->first == (*next_stop)->first)
		{
			target_stop = trip_stops; //target stop contains a pointer to the target stop according to the stop horizon
			break;
		}
	} 
	for(int i=1; i<theParameters->Stop_horizon; i++, ++target_stop)
	{
		if (trip->get_line()->check_last_stop((*target_stop)->first))
		{	
			break;
		}
	}
	return target_stop;
}

vector<Visit_stop*>::iterator Bustrip::get_previous_stop(Bustrip *trip, vector <Visit_stop*> ::iterator& down_stp)
{
	vector <Visit_stop*>::iterator& stop_before = trip->stops.begin();
	for (vector <Visit_stop*> ::iterator stop_before = trip->down_stops.begin(); stop_before<trip->down_stops.end(); ++stop_before)
	{
		if ((*stop_before)->first == (*down_stp)->first)
		{
			--stop_before;
			break;
		}
	}
	return stop_before;
}

vector<Start_trip> Bustrip:: find_trips_to_handle_same_line(vector<Start_trip> line_trips, Visit_stop *target_stop)
{
	vector<Start_trip> trips_to_handle;
	int k = 0;
	//For preceeding trips:
	while (line_trips[k].first != this)
	{
		if (line_trips[k].first->is_stop_in_downstream(target_stop))//if the trip didn't pass the target stop
		{
			trips_to_handle = line_trips[k].first->calc_hist_prdct_arrivals(line_trips[k], target_stop, trips_to_handle);//return the prediction arrival times of trips to handle accrding to historical data
		}
		k++;
	}
	//For proceeding trips:
	int j = 0;
	while (j < theParameters->Bus_horizon && !line_trips[k].first->get_line()->check_last_trip(line_trips[k].first))
	{
		if (line_trips[k].first->is_stop_in_downstream(target_stop))//if the trip didn't pass the target stop
		{
			trips_to_handle = line_trips[k].first->calc_hist_prdct_arrivals(line_trips[k], target_stop, trips_to_handle);//return the prediction arrival times of trips to handle accrding to historical data
		}
		k++, j++;
	}
	return trips_to_handle;
}

vector<Start_trip> Bustrip::find_trips_to_handle_transfer_lines(vector<Start_trip> trips_to_handle)
{
	vector<Start_trip> transfer_trips;
	vector<Visit_stop*> transfer_in_downstream;
	vector <Visit_stop> visit_transfers = this->get_line()->get_transfer_stops();//get the transfer stops for the line
	if (trips_to_handle.size())
	{
		size_t max_stops = trips_to_handle.size();
		for (size_t down_stp_i = 0; down_stp_i < trips_to_handle[max_stops - 1].first->down_stops.size(); ++down_stp_i) //assume that the last trip has the maximum number of stops.
		{
			for (size_t trns_stp_j = 0; trns_stp_j < visit_transfers.size(); ++trns_stp_j)
			{
				if (trips_to_handle[max_stops - 1].first->down_stops[down_stp_i]->first == visit_transfers[trns_stp_j].first)
				{
					transfer_in_downstream.push_back(&visit_transfers[trns_stp_j]);
				}
			}
		}
	}
	
	if (transfer_in_downstream.size()) //if there are transfer stops downstream
	{
		transfer_trips = this->get_transfer_trips(this, transfer_in_downstream, trips_to_handle);
		if (transfer_trips.size())
		{
			for (vector<Start_trip>::iterator tr_it = transfer_trips.begin(); tr_it < transfer_trips.end(); ++tr_it)
			{
				trips_to_handle.push_back((*tr_it)); //At the end trips_to_handle contain all the trips to the optimization
			}
		}
	}//if there is transfer stops downstream
	return trips_to_handle;
}

vector<Visit_stop*>::iterator Bustrip:: get_transfer_target_stop(Bustrip *trip, vector <Visit_stop*> :: iterator& trnsfr_stop)
{
	//vector <Visit_stop*>::iterator& target_stop = trip->get_next_stop(); //target stop contains a pointer to the target stop according to the stop horizon
	//TODO: Find a way to move target_stop without moving the next_stop!!!!
	vector <Visit_stop*>::iterator& target_stop = trip->stops.begin();
	for(vector <Visit_stop*> :: iterator trip_stops = trip->stops.begin(); trip_stops<trip->stops.end(); ++trip_stops)
	{
		if((*trip_stops)->first == (*next_stop)->first)
		{
			target_stop = trip_stops; //target stop contains a pointer to the target stop according to the stop horizon
			break;
		}
	}
	
	while ((*target_stop)->first != (*trnsfr_stop)->first)
	{
		++target_stop;
	}
	for(int i=1; i<theParameters->Stop_horizon; i++, ++target_stop)
	{
		if (trip->get_line()->check_last_stop((*target_stop)->first))
		{	
			break;
		}
	}
	return target_stop;
}


vector<Start_trip> Bustrip::calc_hist_prdct_arrivals(Start_trip trip, Visit_stop *target_stop, vector<Start_trip> trips_to_handle)
{
	//calculate delay/earliness of trips
	double offset = 0.0;
	trip.first->down_stops = trip.first->get_downstream_stops_till_horizon(trip.first, target_stop);
	if (trip.first->get_line()->check_first_trip(trip.first) == true)  //if this is the first trip, get the schedule arrival times
	{
		for (vector <Visit_stop*> ::iterator down_stp = trip.first->down_stops.begin(); down_stp<trip.first->down_stops.end(); ++down_stp)
		{
			if ((*down_stp)->first == (*trip.first->stops.begin())->first)
			{
				(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first]; //offset =0 in this case
			}
			else
			{
				Busstop* last_stp_visited = trip.first->get_last_stop_visited(); //check if the trip enter the stop and didnt exit it, this would be the last visited!!!
				double last_stop_enter_time = trip.first->get_enter_time();
				if (last_stop_enter_time <= 0)
				{
					offset = 0;
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
				else
				{
					offset = last_stop_enter_time - trip.first->stops_map[last_stp_visited];
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
			}
		}
	}
	else
	{
		for (vector <Visit_stop*> ::iterator down_stp = trip.first->down_stops.begin(); down_stp<trip.first->down_stops.end(); ++down_stp)
		{
			double last_departure = (*down_stp)->first->get_last_departure(trip.first->get_line());
			if ((*down_stp)->first->get_had_been_visited(trip.first->get_line()) == true && last_departure >= 0)//if this stop had been visited
			{
				Bustrip* last_trip = (*down_stp)->first->get_last_trip_departure(trip.first->get_line()); //get the last trip visit down_stop
				double last_stop_enter_time = last_trip->get_enter_time();
				offset = last_stop_enter_time - last_trip->stops_map[(*down_stp)->first]; // check if stops_map contains the data of times!!!
				(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];  // check if stops_map contains the data of times!!! 
			}
			//return trips.begin(); // if no trip had visited the stop yet then the first trip is the expected next arrival
			else  //if the down_stp didnt visit yet, then the offset is according to the same trip at the last stop it visited
			{
				Busstop* last_stp_visited = trip.first->get_last_stop_visited(); //check if the trip enter the stop and didnt exit it, this would be the last visited!!!
				double last_stop_enter_time = trip.first->get_enter_time();
				if (last_stop_enter_time <= 0)
				{
					offset = 0;
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
				else
				{
					offset = last_stop_enter_time - trip.first->stops_map[last_stp_visited];
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
			}
		}
	}
	trips_to_handle.push_back(trip);
	return trips_to_handle;
}

vector<Start_trip> Bustrip::get_transfer_trips(Bustrip *trip, vector<Visit_stop*> transfer_in_downstream, vector<Start_trip> trips_to_handle)
{
	vector<Start_trip> trips_in_transfer;
	vector<Start_trip> transfer_trips;
	for (vector<Visit_stop*>::iterator& tr_stps = transfer_in_downstream.begin(); tr_stps< transfer_in_downstream.end(); ++tr_stps)
		//for(size_t tr_stps_i =0; tr_stps_i<transfer_in_downstream.size(); ++tr_stps_i)//for each transfer stop
	{
		map<int, vector <Busstop*>> trnsfr_lines = trip->get_line()->get_transfer_lines(trip->get_line());//get map of the transfer lines
		vector<double> limited_tr_stop = trip->get_first_and_last_trips(trips_to_handle, *tr_stps);//return the time of the first trip and last trip on the transfer stop
		for (map<int, vector <Busstop*>>::iterator tr_it = trnsfr_lines.begin(); tr_it != trnsfr_lines.end(); ++tr_it)//for each transfer line
		{
			if ((*tr_it).first == trip->get_line()->get_id()) break; //loop is on the same trip
			Busline* transfer_line = trip->get_tr_line((*tr_stps)->first, (*tr_it).first); //get the transfer line according to the transfer stop and id of line

			vector<Start_trip> tr_trip = transfer_line->get_trips();  //return the trips for the transfer line
			for (vector<Start_trip>::iterator trnsfr_it = tr_trip.begin(); trnsfr_it<tr_trip.end(); ++trnsfr_it)
			{
				if ((*trnsfr_it).first->is_stop_in_downstream(*tr_stps))//if the trip didn't pass the target stop
				{
					//first calculate the new target_stop taking into account the transfer stop + stop horizon
					vector <Visit_stop*>::iterator transfer_target_stop = (*trnsfr_it).first->get_transfer_target_stop((*trnsfr_it).first, tr_stps);
					transfer_trips = (*trnsfr_it).first->calc_hist_prdct_arrivals((*trnsfr_it), *transfer_target_stop, transfer_trips);//return the prediction arrival times of trips to handle accrding to historical data
																																	   //transfer_trips = (*trnsfr_it).first->calc_predict_times((*trnsfr_it), *transfer_target_stop, transfer_trips);//check if it returns the updated trips_to_handle????
				}
				//k++;
			}
			trips_in_transfer = trip->get_trips_in_transfer_line(transfer_trips, *tr_stps, limited_tr_stop, trips_in_transfer);
			if (trips_in_transfer.size())
			{
				/*for (size_t tr_in = 0; tr_in<trips_awaiting_transfers.size(); ++tr_in)//trips_awaiting_transfers is a busstop
				{
					if (theParameters->Real_time_control_info == 1) {
						//trips_in_transfer[tr_in].first->trip_occupancy.push_back(make_pair(trips_in_transfer[tr_in].first, trips_in_transfer[tr_in].first->get_busv()->get_occupancy()));
					}
					else {
						//here we have to put function that get the occupancy of the trip at this stop
					}

				}
				*/
			}
		}// for transfer_lines
	}//for transfer stops
	return trips_in_transfer;
}


vector<Start_trip> Bustrip::calc_predict_times(Start_trip trip, Visit_stop *target_stop, vector<Start_trip> trips_to_handle)
{				
	//calculate delay/earliness of trips
	double offset = 0.0; 
	trip.first->down_stops = trip.first->get_downstream_stops_till_horizon(trip.first, target_stop); 
	if(trip.first->get_line()->check_first_trip(trip.first) == true)  //if this is the first trip, get the schedule arrival times
	{
		for(vector <Visit_stop*> :: iterator down_stp=trip.first->down_stops.begin(); down_stp<trip.first->down_stops.end(); ++down_stp)
		{
			if((*down_stp)->first == (*trip.first->stops.begin())->first)
			{
				(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first]; //offset =0 in this case
			}
			else
			{
				Busstop* last_stp_visited = trip.first->get_last_stop_visited(); //check if the trip enter the stop and didnt exit it, this would be the last visited!!!
				double last_stop_enter_time = trip.first->get_enter_time();
				if (last_stop_enter_time <=0)
				{
					offset = 0; 
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
				else
				{
					offset = last_stop_enter_time - trip.first->stops_map[last_stp_visited]; 
					//(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
					vector<Visit_stop*>::iterator previous_stop = trip.first->get_previous_stop(trip.first, down_stp);
					(*down_stp)->second = offset + trip.first->stops_map[(*previous_stop)->first];//the arrival time 

					//Now get data of dwell time, travel time.. from historical data
					LineIdStopId lineIdStopIdSched(trip.first->get_line()->get_id(), (*previous_stop)->first->get_id());
					//check why this not working???
					(*previous_stop)->first->riding_time= make_pair((*previous_stop)->first, (*((*previous_stop)->first->history_summary_map_))[lineIdStopIdSched]->dwell_time);
					
					(*((*previous_stop)->first->history_summary_map_))[lineIdStopIdSched]->dwell_time;
					Visit_stop tmp = **previous_stop;
					(*previous_stop)->first->dwell_time=make_pair(tmp.first,tmp.second);//make_pair(trip.first,db));

					//(*previous_stop)->first->riding_time.push_back(trip.first, (*previous_stop)->first->(*history_summary_map_)[lineIdStopIdSched]->dwell_time); //riding time is a decision variable and can be changed, think how to do this!!! 
					// (*down_stp)->second contains the expected arrival time for the bus to the stop at(bus, stop) = at(bus,stop-1) + dwell_time(bus, stop-1) + holding_time(bus,stop-1)+ riding_time(bus, stop-1)
					//double expect_arrv = last_stop_enter_time + (*previous_stop)->first->dwell_time.back().second + (*previous_stop)->first->holding_time.back().second + (*previous_stop)->first->riding_time.back().second;
					//(*down_stp)->first->expected_arrival_time.push_back((*down_stp)->first, expect_arrv);

					//(*down_stp)->second = last_stop_enter_time + (*previous_stop)->first->dwell_time.back().second + (*previous_stop)->first->holding_time.back().second + (*previous_stop)->first->riding_time.back().second;




					//hist_set* hist = new hist_set(line_id, trip_id, vehicle_id, stop_id, entering_time, sched_arr_time, dwell_time, lateness, exit_time, riding_time, riding_pass_time, time_since_arr, time_since_dep, nr_alighting, nr_boarding, occupancy, nr_waiting, total_waiting_time, holding_time);
				}
			}
		}
	}
	else
	{
		for(vector <Visit_stop*> :: iterator down_stp = trip.first->down_stops.begin(); down_stp<trip.first->down_stops.end(); ++down_stp)
		{
			double last_departure = (*down_stp)->first->get_last_departure(trip.first->get_line());
			if ((*down_stp)->first->get_had_been_visited(trip.first->get_line()) == true && last_departure>=0)//if this stop had been visited
			{
				Bustrip* last_trip = (*down_stp)->first->get_last_trip_departure(trip.first->get_line()); //get the last trip visit down_stop
				double last_stop_enter_time = last_trip->get_enter_time();
				offset = last_stop_enter_time - last_trip->stops_map[(*down_stp)->first]; // check if stops_map contains the data of times!!!
				(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];  // check if stops_map contains the data of times!!! 
			}
				//return trips.begin(); // if no trip had visited the stop yet then the first trip is the expected next arrival
			else  //if the down_stp didnt visit yet, then the offset is according to the same trip at the last stop it visited
			{
				Busstop* last_stp_visited = trip.first->get_last_stop_visited(); //check if the trip enter the stop and didnt exit it, this would be the last visited!!!
				double last_stop_enter_time = trip.first->get_enter_time();
				if (last_stop_enter_time <=0)
				{
					offset = 0; 
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
				else
				{
					offset = last_stop_enter_time - trip.first->stops_map[last_stp_visited]; 
					(*down_stp)->second = offset + trip.first->stops_map[(*down_stp)->first];
				}
			}
		}
	}
	trips_to_handle.push_back(trip);
	return trips_to_handle;
}
	

vector<double> Bustrip::get_first_and_last_trips(vector<Start_trip> trips_to_handle,Visit_stop* transfer_stop)
{
	vector<double> first_and_last_times;
	for(vector<Start_trip>::iterator tr_it = trips_to_handle.begin(); tr_it < trips_to_handle.end(); ++tr_it)
	{
		for(vector<Visit_stop*>::iterator down_stp = tr_it->first->down_stops.begin(); down_stp< tr_it->first->down_stops.end(); ++down_stp)
		{
			if((*down_stp)->first == transfer_stop->first)
			{
				first_and_last_times.push_back((*down_stp)->second);
				break;
			}
		}
		if(first_and_last_times.size()) break;
	}
	for(vector<Visit_stop*>::iterator down_stp = trips_to_handle.back().first->down_stops.begin(); down_stp< trips_to_handle.back().first->down_stops.end(); ++down_stp)
	{
		if((*down_stp)->first == transfer_stop->first)
		{
			first_and_last_times.push_back((*down_stp)->second);
			break;
		}
	}
	return first_and_last_times;
}

Busline* Bustrip::get_tr_line(Busstop* it_stp, int tr_id)
{
	Busline* transfer_line;
	vector<Busline*> network_lines = (*it_stp).get_lines();
	for(vector<Busline*>::iterator tr_lines = network_lines.begin(); tr_lines < network_lines.end(); ++tr_lines)
	{
		if((*tr_lines)->get_id() == tr_id)
		{
			transfer_line = (*tr_lines);
			break;
		}
	}
	return transfer_line;
}

vector<Visit_stop*> Bustrip::get_transfers_in_downstream(Bustrip* trip, vector<Visit_stop*> transfer_in_downstream, vector <Visit_stop> visit_transfers)
{
	for(size_t down_stp_i= 0;  down_stp_i< trip->down_stops.size(); ++down_stp_i)
	{
		for( size_t trns_stp_j = 0; trns_stp_j < visit_transfers.size(); ++trns_stp_j)
		{
			if(trip->down_stops[down_stp_i]->first == visit_transfers[trns_stp_j].first)
			{
				transfer_in_downstream.push_back(&visit_transfers[trns_stp_j]);
			}
		}
	}
	return (transfer_in_downstream);
}

bool Bustrip::is_trip_exist (vector<Start_trip> trips_in_transfer, Bustrip* transfer_trip)
{
	for(vector<Start_trip>::iterator trnsfer_itr=trips_in_transfer.begin(); trnsfer_itr<trips_in_transfer.end(); ++trnsfer_itr)
	{
		if(trnsfer_itr->first==transfer_trip) return true;
	}
	return false;
}

double Bustrip::get_predict_time_to_trnsfr(Visit_stop *transfer_stop)
{
	for(vector<Visit_stop*>::iterator down_stp=down_stops.begin(); down_stp<down_stops.end();++down_stp)
	{
		if(*down_stp == transfer_stop) return (*down_stp)->second;
	}
	return 0;
}

vector<Start_trip> Bustrip::get_trips_in_transfer_line(vector<Start_trip> transfer_trips, Visit_stop *transfer_stop ,vector<double> limited_tr_stop, vector<Start_trip> trips_in_transfer)
{
	for(size_t tr=transfer_trips.size()-1; tr=0; --tr)
	{
		if(!transfer_trips[tr].first->is_trip_exist(trips_in_transfer,transfer_trips[tr].first))
		{
			double headway = transfer_trips[tr].first->get_line()->calc_curr_line_headway();//need to check if the trip format isnt 3 how it works
			double predict_trnsfr = transfer_trips[tr].first->get_predict_time_to_trnsfr(transfer_stop);
			if(predict_trnsfr>=limited_tr_stop[0]- headway && predict_trnsfr<=limited_tr_stop[1]+ headway)
			{
				trips_in_transfer.push_back(transfer_trips[tr]);
			}
		}
	}
	return trips_in_transfer;
}

bool Bustrip:: is_stop_in_downstream(Visit_stop *target_stop)
{
	for(vector <Visit_stop*> :: iterator stop = next_stop; stop < stops.end(); stop++)
	{
		if ((*stop)->first == (target_stop)->first)
		{return true;}
	}
	return false;
}

bool Bustrip::check_first_stop_in_handled(vector<Visit_stop*> down_stops, Busstop* stop)
{
	if (down_stops.front()->first == stop)
		return true;
 return false;
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
		double nr_standees_alighting = max(0.0, busv->get_occupancy() - nr_alighting - busv->get_number_seats());
		double nr_standees_boarding = max(0.0, busv->get_occupancy() - nr_alighting - busv->get_number_seats()); //we set nr_standees_boarding = nr_standees_alighting, for that, they have the same calculation
		double crowdedness_ratio_alighting = nr_standees_alighting / (busv->get_capacity()- busv->get_number_seats());
		double crowdedness_ratio_boarding = nr_standees_boarding / (busv->get_capacity()- busv->get_number_seats());
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

vector <Visit_stop*> Bustrip::get_downstream_stops_till_horizon(Bustrip* trip, Visit_stop *target_stop)
{
	vector <Visit_stop*> remaining_stops;
	vector <Visit_stop*> :: iterator next_stp = trip->get_next_stop();
	for(vector <Visit_stop*> :: iterator stop = next_stp; stop < stops.end(); stop++)
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

/*Busstop::Busstop(int id_, string name_, int link_id_, double position_, double length_, bool has_bay_, bool can_overtake_, double min_DT_, int rti_, map<LineIdStopId, hist_set*, CompareLineStop>* history_summary_map_, map<LineIdStopId, hist_demand*, CompareLineStop>* hist_demand_map_, map<LineIdStopId, LineStationData*, CompareLineStop>* hist_dmnd_map_)
{
}*/

Busstop::Busstop (int id_, string name_, int link_id_, double position_, double length_, bool has_bay_, bool can_overtake_, double min_DT_, int rti_,
	map<LineIdStopId, hist_set*, CompareLineStop>* history_summary_map, map<LineIdStopId, hist_demand*, CompareLineStop>* hist_demand_map, map<LineIdStopId, LineStationData*, CompareLineStop>* hist_dmnd_map, map<LineIdStopId, list<pair<int, double>>, CompareLineStop>* transfer_to_my_line):
	id(id_), name(name_), link_id(link_id_), position (position_), length(length_), 
	has_bay(has_bay_), can_overtake(can_overtake_), min_DT(min_DT_), rti (rti_),
	history_summary_map_(history_summary_map), hist_demand_map_(hist_demand_map), hist_dmnd_map_(hist_dmnd_map), transfer_to_my_line_(transfer_to_my_line)
	
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
			if (final_stop == false)
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
							if(theParameters->demand_format == 3)
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
		trip->record_passenger_loads(trip->get_next_stop());
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
			// for real-time corridor control 		
			case 10:			
				if ((trip->get_line()->is_line_timepoint(this)) || (trip->get_line()->is_line_transfer_stop(this)) && !(trip->get_line()->check_last_trip(trip))) // if it is a time point or transfer stop and it is not the last trip
				{
					vector <Visit_stop*>::iterator target_stop = trip->get_target_stop(trip, trip->get_next_stop());
					vector<Start_trip> line_trips = trip->get_line()->get_trips();  //return the trips for the line
					//trips to handle on the same line, for trips that already visited my stop and did not pass the target stop
					//and trips behind me according to the bus_horizon
					vector<Start_trip> trips_to_handle = trip->find_trips_to_handle_same_line(line_trips, *target_stop);
					
					//deal with synchronizing transfer stops:
					if (theParameters->transfer_sync == 1) {// if we are synchronizing transfers
						vector<Start_trip> trips_to_handle = trip->find_trips_to_handle_transfer_lines(trips_to_handle);
					}

				    //return the initial value of the TT and H times for trips in the trips_to_handle
					//The TT is form the historical data and the initila holding time is zero.
					vector<double> opt_var = get_ini_val_vars(trips_to_handle);
					vector<double> left_constrains = get_left_constraints(trips_to_handle);
					vector<double> right_constrains = get_right_constraints(trips_to_handle);
					double Z = total_passengers_time(trips_to_handle, opt_var);

					//return time + dwelltime;  //till now, because I didnt finish the implementation of case 10
				//}//if time point or transfer stop

				// account for passengers that board while the bus is holded at the time point
				// double holding_time = last_departures[trip->get_line()].second - time - dwelltime;
				// int additional_boarding = random -> poisson ((get_arrival_rates (trip)) * holding_time / 3600.0 );
				// nr_boarding += additional_boarding;
				// int curr_occupancy = trip->get_busv()->get_occupancy();
				// trip->get_busv()->set_occupancy(curr_occupancy + additional_boarding); // Updating the occupancy
				// return max(ready_to_depart, holding_departure_time);

					return 0.0;
				}
				break;
				
		default:
			return time + dwelltime;
	}
	return time + dwelltime;
}


double Busstop::total_passengers_time(vector<Start_trip> trips_to_handle, vector<double> ini_vars) //(const column_vector& m)
{
	calc_expected_arrival_departue(trips_to_handle, ini_vars);
	calc_direct_demand(trips_to_handle, ini_vars);
	calc_transfer_demand(trips_to_handle, ini_vars);
			
	
			


		
	
	return 0.0;
}

void Busstop::calc_expected_arrival_departue(vector<Start_trip> trips_to_handle, vector<double> ini_vars)
{
	int iter_ini = 0;
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			Busstop* hndle_stop = trips_to_handle[tr_it].first->down_stops[st_it]->first;
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), hndle_stop->get_id());
			hist_set* his_st = (*hndle_stop->history_summary_map_)[lineIdStopId];
			hndle_stop->dwell_time = make_pair(hndle_stop, his_st->dwell_time);

			//if the stop is the first stop in the line then the arrival time is according the offset I calculated in "calc_hist_prdct_arrivals" 
			if (trips_to_handle[tr_it].first->check_first_stop_in_handled(trips_to_handle[tr_it].first->down_stops, hndle_stop))
			{
				hndle_stop->expected_arrival_time = make_pair(hndle_stop, his_st->sched_arr_time);
			}
			else //at(bus,stop) = at(bus, stop-1) + dwell_time(bus,stop-1) + tt(bus,stop-1) + H(bus,stop-1)
			{
				Busstop* prvs_stop = trips_to_handle[tr_it].first->down_stops[st_it - 1]->first;
				LineIdStopId lineIdStopId_prvs(trips_to_handle[tr_it].first->get_line()->get_id(), prvs_stop->get_id());
				hist_set* hist = (*prvs_stop->history_summary_map_)[lineIdStopId_prvs];
				double dwt_prvs = hist->dwell_time;
				double lst_arvl = prvs_stop->expected_arrival_time.second;
				double tt_prvs = hist->riding_time;
				double expected_arrival = lst_arvl + dwt_prvs + tt_prvs + ini_vars[iter_ini - 1 + ini_vars.size() / 2];
				hndle_stop->expected_arrival_time = make_pair(hndle_stop, expected_arrival);
			}
			//departure time = arrival time + dwell time + holding time 
			double expctd_dprtr = hndle_stop->expected_arrival_time.second + hndle_stop->dwell_time.second + ini_vars[iter_ini + ini_vars.size() / 2]; //holding time is the last elements of the ini_vars
			hndle_stop->expected_departure_time = make_pair(hndle_stop, expctd_dprtr);
			iter_ini += 1;
		}
	}
}

void Busstop::calc_direct_demand(vector<Start_trip> trips_to_handle, vector<double> ini_vars)
{
	//Number of passengers that want to board the stop:
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			Busstop* hndle_stop = trips_to_handle[tr_it].first->down_stops[st_it]->first;
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), hndle_stop->get_id());

			hist_demand* hst_dmnd = (*hndle_stop->hist_demand_map_)[lineIdStopId];
			if (trips_to_handle[tr_it].first->get_line()->check_first_stop(hndle_stop))
			{
				double expctd_baord = hst_dmnd->origin_d *hndle_stop->expected_departure_time.second;
				hndle_stop->trip_tmp_boardings = make_pair(hndle_stop, expctd_baord);
			}
			else if (trips_to_handle[tr_it].first->check_first_stop_in_handled(trips_to_handle[tr_it].first->down_stops, hndle_stop))
			{
				double prvs_dprt = trips_to_handle[tr_it].first->get_last_stop_exit_time();
				double expctd_baord = hst_dmnd->origin_d *(hndle_stop->expected_departure_time.second - prvs_dprt);
				hndle_stop->trip_tmp_boardings = make_pair(hndle_stop, expctd_baord);
			}
			else {
				Busstop* prvs_stop = trips_to_handle[tr_it].first->down_stops[st_it - 1]->first;
				double expctd_baord = hst_dmnd->origin_d *(hndle_stop->expected_departure_time.second - prvs_stop->expected_departure_time.second);
				hndle_stop->trip_tmp_boardings = make_pair(hndle_stop, expctd_baord);
			}
			double expctd_alght = hst_dmnd->destination_d;
			hndle_stop->trip_nr_alightings = make_pair(hndle_stop, expctd_alght);
		}
	}
}

void Busstop::calc_transfer_demand(vector<Start_trip> trips_to_handle, vector<double> ini_vars)
{
	//Number of passengers that want to board the stop:
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); ++tr_it)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); ++st_it)
		{
			//pair<int,int>,list<int> mymap;
			Busstop* hndle_stop = trips_to_handle[tr_it].first->down_stops[st_it]->first;
			LineIdStopId lineIdStopId(trips_to_handle[tr_it].first->get_line()->get_id(), hndle_stop->get_id());

			hist_demand* hst_dmnd = (*hndle_stop->hist_demand_map_)[lineIdStopId];
			if (trips_to_handle[tr_it].first->get_line()->is_line_transfer_stop(hndle_stop))
			{
				list<pair<int, double>> trip_arrival;
				map<int, vector <Busstop*>> trnsfr_lines = trips_to_handle[tr_it].first->get_line()->get_transfer_lines(trips_to_handle[tr_it].first->get_line());
				for (map<int, vector <Busstop*>>::iterator tr_lines_it = trnsfr_lines.begin(); tr_lines_it != trnsfr_lines.end(); ++tr_lines_it)
				{
					for (vector<Busstop*>::iterator tr_st_it = tr_lines_it->second.begin(); tr_st_it != tr_lines_it->second.end(); ++tr_st_it)
					{
						if ((*tr_st_it)->get_id() == hndle_stop->get_id())// this is the transfer stop
						{
							//now I have to find the trips of the transfer line
							for (size_t tr_it1 = 0; tr_it1 < trips_to_handle.size(); ++tr_it1)
							{
								
								if (trips_to_handle[tr_it1].first->get_line()->get_id() == tr_lines_it->first)
								{
									for (size_t st_it1 = 0; st_it1 < trips_to_handle[tr_it1].first->down_stops.size(); ++st_it1)
									{
										Busstop* trnsfr_stop = trips_to_handle[tr_it1].first->down_stops[st_it1]->first;
										if (trnsfr_stop->get_id() == hndle_stop->get_id())
										{
											trip_arrival.push_back(make_pair(trips_to_handle[tr_it1].first->get_id(), trnsfr_stop->expected_arrival_time.second));
										}
									}
								}
							}
						}

						
					}
					
				}

				sort(trip_arrival.begin(), trip_arrival.end(), [](const pair<int, double>&x, 
					const pair<int, double>& y) {
					return x.second < y.second;
				});
				list<pair<int, double>> trip_departure;
				for (size_t tr_it1 = 0; tr_it1 < trips_to_handle.size(); ++tr_it1)
				{
					if (trips_to_handle[tr_it1].first->get_line()->get_id() == trips_to_handle[tr_it].first->get_line()->get_id())
					{
						for (size_t st_it1 = 0; st_it1 < trips_to_handle[tr_it1].first->down_stops.size(); ++st_it1)
						{
							Busstop* trnsfr_stop = trips_to_handle[tr_it1].first->down_stops[st_it1]->first;
							if (trnsfr_stop->get_id() == hndle_stop->get_id())
							{
								trip_departure.push_back(make_pair(trips_to_handle[tr_it1].first->get_line()->get_id(), trnsfr_stop->expected_departure_time.second));
							}
						}
					}
				}
				for (pair<int,double> trip : trip_departure)
				{
					if (trip_arrival.size() == 0)
						break;
					LineIdStopId tmp_pair(trip.first, hndle_stop->get_id());
					//pair<int, int> tmp_pair = make_pair(trip.first, hndle_stop->get_id());
					
					//[tmp_pair] = list<int>();
					while (trip_arrival.front().second<trip.second) {
						pair<int, double> temp = trip_arrival.front();
						trip_arrival.pop_front();
						(*hndle_stop->transfer_to_my_line_)[tmp_pair].push_back(temp);
						LineStationData* linestat = (*hndle_stop->hist_dmnd_map_)[lineIdStopId];
						for (list<BoardDemandTransfer>::iterator brd_tr_it = linestat->boardingTransfers.begin(); brd_tr_it != linestat->boardingTransfers.end(); brd_tr_it++)
						{
							if (brd_tr_it->previous_line_== temp.first & brd_tr_it->previous_station_ == hndle_stop->get_id())
							{
								double tr_board = brd_tr_it->passengers_number_ *(hndle_stop->expected_departure_time.second - temp.second);
									hndle_stop->trip_tmp_trnsfr_boardings = make_pair(hndle_stop, tr_board);
							}
						}
					}
				}
				
					
			}
			
			
			
		}
	}
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

hist_set:: hist_set ()
{
}

hist_set:: hist_set (int line_id_, int trip_id_, int vehicle_id_, int stop_id_, double entering_time_, double sched_arr_time_, double dwell_time_, 
					 double lateness_, double exit_time_, double riding_time_, double riding_pass_time_, double time_since_arr_, double time_since_dep_, 
					 double nr_alighting_, double nr_boarding_, double occupancy_, double nr_waiting_, double total_waiting_time_, double holding_time_)
{
	line_id = line_id_;
	trip_id = trip_id_;
	vehicle_id = vehicle_id_;
	stop_id = stop_id_;
	entering_time = entering_time_;
	sched_arr_time = sched_arr_time_;
	dwell_time = dwell_time_;
	lateness = lateness_;
	exit_time = exit_time_;
	riding_time = riding_time_;
	riding_pass_time = riding_pass_time_;
	time_since_arr = time_since_arr_;
	time_since_dep = time_since_dep_;
	nr_alighting = nr_alighting_;
	nr_boarding = nr_boarding_;
	occupancy = occupancy_;
	nr_waiting = nr_waiting_;
	total_waiting_time = total_waiting_time_;
	holding_time = holding_time_;
}
hist_set::~hist_set()
{
}
hist_paths::hist_paths()
{
}

hist_paths::hist_paths(int passenger_ID_, int origin_ID_, int destination_ID_, double start_time_, double total_walking_time_,
	double total_waiting_time_, double total_waiting_time_denied_, double total_IVT_,
	double total_IVT_crowding_, double end_time_, vector<int> stops_, vector<int> trips_)
{
	passenger_ID = passenger_ID_;
	origin_ID = origin_ID_;
	destination_ID = destination_ID_;
	start_time = start_time_;
	total_walking_time = total_walking_time_;
	total_waiting_time = total_waiting_time_;
	total_waiting_time_denied = total_waiting_time_denied_;
	total_IVT = total_IVT_;
	total_IVT_crowding = total_IVT_crowding_;
	end_time = end_time_;
	stops = stops_;
	trips = trips_;
}

hist_paths::~hist_paths()
{
}

hist_demand::hist_demand()
{
}

hist_demand::hist_demand(int stop_id_, int line_id_, double origin_d_, double destination_d_, double transfer_board_d_, double transfer_alight_d_)
{
	stop_id = stop_id_;
	line_id = line_id_;
	origin_d = origin_d_;
	destination_d = destination_d_;
	transfer_board_d = transfer_board_d_;
	transfer_alight_d = transfer_alight_d_;
}

hist_demand::~hist_demand()
{
}
