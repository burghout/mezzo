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
	for(size_t i=0; i<tr_stops.size(); i++)
	{
		visit_transfers.push_back(make_pair(tr_stops[i],0.0));
	}
	return (visit_transfers);
}

bool Busline::check_stop_in_line_is_transfer(map<int, vector <Busstop*>> trnsfr_lines,Busline* line, Busstop* stop)
{
	for (map<int, vector <Busstop*>>::iterator tr_it = trnsfr_lines.begin(); tr_it != trnsfr_lines.end(); tr_it++)
	{
		for (vector <Busstop*> ::iterator stop_it = tr_it->second.begin(); stop_it != tr_it->second.end(); stop_it++)
		{
			if (tr_it->first == line->get_id() && (*stop_it)->get_id() == stop->get_id())
			{
				return true;
			}
		}
	}
	return false;
}

int Busstop::get_variables_size(vector<Bustrip*> trips_to_handle)
{
	int v_size = 0;
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++, v_size++);
	}
	return (v_size*2);
}

column_vector Busstop:: get_ini_val_vars(vector<Bustrip*> trips_to_handle, int vr_sz)
{
	column_vector opt_var(vr_sz);
	vector<double> temp_val;
	//double ii = opt_var(1, 0);
	int i = 0;
	//double* my_arr = new double(vr_sz);
	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			//since the riding time is time from departure of previous stop to this stop, 
			//I have to get the riding time to the next stop
			vector <Visit_stop*> ::iterator nxt_stop_it = trips_to_handle[tr_it]->get_next_stop_specific_stop(trips_to_handle[tr_it]->down_stops[st_it]->first);
			Visit_stop* next_stop1 = *(nxt_stop_it);
			
			//Busstop* next_stop = trips_to_handle[tr_it]->get_next_stop_specific_stop(trips_to_handle[tr_it]->down_stops[st_it]->first);
			LineIdStopId lineIdStopId(trips_to_handle[tr_it]->get_line()->get_id(), next_stop1->first->get_id());
			//Busstop* this_stop = trips_to_handle[tr_it]->down_stops[st_it]->first;
			hist_set* tmp = (*trips_to_handle[tr_it]->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];
			//down_stops[st_it]->first->riding_time = make_pair(trips_to_handle[tr_it].first->down_stops[st_it]->first, tmp->riding_time);
			//my_arr[i] = tmp->riding_time;
			if (tmp == NULL)
				opt_var(i) = 20;
			else opt_var(i) = tmp->riding_time;
			temp_val.push_back(opt_var(i));
			i++;
		}
	}
	//for the holding time-Zero as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			(opt_var)(i) = 0;
			temp_val.push_back(opt_var(i));
			i++;
		}
	}
	return (opt_var);
}

column_vector Busstop::get_left_constraints(vector<Bustrip*> trips_to_handle, int vr_sz)
{
	column_vector left_cnstrnts(vr_sz);
	int i = 0;
	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			//since the riding time is time from departure of previous stop to this stop, 
			//I have to get the riding time to the next stop
			vector <Visit_stop*> ::iterator nxt_stop_it = trips_to_handle[tr_it]->get_next_stop_specific_stop(trips_to_handle[tr_it]->down_stops[st_it]->first);
			Visit_stop* next_stop1 = *(nxt_stop_it);
			//Busstop* next_stop = trips_to_handle[tr_it]->get_next_stop_specific_stop(trips_to_handle[tr_it]->down_stops[st_it]->first);
			LineIdStopId lineIdStopId(trips_to_handle[tr_it]->get_line()->get_id(), next_stop1->first->get_id());
			hist_set* tmp = (*trips_to_handle[tr_it]->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];
			
			//Vehicle* veh = (Vehicle*)(trips_to_handle[tr_it].first->get_busv());
			//int st_link_id = trips_to_handle[tr_it].first->down_stops[st_it]->first->get_link_id();
			//vector<Link*> links = veh->get_route()->get_links();
			double links_length = find_length_between_stops(trips_to_handle[tr_it], trips_to_handle[tr_it]->down_stops[st_it]->first);

			/* in case we get the speed from speed density
			double ro = link->density();
			double speed = link->get_sdfunc()->speed(ro);
			*/
			double speed;
			if(tmp==NULL) 
				 speed = links_length / 20;
			else 
				 speed = links_length / tmp->riding_time;
			//assume we can increase speed 
			double left_cons;
			//the last trip we fixed it and not change
			if(trips_to_handle[tr_it]->check_last_trip_in_line_handled(trips_to_handle)==true)
			{
				left_cons = links_length / speed;
			}				
			else left_cons = links_length / (speed + theParameters->speed_acceleration);
			left_cnstrnts(i) = left_cons;
			i++;
		}
	}
	//for the holding time-Zero as left_constraints
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			left_cnstrnts(i) = 0;
			i++;
		}
	}
	vector<double> temp_l;
	for (size_t clm_it = 0; clm_it< left_cnstrnts.size(); clm_it++)
	{
		temp_l.push_back(left_cnstrnts(clm_it));
	}
	return left_cnstrnts;
}

column_vector Busstop::get_right_constraints(vector<Bustrip*> trips_to_handle, int vr_sz)
{
	column_vector right_cnstrnts(vr_sz);
	int i = 0;
	//for travel time- average travel time from historical data as initial vars
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			//since the riding time is time from departure of previous stop to this stop, 
			//I have to get the riding time to the next stop
			vector <Visit_stop*> ::iterator nxt_stop_it = trips_to_handle[tr_it]->get_next_stop_specific_stop(trips_to_handle[tr_it]->down_stops[st_it]->first);
			Visit_stop* next_stop1 = *(nxt_stop_it);

			LineIdStopId lineIdStopId(trips_to_handle[tr_it]->get_line()->get_id(), next_stop1->first->get_id());

			hist_set* tmp = (*trips_to_handle[tr_it]->down_stops[st_it]->first->history_summary_map_)[lineIdStopId];

			double links_length = find_length_between_stops(trips_to_handle[tr_it], trips_to_handle[tr_it]->down_stops[st_it]->first);
			double speed;
			if(tmp==NULL)
				speed = links_length / 20;
			else speed = links_length / tmp->riding_time;

			double right_cons;
			//the last trip we fixed it and not change
			if (trips_to_handle[tr_it]->check_last_trip_in_line_handled(trips_to_handle) == true || speed - theParameters->speed_deceleration<0)
				right_cons = links_length / speed;
			else right_cons = links_length / (speed - theParameters->speed_deceleration);
			right_cnstrnts(i) = right_cons;
			i++;
		}
	}
	//for the holding time
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			double line_headway = trips_to_handle[tr_it]->get_line()->calc_curr_line_headway();
			if (trips_to_handle[tr_it]->check_last_trip_in_line_handled(trips_to_handle) == true)
				right_cnstrnts(i)=0;
			else 
				right_cnstrnts(i) = trips_to_handle[tr_it]->get_line()->get_max_headway_holding()*line_headway;
			i++;
		}
	}
	vector<double> temp_r;
	for (size_t clm_it = 0; clm_it< right_cnstrnts.size(); clm_it++)
	{
		temp_r.push_back(right_cnstrnts(clm_it));
	}
	return right_cnstrnts;
}

double Busstop::find_length_between_stops(Bustrip* trip, Busstop* stop)
{
	double links_length = 0;
	vector<Link*> links = trip->get_line()->get_busroute()->get_links();
	vector<Link*> stoplinks = trip->get_links_for_stop(stop);
	for (vector<Link*>::iterator iter_links = stoplinks.begin(); iter_links < stoplinks.end(); iter_links++)
	{
		links_length += (*iter_links)->get_length();
	}
	return links_length;
}


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

bool Busline::check_first_trip_in_handled(vector<Bustrip*> trips,Bustrip* trip)
{
	for (vector<Bustrip*>::iterator tr = trips.begin(); tr != trips.end(); tr++)
	{
		if ((*tr)->get_line() == trip->get_line())
		{
			if ((*tr) == trip){
				return true;
			}
			else {
				return false;
			}
		}	
	}
	return false;
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

	Base_departure.clear();
	dwell_time.clear();
	expected_arrival_time.clear();
	expected_departure_time.clear();
	tmp_boardings.clear();  
	nr_boardings.clear(); 
	tmp_board_orgn_trnsfr.clear(); 
	nr_board_orgn_trnsfr.clear(); 
	tmp_trnsfr_boardings.clear();
	nr_trnsfr_boardings.clear();
	dnb_direct.clear(); 
	dnb_frst_trnsfr.clear();
	dnb_trnsfr.clear(); 
	nr_board_dnb_direct.clear();
	nr_board_dnb_frst_trnsfr.clear(); 
	nr_board_dnb_trnsfr.clear(); 
	tmp_occupancy_bus.clear();
	occupancy_bus.clear();
	nr_alightings.clear(); 
	nr_trnsfr_alightings.clear(); 
	nr_dnb_alightings.clear();
	nr_trnfr_dnb_alightings.clear();

	num_tmp_boardings.clear();
	num_boardings.clear();
	num_tmp_trnsfr_frst_boardings.clear();
	num_trnsfr_frst_boardings.clear();
	num_tmp_trnsfr_boardings.clear();
	num_trnsfr_boardings.clear();
	num_ndb_boardings.clear();
	num_ndb_frst_trnsfr_boardings.clear();
	num_ndb_trnfr_boardings.clear();
	num_board_ndb_boardings.clear(); 
	num_board_ndb_frst_transfer.clear(); 
	num_board_ndb_transfer.clear();
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
	vector <Visit_stop*>::iterator& target_stop = trip->stops.begin();
	for(vector <Visit_stop*> :: iterator trip_stops = trip->stops.begin(); trip_stops<trip->stops.end(); trip_stops++)
	{
		if((*trip_stops)->first == (*next_stop)->first)
		{
			target_stop = trip_stops; //target stop contains a pointer to the target stop according to the stop horizon
			if (trip->get_line()->check_last_stop((*target_stop)->first)) {
				target_stop--;
			}
			break;
		}
	} 
	for(int i=1; i<theParameters->Stop_horizon && target_stop<trip->stops.end(); i++)
	{
		if (trip->get_line()->check_last_stop((*target_stop)->first))
		{	
			target_stop--;
			return target_stop;
		}
		target_stop++;
		if (trip->get_line()->check_last_stop((*target_stop)->first))
		{
			target_stop--;
		}

	}
	return target_stop;
}

Busstop* Bustrip::get_previous_stop(Busstop* stop)
{
	vector <Visit_stop*> ::iterator stp_bfr;
	for (stp_bfr = stops.begin(); stp_bfr != stops.end(); stp_bfr++)
	{
		if ((*stp_bfr)->first->get_id() == stop->get_id())
		{
			--stp_bfr;
			break;
		}
	}
	return (*stp_bfr)->first;
}

vector <Visit_stop*>::iterator Bustrip::get_next_stop_specific_stop(Busstop* stop)
{
	vector<Visit_stop*> ::iterator next_stp;
	for (next_stp = stops.begin(); next_stp != stops.end(); next_stp++) {
		if ((*next_stp)->first == stop) {
			++next_stp;
			break;
		}
	}
	return next_stp;
}

Busstop* Bustrip::get_stop_for_previous_trip(Busstop* stop)
{
	Bustrip* prvs_trip = this->get_line()->get_previous_trip(this);
	Busstop* spec_stop;
	for (vector<Visit_stop*>::iterator st_it = prvs_trip->stops.begin(); st_it != prvs_trip->stops.end(); st_it++)
	{
		if (stop->get_id() == (*st_it)->first->get_id())
		{
			spec_stop = (*st_it)->first;
			break;
		}
	}
	return spec_stop;
}

vector<Busstop*> Bustrip::get_downstream_stops_from_spc_stop(Busstop* stop)
{
	vector <Busstop*> remaining_stops;
	vector <Visit_stop*> ::iterator next_stp = stops.begin();
	vector <Visit_stop*> ::iterator stop_it;
	for (stop_it = next_stp; stop_it < stops.end(); stop_it++)
	{
		if ((*stop_it)->first->get_id() == stop->get_id())
		{
			break;
		}
	}
	
	stop_it++;
	while(stop_it<stops.end())
	{
		remaining_stops.push_back((*stop_it)->first);
		stop_it++;
	}
	return remaining_stops;
}

vector<Bustrip*> Bustrip:: find_trips_to_handle_same_line(vector<Start_trip> line_trips, Visit_stop *target_stop, Busstop* hndle_stop)
{
	
	int k = 0;
	//For preceeding trips:
	while (line_trips[k].first != this)
	{
		if (line_trips[k].first->is_stop_in_downstream(target_stop))//if the trip didn't pass the target stop
		{
			Bustrip* trip_hndle = calc_hist_prdct_arrivals(line_trips[k], target_stop);
			
			Busstop* first_handled = trip_hndle->down_stops.front()->first;
			if (first_handled->get_id() == hndle_stop->get_id()) //means that the previous trip is still in the handled stop, but we will not change its exit time
				trip_hndle->down_stops.erase(trip_hndle->down_stops.begin());

			vector<Visit_stop*> ::iterator this_stop;
			for (this_stop = line_trips[k].first->stops.begin(); this_stop != line_trips[k].first->stops.end(); this_stop++) {
				if ((*this_stop)->first == first_handled) {
					break;
				}
			}
			for (; this_stop != line_trips[k].first->stops.end(); this_stop++)
			{
				Busstop* stop_it = (*this_stop)->first;
				if (stop_it->get_id() == hndle_stop->get_id())
					int st = hndle_stop->get_id();
			}
			trips_to_handle.push_back(trip_hndle);//return the prediction arrival times of trips to handle accrding to historical data	
		}
		k++;
	}
	//For proceeding trips:
	int j = 0;
	while (j < theParameters->Bus_horizon && !line_trips[k].first->get_line()->check_last_trip(line_trips[k].first))
	{
		if (line_trips[k].first->is_stop_in_downstream(target_stop))//if the trip didn't pass the target stop
		{
			trips_to_handle.push_back(calc_hist_prdct_arrivals(line_trips[k], target_stop));//return the prediction arrival times of trips to handle accrding to historical data
		}
		k++, j++;
	}
	return trips_to_handle;
}

vector<Bustrip*> Bustrip::find_trips_to_handle_transfer_lines(vector<Bustrip*> trips_to_handle)
{
	vector<Bustrip*> transfer_trips;
	vector<Visit_stop*> transfer_in_downstream;
	vector <Visit_stop> visit_transfers = this->get_line()->get_transfer_stops();//get the transfer stops for the line
	if (visit_transfers.size() == 0) {
		return trips_to_handle;
	}
	if (trips_to_handle.size())
	{
		size_t max_stops = trips_to_handle.size();
		for (size_t down_stp_i = 0; down_stp_i < trips_to_handle[max_stops - 1]->down_stops.size(); down_stp_i++) //assume that the last trip has the maximum number of stops.
		{
			for (size_t trns_stp_j = 0; trns_stp_j < visit_transfers.size(); trns_stp_j++)
			{
				if (trips_to_handle[max_stops - 1]->down_stops[down_stp_i]->first == visit_transfers[trns_stp_j].first)
				{
					transfer_in_downstream.push_back(&visit_transfers[trns_stp_j]);
				}
			}
		}
	}
	
	if (transfer_in_downstream.size()) //if there are transfer stops downstream
	{
		transfer_trips = this->get_transfer_trips(transfer_in_downstream, trips_to_handle);
		if (transfer_trips.size())
		{
			for (vector<Bustrip*>::iterator tr_it = transfer_trips.begin(); tr_it < transfer_trips.end(); tr_it++)
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
	vector <Visit_stop*>::iterator& tr_target_stop = trip->stops.begin();
	for(vector <Visit_stop*> :: iterator trip_stops = trip->stops.begin(); trip_stops<trip->stops.end(); trip_stops++)
	{
		if((*trip_stops)->first == (*next_stop)->first)
		{
			tr_target_stop = trip_stops; //target stop contains a pointer to the target stop according to the stop horizon
			if (trip->get_line()->check_last_stop((*tr_target_stop)->first)) {
				tr_target_stop--;
			}
			break;
		}
	}
	
	while ((*tr_target_stop)->first != (*trnsfr_stop)->first)
	{
		++tr_target_stop;
	}
	/*for now do not take horizon for the transfer line
	for(int i=1; i<theParameters->Stop_horizon; i++, ++tr_target_stop)
	{
		if (trip->get_line()->check_last_stop((*tr_target_stop)->first))
		{	
			tr_target_stop--;
			break;
		}
	}*/
	if(trip->get_line()->check_last_stop((*tr_target_stop)->first))
	{
		tr_target_stop--;
	}
	return tr_target_stop;
}


Bustrip* Bustrip::calc_hist_prdct_arrivals(Start_trip trip, Visit_stop *target_stop)
{
	//calculate delay/earliness of trips
	trip.first->down_stops = trip.first->get_downstream_stops_till_horizon(trip.first, target_stop);
	
	for (size_t st_it = 0; st_it < trip.first->down_stops.size(); st_it++)
	{
		Busstop* hndle_stop = trip.first->down_stops[st_it]->first;
		LineIdStopId lineIdStopId(trip.first->get_line()->get_id(), hndle_stop->get_id());
		hist_set* his_st = (*hndle_stop->history_summary_map_)[lineIdStopId];
		if (trip.first->get_id() == this->get_id() && st_it == 0)//means this is the handled stop
			trip.first->dwell_time[hndle_stop] = down_stops[st_it]->first->get_dwell_time();
		else {
			if (his_st == NULL)
				trip.first->dwell_time[hndle_stop] = 5; //5 minutes as minimum dwell time
			else
				trip.first->dwell_time[hndle_stop] = his_st->dwell_time;
		}
			
		//if the stop is the first stop in the line then the arrival time is according the offset I calculated in "calc_hist_prdct_arrivals" 
		if (trip.first->get_line()->check_first_stop(hndle_stop))
		{
			if (trip.first->enter_time > 0) {//means that this is the handle trip, bus already enter the stop
				trip.first->expected_arrival_time[hndle_stop] = trip.first->enter_time;
			}
			else {
				double sched_arr = trip.first->scheduled_arrival_time(hndle_stop);
				trip.first->expected_arrival_time[hndle_stop] = sched_arr;
				trip.first->down_stops[st_it]->second = sched_arr;
			}
		}
		else //at(bus,stop) = at(bus, stop-1) + dwell_time(bus,stop-1) + tt(bus,stop-1) + H(bus,stop-1)
		{
			if (st_it == 0) //means that the previous stop served before
			{
				Busstop* prvs_stop = trip.first->get_previous_stop(hndle_stop);
				LineIdStopId lineIdStopId_prvs(trip.first->get_line()->get_id(), prvs_stop->get_id());
				//hist_set* hist = (*prvs_stop->history_summary_map_)[lineIdStopId_prvs];
				double lst_dprtr = prvs_stop->get_last_departure(trip.first->get_line());
				if (trip.first->get_id()==this->get_id())//that means that this is the handle trip, and now the bus enetr the stop
				{
					trip.first->expected_arrival_time[hndle_stop] = trip.first->enter_time;
				}
				else {	
					double tt_prvs;
					if (his_st == NULL)
						tt_prvs = 20; //20 minutes for riding time
					else
						tt_prvs = his_st->riding_time; //riding time is for the previous
					double expected_arrival = lst_dprtr + tt_prvs;
					
					trip.first->expected_arrival_time[hndle_stop] = expected_arrival;
				}
				trip.first->down_stops[st_it]->second = trip.first->expected_arrival_time[hndle_stop];
			}
			else {
				Busstop* prvs_stop = trip.first->down_stops[st_it - 1]->first;
				LineIdStopId lineIdStopId_prvs(trip.first->get_line()->get_id(), prvs_stop->get_id());
				double dwt_prvs = trip.first->dwell_time[prvs_stop];
				double lst_arvl = trip.first->expected_arrival_time[prvs_stop];
				double tt_prvs;
				if (his_st == NULL)
					tt_prvs = 20;
				else tt_prvs = his_st->riding_time;//riding time is for the previous
				double expected_arrival = lst_arvl + dwt_prvs + tt_prvs;
				trip.first->expected_arrival_time[hndle_stop] = expected_arrival;
				trip.first->down_stops[st_it]->second = expected_arrival;
			}
		}
		if (trip.first->get_line()->check_first_trip(trip.first) == true)
		{
			trip.first->Base_departure[hndle_stop] = trip.first->expected_arrival_time[hndle_stop] + trip.first->dwell_time[hndle_stop];
		}
	}
	return (trip.first);
}

vector<Bustrip*> Bustrip::get_transfer_trips(vector<Visit_stop*> transfer_in_downstream, vector<Bustrip*> trips_to_handle)
{
	vector<Bustrip*> trips_in_transfer;
	vector<Bustrip*> transfer_trips;
	map<int, vector <Busstop*>> trnsfr_lines = get_line()->get_transfer_lines(get_line());//get map of the transfer lines
	for (vector<Visit_stop*>::iterator& tr_stps = transfer_in_downstream.begin(); tr_stps< transfer_in_downstream.end(); tr_stps++)
		//for(size_t tr_stps_i =0; tr_stps_i<transfer_in_downstream.size(); ++tr_stps_i)//for each transfer stop
	{
		vector<double> limited_tr_stop = get_first_and_last_trips(trips_to_handle, *tr_stps);//return the time of the first trip and last trip on the transfer stop
		for (map<int, vector <Busstop*>>::iterator tr_it = trnsfr_lines.begin(); tr_it != trnsfr_lines.end(); tr_it++)//for each transfer line
		{
			if ((*tr_it).first == get_line()->get_id()) continue; //loop is on the same trip
			Busline* transfer_line = get_tr_line((*tr_stps)->first, (*tr_it).first); //get the transfer line according to the transfer stop and id of line

			vector<Start_trip> tr_trip = transfer_line->get_trips();  //return the trips for the transfer line
			for (vector<Start_trip>::iterator trnsfr_it = tr_trip.begin(); trnsfr_it<tr_trip.end(); trnsfr_it++)
			{
				if ((*trnsfr_it).first->is_stop_in_downstream(*tr_stps))//if the trip didn't pass the target stop
				{
					//first calculate the new target_stop taking into account the transfer stop + stop horizon
					vector <Visit_stop*>::iterator transfer_target_stop = (*trnsfr_it).first->get_transfer_target_stop((*trnsfr_it).first, tr_stps);
					
					vector<Bustrip*>::iterator it;
					it = find(transfer_trips.begin(), transfer_trips.end(), (*trnsfr_it).first);
					if (it != transfer_trips.end())//means that this trip already exist with previou transfer stop
					{
						transfer_trips.erase(it);
					}
					transfer_trips.push_back (calc_hist_prdct_arrivals((*trnsfr_it), *transfer_target_stop));//return the prediction arrival times of trips to handle accrding to historical data
				}
				//k++;
			}
			trips_in_transfer = get_trips_in_transfer_line(transfer_trips, *tr_stps, limited_tr_stop);
		}// for transfer_lines
	}//for transfer stops
	return trips_in_transfer;
}
	

vector<double> Bustrip::get_first_and_last_trips(vector<Bustrip*> trips_to_handle,Visit_stop* transfer_stop)
{
	vector<double> first_and_last_times;
	for(vector<Bustrip*>::iterator tr_it = trips_to_handle.begin(); tr_it < trips_to_handle.end(); tr_it++)
	{
		for(vector<Visit_stop*>::iterator down_stp = (*tr_it)->down_stops.begin(); down_stp< (*tr_it)->down_stops.end(); down_stp++)
		{
			if((*down_stp)->first == transfer_stop->first)
			{
				first_and_last_times.push_back((*down_stp)->second);
				break;
			}
		}
		if(first_and_last_times.size()) break;
	}
	for(vector<Visit_stop*>::iterator down_stp = trips_to_handle.back()->down_stops.begin(); down_stp< trips_to_handle.back()->down_stops.end(); down_stp++)
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
	for(vector<Busline*>::iterator tr_lines = network_lines.begin(); tr_lines < network_lines.end(); tr_lines++)
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
	for(size_t down_stp_i= 0;  down_stp_i< trip->down_stops.size(); down_stp_i++)
	{
		for( size_t trns_stp_j = 0; trns_stp_j < visit_transfers.size(); trns_stp_j++)
		{
			if(trip->down_stops[down_stp_i]->first == visit_transfers[trns_stp_j].first)
			{
				transfer_in_downstream.push_back(&visit_transfers[trns_stp_j]);
			}
		}
	}
	return (transfer_in_downstream);
}

bool Bustrip::is_trip_exist (vector<Bustrip*> trips_in_transfer, Bustrip* transfer_trip)
{
	for(vector<Bustrip*>::iterator trnsfer_itr=trips_in_transfer.begin(); trnsfer_itr<trips_in_transfer.end(); trnsfer_itr++)
	{
		if((*trnsfer_itr) == transfer_trip) return true;
	}
	return false;
}

double Bustrip::get_predict_time_to_trnsfr(Visit_stop *transfer_stop)
{
	double pred_time;
	for(vector<Visit_stop*>::iterator down_stp=down_stops.begin(); down_stp<down_stops.end();++down_stp)
	{
		if ((*down_stp)->first->get_id() == transfer_stop->first->get_id())
		{
			pred_time = (*down_stp)->second;
			break;
		}
	}
	return pred_time;
}

vector<Bustrip*> Bustrip::get_trips_in_transfer_line(vector<Bustrip*> transfer_trips, Visit_stop *transfer_stop ,vector<double> limited_tr_stop)
{
	 vector<Bustrip*> trips_in_transfer;
	for(size_t tr=0; tr < transfer_trips.size(); tr++)
	{
		//double headway = transfer_trips[tr].first->get_line()->calc_curr_line_headway();//need to check if the trip format isnt 3 how it works
		double predict_trnsfr = transfer_trips[tr]->get_predict_time_to_trnsfr(transfer_stop);
		//since at has to be less that dt in order to board transfer;and we compare with arrival, so i took -headway
		//if(predict_trnsfr>=(limited_tr_stop[0]-transfer_trips[tr]->get_line()->calc_curr_line_headway()) && predict_trnsfr<=limited_tr_stop[1])
		if (predict_trnsfr >= limited_tr_stop[0] && predict_trnsfr <= limited_tr_stop[1])
		{
			trips_in_transfer.push_back(transfer_trips[tr]);
		}
	}
	return trips_in_transfer;
}

bool Bustrip::check_last_trip_in_line_handled(vector<Bustrip*> trips_to_handle)
{
	if (this->get_id() == trips_to_handle.back()->get_id())
		return true;

	vector<int> trip_ids;
	for (size_t tr_it = 0; tr_it < trips_to_handle.size() - 1; tr_it++)
	{
		if (trips_to_handle[tr_it]->get_line()->get_id() != trips_to_handle[tr_it + 1]->get_line()->get_id())
		{
			trip_ids.push_back(trips_to_handle[tr_it]->get_id());
		}
	}
	for (int trip_id : trip_ids)
	{
		if (trip_id == this->get_id())
			return true;
	}
	
	return false;

}

pair<Bustrip*, Busstop*> Bustrip:: get_specific_trip(vector<Start_trip> trips_to_handle, int trip_id, int stop_id)
{
	pair<Bustrip*, Busstop*> spec_trip_stop;
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		Bustrip* handle_trip = trips_to_handle[tr_it].first;
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it].first->down_stops.size(); st_it++)
		{
			if (handle_trip->get_id() == trip_id && down_stops[st_it]->first->get_id() == stop_id)
			{
				spec_trip_stop = make_pair(handle_trip, down_stops[st_it]->first);
				break;
			}
		}
	}
	return(spec_trip_stop);
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
vector<Link*> Bustrip::get_links_for_stop(Busstop* stop)
{
	vector <Visit_stop*> ::iterator nxt_stop_it = this->get_next_stop_specific_stop(stop);
	Visit_stop* next_stop1 = *(nxt_stop_it);
	int st_link_id = stop->get_link_id();
	int next_stop_link = next_stop1->first->get_link_id();

	//vector<Link*> links = veh->get_route()->get_links();
	vector<Link*> links = this->get_line()->get_busroute()->get_links();
	vector<Link*> stoplinks;

	for (vector<Link*>::iterator iter_links = links.begin(); iter_links < links.end(); iter_links++)
	{
		if ((*iter_links)->get_id() == st_link_id) 
		{
			while ((*iter_links)->get_id() != next_stop_link) {
				stoplinks.push_back(*iter_links);
				++iter_links;
			}
			break;
		}			
	}
	return stoplinks;
}

double Bustrip::get_trnsfr_expected_departure(vector<Bustrip*> trips_to_handle, Busstop* handle_stop)
{
	double dep_time;
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		Bustrip* handle_trip = trips_to_handle[tr_it];
		if (this->get_id() == handle_trip->get_id())
		{
			for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
			{
				if (down_stops[st_it]->first->get_id() == handle_stop->get_id())
				{
					dep_time = handle_trip->expected_departure_time[down_stops[st_it]->first];
				}
			}
		}
	}
	return dep_time;
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
		if (trip->get_last_stop_visited() == (*stop)->first && trip->get_id() != this->get_id())//that means this trip enter the stop and didnt exit it yet
		{
			int prb = 1;
			continue;
		}
		remaining_stops.push_back(*stop);
		if ((*stop)->first == (target_stop)->first)
		{
			return remaining_stops;
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
	map<LineIdStopId, hist_set*, CompareLineStop>* history_summary_map, map<LineIdStopId, LineStationData*, CompareLineStop>* hist_dmnd_map):
	id(id_), name(name_), link_id(link_id_), position (position_), length(length_), 
	has_bay(has_bay_), can_overtake(can_overtake_), min_DT(min_DT_), rti (rti_),
	history_summary_map_(history_summary_map), hist_dmnd_map_(hist_dmnd_map)
	
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
	
		if (exiting_trip->get_complying() == true && exiting_trip->get_line()->get_holding_strategy() == 10)//Hend added 11/7/17 to update the ccalculations of the p
		{
			if (exiting_trip->get_line()->check_last_stop(this) == false && exiting_trip->get_line()->check_last_trip(exiting_trip) == false)
			{
				update_predictions(exiting_trip, this, time);
				
				/*********************For summary report*************/
				double delta_time;
				if (exiting_trip->get_line()->check_first_trip(exiting_trip))
				{
					if (exiting_trip->expected_departure_time[this] < theParameters->start_pass_generation)
						delta_time = 0;
					else if (exiting_trip->expected_departure_time[this] - theParameters->start_pass_generation < exiting_trip->get_line()->calc_curr_line_headway() + exiting_trip->expected_departure_time[this] - exiting_trip->Base_departure[this])
						delta_time = exiting_trip->expected_departure_time[this] - theParameters->start_pass_generation;
					else
						delta_time = exiting_trip->get_line()->calc_curr_line_headway() + exiting_trip->expected_departure_time[this] - exiting_trip->Base_departure[this];
				}

				else {
					Bustrip* prvs_trip = exiting_trip->get_line()->get_previous_trip(exiting_trip);
					double last_dp = prvs_trip->expected_departure_time[this];
					delta_time = exiting_trip->expected_departure_time[this] - last_dp;
				}

				if (exiting_trip->get_line()->check_last_stop(this) == false)
				{
					vector <Visit_stop*> ::iterator nxt_stop_it = exiting_trip->get_next_stop_specific_stop(this);
					Visit_stop* next_stop1 = *(nxt_stop_it);
					ofstream HendDifffile;
					HendDifffile.open("Occupancy Diff.txt", fstream::app);
					//HendDifffile << "\n";
					HendDifffile << "Trip id:" << exiting_trip->get_id() << ";Stop_id:" << this->get_id() << ";Real Occupancy:" <<
						exiting_trip->get_busv()->get_occupancy() << ";Expected previous Occupancy:" << exiting_trip->occupancy_bus[this] <<
						";Delta time:" << delta_time << ";Expected arrival time:" << exiting_trip->expected_arrival_time[this]<<
						";Expected Occupancy:" << exiting_trip->occupancy_bus[next_stop1->first] <<
						";Expected departure time:" << exiting_trip->expected_departure_time[this] <<
						";boarding pass:" << exiting_trip->nr_boardings[this] << ";alighting pass:" << exiting_trip->nr_alightings[this] <<
						";denied pass:" << exiting_trip->dnb_direct[this]<< ";board from denied pass:" << exiting_trip->nr_board_dnb_direct[this]<<
						";alight from denied pass:" << exiting_trip->nr_dnb_alightings[this]<< endl;
					//HendDifffile << "Expected Occupancy:	" << exiting_trip->occupancy_bus[this]<< endl;

					HendDifffile.flush();
					HendDifffile.close();
				}
				/**********************************/
			}
		}
		else if (exiting_trip->get_complying() == true && exiting_trip->get_line()->get_holding_strategy() == 11)//Hend added 4/12/17 to update the calculations of the passengers
		{
			if (exiting_trip->get_line()->check_last_stop(this) == false && exiting_trip->get_line()->check_last_trip(exiting_trip) == false)
			{
				update_predictions_notrnsfr(exiting_trip, this, time);
			}
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
	
		double speed;
		if (veh->controlled_speed != 0) {
			speed = veh->controlled_speed;
		}
		else speed = veh->get_curr_link()->get_sdfunc()->speed(ro);
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
		entering_trip->set_enter_time(time);
		exit_time = calc_exiting_time(eventlist, entering_trip, time); // get the expected exit time according to dwell time calculations and time point considerations
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
		case 23://same as case 12 but with log distribution: for the Metronit case study
			crowding_factor = trip->crowding_dt_factor(nr_boarding, nr_alighting);
			dwelltime = dwell_constant + dt_func->boarding_coefficient * crowding_factor.first * nr_boarding + dt_func->alighting_cofficient * crowding_factor.second * nr_alighting;
			dwelltime = log(dwelltime); //log return the ln of the dwell time
			break;
	}
	if (dt_func->dwell_time_function_form == 23)
	{
		dwelltime = exp(random->nrandom(dwelltime, dt_func->dwell_std_error));
	}
	else {
		dwelltime = max(dwelltime + random->nrandom(0, dt_func->dwell_std_error), dt_func->dwell_constant + min_DT + random->nrandom(0, dt_func->dwell_std_error));
	}
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
	if (trip->get_complying() == true && trip->get_line()->get_holding_strategy() == 10)
	{
		if (trip->get_line()->check_last_stop(this) == false && trip->get_line()->check_last_trip(trip) == false) {
			pair<double, double> tt_hold = calc_holding_departure_travel_time(trip, time, this);
			holding_departure_time = tt_hold.second;
			//for the travel time, we implement all the acc/decc in the current link. So I have to caclulat the new time
			double links_length = find_length_between_stops(trip, this);
			double new_speed = links_length / tt_hold.first;
			double curr_link_length = trip->get_busv()->get_curr_link()->get_length();
			double curr_link_time = tt_hold.first - ((links_length - curr_link_length) / trip->get_busv()->get_curr_link()->get_sdfunc()->speed(0));
			new_speed = curr_link_length / curr_link_time;
			trip->get_busv()->controlled_speed = new_speed;
		}
		else {
			trip->get_busv()->controlled_speed = 0;//then if controlled_speed=0 so freeflowtime
		}
	}
	else if (trip->get_complying() == true)
	{
		holding_departure_time = calc_holding_departure_time(trip, time); //David added 2016-04-01
	}
	double ready_to_depart = max(time + dwelltime, holding_departure_time);
	//Hend added 16/7/17 for holding_strategy=10: update the departure time for future calculations:
	trip->expected_departure_time[this] = ready_to_depart;
		

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
			// Rule-based headway-based control with passenger load to boarding ratio (based on Erik's formulation)
			// basically copying case 9, but with different assumptions regarding the real time information (just position of buses
			//and historical data
			case 11:
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
																																				// double average_planned_headway = (trip->get_line()->calc_curr_line_headway_forward() + trip->get_line()->calc_curr_line_headway())/2;
						double pass_ratio;

						// vector <Visit_stop*> :: iterator& next_stop = trip->get_next_stop(); // finding the arrival rate (lambda) at the next stop along this line 
						double sum_arrival_rate_next_stops = 0.0;
						for (vector <Visit_stop*> ::iterator trip_stops = trip->get_next_stop(); trip_stops < trip->stops.end(); trip_stops++)
						{
							LineIdStopId lineIdStopId(trip->get_line()->get_id(), (*trip_stops)->first->get_id());

							LineStationData* linestatdata = (*this->hist_dmnd_map_)[lineIdStopId];

							if (linestatdata != NULL)//that means this stop didnt have any data
							{
								for (list<BoardDemand>::iterator brd_it = linestatdata->boardings.begin(); brd_it != linestatdata->boardings.end(); brd_it++)
								{
									sum_arrival_rate_next_stops += brd_it->passengers_number_;
								}
								for (list<BoardDemandTransfer>::iterator brdtr_it = linestatdata->boardingTransfers.begin(); brdtr_it != linestatdata->boardingTransfers.end(); brdtr_it++)
								{
									sum_arrival_rate_next_stops += brdtr_it->passengers_number_;

								}
								for (list<TransferFirstBoard>::iterator brdfrst_it = linestatdata->firstBoardingTransfers.begin(); brdfrst_it != linestatdata->firstBoardingTransfers.end(); brdfrst_it++)
								{
									sum_arrival_rate_next_stops += brdfrst_it->passengers_number_;
								}
							}
						}
						
						if (sum_arrival_rate_next_stops == 0)
						{
							pass_ratio = 0.0;
						}
						else
						{
							//find the delta of the departure times
							double delta_time;
							if (trip->get_line()->check_first_trip(trip))
							{
								if (trip->expected_departure_time[this] < theParameters->start_pass_generation)
									delta_time = 0;
								else if (trip->expected_departure_time[this] - theParameters->start_pass_generation < trip->get_line()->calc_curr_line_headway() + trip->expected_departure_time[this] - trip->Base_departure[this])
									delta_time = trip->expected_departure_time[this] - theParameters->start_pass_generation;
								else
									delta_time = trip->get_line()->calc_curr_line_headway() + trip->expected_departure_time[this] - trip->Base_departure[this];
							}

							else {
								//Busstop* prvs_vst_stop = hndle_trip->get_stop_for_previous_trip(hndle_stop);
								Bustrip* prvs_trip = trip->get_line()->get_previous_trip(trip);
								map <Busline*, pair<Bustrip*, double> > lst_dprt = this->get_last_departures();
								double last_dp = prvs_trip->expected_departure_time[this];//check if this updated if trip already exit the stop
								if (lst_dprt.size() > 0)
								{
									for (map <Busline*, pair<Bustrip*, double>>::iterator dprt_it = lst_dprt.begin(); dprt_it != lst_dprt.end(); dprt_it++)
									{
										if (dprt_it->first->get_id() == prvs_trip->get_line()->get_id() && dprt_it->second.first->get_id() == prvs_trip->get_id() &&
											dprt_it->second.second > 0)
										{
											if (last_dp != dprt_it->second.second)
												last_dp = dprt_it->second.second;
										}
									}
								}
								delta_time = trip->expected_departure_time[this] - last_dp;
								//delta_time = hndle_trip->expected_departure_time[hndle_stop] - prvs_trip->expected_departure_time[hndle_stop];
							}
													
							//Calc expedted boarding, alighting and capacity
							this->calc_boarding_demand_no_trnsfr(trip, this, delta_time); // calc the expected boarding
							this->calc_alight_demand_no_trnsfr(trip, this);//calc the expected alight demand
							this->calc_occupancy_for_next_stop_notrnsfr(trip, this);//calc the expected occupancy for next stop
							this->calc_passengers_according_to_capacity_notrnsfr(trip, this); //calc the boarding passenger that could board
							this->calc_denied_boarding_passengers_notrnsfr(trip, this); // calc the denied boarding according the expected occupancy
							
							double boardings = trip->nr_boardings[this] + trip->nr_board_dnb_direct[this];
							double alightings = trip->nr_alightings[this] + trip->nr_dnb_alightings[this];

							sum_arrival_rate_next_stops = sum_arrival_rate_next_stops / 3600;
							pass_ratio = (trip->occupancy_bus[this] - alightings + boardings) / (2 * 2 * sum_arrival_rate_next_stops);
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

Bustrip* global_trip;
double traffic_time(const column_vector& m)
{
	/*vector<double> temp_val;
	for(size_t clm_it=0; clm_it< m.size(); clm_it++)
	{
		temp_val.push_back(m(clm_it));
	}*/
	global_trip->reset_expected_demand(global_trip->trips_to_handle);
	global_trip->calc_expected_arrival_departue(global_trip->trips_to_handle, m);
	
	double total_pass_time = 0;
	for (size_t tr_it = 0; tr_it < global_trip->trips_to_handle.size(); tr_it++)
	{
		Bustrip* handle_trip = global_trip->trips_to_handle[tr_it];
		for (size_t st_it = 0; st_it < global_trip->trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{

			Busstop* handle_stop = handle_trip->down_stops[st_it]->first;

			handle_stop->calc_boarding_demand(handle_trip, handle_stop);  //boarding as first board
			handle_stop->calc_alighting_demand(handle_trip, handle_stop); // alight as destination, and alight to transfer
			if(handle_trip->get_line()->get_id()== global_trip->trips_to_handle.front()->get_line()->get_id())
				handle_stop->calc_board_transfer_demand(global_trip->trips_to_handle, handle_trip, handle_stop); //board as transfer boarding
			handle_stop->calc_occupancy_for_next_stop(handle_trip, handle_stop);//in order to check if all demand can achieved
			handle_stop->calc_passengers_according_to_capacity(handle_trip, handle_stop);//board just passengers that can board according the capacity of vehicle
			handle_stop->calc_denied_boarding_passengers(handle_trip, handle_stop);//calc num of passengers could not board the trip
			total_pass_time += handle_stop->calc_total_pass_times(global_trip->trips_to_handle, handle_trip, handle_stop, m);
		}
	}

	return total_pass_time;
}

void Busstop::update_predictions(Bustrip* trip, Busstop* handle_stop, double exit_time)
{
	trip->expected_arrival_time[handle_stop] = trip->get_enter_time();
	trip->expected_departure_time[handle_stop] = exit_time;
		
	if (trip->get_line()->check_first_trip(trip) == false)
	{
		Bustrip* prvs_trip = trip->get_line()->get_previous_trip(trip);
		if (prvs_trip->get_id() != (trip->get_id() - 1))
			int ppp = 1;
		double last_dp = prvs_trip->expected_departure_time[this];
		if (trip->expected_departure_time[handle_stop] < last_dp)
		{
			trip->expected_departure_time[handle_stop] = last_dp + 0.1;
		}
	}
	trip->dwell_time[handle_stop] = handle_stop->get_dwell_time();
	if (trip->get_line()->check_first_trip(trip) == true)
	{
		trip->Base_departure[handle_stop] = trip->expected_arrival_time[handle_stop] + trip->dwell_time[handle_stop];
	}
	handle_stop->calc_boarding_demand(trip, handle_stop);  //boarding as first board
	handle_stop->calc_alighting_demand(trip, handle_stop); // alight as destination, and alight to transfer
	if (trip->get_line()->get_id() == global_trip->trips_to_handle.front()->get_line()->get_id())
		handle_stop->calc_board_transfer_demand(global_trip->trips_to_handle, trip, handle_stop); //board as transfer boarding
	handle_stop->calc_occupancy_for_next_stop(trip, handle_stop);//in order to check if all demand can achieved
	vector <Visit_stop*> ::iterator nxt_stop_it = trip->get_next_stop_specific_stop(handle_stop);
	Visit_stop* next_stop1 = *(nxt_stop_it);

	
	if (trip->occupancy_bus[next_stop1->first] < -1 || trip->occupancy_bus[handle_stop] < -1) {
		double prob = trip->occupancy_bus[handle_stop];
		double prob2 = trip->occupancy_bus[next_stop1->first];
		int jjj = 1;
	}
	
	handle_stop->calc_passengers_according_to_capacity(trip, handle_stop);//board just passengers that can board according the capacity of vehicle
	handle_stop->calc_denied_boarding_passengers(trip, handle_stop);//calc num of passengers could not board the trip
}

void Busstop::update_predictions_notrnsfr(Bustrip* trip, Busstop* handle_stop, double exit_time)
{
	trip->expected_arrival_time[handle_stop] = trip->get_enter_time();
	trip->expected_departure_time[handle_stop] = exit_time;

	if (trip->get_line()->check_first_trip(trip) == false)
	{
		Bustrip* prvs_trip = trip->get_line()->get_previous_trip(trip);
		double last_dp = prvs_trip->expected_departure_time[this];
		if (trip->expected_departure_time[handle_stop] < last_dp)
		{
			trip->expected_departure_time[handle_stop] = last_dp + 0.1;
		}
	}
	trip->dwell_time[handle_stop] = handle_stop->get_dwell_time();
	if (trip->get_line()->check_first_trip(trip) == true)
	{
		trip->Base_departure[handle_stop] = trip->expected_arrival_time[handle_stop] + trip->dwell_time[handle_stop];
	}
	//calc the delta time
	double delta_time;
	if (trip->get_line()->check_first_trip(trip))
	{
		if (trip->expected_departure_time[this] < theParameters->start_pass_generation)
			delta_time = 0;
		else if (trip->expected_departure_time[this] - theParameters->start_pass_generation < trip->get_line()->calc_curr_line_headway() + trip->expected_departure_time[this] - trip->Base_departure[this])
			delta_time = trip->expected_departure_time[this] - theParameters->start_pass_generation;
		else
			delta_time = trip->get_line()->calc_curr_line_headway() + trip->expected_departure_time[this] - trip->Base_departure[this];
	}

	else {
		//Busstop* prvs_vst_stop = hndle_trip->get_stop_for_previous_trip(hndle_stop);
		Bustrip* prvs_trip = trip->get_line()->get_previous_trip(trip);
		map <Busline*, pair<Bustrip*, double> > lst_dprt = this->get_last_departures();
		double last_dp = prvs_trip->expected_departure_time[this];//check if this updated if trip already exit the stop
		if (lst_dprt.size() > 0)
		{
			for (map <Busline*, pair<Bustrip*, double>>::iterator dprt_it = lst_dprt.begin(); dprt_it != lst_dprt.end(); dprt_it++)
			{
				if (dprt_it->first->get_id() == prvs_trip->get_line()->get_id() && dprt_it->second.first->get_id() == prvs_trip->get_id() &&
					dprt_it->second.second > 0)
				{
					if (last_dp != dprt_it->second.second)
						last_dp = dprt_it->second.second;
				}
			}
		}
		delta_time = trip->expected_departure_time[this] - last_dp;
	}

	handle_stop->calc_boarding_demand_no_trnsfr(trip, handle_stop,delta_time);  //boarding 
	handle_stop->calc_alight_demand_no_trnsfr(trip, handle_stop); // alighting 
	handle_stop->calc_occupancy_for_next_stop_notrnsfr(trip, handle_stop);//in order to check if all demand can achieved
	handle_stop->calc_passengers_according_to_capacity_notrnsfr(trip, handle_stop);//board just passengers that can board according the capacity of vehicle
	handle_stop->calc_denied_boarding_passengers_notrnsfr(trip, handle_stop);//calc num of passengers could not board the trip
}

pair<double,double> Busstop::calc_holding_departure_travel_time(Bustrip* trip, double time, Busstop* stop)
{
	global_trip = trip;
	pair<double, double> test;
	if ((trip->get_line()->is_line_timepoint(this)) || (trip->get_line()->is_line_transfer_stop(this)) && !(trip->get_line()->check_last_trip(trip))) // if it is a time point or transfer stop and it is not the last trip
	{
		vector <Visit_stop*>::iterator target_stop = trip->get_target_stop(trip, trip->get_next_stop());
		vector<Start_trip> line_trips = trip->get_line()->get_trips();  //return the trips for the line
																		//trips to handle on the same line, for trips that already visited my stop and did not pass the target stop
																		//and trips behind me according to the bus_horizon
		trip->trips_to_handle.clear();
		trip->trips_to_handle = trip->find_trips_to_handle_same_line(line_trips, *target_stop, stop);

		//deal with synchronizing transfer stops:
		if (theParameters->transfer_sync == 1) {// if we are synchronizing transfers
			trip->trips_to_handle = trip->find_trips_to_handle_transfer_lines(trip->trips_to_handle);
		}

		//return the initial value of the TT and H times for trips in the trips_to_handle
		//The TT is form the historical data and the initila holding time is zero.
		int variable_size = get_variables_size(trip->trips_to_handle);

		column_vector starting_point(variable_size);
		starting_point = get_ini_val_vars(trip->trips_to_handle, variable_size);
		vector<double> start;
		for (int ii = 0; ii < starting_point.size(); ii++)
		{
			start.push_back(starting_point(ii));
		}
		//column_vector starting_point = get_ini_val_vars(trip->trips_to_handle);
		column_vector left_constrains = get_left_constraints(trip->trips_to_handle, variable_size);
		column_vector right_constrains = get_right_constraints(trip->trips_to_handle, variable_size);
		//double tot_time = trip->total_passengers_time(starting_point);
	
		vector<double> lft;
		for (int ii = 0; ii < left_constrains.size(); ii++)
		{
			lft.push_back(left_constrains(ii));
		}
		vector<double> rght;
		for (int ii = 0; ii < right_constrains.size(); ii++)
		{
			rght.push_back(right_constrains(ii));
		}


		dlib::find_min_box_constrained(dlib::bfgs_search_strategy(), dlib::objective_delta_stop_strategy(1e-4),
			traffic_time, dlib::derivative(traffic_time), starting_point,
			left_constrains, right_constrains);
		
		
		//return time + dwelltime;  //till now, because I didnt finish the implementation of case 10
		//}//if time point or transfer stop

		// account for passengers that board while the bus is holded at the time point
		// double holding_time = last_departures[trip->get_line()].second - time - dwelltime;
		// int additional_boarding = random -> poisson ((get_arrival_rates (trip)) * holding_time / 3600.0 );
		// nr_boarding += additional_boarding;
		// int curr_occupancy = trip->get_busv()->get_occupancy();
		// trip->get_busv()->set_occupancy(curr_occupancy + additional_boarding); // Updating the occupancy
		// return max(ready_to_depart, holding_departure_time);
		
		//since the travel time we got contains the time in the link and in the server. and we implement the change 
		// in the link, I have to put all the acc/ decc in the link
		double delta;
		if (trip->occupancy_bus[this] < 0 || trip->nr_boardings[this] < 0)
		{
			if (trip->get_line()->check_first_trip(trip) == false)
			{
				Bustrip* prvs_trip = trip->get_line()->get_previous_trip(trip);
				delta=trip->expected_departure_time[this]-prvs_trip->expected_departure_time[this];
			}

		}
		
		vector<double> solution;
		for (int ii = 0; ii < starting_point.size(); ii++)
		{
			solution.push_back(starting_point(ii));
		}
		
		int index = 0;//index to the ini_vars (initial values)
		int check = 0;
		for (size_t tr_it = 0; tr_it < trip->trips_to_handle.size(); tr_it++)
		{
			for (size_t st_it = 0; st_it < trip->trips_to_handle[tr_it]->down_stops.size(); st_it++)
			{
				if (trip->trips_to_handle[tr_it] == trip && (trip->trips_to_handle[tr_it]->down_stops[st_it])->first == this)
				{
					check = 1;
					break;
				}
				else { index++; }
			}
			if (check == 1) { break; }
		}

		
		double stops_distance = find_length_between_stops(trip, this);
		
		vector <Visit_stop*> ::iterator nxt_stop_it = trip->get_next_stop_specific_stop(this);
		Visit_stop* next_stop1 = *(nxt_stop_it);
		LineIdStopId lineIdStopId(trip->get_line()->get_id(), next_stop1->first->get_id());
		hist_set* tmp = (*this->history_summary_map_)[lineIdStopId];
		double avg_speed;
		if(tmp==NULL)
			avg_speed = stops_distance / 20;
		else avg_speed = stops_distance / tmp->riding_time;
		double new_speed = stops_distance / starting_point(index);
		double acc_speed = new_speed - avg_speed;
		
		//assume links for PT only
		double free_speed = trip->get_busv()->get_curr_link()->get_sdfunc()->speed(0.0);
		new_speed = free_speed + acc_speed;
		double controlled_time = stops_distance / new_speed;
		

		test = make_pair(controlled_time, trip->expected_departure_time[this]);
	
		ofstream Hendfile;
		Hendfile.open("ControlStrategy10.txt", fstream::app);
		//Hendfile << "\n";
		//Hendfile << "Number of variables: "<< variable_size<<endl;
		/*Hendfile << "Solution is:" << endl;
		for (int ii = 0; ii < variable_size; ii++)
		{
			Hendfile << ii<<":  "<< starting_point(ii) << endl;
		}*/
		Hendfile << "Trip_id:" << trip->get_id() << ";Stop_id:" << this->get_id() << ";Travel_time:" << starting_point(index) <<
			";Left_con_time:" << left_constrains(index) << ";Right_con_time:" << right_constrains(index) <<
			";Change_time:" << starting_point(index) - start[index] << ";Change_Speed:" << (new_speed - free_speed)*3.6 <<
			";Holding_time:" << starting_point(index + variable_size / 2) << ";Number of variables:" << variable_size << endl;

		Hendfile.flush();
		Hendfile.close();
		
	}
	trip->reset_expected_demand(trip->trips_to_handle);
	return test;

}


void Bustrip::calc_expected_arrival_departue(vector<Bustrip*> trips, const column_vector& ini_vars)
{
	int iter_ini = 0;
	for (size_t tr_it = 0; tr_it < trips.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips[tr_it]->down_stops.size(); st_it++)
		{
			Busstop* hndle_stop = trips[tr_it]->down_stops[st_it]->first;
			LineIdStopId lineIdStopId(trips[tr_it]->get_line()->get_id(), hndle_stop->get_id());
			hist_set* his_st = (*hndle_stop->history_summary_map_)[lineIdStopId];
			if (trips[tr_it]->get_id() == this->get_id() && st_it == 0)//means this is the handled stop
				trips[tr_it]->dwell_time[hndle_stop] = down_stops[st_it]->first->get_dwell_time();
			else
				if (his_st == NULL)
					trips[tr_it]->dwell_time[hndle_stop] = 5;
				else trips[tr_it]->dwell_time[hndle_stop] = his_st->dwell_time;

			//if the stop is the first stop in the line then the arrival time is according the offset I calculated in "calc_hist_prdct_arrivals" 
			if (trips[tr_it]->get_line()->check_first_stop(hndle_stop))
			{
				if (trips[tr_it]->enter_time > 0) {//means that this is the handle trip, bus already enter the stop
					trips[tr_it]->expected_arrival_time[hndle_stop] = trips[tr_it]->enter_time;
				}
				else {
					double sched_arr = trips[tr_it]->scheduled_arrival_time(hndle_stop);
					trips[tr_it]->expected_arrival_time[hndle_stop] = sched_arr;
				}
			}
			else //at(bus,stop) = at(bus, stop-1) + dwell_time(bus,stop-1) + tt(bus,stop-1) + H(bus,stop-1)
			{
				if (st_it == 0) //means that the previous stop served before
				{
					Busstop* prvs_stop = trips[tr_it]->get_previous_stop(hndle_stop);
					LineIdStopId lineIdStopId_prvs(trips[tr_it]->get_line()->get_id(), prvs_stop->get_id());
					//hist_set* hist = (*prvs_stop->history_summary_map_)[lineIdStopId_prvs];
					double lst_dprtr = prvs_stop->get_last_departure(trips[tr_it]->get_line());
					if (trips[tr_it]->get_id() == this->get_id()) {//means that this is the handle trip, bus already enter the stop
						trips[tr_it]->expected_arrival_time[hndle_stop] = trips[tr_it]->enter_time;
					}
					else {
						double tt_prvs;
						if (his_st == NULL)
							tt_prvs = 20;
						else tt_prvs = his_st->riding_time; //riding time is for the previous
						double expected_arrival = lst_dprtr + tt_prvs;
						if (trips[tr_it]->check_last_trip_in_line_handled(trips) == true)
						{
							double exp_arr = trips[tr_it]->scheduled_arrival_time(hndle_stop);
							if (exp_arr > expected_arrival)
								expected_arrival = exp_arr;
						}
						trips[tr_it]->expected_arrival_time[hndle_stop] = expected_arrival;
					}
				}
				else {
					Busstop* prvs_stop = trips[tr_it]->down_stops[st_it - 1]->first;
					LineIdStopId lineIdStopId_prvs(trips[tr_it]->get_line()->get_id(), prvs_stop->get_id());
					double dwt_prvs = trips[tr_it]->dwell_time[prvs_stop];
					double lst_arvl = trips[tr_it]->expected_arrival_time[prvs_stop];
					double tt_prvs = ini_vars(iter_ini - 1);
					double expected_arrival = lst_arvl + dwt_prvs + tt_prvs + ini_vars(iter_ini - 1 + ini_vars.size() / 2);
					if (trips[tr_it]->check_last_trip_in_line_handled(trips) == true)
					{
						double exp_arr = trips[tr_it]->scheduled_arrival_time(hndle_stop);
						if (exp_arr > expected_arrival)
							expected_arrival = exp_arr;
					}
					trips[tr_it]->expected_arrival_time[hndle_stop] = expected_arrival;
				}
			}
			//departure time = arrival time + dwell time + holding time
			double expctd_dprtr = trips[tr_it]->expected_arrival_time[hndle_stop] + trips[tr_it]->dwell_time[hndle_stop] + ini_vars(iter_ini + ini_vars.size() / 2); //holding time is the last elements of the ini_vars
			trips[tr_it]->expected_departure_time[hndle_stop] = expctd_dprtr;
			if (trips[tr_it]->get_line()->check_first_trip(trips[tr_it]) == false)
			{
				Bustrip* prvs_trip = trips[tr_it]->get_line()->get_previous_trip(trips[tr_it]);
				if(trips[tr_it]->expected_departure_time[hndle_stop]<prvs_trip->expected_departure_time[hndle_stop])
					int cc = 1;
			}
			iter_ini += 1;
		}
	}
}

void Bustrip::reset_expected_demand(vector<Bustrip*> trips)
{
	for (size_t tr_it = 0; tr_it < trips.size(); tr_it++)
	{
		Bustrip* hndle_trip = trips[tr_it];
		Busstop* first_handled = hndle_trip->down_stops.front()->first;

		vector<Visit_stop*> ::iterator this_stop;
		for (this_stop = hndle_trip->stops.begin(); this_stop != hndle_trip->stops.end(); this_stop++) {
			if ((*this_stop)->first == first_handled) {
				break;
			}
		}
		for (; this_stop != hndle_trip->stops.end(); this_stop++)
			//for (size_t st_it = 0; st_it < trips[tr_it]->down_stops.size(); st_it++)
		{
			Busstop* hndle_stop = (*this_stop)->first;
			hndle_trip->dwell_time.erase(hndle_stop);
			hndle_trip->expected_arrival_time.erase(hndle_stop);
			hndle_trip->expected_departure_time.erase(hndle_stop);
			hndle_trip->tmp_boardings.erase(hndle_stop);
			hndle_trip->nr_boardings.erase(hndle_stop);
			hndle_trip->tmp_board_orgn_trnsfr.erase(hndle_stop);
			hndle_trip->nr_board_orgn_trnsfr.erase(hndle_stop);
			hndle_trip->tmp_trnsfr_boardings.erase(hndle_stop);
			hndle_trip->nr_trnsfr_boardings.erase(hndle_stop);
			hndle_trip->dnb_direct.erase(hndle_stop);
			hndle_trip->dnb_frst_trnsfr.erase(hndle_stop);
			hndle_trip->dnb_trnsfr.erase(hndle_stop);
			hndle_trip->nr_board_dnb_direct.erase(hndle_stop);
			hndle_trip->nr_board_dnb_frst_trnsfr.erase(hndle_stop);
			hndle_trip->nr_board_dnb_trnsfr.erase(hndle_stop);

			hndle_trip->nr_alightings.erase(hndle_stop);
			hndle_trip->nr_trnsfr_alightings.erase(hndle_stop);
			hndle_trip->nr_dnb_alightings.erase(hndle_stop);
			hndle_trip->nr_trnfr_dnb_alightings.erase(hndle_stop);

			hndle_trip->num_tmp_boardings.erase(hndle_stop);
			hndle_trip->num_boardings.erase(hndle_stop);
			hndle_trip->num_tmp_trnsfr_frst_boardings.erase(hndle_stop);
			hndle_trip->num_trnsfr_frst_boardings.erase(hndle_stop);
			hndle_trip->num_tmp_trnsfr_boardings.erase(hndle_stop);
			hndle_trip->num_trnsfr_boardings.erase(hndle_stop);
			hndle_trip->num_ndb_boardings.erase(hndle_stop);
			hndle_trip->num_ndb_frst_trnsfr_boardings.erase(hndle_stop);
			hndle_trip->num_ndb_trnfr_boardings.erase(hndle_stop);
			hndle_trip->num_board_ndb_boardings.erase(hndle_stop);
			hndle_trip->num_board_ndb_frst_transfer.erase(hndle_stop);
			hndle_trip->num_board_ndb_transfer.erase(hndle_stop);
			hndle_trip->Base_departure.erase(hndle_stop);

			if (hndle_trip->get_line()->check_last_stop(hndle_stop) == false)
			{
				vector <Visit_stop*> ::iterator nxt_stop_it = hndle_trip->get_next_stop_specific_stop(hndle_stop);
				Visit_stop* next_stop1 = *(nxt_stop_it);
				hndle_trip->tmp_occupancy_bus.erase(next_stop1->first);
				hndle_trip->occupancy_bus.erase(next_stop1->first);
			}
		}
	}
}

void Bustrip::reset_handledstop_demand(Bustrip* hndle_trip, Busstop* hndle_stop)
{
	hndle_trip->dwell_time.erase(hndle_stop);
	hndle_trip->expected_arrival_time.erase(hndle_stop);
	hndle_trip->expected_departure_time.erase(hndle_stop);
	hndle_trip->tmp_boardings.erase(hndle_stop);
	hndle_trip->nr_boardings.erase(hndle_stop);
	hndle_trip->tmp_board_orgn_trnsfr.erase(hndle_stop);
	hndle_trip->nr_board_orgn_trnsfr.erase(hndle_stop);
	hndle_trip->tmp_trnsfr_boardings.erase(hndle_stop);
	hndle_trip->nr_trnsfr_boardings.erase(hndle_stop);
	hndle_trip->dnb_direct.erase(hndle_stop);
	hndle_trip->dnb_frst_trnsfr.erase(hndle_stop);
	hndle_trip->dnb_trnsfr.erase(hndle_stop);
	hndle_trip->nr_board_dnb_direct.erase(hndle_stop);
	hndle_trip->nr_board_dnb_frst_trnsfr.erase(hndle_stop);
	hndle_trip->nr_board_dnb_trnsfr.erase(hndle_stop);

	hndle_trip->nr_alightings.erase(hndle_stop);
	hndle_trip->nr_trnsfr_alightings.erase(hndle_stop);
	hndle_trip->nr_dnb_alightings.erase(hndle_stop);
	hndle_trip->nr_trnfr_dnb_alightings.erase(hndle_stop);

	hndle_trip->num_tmp_boardings.erase(hndle_stop);
	hndle_trip->num_boardings.erase(hndle_stop);
	hndle_trip->num_tmp_trnsfr_frst_boardings.erase(hndle_stop);
	hndle_trip->num_trnsfr_frst_boardings.erase(hndle_stop);
	hndle_trip->num_tmp_trnsfr_boardings.erase(hndle_stop);
	hndle_trip->num_trnsfr_boardings.erase(hndle_stop);
	hndle_trip->num_ndb_boardings.erase(hndle_stop);
	hndle_trip->num_ndb_frst_trnsfr_boardings.erase(hndle_stop);
	hndle_trip->num_ndb_trnfr_boardings.erase(hndle_stop);
	hndle_trip->num_board_ndb_boardings.erase(hndle_stop);
	hndle_trip->num_board_ndb_frst_transfer.erase(hndle_stop);
	hndle_trip->num_board_ndb_transfer.erase(hndle_stop);

	if (hndle_trip->get_line()->check_last_stop(hndle_stop) == false)
	{
		vector <Visit_stop*> ::iterator nxt_stop_it = this->get_next_stop_specific_stop(hndle_stop);
		Visit_stop* next_stop1 = *(nxt_stop_it);
		hndle_trip->tmp_occupancy_bus.erase(next_stop1->first);
		hndle_trip->occupancy_bus.erase(next_stop1->first);
	}
}


void Busstop::calc_boarding_demand(Bustrip* hndle_trip, Busstop* hndle_stop)
{
	//Number of passengers that want to board the stop and alight the stop:
	LineIdStopId lineIdStopId(hndle_trip->get_line()->get_id(), hndle_stop->get_id());

	LineStationData* linestatdata = (*hndle_stop->hist_dmnd_map_)[lineIdStopId];
	
	double delta_time;
	if (hndle_trip->get_line()->check_first_trip(hndle_trip))
	{
		if (hndle_trip->expected_departure_time[hndle_stop] < theParameters->start_pass_generation)
			delta_time = 0;
		else if (hndle_trip->expected_departure_time[hndle_stop] - theParameters->start_pass_generation < hndle_trip->get_line()->calc_curr_line_headway() + hndle_trip->expected_departure_time[hndle_stop] - hndle_trip->Base_departure[hndle_stop]) 
			delta_time = hndle_trip->expected_departure_time[hndle_stop] - theParameters->start_pass_generation;
		else
			delta_time = hndle_trip->get_line()->calc_curr_line_headway() + hndle_trip->expected_departure_time[hndle_stop] - hndle_trip->Base_departure[hndle_stop];
	}
	
	else {
		//Busstop* prvs_vst_stop = hndle_trip->get_stop_for_previous_trip(hndle_stop);
		Bustrip* prvs_trip = hndle_trip->get_line()->get_previous_trip(hndle_trip);
		map <Busline*, pair<Bustrip*, double> > lst_dprt = hndle_stop->get_last_departures();
		double last_dp = prvs_trip->expected_departure_time[hndle_stop];//check if this updated if trip already exit the stop
		if (lst_dprt.size() > 0)
		{
			for (map <Busline*, pair<Bustrip*, double>>::iterator dprt_it = lst_dprt.begin(); dprt_it != lst_dprt.end(); dprt_it++)
			{
				if (dprt_it->first->get_id() == prvs_trip->get_line()->get_id() && dprt_it->second.first->get_id() == prvs_trip->get_id() &&
					dprt_it->second.second > 0)
				{
					if(last_dp!=dprt_it->second.second)
						last_dp = dprt_it->second.second;
				}
			}
		}
		delta_time = hndle_trip->expected_departure_time[hndle_stop] - last_dp;
		//delta_time = hndle_trip->expected_departure_time[hndle_stop] - prvs_trip->expected_departure_time[hndle_stop];
	}

	if (linestatdata == NULL)//that means this stop didnt have any data
	{
		hndle_trip->tmp_boardings[hndle_stop] = 0;
		hndle_trip->tmp_board_orgn_trnsfr[hndle_stop] = 0;
	}
	else {
		for (list<BoardDemand>::iterator brd_it = linestatdata->boardings.begin(); brd_it != linestatdata->boardings.end(); brd_it++)
		{
			pair<map<int, double>::iterator, bool > result;
			result=hndle_trip->num_tmp_boardings[hndle_stop].insert(make_pair(brd_it->destination_, brd_it->passengers_number_*delta_time / 3600));
			if (result.second == false)
			{
				hndle_trip->num_tmp_boardings[hndle_stop][brd_it->destination_] += brd_it->passengers_number_*delta_time / 3600;
			}
		}

		//Please notice that for the first board the alight is for the alight stop and not the destination
		for (list<TransferFirstBoard>::iterator brd_tr_it = linestatdata->firstBoardingTransfers.begin(); brd_tr_it != linestatdata->firstBoardingTransfers.end(); brd_tr_it++)
		{
			pair<map<int, double>::iterator, bool > result;
			result= hndle_trip->num_tmp_trnsfr_frst_boardings[hndle_stop].insert(make_pair(brd_tr_it->alight_stop_, brd_tr_it->passengers_number_ *delta_time / 3600));
			if (result.second == false)
			{
				hndle_trip->num_tmp_trnsfr_frst_boardings[hndle_stop][brd_tr_it->alight_stop_] += brd_tr_it->passengers_number_ *delta_time / 3600;
			}
		}

		double sum = 0;
		for (map<int, double>::iterator tmp_board_it = hndle_trip->num_tmp_boardings[hndle_stop].begin(); tmp_board_it != hndle_trip->num_tmp_boardings[hndle_stop].end(); tmp_board_it++)
		{
			sum += tmp_board_it->second;
		}
		hndle_trip->tmp_boardings[hndle_stop] = sum;

		sum = 0;
		for (map<int, double>::iterator tmp_tr_board_it = hndle_trip->num_tmp_trnsfr_frst_boardings[hndle_stop].begin(); tmp_tr_board_it != hndle_trip->num_tmp_trnsfr_frst_boardings[hndle_stop].end(); tmp_tr_board_it++)
		{
			sum += tmp_tr_board_it->second;
		}
		hndle_trip->tmp_board_orgn_trnsfr[hndle_stop] = sum;
	}
}


void Busstop::calc_boarding_demand_no_trnsfr(Bustrip* hndle_trip, Busstop* hndle_stop, double delta_time)
{
	// Number of passengers that want to board the stop and alight the stop :
	LineIdStopId lineIdStopId(hndle_trip->get_line()->get_id(), hndle_stop->get_id());
	LineStationData* linestatdata = (*hndle_stop->hist_dmnd_map_)[lineIdStopId];
	//For strategy 11 no dealing with transfers, then all boardings are regular boardings
	hndle_trip->tmp_board_orgn_trnsfr[hndle_stop] = 0;
	hndle_trip->tmp_trnsfr_boardings[hndle_stop] = 0;

	if (linestatdata == NULL)//that means this stop didnt have any data
	{
		hndle_trip->tmp_boardings[hndle_stop] = 0;
		
	}
	else {
		for (list<BoardDemand>::iterator brd_it = linestatdata->boardings.begin(); brd_it != linestatdata->boardings.end(); brd_it++)
		{
			pair<map<int, double>::iterator, bool > result;
			result = hndle_trip->num_tmp_boardings[hndle_stop].insert(make_pair(brd_it->destination_, brd_it->passengers_number_*delta_time / 3600));
			if (result.second == false)
			{
				hndle_trip->num_tmp_boardings[hndle_stop][brd_it->destination_] += brd_it->passengers_number_*delta_time / 3600;
			}
		}

		//Please notice that for the first board the alight is for the alight stop and not the destination
		for (list<TransferFirstBoard>::iterator brd_tr_it = linestatdata->firstBoardingTransfers.begin(); brd_tr_it != linestatdata->firstBoardingTransfers.end(); brd_tr_it++)
		{
			pair<map<int, double>::iterator, bool > result;
			result = hndle_trip->num_tmp_boardings[hndle_stop].insert(make_pair(brd_tr_it->alight_stop_, brd_tr_it->passengers_number_ *delta_time / 3600));
			if (result.second == false)
			{
				hndle_trip->num_tmp_boardings[hndle_stop][brd_tr_it->alight_stop_] += brd_tr_it->passengers_number_ *delta_time / 3600;
			}
		}
		for (list<BoardDemandTransfer>::iterator brd_tr_it = linestatdata->boardingTransfers.begin(); brd_tr_it != linestatdata->boardingTransfers.end(); brd_tr_it++)
		{
			pair<map<int, double>::iterator, bool > result;
			result = hndle_trip->num_tmp_boardings[hndle_stop].insert(make_pair(brd_tr_it->alight_stop_, brd_tr_it->passengers_number_*delta_time / 3600));
			if (result.second == false)
			{
				hndle_trip->num_tmp_boardings[hndle_stop][brd_tr_it->alight_stop_] += brd_tr_it->passengers_number_*delta_time / 3600;
			}
		}

		double sum = 0;
		for (map<int, double>::iterator tmp_board_it = hndle_trip->num_tmp_boardings[hndle_stop].begin(); tmp_board_it != hndle_trip->num_tmp_boardings[hndle_stop].end(); tmp_board_it++)
		{
			sum += tmp_board_it->second;
		}
		hndle_trip->tmp_boardings[hndle_stop] = sum;
		
	}
}

void Busstop::calc_alighting_demand(Bustrip* hndle_trip, Busstop* hndle_stop)
{
	//Calculate number alightings
	double nr_alights = 0;
	double nr_tr_alights = 0;
	double nr_alight_ndb = 0;
	double nr_alight_tr_ndb = 0;
	if (hndle_trip->get_line()->check_first_stop(hndle_stop)) {
		nr_alights = 0;
		nr_tr_alights = 0;
		nr_alight_ndb = 0;
		nr_alight_tr_ndb = 0;
	}
	else {
		for (vector<Visit_stop*>::iterator stops_it1 = hndle_trip->stops.begin(); stops_it1 != hndle_trip->stops.end(); stops_it1++)
		{
			if ((*stops_it1)->first == hndle_stop) {
				break;
			}
			for (pair<int, double> brd_list : hndle_trip->num_boardings[(*stops_it1)->first])
			//for (pair<int, double> brd_list : (*stops_it1)->first->num_boardings)
			{
				if (brd_list.first == hndle_stop->get_id()){
					nr_alights += brd_list.second;
					break;
				}
			}
			for (pair<int, double> brd_trnfr_list : hndle_trip->num_trnsfr_frst_boardings[(*stops_it1)->first])
			{
				if (brd_trnfr_list.first == hndle_stop->get_id()) {
					nr_tr_alights += brd_trnfr_list.second;
					break;
				}
			}
			for (pair<int, pair<double,double>> brd_trnfr_list : hndle_trip->num_trnsfr_boardings[(*stops_it1)->first])
			{
				if (brd_trnfr_list.first == hndle_stop->get_id()) {
					nr_tr_alights += brd_trnfr_list.second.first;
					break;
				}
			}
			if (hndle_trip->get_line()->check_first_trip(hndle_trip) == false)
			{
				for (pair<int, double> brd_dnb_list : hndle_trip->num_board_ndb_boardings[(*stops_it1)->first])
				{
					if (brd_dnb_list.first == hndle_stop->get_id()) {
						nr_alight_ndb += brd_dnb_list.second;
						break;
					}
				}
				for (pair<int, double> brd_trnfdnb_list : hndle_trip->num_board_ndb_frst_transfer[(*stops_it1)->first])
				{
					if (brd_trnfdnb_list.first == hndle_stop->get_id()) {
						nr_alight_tr_ndb += brd_trnfdnb_list.second;
						break;
					}
				}
				for (pair<int, double> brd_trnfdnb_list : hndle_trip->num_board_ndb_transfer[(*stops_it1)->first])
				{
					if (brd_trnfdnb_list.first == hndle_stop->get_id()) {
						nr_alight_tr_ndb += brd_trnfdnb_list.second;
						break;
					}
				}
			}

		}
	}
	hndle_trip->nr_alightings[hndle_stop] =nr_alights;
	hndle_trip->nr_trnsfr_alightings[hndle_stop] = nr_tr_alights;
	hndle_trip->nr_dnb_alightings[hndle_stop] = nr_alight_ndb;
	hndle_trip->nr_trnfr_dnb_alightings[hndle_stop] = nr_alight_tr_ndb;
}


void Busstop::calc_alight_demand_no_trnsfr(Bustrip* hndle_trip, Busstop* hndle_stop)
{
	//Calculate number alightings
	double nr_alights = 0;
	double nr_alight_ndb = 0;
	if (hndle_trip->get_line()->check_first_stop(hndle_stop)) {
		nr_alights = 0;
		nr_alight_ndb = 0;
	}
	else {
		for (vector<Visit_stop*>::iterator stops_it1 = hndle_trip->stops.begin(); stops_it1 != hndle_trip->stops.end(); stops_it1++)
		{
			if ((*stops_it1)->first == hndle_stop) {
				break;
			}
			for (pair<int, double> brd_list : hndle_trip->num_boardings[(*stops_it1)->first])
				//for (pair<int, double> brd_list : (*stops_it1)->first->num_boardings)
			{
				if (brd_list.first == hndle_stop->get_id()) {
					nr_alights += brd_list.second;
					break;
				}
			}
			
			if (hndle_trip->get_line()->check_first_trip(hndle_trip) == false)
			{
				for (pair<int, double> brd_dnb_list : hndle_trip->num_board_ndb_boardings[(*stops_it1)->first])
				{
					if (brd_dnb_list.first == hndle_stop->get_id()) {
						nr_alight_ndb += brd_dnb_list.second;
						break;
					}
				}
			}
		}
	}
	hndle_trip->nr_alightings[hndle_stop] = nr_alights;
	hndle_trip->nr_dnb_alightings[hndle_stop] = nr_alight_ndb;
}

void Busstop::calc_board_transfer_demand(vector<Bustrip*> trips, Bustrip* hndle_trip, Busstop* hndle_stop)
{
	//Number of passengers that want to board the stop from transfer:
	
	if (hndle_trip->get_line()->is_line_transfer_stop(hndle_stop))
	{
		//deal with transfers to my trip
		vector<pair<Bustrip*, double>> trip_arrival = get_expctd_arrival_transfers(hndle_trip, hndle_stop, trips);
		//list<pair<Bustrip*, Busstop*>> sorted_trnsfers = sort_transfers_by_arrival(trip_arrival);
		if (trip_arrival.size() > 0)
		{
			sort(trip_arrival.begin(), trip_arrival.end(), [](const pair<Bustrip*, double>&x,
				const pair<Bustrip*, double>& y) {
				return x.second < y.second;
			});
			vector<pair<int, double>> trip_departure = get_departures_of_myLine(hndle_trip, hndle_stop, trips);

			calc_transfer_to_myLine(trips, trip_departure, trip_arrival, hndle_stop, hndle_trip);
		}
		
	}// if transfer stop
	
}

vector<pair<Bustrip*, double>> Busstop::get_expctd_arrival_transfers(Bustrip* trip, Busstop* hndle_stop, vector<Bustrip*> trips)
{
	vector<pair<Bustrip*, double>> trips_in_trnsfrs;
	map<int, vector <Busstop*>> trnsfr_lines = trip->get_line()->get_transfer_lines(trip->get_line());
	for (map<int, vector <Busstop*>>::iterator tr_lines_it = trnsfr_lines.begin(); tr_lines_it != trnsfr_lines.end(); tr_lines_it++)
	{
		for (vector<Busstop*>::iterator tr_st_it = tr_lines_it->second.begin(); tr_st_it != tr_lines_it->second.end(); tr_st_it++)
		{
			if ((*tr_st_it)->get_id() == hndle_stop->get_id())// this is the transfer stop
			{
				//now I have to find the trips of the transfer line
				for (size_t tr_it1 = 0; tr_it1 < trips.size(); tr_it1++)
				{
					if (trips[tr_it1]->get_line()->get_id() == tr_lines_it->first)
					{
						for (size_t st_it1 = 0; st_it1 < trips[tr_it1]->down_stops.size(); st_it1++)
						{
							Bustrip* trn_trp = trips[tr_it1];
							Busstop* trnsfr_stop = trips[tr_it1]->down_stops[st_it1]->first;
							if (trnsfr_stop->get_id() == hndle_stop->get_id())
							{
								trips_in_trnsfrs.push_back(make_pair(trn_trp, trn_trp->expected_arrival_time[trnsfr_stop]));
							}
						}
					}
				}
			}
		}
	}
	return trips_in_trnsfrs;	
}


vector<pair<int, double>>Busstop::get_departures_of_myLine(Bustrip* trip, Busstop* hndle_stop, vector<Bustrip*> trips_to_handle)
{
	vector<pair<int, double>> trip_departure;
	for (size_t tr_it1 = 0; tr_it1 < trips_to_handle.size(); tr_it1++)
	{
		if (trips_to_handle[tr_it1]->get_line()->get_id() == trip->get_line()->get_id())
		{
			for (size_t st_it1 = 0; st_it1 < trips_to_handle[tr_it1]->down_stops.size(); st_it1++)
			{
				Busstop* trnsfr_stop = trips_to_handle[tr_it1]->down_stops[st_it1]->first;
				if (trnsfr_stop->get_id() == hndle_stop->get_id())
				{
					trip_departure.push_back(make_pair(trips_to_handle[tr_it1]->get_id(), trips_to_handle[tr_it1]->expected_departure_time[trnsfr_stop]));
				}
			}
		}
	}
	
	sort(trip_departure.begin(), trip_departure.end(), [](const pair<int, double>&x,
		const pair<int, double>&y) {
		return x.second < y.second;
	});
	return trip_departure;
}

void Busstop::calc_transfer_to_myLine(vector<Bustrip*> trips_to_handle, vector<pair<int, double>>trip_departure, vector<pair<Bustrip*, double>> trip_arrival, Busstop* hndle_stop, Bustrip* hndle_trip)
{
	LineIdStopId lineIdStopId(hndle_trip->get_line()->get_id(), hndle_stop->get_id()); 
	LineStationData* linestatdata = (*hndle_stop->hist_dmnd_map_)[lineIdStopId];

	if (linestatdata != NULL)
	{
		for (pair<int, double> trip : trip_departure)
		{
			if (trip_arrival.size() == 0)
				break;
			LineIdStopId tmp_pair(trip.first, hndle_stop->get_id());
			//pair<int, int> tmp_pair = make_pair(trip.first, hndle_stop->get_id());

			//[tmp_pair] = list<int>();

			while (trip_arrival.size()>0 && trip_arrival.front().second < trip.second) {
				pair<Bustrip*, double> temp = trip_arrival.front();
				trip_arrival.erase(trip_arrival.begin()); //pop_front did not work here!!
				pair<int, double> stop_arrv = make_pair(temp.first->get_id(), temp.second);
				//(*hndle_stop->transfer_to_my_line_)[tmp_pair].push_back(make_pair(temp.first->get_id(), stop_arrv));
				if (hndle_trip->get_id() == trip.first)
				{
					for (list<BoardDemandTransfer>::iterator brd_tr_it = linestatdata->boardingTransfers.begin(); brd_tr_it != linestatdata->boardingTransfers.end(); brd_tr_it++)
					{
						//pair<Bustrip*, Busstop*> vst_trip = hndle_trip->get_specific_trip(trips_to_handle, temp.first.first,temp.first.second);
						if (brd_tr_it->previous_line_ == temp.first->get_line()->get_id())
						{
							double tr_board;
							double dprtr = temp.first->get_trnsfr_expected_departure(trips_to_handle, hndle_stop);
							if (temp.first->get_line()->check_first_trip(temp.first))
							{

								tr_board = brd_tr_it->passengers_number_ *(dprtr - hndle_trip->get_line()->calc_curr_line_headway()) / 3600;
							}
							else {
								//Busstop* prvs_stop_trip = temp.first->get_stop_for_previous_trip(hndle_stop);
								Bustrip* prvs_trip = temp.first->get_line()->get_previous_trip(temp.first);
								tr_board = brd_tr_it->passengers_number_ *(dprtr - prvs_trip->expected_departure_time[hndle_stop]) / 3600;

							}

							//list<pair<int, pair<double, double>>> 
							pair<double, double> trnfrnum_arrv = make_pair(tr_board, temp.second);
							pair<int, pair<double, double>> try_1 = make_pair(brd_tr_it->alight_stop_, trnfrnum_arrv);
							hndle_trip->num_tmp_trnsfr_boardings[hndle_stop].push_back(try_1);

						}
					}
				}
			}
		}//for trip departure
	}
	else {//linestat=null
		pair<double, double> trnfrnum_arrv = make_pair(0, 0);
		pair<int, pair<double, double>> try_1 = make_pair(0, trnfrnum_arrv);
		hndle_trip->num_tmp_trnsfr_boardings[hndle_stop].push_back(try_1);
	}
}

void Busstop::calc_occupancy_for_next_stop(Bustrip* handle_trip, Busstop* handle_stop)
{
	double occupancy = 0;
	//LineIdStopId lineIdStopId(handle_trip->get_line()->get_id(), handle_stop->get_id());
	//LineStationData* linestatdata = (*handle_stop->hist_dmnd_map_)[lineIdStopId];
	
	//if (linestatdata != NULL)
	if (handle_trip->get_line()->check_first_stop(handle_stop)) {
		handle_trip->tmp_occupancy_bus[handle_stop] = 0;
		handle_trip->occupancy_bus[handle_stop] = 0;
	}
	if (handle_trip->get_line()->check_last_stop(handle_stop) == false)
	{
		if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
			double delta1 = handle_trip->occupancy_bus[handle_stop] - 
				( handle_trip->nr_alightings[handle_stop] 
				+ handle_trip->nr_trnsfr_alightings[handle_stop]);

			occupancy = delta1+
				handle_trip->tmp_boardings[handle_stop] +
				handle_trip->tmp_board_orgn_trnsfr[handle_stop] +
				handle_trip->tmp_trnsfr_boardings[handle_stop];
		}
		else {//get the data according denied boarding in the same stop for the previous trip
			//Busstop* prvs_stop_trip = handle_trip->get_stop_for_previous_trip(handle_stop);
			Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
			double delta2 = handle_trip->occupancy_bus[handle_stop] -
				(handle_trip->nr_alightings[handle_stop] +
					handle_trip->nr_dnb_alightings[handle_stop] +
					handle_trip->nr_trnfr_dnb_alightings[handle_stop] -
					handle_trip->nr_trnsfr_alightings[handle_stop]
					);

			occupancy =  delta2 +
				handle_trip->tmp_boardings[handle_stop] +
				handle_trip->tmp_board_orgn_trnsfr[handle_stop] +
				handle_trip->tmp_trnsfr_boardings[handle_stop]  + 
				prvs_trip->dnb_direct[handle_stop] +
				prvs_trip->dnb_frst_trnsfr[handle_stop]+ 
				prvs_trip->dnb_trnsfr[handle_stop];
		}
		if (occupancy < -1 || handle_trip->occupancy_bus[handle_stop] < -1) {
			double occup = handle_trip->occupancy_bus[handle_stop];

		}
		vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
		Visit_stop* next_stop1 = *(nxt_stop_it);
		
		//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);
		handle_trip->tmp_occupancy_bus[next_stop1->first] = occupancy;
		double min_occ = 0;
		if (occupancy < handle_trip->get_busv()->get_capacity()) {
			min_occ = occupancy;
		}
		else {
			min_occ = handle_trip->get_busv()->get_capacity();
		}
		handle_trip->occupancy_bus[next_stop1->first] = min_occ;
	}
}

void Busstop::calc_occupancy_for_next_stop_notrnsfr(Bustrip* handle_trip, Busstop* handle_stop)
{
	double occupancy = 0;

	//if (linestatdata != NULL)
	if (handle_trip->get_line()->check_first_stop(handle_stop)) {
		handle_trip->tmp_occupancy_bus[handle_stop] = 0;
		handle_trip->occupancy_bus[handle_stop] = 0;
	}
	if (handle_trip->get_line()->check_last_stop(handle_stop) == false)
	{
		if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
			double delta1 = handle_trip->occupancy_bus[handle_stop] -
				handle_trip->nr_alightings[handle_stop];

			occupancy = delta1 +
				handle_trip->tmp_boardings[handle_stop];
		}
		else {//get the data according denied boarding in the same stop for the previous trip
			  //Busstop* prvs_stop_trip = handle_trip->get_stop_for_previous_trip(handle_stop);
			Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
			double delta2 = handle_trip->occupancy_bus[handle_stop] -
				(handle_trip->nr_alightings[handle_stop] +
					handle_trip->nr_dnb_alightings[handle_stop]);

			occupancy = delta2 +
				handle_trip->tmp_boardings[handle_stop] +
				prvs_trip->dnb_direct[handle_stop];
		}
		vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
		Visit_stop* next_stop1 = *(nxt_stop_it);

		//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);
		handle_trip->tmp_occupancy_bus[next_stop1->first] = occupancy;
		double min_occ = 0;
		if (occupancy < handle_trip->get_busv()->get_capacity()) {
			min_occ = occupancy;
		}
		else {
			min_occ = handle_trip->get_busv()->get_capacity();
		}
		handle_trip->occupancy_bus[next_stop1->first] = min_occ;
	}
}

void Busstop::calc_passengers_according_to_capacity(Bustrip* handle_trip, Busstop* handle_stop)
{
	if (handle_trip->get_line()->check_last_stop(handle_stop) == false)
	{
		vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
		Visit_stop* next_stop1 = *(nxt_stop_it);
		LineIdStopId lineIdStopId(handle_trip->get_line()->get_id(), handle_stop->get_id());
		LineStationData* linestatdata = (*handle_stop->hist_dmnd_map_)[lineIdStopId];

		if (linestatdata == NULL)
		{
			handle_trip->num_boardings[handle_stop].insert(make_pair(0,0));
			handle_trip->num_trnsfr_boardings[handle_stop].push_back(make_pair(0,make_pair(0, 0)));
			handle_trip->num_trnsfr_frst_boardings[handle_stop].insert(make_pair(0, 0));
			handle_trip->num_board_ndb_boardings[handle_stop].insert(make_pair(0, 0));
			handle_trip->num_board_ndb_frst_transfer[handle_stop].insert(make_pair(0, 0));
			handle_trip->num_board_ndb_transfer[handle_stop].insert(make_pair(0, 0));
			handle_trip->nr_boardings[handle_stop] = 0;
			handle_trip->nr_board_orgn_trnsfr[handle_stop] = 0;
			handle_trip->nr_trnsfr_boardings[handle_stop] = 0;
			handle_trip->nr_board_dnb_direct[handle_stop] = 0;
			handle_trip->nr_board_dnb_frst_trnsfr[handle_stop] = 0;
			handle_trip->nr_board_dnb_trnsfr[handle_stop] = 0;
		}
		//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);
		if (handle_trip->tmp_occupancy_bus[next_stop1->first] <= handle_trip->get_busv()->get_capacity()) {
			handle_trip->num_boardings[handle_stop] = handle_trip->num_tmp_boardings[handle_stop];
			handle_trip->num_trnsfr_boardings[handle_stop] = handle_trip->num_tmp_trnsfr_boardings[handle_stop];
			handle_trip->num_trnsfr_frst_boardings[handle_stop] = handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop];
			if (handle_trip->get_line()->check_first_trip(handle_trip) == false)
			{
				Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
				//Busstop* prvs_stop_trip = handle_trip->get_stop_for_previous_trip(handle_stop);
				//handle_stop->num_board_ndb_boardings = prvs_stop_trip->num_ndb_boardings;
				handle_trip->num_board_ndb_boardings[handle_stop] = prvs_trip->num_ndb_boardings[handle_stop];
				handle_trip->num_board_ndb_frst_transfer[handle_stop] = prvs_trip->num_ndb_frst_trnsfr_boardings[handle_stop];
				handle_trip->num_board_ndb_transfer[handle_stop] = prvs_trip->num_ndb_trnfr_boardings[handle_stop];
			}
		}
		else {
			map<int, double>::iterator it;
			double percent;
			double tmp = 0;
			if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
				percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
					(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop]))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
					(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop])));

				for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					handle_trip->num_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
				}
				for (it = handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].begin(); it != handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					handle_trip->num_trnsfr_frst_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
				}
				list<pair<int, pair<double, double>>>::iterator it2;
				for (it2 = handle_trip->num_tmp_trnsfr_boardings[handle_stop].begin(); it2 != handle_trip->num_tmp_trnsfr_boardings[handle_stop].end(); it2++)
				{
					tmp = it2->second.first * percent;
					pair<double, double> tt = make_pair(tmp, it2->second.second);
					handle_trip->num_trnsfr_boardings[handle_stop].push_back(make_pair(it2->first, tt));
				}

			}
			else {
				pair<map<int, double>::iterator, bool > result;
				//Busstop* prvs_stop_trip = handle_trip->get_stop_for_previous_trip(handle_stop);
				Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
				percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
					(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] + handle_trip->nr_trnfr_dnb_alightings[handle_stop]))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
					(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] + handle_trip->nr_trnfr_dnb_alightings[handle_stop])));
				for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					handle_trip->num_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
				}
				for (it = handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].begin(); it != handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					result= handle_trip->num_trnsfr_frst_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
					if (result.second == false)
					{
						handle_trip->num_trnsfr_frst_boardings[handle_stop][it->first] += tmp;
					}
				}
				list<pair<int, pair<double, double>>>::iterator it2;
				for (it2 = handle_trip->num_tmp_trnsfr_boardings[handle_stop].begin(); it2 != handle_trip->num_tmp_trnsfr_boardings[handle_stop].end(); it2++)
				{
					tmp = it2->second.first * percent;
					pair<double, double> tt = make_pair(tmp, it2->second.second);
					handle_trip->num_trnsfr_boardings[handle_stop].push_back(make_pair(it2->first, tt));
				}

				for (it = prvs_trip->num_ndb_boardings[handle_stop].begin(); it != prvs_trip->num_ndb_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					result = handle_trip->num_board_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
					if (result.second == false)
					{
						handle_trip->num_board_ndb_boardings[handle_stop][it->first] += tmp;
					}
				}
				for (it = prvs_trip->num_ndb_frst_trnsfr_boardings[handle_stop].begin(); it != prvs_trip->num_ndb_frst_trnsfr_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					result = handle_trip->num_board_ndb_frst_transfer[handle_stop].insert(pair<int, double>(it->first, tmp));
					if (result.second == false)
					{
						handle_trip->num_board_ndb_frst_transfer[handle_stop][it->first] += tmp;
					}
				}
			}
		}
		//Now summarize the maps to get pair of each boardings:
		double summury = 0;
		map<int, double>::iterator iter;
		for (iter = handle_trip->num_boardings[handle_stop].begin(); iter != handle_trip->num_boardings[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_boardings[handle_stop] = summury;

		summury = 0;
		for (iter = handle_trip->num_trnsfr_frst_boardings[handle_stop].begin(); iter != handle_trip->num_trnsfr_frst_boardings[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_board_orgn_trnsfr[handle_stop] = summury;

		summury = 0;
		list<pair<int, pair<double, double>>>::iterator iter2;
		for (iter2 = handle_trip->num_trnsfr_boardings[handle_stop].begin(); iter2 != handle_trip->num_trnsfr_boardings[handle_stop].end(); iter2++)
		{
			summury += iter2->second.first;
		}
		handle_trip->nr_trnsfr_boardings[handle_stop] = summury;

		summury = 0;
		for (iter = handle_trip->num_board_ndb_boardings[handle_stop].begin(); iter != handle_trip->num_board_ndb_boardings[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_board_dnb_direct[handle_stop] = summury;

		summury = 0;
		for (iter = handle_trip->num_board_ndb_frst_transfer[handle_stop].begin(); iter != handle_trip->num_board_ndb_frst_transfer[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_board_dnb_frst_trnsfr[handle_stop] = summury;

		summury = 0;
		for (iter = handle_trip->num_board_ndb_transfer[handle_stop].begin(); iter != handle_trip->num_board_ndb_transfer[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_board_dnb_trnsfr[handle_stop] = summury;
	}
}

void Busstop::calc_passengers_according_to_capacity_notrnsfr(Bustrip* handle_trip, Busstop* handle_stop)
{
	if (handle_trip->get_line()->check_last_stop(handle_stop) == false)
	{
		vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
		Visit_stop* next_stop1 = *(nxt_stop_it);
		LineIdStopId lineIdStopId(handle_trip->get_line()->get_id(), handle_stop->get_id());
		LineStationData* linestatdata = (*handle_stop->hist_dmnd_map_)[lineIdStopId];

		if (linestatdata == NULL)
		{
			handle_trip->num_boardings[handle_stop].insert(make_pair(0, 0));
			handle_trip->num_board_ndb_boardings[handle_stop].insert(make_pair(0, 0));
			handle_trip->nr_boardings[handle_stop] = 0;
			handle_trip->nr_board_dnb_direct[handle_stop] = 0;
		}
		//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);
		if (handle_trip->tmp_occupancy_bus[next_stop1->first] <= handle_trip->get_busv()->get_capacity()) {
			handle_trip->num_boardings[handle_stop] = handle_trip->num_tmp_boardings[handle_stop];
			if (handle_trip->get_line()->check_first_trip(handle_trip) == false)
			{
				Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
				handle_trip->num_board_ndb_boardings[handle_stop] = prvs_trip->num_ndb_boardings[handle_stop];
			}
		}
		else {
			map<int, double>::iterator it;
			double percent;
			double tmp = 0;
			if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
				percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
					handle_trip->nr_alightings[handle_stop] )) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
					(handle_trip->occupancy_bus[handle_stop] - handle_trip->nr_alightings[handle_stop] ));

				for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					handle_trip->num_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
				}
			}
			else {
				pair<map<int, double>::iterator, bool > result;
				//Busstop* prvs_stop_trip = handle_trip->get_stop_for_previous_trip(handle_stop);
				Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
				percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
					(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] ))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
					(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] )));
				for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					handle_trip->num_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
				}
				
				for (it = prvs_trip->num_ndb_boardings[handle_stop].begin(); it != prvs_trip->num_ndb_boardings[handle_stop].end(); it++)
				{
					tmp = it->second * percent;
					result = handle_trip->num_board_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, tmp));
					if (result.second == false)
					{
						handle_trip->num_board_ndb_boardings[handle_stop][it->first] += tmp;
					}
				}
			}
		}
		//Now summarize the maps to get pair of each boardings:
		double summury = 0;
		map<int, double>::iterator iter;
		for (iter = handle_trip->num_boardings[handle_stop].begin(); iter != handle_trip->num_boardings[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_boardings[handle_stop] = summury;

		summury = 0;
		for (iter = handle_trip->num_board_ndb_boardings[handle_stop].begin(); iter != handle_trip->num_board_ndb_boardings[handle_stop].end(); iter++)
		{
			summury += iter->second;
		}
		handle_trip->nr_board_dnb_direct[handle_stop] = summury;
	}
}

void Busstop::calc_denied_boarding_passengers(Bustrip* handle_trip, Busstop* handle_stop)
{
	map<int, double>::iterator it;
	double percent; 
	vector<Busstop*> down_stops = handle_trip->get_downstream_stops_from_spc_stop(handle_stop);
	
	vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
	Visit_stop* next_stop1 = *(nxt_stop_it);
	//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);

	if (handle_trip->tmp_occupancy_bus[next_stop1->first] <= handle_trip->get_busv()->get_capacity()) {
		for (vector<Busstop*> ::iterator itr = down_stops.begin(); itr != down_stops.end(); itr++)
		{
			int num_id = (*itr)->get_id();
			handle_trip->num_ndb_boardings[handle_stop].insert(pair<int,double> (num_id, 0));
			handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop].insert(pair<int, double>(num_id, 0));
			handle_trip->num_ndb_trnfr_boardings[handle_stop].insert(pair<int, double>(num_id, 0));
		}
	}
	else {
		pair<map<int, double>::iterator, bool > result;
		if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
			percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
				(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop]))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
				(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop])));

			for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
			{	
				double dn_board = it->second * (1- percent);
				result = handle_trip->num_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_boardings[handle_stop][it->first] += dn_board;
				}
			}
			for (it = handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].begin(); it != handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].end(); it++)
			{
				double dn_board = it->second * (1 - percent);
				result = handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop][it->first] += dn_board;
				}
			}
			list<pair<int, pair<double, double>>>::iterator iter2;
			for (iter2 = handle_trip->num_tmp_trnsfr_boardings[handle_stop].begin(); iter2 != handle_trip->num_tmp_trnsfr_boardings[handle_stop].end(); iter2++)
			{
				double dn_board = iter2->second.first * (1 - percent);
				result = handle_trip->num_ndb_trnfr_boardings[handle_stop].insert(pair<int, double>(iter2->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_trnfr_boardings[handle_stop][iter2->first] += dn_board;
				}
			}
		}
		else { //not first trip
			percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
				(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] + handle_trip->nr_trnfr_dnb_alightings[handle_stop]))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
				(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_trnsfr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] + handle_trip->nr_trnfr_dnb_alightings[handle_stop])));
			//Busstop* prvs_vst_stop = handle_trip->get_stop_for_previous_trip(handle_stop);
			Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
			for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
			{
				double dn_board = (it->second + prvs_trip->num_ndb_boardings[handle_stop][it->first])* (1 - percent);
				result= handle_trip->num_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_boardings[handle_stop][it->first] += dn_board;
				}
			}
			for (it = handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].begin(); it != handle_trip->num_tmp_trnsfr_frst_boardings[handle_stop].end(); it++)
			{
				double dn_board = (it->second + prvs_trip->num_ndb_frst_trnsfr_boardings[handle_stop][it->first]) * (1 - percent);
				result=handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop][it->first] += dn_board;
				}
			}

			list<pair<int, pair<double, double>>>::iterator iter2;
			for (iter2 = handle_trip->num_tmp_trnsfr_boardings[handle_stop].begin(); iter2 != handle_trip->num_tmp_trnsfr_boardings[handle_stop].end(); iter2++)
			{
				double dn_board = (iter2->second.first + prvs_trip->num_ndb_trnfr_boardings[handle_stop][iter2->first]) * (1 - percent);
				result= handle_trip->num_ndb_trnfr_boardings[handle_stop].insert(pair<int, double>(iter2->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_trnfr_boardings[handle_stop][iter2->first] += dn_board;
				}
			}
		}

	}
	//Now summarize the denied boarding:
	double smry = 0; 
	for (it = handle_trip->num_ndb_boardings[handle_stop].begin(); it != handle_trip->num_ndb_boardings[handle_stop].end(); it++)
	{
		smry += it->second;
	}
	handle_trip->dnb_direct[handle_stop] = smry;
	
	smry = 0;
	for (it = handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop].begin(); it != handle_trip->num_ndb_frst_trnsfr_boardings[handle_stop].end(); it++)
	{
		smry += it->second;
	}
	handle_trip->dnb_frst_trnsfr[handle_stop] = smry;

	smry = 0;
	for (it = handle_trip->num_ndb_trnfr_boardings[handle_stop].begin(); it != handle_trip->num_ndb_trnfr_boardings[handle_stop].end(); it++)
	{
		smry += it->second;
	}
	handle_trip->dnb_trnsfr[handle_stop] = smry;
}

void Busstop::calc_denied_boarding_passengers_notrnsfr(Bustrip* handle_trip, Busstop* handle_stop)
{
	map<int, double>::iterator it;
	double percent;
	vector<Busstop*> down_stops = handle_trip->get_downstream_stops_from_spc_stop(handle_stop);

	vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
	Visit_stop* next_stop1 = *(nxt_stop_it);
	//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);

	if (handle_trip->tmp_occupancy_bus[next_stop1->first] <= handle_trip->get_busv()->get_capacity()) {
		for (vector<Busstop*> ::iterator itr = down_stops.begin(); itr != down_stops.end(); itr++)
		{
			int num_id = (*itr)->get_id();
			handle_trip->num_ndb_boardings[handle_stop].insert(pair<int, double>(num_id, 0));
		}
	}
	else {
		pair<map<int, double>::iterator, bool > result;
		if (handle_trip->get_line()->check_first_trip(handle_trip) == true) {
			percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
				handle_trip->nr_alightings[handle_stop] )) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
				(handle_trip->occupancy_bus[handle_stop] - handle_trip->nr_alightings[handle_stop]));

			for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
			{
				double dn_board = it->second * (1 - percent);
				result = handle_trip->num_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_boardings[handle_stop][it->first] += dn_board;
				}
			}
		}
		else { //not first trip
			percent = (handle_trip->get_busv()->get_capacity() - (handle_trip->occupancy_bus[handle_stop] -
				(handle_trip->nr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop] + handle_trip->nr_trnfr_dnb_alightings[handle_stop]))) / (handle_trip->tmp_occupancy_bus[next_stop1->first] -
				(handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + handle_trip->nr_dnb_alightings[handle_stop])));
			//Busstop* prvs_vst_stop = handle_trip->get_stop_for_previous_trip(handle_stop);
			Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
			for (it = handle_trip->num_tmp_boardings[handle_stop].begin(); it != handle_trip->num_tmp_boardings[handle_stop].end(); it++)
			{
				double dn_board = (it->second + prvs_trip->num_ndb_boardings[handle_stop][it->first])* (1 - percent);
				result = handle_trip->num_ndb_boardings[handle_stop].insert(pair<int, double>(it->first, dn_board));
				if (result.second == false)
				{
					handle_trip->num_ndb_boardings[handle_stop][it->first] += dn_board;
				}
			}
		}
	}
	//Now summarize the denied boarding:
	double smry = 0;
	for (it = handle_trip->num_ndb_boardings[handle_stop].begin(); it != handle_trip->num_ndb_boardings[handle_stop].end(); it++)
	{
		smry += it->second;
	}
	handle_trip->dnb_direct[handle_stop] = smry;
}

double Busstop::calc_total_pass_times(vector<Bustrip*> trips_to_handle, Bustrip* handle_trip, Busstop* handle_stop, const column_vector& ini_vars)
{
	double tot_passengers_time = 0;
	int index = 0;//index to the ini_vars (initial values)
	int check = 0; 
	for (size_t tr_it = 0; tr_it < trips_to_handle.size(); tr_it++)
	{
		for (size_t st_it = 0; st_it < trips_to_handle[tr_it]->down_stops.size(); st_it++)
		{
			if (trips_to_handle[tr_it] == handle_trip && (trips_to_handle[tr_it]->down_stops[st_it])->first == handle_stop)
			{
				check = 1;
				break;
			}
			else { index++; }
		}
		if (check == 1) { break; }
	}
	
	double delta_time;
	if (handle_trip->get_line()->check_first_trip(handle_trip))
	{
		if (handle_trip->expected_departure_time[handle_stop] < theParameters->start_pass_generation)
			delta_time = 0;
		else if (handle_trip->expected_departure_time[handle_stop] - theParameters->start_pass_generation < handle_trip->get_line()->calc_curr_line_headway() + handle_trip->expected_departure_time[handle_stop] - handle_trip->Base_departure[handle_stop])
			delta_time = handle_trip->expected_departure_time[handle_stop] - theParameters->start_pass_generation;
		else
			delta_time = handle_trip->get_line()->calc_curr_line_headway() + handle_trip->expected_departure_time[handle_stop] - handle_trip->Base_departure[handle_stop];
	}
	//if i am the first trip to handle and not the first trip, then this stop had been served before //Check this!!!
	/*else if (handle_trip->get_line()->check_first_trip_in_handled(trips_to_handle, handle_trip))
	{
		if (handle_stop->get_last_departure(handle_trip->get_line()) > 0) //that mean that this stop had served before, I assume that is is right all the time
		//seince if this the first in handled it seems that the previous trip already served this stop!
		{
			double prvs_dprt = handle_stop->get_last_departure(handle_trip->get_line());//check if this is positive all the time
			delta_time = handle_trip->expected_departure_time[handle_stop] - prvs_dprt;
		}
	}*/
	else {
		//Busstop* prvs_vst_stop = handle_trip->get_stop_for_previous_trip(handle_stop);
		Bustrip* prvs_trip = handle_trip->get_line()->get_previous_trip(handle_trip);
		map <Busline*, pair<Bustrip*, double> > lst_dprt = handle_stop->get_last_departures();
		double last_dp = prvs_trip->expected_departure_time[handle_stop];
		if (lst_dprt.size() > 0) {
			for (map <Busline*, pair<Bustrip*, double>>::iterator dprt_it = lst_dprt.begin(); dprt_it != lst_dprt.end(); dprt_it++)
			{
				if (dprt_it->first->get_id() == prvs_trip->get_line()->get_id() && dprt_it->second.first->get_id() == prvs_trip->get_id() &&
					dprt_it->second.second > 0)
				{
					last_dp = dprt_it->second.second;
				}
			}
		}

		delta_time = handle_trip->expected_departure_time[handle_stop] - last_dp;
	}
	
	//in order to prevent overtaking
	if (delta_time < 0)
		delta_time = -10 * delta_time;
	
	vector <Visit_stop*> ::iterator nxt_stop_it = handle_trip->get_next_stop_specific_stop(handle_stop);
	Visit_stop* next_stop1 = *(nxt_stop_it);
	double tt = ini_vars(index);
	double hld = ini_vars(index + ini_vars.size() / 2);
	//Busstop* next_stop = handle_trip->get_next_stop_specific_stop(handle_stop);
	double tot_dwell_time = (handle_trip->occupancy_bus[handle_stop] - (handle_trip->nr_alightings[handle_stop] + 
		handle_trip->nr_trnsfr_alightings[handle_stop])) * (handle_trip->dwell_time[handle_stop] + hld);
	double travel_time = tt * handle_trip->occupancy_bus[next_stop1->first];
	double wait_time = (handle_trip->nr_boardings[handle_stop] + handle_trip->nr_board_orgn_trnsfr[handle_stop] )*(delta_time/2);
	double trnsfr_time = handle_trip->calc_transfer_time(handle_stop);
	double denied_boarding_time = 0;
	if (handle_trip->get_line()->check_first_trip(handle_trip)==false)
	{
		denied_boarding_time += ((handle_trip->dnb_direct[handle_stop] + handle_trip->dnb_frst_trnsfr[handle_stop] + handle_trip->dnb_trnsfr[handle_stop])*delta_time);
	}
	
	/*if (delta_time < 0)
		tot_passengers_time = 999999999.9 + theParameters->dwell_time_weight*abs(tot_dwell_time) + theParameters->riding_time_weight*abs(travel_time) +
			theParameters->waiting_time_weight*	(abs(wait_time) + abs(trnsfr_time) + abs(denied_boarding_time));
	else*/
		tot_passengers_time = theParameters->dwell_time_weight*tot_dwell_time + theParameters->riding_time_weight*travel_time +
		theParameters->waiting_time_weight*	(wait_time + trnsfr_time + denied_boarding_time);
		
	return tot_passengers_time;
}


double Bustrip::calc_transfer_time(Busstop* handle_stop)
{
	list<pair<int, pair<double, double>>>::iterator it2;
	double trnsr_time = 0;
	for (it2 = this->num_trnsfr_boardings[handle_stop].begin(); it2 != this->num_trnsfr_boardings[handle_stop].end(); it2++)
	{
		double dlta_time = this->expected_departure_time[handle_stop] - it2->second.second;
		trnsr_time += it2->second.first * dlta_time;
	}
	return trnsr_time;
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
