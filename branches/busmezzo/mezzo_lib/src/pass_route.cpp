#include "pass_route.h"

Pass_path:: Pass_path ()
{
}
Pass_path:: Pass_path (int path_id, vector<vector<Busline*> > alt_lines_)
{
	p_id = path_id;
	alt_lines = alt_lines_;
	number_of_transfers = find_number_of_transfers();
}
Pass_path:: Pass_path (int path_id, vector<vector<Busline*> > alt_lines_, vector <vector <Busstop*> > alt_transfer_stops_)
{
	p_id = path_id;
	alt_lines = alt_lines_;
	alt_transfer_stops = alt_transfer_stops_;
	number_of_transfers = find_number_of_transfers();
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

Pass_path::Pass_path (int path_id, vector<vector<Busline*> > alt_lines_, vector <vector <Busstop*> > alt_transfer_stops_, vector<double> walking_distances_)
{
	p_id = path_id;
	alt_lines = alt_lines_;
	alt_transfer_stops = alt_transfer_stops_;
	walking_distances = walking_distances_;
	number_of_transfers = find_number_of_transfers();
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

Pass_path::~Pass_path()
{
}

void Pass_path::reset()
{
	alt_lines.clear();
	alt_transfer_stops.clear();
	number_of_transfers = 0;
	scheduled_in_vehicle_time = 0;
	scheduled_headway = 0;
	arriving_bus_rellevant = 0;
}

int Pass_path::find_number_of_transfers ()
{
	int nr_trans = 0; 
	if (alt_lines.empty() == true)
	{
		return -1;
	}
	for (vector<vector<Busline*> >::iterator iter_count = alt_lines.begin(); iter_count < alt_lines.end(); iter_count++)
	{
		nr_trans++;
	}	
	return nr_trans-1; // omitting origin and destination stops
}

double Pass_path::calc_total_scheduled_in_vehicle_time (double time)
{
	IVT.clear();
	double sum_in_vehicle_time = 0.0;
	vector<vector <Busstop*> >::iterator iter_alt_transfer_stops = alt_transfer_stops.begin();
	iter_alt_transfer_stops++; // starting from the second stop
	for (vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin(); iter_alt_lines < alt_lines.end(); iter_alt_lines++)
	{
		IVT.push_back((*iter_alt_lines).front()->calc_curr_line_ivt((*iter_alt_transfer_stops).front(),(*(iter_alt_transfer_stops+1)).front(),alt_transfer_stops.front().front()->get_rti(),time));
		sum_in_vehicle_time += IVT.back();
		iter_alt_transfer_stops++;
		iter_alt_transfer_stops++; 
	}
	return (sum_in_vehicle_time/60); // minutes
}

double Pass_path::calc_total_in_vehicle_time (double time, Passenger* pass)
{
	IVT.clear();
	double sum_in_vehicle_time = 0.0;
	vector<vector <Busstop*> >::iterator iter_alt_transfer_stops = alt_transfer_stops.begin();
	iter_alt_transfer_stops++; // starting from the second stop
	for (vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin(); iter_alt_lines < alt_lines.end(); iter_alt_lines++)
	{
		double ivtt = 0;
		
		if (theParameters->in_vehicle_d2d_indicator)
		{
			bool has_reached_boarding_stop = false;

			for (vector<Busstop*>::iterator iter_leg_stops = iter_alt_lines->front()->stops.begin(); iter_leg_stops < iter_alt_lines->front()->stops.end(); iter_leg_stops++)
			{ //the anticipated in vehicle travel time is specific for each leg of the trip
				if (has_reached_boarding_stop)
				{
					double leg_ivtt;
					if (pass->any_previous_exp_ivtt(iter_alt_transfer_stops->front(), iter_alt_lines->front(), *iter_leg_stops))
					{
						leg_ivtt = pass->get_anticipated_ivtt(iter_alt_transfer_stops->front(), iter_alt_lines->front(), *iter_leg_stops);
					}
					else
					{
						leg_ivtt = iter_alt_lines->front()->calc_curr_line_ivt(*(iter_leg_stops-1), *iter_leg_stops, alt_transfer_stops.front().front()->get_rti(), time);
					}
					
					ivtt += leg_ivtt;

					if ((*iter_leg_stops)->get_id() == (iter_alt_transfer_stops+1)->front()->get_id()) break; //Break if the alighting stop is reached
				}
				else if ((*iter_leg_stops)->get_id() == iter_alt_transfer_stops->front()->get_id())
				{
					has_reached_boarding_stop = true;
				}
			}
		}
		else
		{
			ivtt = iter_alt_lines->front()->calc_curr_line_ivt(iter_alt_transfer_stops->front(), (iter_alt_transfer_stops+1)->front(), alt_transfer_stops.front().front()->get_rti(), time);
		}
		IVT.push_back(ivtt);
		sum_in_vehicle_time += IVT.back();
		iter_alt_transfer_stops++;
		iter_alt_transfer_stops++; 
	}
	return (sum_in_vehicle_time/60); // minutes
}

double Pass_path::calc_total_walking_distance()
{
	double sum_walking_distance = 0.0;
	for (vector <double>::iterator iter_walking = walking_distances.begin(); iter_walking < walking_distances.end(); iter_walking++)
	{
		sum_walking_distance += (*iter_walking);
	}
	return (sum_walking_distance); // meters
}

double Pass_path::calc_total_waiting_time (double time, bool without_first_waiting, bool alighting_decision, double avg_walking_speed, Passenger* pass)
{
	double sum_waiting_time = 0.0;
	bool first_line = true;
	vector <vector <Busstop*> >::iterator alt_transfer_stops_iter = alt_transfer_stops.begin() + 1;
	vector<Busstop*> first_stops = alt_transfer_stops.front();
	vector<Busstop*> second_stops = (*alt_transfer_stops_iter);
	vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin();
	if (without_first_waiting == true) // if it is calculated for an arriving vehicle, don't include waiting time for the first leg in the calculations
	{
		alt_transfer_stops_iter++;
		alt_transfer_stops_iter++;
		iter_alt_lines++;
		first_line = false;
	}
	bool first_entrance = true;
	if (alighting_decision == true) //  besides in case the calculation is for an alighting decision
	{
		first_line = false;
	}
	vector<double>::iterator iter_IVT = IVT.begin();
	vector<double>::iterator iter_walk = walking_distances.begin();
	double pass_arrival_time_at_next_stop;
	double sum_IVT = 0.0;
	double sum_walking_times = 0.0;
	for (; iter_alt_lines < alt_lines.end(); iter_alt_lines++)
	{	
		double wt_pk = 0.0;
		double wt_rti = 0.0;
		double leg_waiting_time = 0.0;
		if (first_entrance == false) // in all cases beside the first entrance
		{
			first_line = false;
			alt_transfer_stops_iter++;
			alt_transfer_stops_iter++;
			sum_IVT += (*iter_IVT);
			iter_IVT++;
			iter_walk++;
		}
		sum_walking_times += (((*iter_walk) / avg_walking_speed) * 60); // in seconds
		pass_arrival_time_at_next_stop = time + (sum_waiting_time*60) + sum_walking_times + sum_IVT;
		wt_pk = (calc_curr_leg_headway((*iter_alt_lines), alt_transfer_stops_iter, pass_arrival_time_at_next_stop) / 2);
		first_entrance = false;
		bool leg_has_RTI;
		int RTI_availability = theParameters->real_time_info;
		if (theParameters->real_time_info == 4)
		{
			RTI_availability = first_stops.front()->get_rti();
		}
		if (pass->get_pass_RTI_network_level() == 1)
		{
			RTI_availability = 3;
		}
		switch (RTI_availability) 
		{
			case 0:
				// all legs are calculated based on headway or time-table
				leg_has_RTI = false;
				break;
			case 1:
				// first leg is calculated based on real-time if it is an alternative of staying at the same stop (stop1==stop2),
				//otherwise (involves connection) - based on headway or time-table, while downstream legs are estimated based on headway or time-table	
				if (first_line == true)
				{
					if (second_stops.size() == 1 && first_stops.front() == second_stops.front()) // staying at the same stop
					{
						leg_has_RTI = true;
						wt_rti = calc_curr_leg_waiting_RTI((*iter_alt_lines), alt_transfer_stops_iter, pass_arrival_time_at_next_stop);
					}
					else // using a connected stop
					{
						leg_has_RTI = false;
					}
				}
				else
				{
					leg_has_RTI = false;
					break;
				}

			case 2:
				// first leg is calculated based on real-time, while other legs are estimated based on headway or time-table
				if (first_line == true)
				{
					leg_has_RTI = true;
					wt_rti = calc_curr_leg_waiting_RTI((*iter_alt_lines), alt_transfer_stops_iter, pass_arrival_time_at_next_stop);
					break; 
				}
				else
				{
					leg_has_RTI = false;
					break;
				}
			case 3:
				// all legs are estimated based on real-time info
				leg_has_RTI = true;
				wt_rti = calc_curr_leg_waiting_RTI((*iter_alt_lines), alt_transfer_stops_iter, pass_arrival_time_at_next_stop);
				break;
		}

		if (theParameters->pass_day_to_day_indicator == false) // only for no previous day operations
		{
			if (leg_has_RTI == true)
			{
				leg_waiting_time = theParameters->default_alpha_RTI * wt_rti + (1-theParameters->default_alpha_RTI) * wt_pk; 
			}
			else
			{
				leg_waiting_time = wt_pk; // VALID only when RTI level is stable over days
			}
		}
		else //if (theParameters->pass_day_to_day_indicator == true) // only for Day2Day operations
		{
			double alpha_exp, alpha_RTI;
			bool previous_exp_ODSL = pass->any_previous_exp_ODSL((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front());
			if (previous_exp_ODSL == false)
			{
				alpha_exp = 0;
				alpha_RTI = theParameters->default_alpha_RTI;
			}
			else
			{
				alpha_exp = pass->get_alpha_exp((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front());
				alpha_RTI = pass->get_alpha_RTI((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front());
			}

			if (leg_has_RTI == true)
			{
				leg_waiting_time = alpha_exp * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front()) + alpha_RTI * wt_rti + (1-alpha_RTI-alpha_exp)*wt_pk; 	
			}
			else
			{
				//leg_waiting_time = alpha_exp * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front()) + (1-alpha_exp)*wt_pk; // VALID only when RTI level is stable over days
				leg_waiting_time = alpha_exp / (1 - alpha_RTI) * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front()) + (1 - alpha_exp - alpha_RTI) / (1 - alpha_RTI) * wt_pk; //Changed by Jens 2014-06-24
			}
		}
		sum_waiting_time += leg_waiting_time;
	}
	return sum_waiting_time; // minutes
}

/*
double Pass_path::calc_total_scheduled_waiting_time (double time, bool without_first_waiting)
{
	double sum_waiting_time = 0.0;
	vector <vector <Busstop*> >::iterator alt_transfer_stops_iter = alt_transfer_stops.begin() + 1;
	vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin();
	if (without_first_waiting == true) // if it is calculated for an arriving vehicle, don't include waiting time for the first leg in the calculations
	{
		alt_transfer_stops_iter++;
		alt_transfer_stops_iter++;
		iter_alt_lines++;
	}
	for (; iter_alt_lines < alt_lines.end(); iter_alt_lines++)
	{
		// for each leg the relevant time is the time when the passenger is expected to arrive at the stop and start waiting
		sum_waiting_time += calc_curr_leg_waiting_schedule((*iter_alt_lines), alt_transfer_stops_iter, time);
		alt_transfer_stops_iter++;
		alt_transfer_stops_iter++;
		Bustrip* next_trip = (*iter_alt_lines).front()->find_next_scheduled_trip_at_stop((*alt_transfer_stops_iter).front(), time);
		// update the time for calculating the waiting time according to the expected arrival time at the next stop
		time = next_trip->stops_map[(*alt_transfer_stops_iter).front()]; 
	}
	return sum_waiting_time; // minutes
}
*/


/** @ingroup PassengerDecisionParameters
    Returns the accumulated headway of a given transit leg in minutes (sum of all vehicle frequencies for all lines that can serve the line leg of this trip). Used to calculate
    the prior-knowledge expected waiting time in calc_total_waiting_time. To deal with the problem of double counting frequencies for drt lines, the accumulated frequency of all DRT lines
    is considered to be a result of the average planned_headway over all DRT lines serving this transit leg.

    @todo
        - For a scenario without walking and a bi-directional line, it may not matter what the prior knowledge expected waiting time is. It may screw up the outputs of day2day potentially.
        Problem for the future however is that the alternative lines for this leg includes ALL DRT lines, each with their own individual headway. When calculating the accumulated frequency
        then we are double, triple...etc. count the frequency of a given ODstop path that includes multiple DRT lines of the same service.
*/
double Pass_path::calc_curr_leg_headway (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double time)
{
	double accumlated_frequency = 0.0;
    double drt_sum_frequency = 0.0; //sum of all planned frequencies for all drt lines included in leg_lines for this path, used for calculating average between them
    int drt_line_count = 0; //for calculating the drt average frequency

	//map<Busline*, bool> worth_to_wait = check_maybe_worthwhile_to_wait(leg_lines, stop_iter, 1);
	for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin(); iter_leg_lines < leg_lines.end(); iter_leg_lines++)
	{
		double time_till_next_trip = (*iter_leg_lines)->find_time_till_next_scheduled_trip_at_stop((*stop_iter).front(), time);
		//if (time_till_next_trip < theParameters->max_waiting_time && worth_to_wait[(*iter_leg_lines)] == true) 
		if (time_till_next_trip < theParameters->max_waiting_time) 
			// dynamic filtering rules - consider only if it is available in a pre-defined time frame and it is maybe worthwhile to wait for it
		{
            if ((*iter_leg_lines)->is_flex_line())
            {
                drt_sum_frequency += 3600.0 / (*iter_leg_lines)->get_planned_headway();
                ++drt_line_count;
                continue;
            }

			accumlated_frequency += 3600.0 / ((*iter_leg_lines)->calc_curr_line_headway ());
		}
	}
	if (accumlated_frequency == 0.0) // in case no line is avaliable
	{
		return 0.0;
	}
    
    if (drt_line_count > 0)
    {
        accumlated_frequency += drt_sum_frequency / drt_line_count; //add avg frequency of drt lines (assumes all drt lines are part of the same service)
        DEBUG_MSG_V("Pass_path::calc_curr_leg_headway returning drt adjusted accumulated frequency " << accumlated_frequency << " at time " << time);
    }

	return (60/accumlated_frequency); // minutes
}

/*
double Pass_path::calc_curr_leg_waiting_schedule (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double arriving_time)
{
	Bustrip* next_trip = leg_lines.front()->find_next_scheduled_trip_at_stop((*stop_iter).front(), arriving_time);
	double min_waiting_time = next_trip->stops_map[(*stop_iter).front()];
	map<Busline*, bool> worth_to_wait = check_maybe_worthwhile_to_wait(leg_lines, stop_iter, 1);
	for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin()+1; iter_leg_lines < leg_lines.end(); iter_leg_lines++)
	{
		if (worth_to_wait[(*iter_leg_lines)] == true) 
		// dynamic filtering rules - consider only if it is maybe worthwhile to wait for it
		{
			next_trip = (*iter_leg_lines)->find_next_scheduled_trip_at_stop((*stop_iter).front(), arriving_time);	
			min_waiting_time = min(min_waiting_time, next_trip->stops_map[(*stop_iter).front()] - arriving_time);
		}
	}
	return (min_waiting_time/60); // minutes
}
*/

double Pass_path::calc_curr_leg_waiting_RTI (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double arriving_time)
{ 
	double min_waiting_time;
	bool first_time = true;
	//map<Busline*, bool> worth_to_wait = check_maybe_worthwhile_to_wait(leg_lines, stop_iter, 1);
	for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin(); iter_leg_lines < leg_lines.end(); iter_leg_lines++)
	{
		//if (worth_to_wait[(*iter_leg_lines)] == true) 
		// dynamic filtering rules - consider only if it is maybe worthwhile to wait for it
		//{
			if (first_time == true)
			{
				first_time = false;
				min_waiting_time = (*iter_leg_lines)->time_till_next_arrival_at_stop_after_time((*stop_iter).front(),arriving_time);
			}
			else
			{
				min_waiting_time = min(min_waiting_time, (*iter_leg_lines)->time_till_next_arrival_at_stop_after_time((*stop_iter).front(),arriving_time));
			}
		//}
	}
	return (min_waiting_time/60); // minutes
}

double Pass_path::calc_arriving_utility (double time, Passenger* pass)
// taking into account: transfer penalty + future waiting times + in-vehicle time + walking times
{ 
	double avg_walking_speed = random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4);
	return (random->nrandom(theParameters->transfer_coefficient, theParameters->transfer_coefficient / 4) * number_of_transfers + random->nrandom(theParameters->in_vehicle_time_coefficient, theParameters->in_vehicle_time_coefficient / 4 ) * calc_total_in_vehicle_time(time, pass) + random->nrandom(theParameters->waiting_time_coefficient, theParameters->waiting_time_coefficient / 4) * calc_total_waiting_time (time, true, false, avg_walking_speed, pass) + random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * (calc_total_walking_distance() / avg_walking_speed));
}

/** @ingroup PassengerDecisionParameters
    Waiting utility is returned in staying decisions, connection decisions, path utility calculations. Currently returning a strongly positive utility in case ANY DRT 'line' without any trips assigned to it 
    is included as an alternative in this path.
    
    This also calls calc_total_waiting_time -> calc_curr_leg_headway -> find_time_till_next_scheduled_trip_at_stop which returns a drt planned headway for that specific line if no trips are scheduled yet.
    The find_next_expected_trip_at_stop check seems to be useless. Before I changed the trips vector to a list this returned an iterator to a vector<Start_trip>, where Start_trip is a typedef of pair<Bustrip*,double>.
    It seems that the iterator was never used as an iterator, also the method would by default return either the next scheduled trip, the begin iterator or the end-1 iterator. So checking if the iterator is null seems pointless?
    
    @todo
        - Ask Oded about how this function works in greater detail. Are all alt_lines for a given path considered equivalent by the passenger? It seems arbitrary to loop through the alt_lines and 
        return the first time any of them matches a condition
        
*/
double Pass_path::calc_waiting_utility (vector <vector <Busstop*> >::iterator stop_iter, double time, bool alighting_decision, Passenger* pass)
{	
	stop_iter++;
	if (alt_transfer_stops.size() == 2) // in case is is walking-only path
	{
		return (random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * calc_total_walking_distance()/ random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4));
	}
	vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin();
	for (vector <Busline*>::iterator iter_lines = (*iter_alt_lines).begin(); iter_lines < (*iter_alt_lines).end(); iter_lines++)
	{
        
        if (theParameters->drt && (*iter_lines)->is_flex_line()) 
        {
            DEBUG_MSG_V("Pass_path::calc_waiting_utility returning " << drt_first_rep_waiting_utility << " for path set with flex_line");
            return ::drt_first_rep_waiting_utility; // PassengerDecisionParameters: basically this if condition means that if ANY DRT line is included in the path set of any waiting decision for a passenger then the passenger is heavily incentivized to wait
        }

		Bustrip* next_trip = (*iter_lines)->find_next_expected_trip_at_stop((*stop_iter).front());
		if (pass->line_is_rejected((*iter_lines)->get_id())) // in case the line was already rejected once before, added by Jens 2014-10-16
		{
			return -10.0;
		}
		
		if (next_trip)
		// a dynamic filtering rule - if there is at least one line in the first leg which is available - then this waiting alternative is relevant
		{
			double ivt = calc_total_in_vehicle_time(time, pass); //minutes
			double avg_walking_speed = random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4); //meters per minute, TODO: change this to truncated normal dist?
			double wt = calc_total_waiting_time(time, false, alighting_decision, avg_walking_speed, pass); //minutes

			if (wt*60 < theParameters->max_waiting_time) //Changed by Jens 2015-03-23 to avoid weird effects when the schedule is too pessimistic
			{
				return (random->nrandom(theParameters->transfer_coefficient, theParameters->transfer_coefficient / 4) * number_of_transfers + random->nrandom(theParameters->in_vehicle_time_coefficient, theParameters->in_vehicle_time_coefficient / 4 ) * ivt + random->nrandom(theParameters->waiting_time_coefficient, theParameters->waiting_time_coefficient / 4) * wt + random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * calc_total_walking_distance()/ avg_walking_speed);
			}
		}
		//else 
		//{
  //          if (theParameters->drt && (*iter_lines)->is_flex_line())
  //          {
  //              DEBUG_MSG_V("Pass_path::calc_waiting_utility returning " << drt_first_rep_waiting_utility << " for line with no scheduled trips");
  //              return ::drt_first_rep_waiting_utility; //return a waiting utility parameter for lines with no trips
  //          }
		//}
	} 
	// if none of the lines in the first leg is available - then the waiting alternative is irrelevant
	return -10.0;
}

/** @ingroup PassengerDecisionParameters
    Currently it seems that the 'check worthwhile to wait' condition is only checked in the CSGM. Dynamic indicator does not seemed to ever be set to true, for any scenario configuration.
    First if condition 
    IVT(1) + Max H (1) < IVT (2) then line 2 is not worthwhile to wait for 
    is always followed by
    IVT(1) + H (1) < IVT (2) then line 2 is not worthwhile to wait for
    if it fails

    For Max H(1) we now always return the drt_first_rep_max_headway via calc_max_headway for flex lines and for H(1) we always return the planned_headway via calc_curr_line_headway
*/
map<Busline*, bool> Pass_path::check_maybe_worthwhile_to_wait (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, bool dynamic_indicator)
{
	// based on the complete headway
	map <Busline*,bool> worth_to_wait;
	int rti = (*stop_iter).front()->get_rti();
	for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin(); iter_leg_lines < leg_lines.end(); iter_leg_lines++)
	{
		worth_to_wait[(*iter_leg_lines)] = true;
	}
	if (leg_lines.size() > 1)
		// only if there is what to compare
	{
		if (dynamic_indicator == 0) // in case it is a static filtering rule
		{
			stop_iter++; // the second stops set on the path	
		}
		for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin(); iter_leg_lines < leg_lines.end()-1; iter_leg_lines++)
		{
			for (vector<Busline*>::iterator iter1_leg_lines = iter_leg_lines+1; iter1_leg_lines < leg_lines.end(); iter1_leg_lines++)
			{
				if ((*iter_leg_lines)->calc_curr_line_ivt((*stop_iter).front(),(*(stop_iter+1)).front(), rti,0.0) + (*iter_leg_lines)->calc_max_headway() < (*iter1_leg_lines)->calc_curr_line_ivt((*stop_iter).front(),(*(stop_iter+1)).front(), rti,0.0))		
				{
					// if IVT(1) + Max H (1) < IVT (2) then line 2 is not worthwhile to wait for
					worth_to_wait[(*iter1_leg_lines)] = false;
				}
				// in case it is a dynamic filtering rule
				else 
				{
					if ((*iter_leg_lines)->calc_curr_line_ivt((*stop_iter).front(),(*(stop_iter+1)).front(), rti,0.0) + (*iter_leg_lines)->calc_curr_line_headway() < (*iter1_leg_lines)->calc_curr_line_ivt((*stop_iter).front(),(*(stop_iter+1)).front(),rti,0.0))
					{
						// if IVT(1) + H (1) < IVT (2) then line 2 is not worthwhile to wait for
						worth_to_wait[(*iter1_leg_lines)] = false;
					}
				}
			}
		}
	}
	return worth_to_wait;
}