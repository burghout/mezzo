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

/**
 * @ingroup DRT
 * 
 * Whenever ODstops asks each of its Pass_paths for a total IVT, or total WT estimate...Pass_path will either ask a Busline itself for an estimate for a given leg...or ask a CC for an estimate for a given leg.
 * If a set of lines....is all DRT (or all flex lines)
 * 
 * - Delegates the calculation of IVT for a given line leg to Controlcenter if the leg is flexible. 
 * - For the first leg of this path, the 'exploration' argument will be set to false, meaning the non-default IVT (based on whatever method of the Control center) should be used
 * 
 */
double Pass_path::calc_total_in_vehicle_time (double time, Passenger* pass)
{
	IVT.clear(); //expected IVT for each transit leg in path..
	double sum_in_vehicle_time = 0.0;
	vector<vector <Busstop*> >::iterator iter_alt_transfer_stops = alt_transfer_stops.begin();
	iter_alt_transfer_stops++; // starting from the second stop
	for (vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin(); iter_alt_lines < alt_lines.end(); iter_alt_lines++) //iterator over all transit legs in path
	{
		bool first_line_leg = iter_alt_lines == alt_lines.begin() ? true : false; // should be false for all legs except the first one
		double ivtt = 0;
		
		if (theParameters->in_vehicle_d2d_indicator) // indicates a memory of IVT and crowding experiences
		{
			bool has_reached_boarding_stop = false; 

			for (vector<Busstop*>::iterator iter_leg_stops = iter_alt_lines->front()->stops.begin(); iter_leg_stops < iter_alt_lines->front()->stops.end(); iter_leg_stops++) // iterates through the stops of the first line in the set of available legs
			{ //the anticipated in vehicle travel time is specific for each leg of the trip
				if (has_reached_boarding_stop) //once we have found the boarding stop we're searching for
				{
					double leg_ivtt;
					if (pass->any_previous_exp_ivtt(iter_alt_transfer_stops->front(), iter_alt_lines->front(), *iter_leg_stops))
					{
						leg_ivtt = pass->get_anticipated_ivtt(iter_alt_transfer_stops->front(), iter_alt_lines->front(), *iter_leg_stops);
					}
					else
					{
						if (check_all_flexible_lines((*iter_alt_lines))) // if this is a flexible line leg grab leg_ivtt from CC instead
						{
							assert(theParameters->drt);
							//assert(pass->has_access_to_flexible());

							Busline* line = (*iter_alt_lines).front();
							Controlcenter* CC = line->get_CC();
							leg_ivtt = CC->calc_expected_ivt(line, *(iter_leg_stops - 1), *iter_leg_stops, first_line_leg, time);
						}
						else
						{
							leg_ivtt = iter_alt_lines->front()->calc_curr_line_ivt(*(iter_leg_stops - 1), *iter_leg_stops, alt_transfer_stops.front().front()->get_rti(), time);
						}
					}
					
					ivtt += leg_ivtt;

					if ((*iter_leg_stops)->get_id() == (iter_alt_transfer_stops+1)->front()->get_id()) break; //Break if the alighting stop is reached
				}
				else if ((*iter_leg_stops)->get_id() == iter_alt_transfer_stops->front()->get_id()) // here we search for the potential boarding stop of the traveler....
				{
					has_reached_boarding_stop = true;
				}
			}
		}
		else
		{
			if (check_all_flexible_lines((*iter_alt_lines))) // if this is a flexible line leg grab leg_ivtt from CC instead
			{
				assert(theParameters->drt);
				//assert(pass->has_access_to_flexible());

				Busline* line = (*iter_alt_lines).front();
				Controlcenter* CC = line->get_CC();
				ivtt = CC->calc_expected_ivt(line, iter_alt_transfer_stops->front(), (iter_alt_transfer_stops + 1)->front(), first_line_leg, time);
			}
			else
			{
				ivtt = iter_alt_lines->front()->calc_curr_line_ivt(iter_alt_transfer_stops->front(), (iter_alt_transfer_stops + 1)->front(), alt_transfer_stops.front().front()->get_rti(), time);
			}
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
	vector <vector <Busstop*> >::iterator alt_transfer_stops_iter = alt_transfer_stops.begin() + 1; // now pointing to the connection stop for the first leg
	vector<Busstop*> first_stops = alt_transfer_stops.front(); // 'origin' stops of this path
	vector<Busstop*> second_stops = (*alt_transfer_stops_iter); // set of connection stops
	vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin(); // first leg line set
	if (without_first_waiting == true) // if it is calculated for an arriving vehicle, don't include waiting time for the first leg in the calculations
	{
		alt_transfer_stops_iter++;
		alt_transfer_stops_iter++; //now points to connection stop of second leg set of line legs?
		iter_alt_lines++; // points to leg for which the waiting time is calculated for
		first_line = false; // false means we ignore the first
	}
	bool first_entrance = true;
	if (alighting_decision == true) //  besides in case the calculation is for an alighting decision
	{
		first_line = false;
	}
	vector<double>::iterator iter_IVT = IVT.begin();
    if(iter_IVT == IVT.end()) // if this is a walking only path, or no IVTs are defined
        return 0.0;

	vector<double>::iterator iter_walk = walking_distances.begin(); //one for each pair of alt stops (walk or stay decision)
	double pass_arrival_time_at_next_stop;
	double sum_IVT = 0.0;
	double sum_walking_times = 0.0;
	for (; iter_alt_lines < alt_lines.end(); iter_alt_lines++) //looping over each transit leg set
	{	
		double wt_explore = 0.0; // exploration waiting time for flexible legs
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

		first_entrance = false;
		bool leg_has_RTI = false; // default RTI is false if RTI_availability value is invalid at this point
		int RTI_availability = theParameters->real_time_info;

		//DEBUG_MSG("INFO::Pass_path::calc_total_waiting_time for path " << this->get_id() << ":");
		//DEBUG_MSG("\t real_time_info = " << theParameters->real_time_info);
		if (theParameters->real_time_info == 4)
		{
			RTI_availability = first_stops.front()->get_rti();
		}
		if (pass->get_pass_RTI_network_level() == 1)
		{
			RTI_availability = 3;
		}

		bool flexible_leg = check_all_flexible_lines(*iter_alt_lines); //true if calculating waiting time for a flexible transit leg

		if (check_all_flexible_lines(*iter_alt_lines)) // want flexible transit line specific waiting times instead of fixed
		{
			Busline* service_route = (*iter_alt_lines).front();
			Controlcenter* CC = service_route->get_CC(); //get the control center of this line leg

			//DEBUG_MSG("\t Calculating total waiting time for flexible line " << service_route->get_id());
			//prior knowledge does not exist.... instead we should initialize pk to be 0.0
			//wt_pk = 0.0; // default is just setting wt_pk to zero, should always have credibility coefficient 0.0 for a flexible transit leg
			if (first_line)
			{ 
				leg_has_RTI = true; //just to stay consistent with the fixed line setup
				double walking_time = (((*iter_walk) / avg_walking_speed) * 60); //note that this is in seconds
				wt_rti = CC->calc_expected_wt(service_route, service_route->stops.front(), service_route->stops.back(), first_line, walking_time, pass_arrival_time_at_next_stop); //if first leg, based on real-time, other legs should be exploration param or experience....
				//DEBUG_MSG("\t wt_rti = " << wt_rti);
				wt_rti /= 60; //sec to minutes
			}
			else // no rti for downstream flex legs....
			{
				wt_explore = CC->calc_expected_wt(service_route, service_route->stops.front(), service_route->stops.back(), first_line, 0.0, pass_arrival_time_at_next_stop); //if first leg, based on real-time, other legs should be exploration param or experience....
				//DEBUG_MSG("\t wt_explore = " << wt_explore);
				wt_explore /= 60; //sec to minutes
			}			
		}
		else // treat this set of alt lines as fixed schedule services
		{
			//DEBUG_MSG("\t Calculating total waiting time for fixed line " << (*iter_alt_lines).front()->get_id());
			wt_pk = (calc_curr_leg_headway((*iter_alt_lines), alt_transfer_stops_iter, pass_arrival_time_at_next_stop) / 2);
			//DEBUG_MSG("\t wt_pk = " << wt_pk);
			//DEBUG_MSG("\t RTI_availability = " << RTI_availability);
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
			default:
				DEBUG_MSG("WARNING Pass_path::calc_total_waiting_time invalid RTI availability parameter");
				break;
			}
		}

		// Note: two different methods of calculating anticipated waiting times of passengers, one for fixed transit legs, the other for flex
        if (theParameters->pass_day_to_day_indicator == false) // only for no previous day operations
        {
			if (flexible_leg)
			{
				//DEBUG_MSG("\t RTI = " << leg_has_RTI);
				if (leg_has_RTI)
				{
					leg_waiting_time = wt_rti; // in this case no day2day is used, so RTI is fully trusted
				}
				else
				{
					assert(wt_explore == ::drt_exploration_wt); //temporary check, should always be the default we've set at this point
					leg_waiting_time = wt_explore; //!< @todo this basically means that single-shot runs will always favor using paths with flexible transit legs downstream
				}
			}
			else 
			{
				if (leg_has_RTI == true)
				{
					//DEBUG_MSG("\t wt_rti = " << wt_rti);
					leg_waiting_time = theParameters->default_alpha_RTI * wt_rti + (1 - theParameters->default_alpha_RTI) * wt_pk;
				}
				else
				{
					leg_waiting_time = wt_pk; // VALID only when RTI level is stable over days
				}
			}
        }
        else //if (theParameters->pass_day_to_day_indicator == true) // only for Day2Day operations
        {
            double alpha_exp, alpha_RTI;
            //DEBUG_MSG(output all the alphas you wish to update here....);
            bool previous_exp_ODSL = pass->any_previous_exp_ODSL((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front());
            if (previous_exp_ODSL == false)
            {
                alpha_exp = 0;
                alpha_RTI = theParameters->default_alpha_RTI;
            }
            else
            {
                alpha_exp = pass->get_alpha_exp((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front());
                alpha_RTI = pass->get_alpha_RTI((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front());
            }

			if (flexible_leg) // prior knowledge (pk) does not exist for flexible legs, use exploration waiting time instead... @todo make sure that weights here make sense...need to always add up to one
			{
				if (leg_has_RTI)
				{
					leg_waiting_time = alpha_exp * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front()) + alpha_RTI * wt_rti; //+ (1 - alpha_RTI - alpha_exp) * wt_pk;
				}
				else
				{
					leg_waiting_time = alpha_exp / (1 - alpha_RTI) * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front()) + wt_explore;  // alpha_exp initialized to zero, so wt_explore will be used first, with 100% 'credibility'
					//leg_waiting_time = alpha_exp / (1 - alpha_RTI) * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front()) + (1 - alpha_exp - alpha_RTI) / (1 - alpha_RTI) * wt_pk; //Changed by Jens 2014-06-24
				}
			}
			else //fixed leg
			{
				if (leg_has_RTI == true)
				{
					leg_waiting_time = alpha_exp * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front()) + alpha_RTI * wt_rti + (1 - alpha_RTI - alpha_exp) * wt_pk;
				}
				else
				{
					//leg_waiting_time = alpha_exp * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(),(*iter_alt_lines).front()) + (1-alpha_exp)*wt_pk; // VALID only when RTI level is stable over days
					leg_waiting_time = alpha_exp / (1 - alpha_RTI) * pass->get_anticipated_waiting_time((*alt_transfer_stops_iter).front(), (*iter_alt_lines).front()) + (1 - alpha_exp - alpha_RTI) / (1 - alpha_RTI) * wt_pk; //Changed by Jens 2014-06-24
				}
			}
        }
		//DEBUG_MSG("\t transit leg " << (*iter_alt_lines).front()->get_id() << " final leg_waiting_time: " << leg_waiting_time*60);
		sum_waiting_time += leg_waiting_time;
	}
	//DEBUG_MSG("\t Path " << this->get_id() << " final sum_waiting_time: " << sum_waiting_time*60);
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
    the prior-knowledge expected waiting time in calc_total_waiting_time. 

    @todo
        - For a scenario without walking and a bi-directional line, it may not matter what the prior knowledge expected waiting time is. It may screw up the outputs of day2day potentially.
        Problem for the future however is that the alternative lines for this leg includes ALL DRT lines, each with their own individual headway. When calculating the accumulated frequency
        then we are double, triple...etc. count the frequency of a given ODstop path that includes multiple DRT lines of the same service.

		- Update 2020-09-03: Removed calculating the average 'planned frequency' over all drt lines serving a given stop. Instead we now assume that DRT and fixed line legs are always distinct,
		and leg 'headway' is never calculated for a flexible (i.e. DRT) line leg.
*/
double Pass_path::calc_curr_leg_headway (vector<Busline*> leg_lines, vector <vector <Busstop*> >::iterator stop_iter, double time)
{
	assert(check_all_fixed_lines(leg_lines)); // combining frequencies in this way only makes sense now for fixed lines

	double accumlated_frequency = 0.0;
    //double drt_sum_frequency = 0.0; //sum of all planned frequencies for all drt lines included in leg_lines for this path, used for calculating average between them
    //int drt_line_count = 0; //for calculating the drt average frequency

	//map<Busline*, bool> worth_to_wait = check_maybe_worthwhile_to_wait(leg_lines, stop_iter, 1);
	for (vector<Busline*>::iterator iter_leg_lines = leg_lines.begin(); iter_leg_lines < leg_lines.end(); iter_leg_lines++)
	{
		double time_till_next_trip = (*iter_leg_lines)->find_time_till_next_scheduled_trip_at_stop((*stop_iter).front(), time);
		//if (time_till_next_trip < theParameters->max_waiting_time && worth_to_wait[(*iter_leg_lines)] == true) 
		if (time_till_next_trip < theParameters->max_waiting_time) 
			// dynamic filtering rules - consider only if it is available in a pre-defined time frame and it is maybe worthwhile to wait for it
		{
            //if ((*iter_leg_lines)->is_flex_line())
            //{
            //    drt_sum_frequency += 3600.0 / (*iter_leg_lines)->get_planned_headway();
            //    ++drt_line_count;
            //    continue;
            //}

			accumlated_frequency += 3600.0 / ((*iter_leg_lines)->calc_curr_line_headway ());
		}
	}
	if (accumlated_frequency == 0.0) // in case no line is avaliable
	{
		return 0.0;
	}
    
    //if (drt_line_count > 0)
    //{
    //    accumlated_frequency += drt_sum_frequency / drt_line_count; //add avg frequency of drt lines (assumes all drt lines are part of the same service)
    //    DEBUG_MSG_V("Pass_path::calc_curr_leg_headway returning drt adjusted accumulated frequency " << accumlated_frequency << " at time " << time);
    //}

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
	return (random->nrandom(theParameters->transfer_coefficient, theParameters->transfer_coefficient / 4) * number_of_transfers 
		+ random->nrandom(theParameters->in_vehicle_time_coefficient, theParameters->in_vehicle_time_coefficient / 4 ) * calc_total_in_vehicle_time(time, pass) 
		+ random->nrandom(theParameters->waiting_time_coefficient, theParameters->waiting_time_coefficient / 4) * calc_total_waiting_time (time, true, false, avg_walking_speed, pass) 
		+ random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * (calc_total_walking_distance() / avg_walking_speed));
}

/** @ingroup PassengerDecisionParameters
    Waiting utility is returned in staying decisions, connection decisions, path utility calculations. Currently returning a strongly positive utility in case ANY DRT 'line' without any trips assigned to it 
    is included as an alternative in this path.
    
    This also calls calc_total_waiting_time -> calc_curr_leg_headway -> find_time_till_next_scheduled_trip_at_stop which returns a drt planned headway for that specific line if no trips are scheduled yet.
    The find_next_expected_trip_at_stop check seems to be useless. Before I changed the trips vector to a list this returned an iterator to a vector<Start_trip>, where Start_trip is a typedef of pair<Bustrip*,double>.
    It seems that the iterator was never used as an iterator, also the method would by default return either the next scheduled trip, the begin iterator or the end-1 iterator. So checking if the iterator is null seems pointless?
    
	Basically calc_waiting_utility refers to the utility of waiting for a particular path

    @todo
        - Ask Oded about how this function works in greater detail. Are all alt_lines for a given path considered equivalent by the passenger? It seems arbitrary to loop through the alt_lines and 
        return the first time any of them matches a condition

		- When called for a connection decision:
			@param time: time that the traveler is expected to arrive to the origin stop of the path, 
			@param alighting_decision: always false for connection decision
			@param stop_iter: always points to the second set of stops in the alt_stops vector of this path...
        
*/
double Pass_path::calc_waiting_utility (vector <vector <Busstop*> >::iterator stop_iter, double time, bool alighting_decision, Passenger* pass)
{	
	//DEBUG_MSG("INFO::Pass_path::calc_waiting_utility - calculating utility of waiting for path " << this->get_id() << " for passenger " << pass->get_id() << " at time " << time);
	stop_iter++; // not sure why we always increment this... Why doesn't the caller decide instead where the iterator should point? Moves into to next vector of stops anyways
	if (alt_transfer_stops.size() == 2) // in case is is walking-only path
	{
		return (random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * calc_total_walking_distance()/ random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4));
	}

	vector<vector <Busline*> >::iterator iter_alt_lines = alt_lines.begin(); // only look at the first set of alt_lines in this path

	// looping through the possible lines in the first line leg...
	for (vector <Busline*>::iterator iter_lines = (*iter_alt_lines).begin(); iter_lines < (*iter_alt_lines).end(); iter_lines++)
	{
        // @todo Why do we try and find a next expected trip at stop at all here? An optimization to skip any paths that contain fixed legs with no trips scheduled to them?
		Bustrip* next_trip = (*iter_lines)->find_next_expected_trip_at_stop((*stop_iter).front()); // if a trip is already en-route to stop
		if (pass->line_is_rejected((*iter_lines)->get_id())) // in case the line was already rejected once before, added by Jens 2014-10-16
		{
			return ::large_negative_utility; // why is this here? Only reject a line once?
		}
		
		if (next_trip || (*iter_lines)->is_flex_line()) // basically if this waiting path is relevant....some first leg line needs to be en-route, in the case of flexible lines it doesnt
		// a dynamic filtering rule - if there is at least one line in the first leg which is available - then this waiting alternative is relevant
		{
			double ivt = calc_total_in_vehicle_time(time, pass); //minutes, @note important that this is always called BEFORE calc_total_waiting_time to populate IVT vector of this path
			double avg_walking_speed = random->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4); //meters per minute, TODO: change this to truncated normal dist?
			double wt = calc_total_waiting_time(time, false, alighting_decision, avg_walking_speed, pass); //minutes

			if (wt*60 < theParameters->max_waiting_time) //Changed by Jens 2015-03-23 to avoid weird effects when the schedule is too pessimistic
			{
				return (random->nrandom(theParameters->transfer_coefficient, theParameters->transfer_coefficient / 4) * number_of_transfers + random->nrandom(theParameters->in_vehicle_time_coefficient, theParameters->in_vehicle_time_coefficient / 4 ) * ivt + random->nrandom(theParameters->waiting_time_coefficient, theParameters->waiting_time_coefficient / 4) * wt + random->nrandom(theParameters->walking_time_coefficient, theParameters->walking_time_coefficient/4) * calc_total_walking_distance()/ avg_walking_speed);
			}
		}
	} 
	// if none of the lines in the first leg is available - then the waiting alternative is irrelevant
	return ::large_negative_utility;
}

/** @ingroup PassengerDecisionParameters
    Currently it seems that the 'check worthwhile to wait' condition is only checked in the CSGM. Dynamic indicator does not seemed to ever be set to true, for any scenario configuration.
    First if condition 
    IVT(1) + Max H (1) < IVT (2) then line 2 is not worthwhile to wait for 
    is always followed by
    IVT(1) + H (1) < IVT (2) then line 2 is not worthwhile to wait for
    if it fails

    @todo For Max H(1) we now always return the drt_first_rep_max_headway via calc_max_headway for flex lines and for H(1) we always return the planned_headway via calc_curr_line_headway
		OBS: Since this only seems to be called in CSGM, statically, we will here implement the rule that if line 1 or line 2 that are being compared are dynamically scheduled (we do not know the actual waiting time yet)
		  then we do not eliminate either of them as an alternative. If both line 1 and line 2 are fixed, then we implement this traveler strategy/rule the same as usual.
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
				DEBUG_MSG("INFO::Pass_path::check_maybe_worthwhile_to_wait - comparing lines " << (*iter_leg_lines)->get_id() << " and " << (*iter1_leg_lines)->get_id());
				if ((*iter_leg_lines)->is_flex_line() || (*iter1_leg_lines)->is_flex_line()) // ignore flex lines for this filtering rule since WT and IVT expectations are learned instead
				{
					assert(theParameters->drt);
					continue;
				}
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

bool Pass_path::check_all_flexible_lines(const vector<Busline*>& line_vec) const
{
	if (line_vec.empty()) //empty vector of lines is not a set of flexible transit lines
		return false;

	for(auto line : line_vec)	
	{
		if(!line->is_flex_line())
			return false;
	}

	assert(line_vec.size() == 1); // currently all flexible lines are considered distinct from one another, i.e. never merged into a set of lines a traveler will board opportunisticly.
	return true;
}

bool Pass_path::check_any_flexible_lines() const
{
	for (auto line_leg : alt_lines)
	{
		if (check_all_flexible_lines(line_leg)) // recall fixed and flexible lines are currently never mixed within the same vector of alt_lines
		{
			return true;
		}
	}
	return false;
}

bool Pass_path::check_all_fixed_lines(const vector<Busline*>& line_vec) const
{
	if (line_vec.empty()) //empty vector of lines is not a set of fixed transit lines
		return false;

	for (auto line : line_vec)
	{
		if (line->is_flex_line())
			return false;
	}
	return true;
}

bool Pass_path::is_first_transit_leg_fixed() const
{
	if (alt_lines.empty()) //path with no transit legs (e.g. walking only)
		return false;

	vector<Busline*> first_transit_leg = alt_lines.front();
	if (!check_all_fixed_lines(first_transit_leg))
		return false;

	return true;
}


bool Pass_path::is_first_transit_leg_flexible() const
{
	if (alt_lines.empty()) //path with no transit legs (e.g. walking only)
		return false;

	vector<Busline*> first_transit_leg = alt_lines.front();
	if(!check_all_flexible_lines(first_transit_leg))
		return false;

	return true;
}

Busstop* Pass_path::get_first_transfer_stop() const
{
    Busstop* firsttransfer = nullptr;
    if (number_of_transfers != 0)
        firsttransfer = alt_lines.front().front()->stops.back(); //end stop of first transit leg
    return firsttransfer;
}

Busstop* Pass_path::get_first_dropoff_stop() const
{
	return alt_lines.front().front()->stops.back(); //end stop of first transit leg
}
