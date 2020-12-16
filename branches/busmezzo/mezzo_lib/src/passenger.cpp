///! passenger.cpp: implementation of the passenger class.
#include "passenger.h"
#include "controlcenter.h"

ostream& operator<<(ostream& os, const TransitModeType& obj)
{
	os << static_cast<underlying_type<TransitModeType>::type>(obj);
	return os;
}


Passenger::Passenger ()
{
	boarding_decision = false;
	already_walked = false;
	end_time = 0;
	nr_boardings = 0;
	AWT_first_leg_boarding = 0;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}
	memory_projected_RTI.clear();
	arrival_time_at_stop = 0;
}

Passenger::Passenger (int pass_id, double start_time_, ODstops* OD_stop_, QObject* parent) : QObject(parent)
{
	passenger_id = pass_id;
	start_time = start_time_;
	end_time = 0;
	nr_boardings = 0;
	original_origin = OD_stop_->get_origin();
	OD_stop = OD_stop_;
	boarding_decision = false;
	already_walked = false;
	AWT_first_leg_boarding = 0;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}
	memory_projected_RTI.clear();
	arrival_time_at_stop = 0;
	this_is_the_last_stop = false;
}

Passenger::~Passenger()
{
    delete random;
}


void Passenger::reset ()
{
	boarding_decision = false;
	already_walked = false;
	chosen_mode_ = TransitModeType::Null;
    curr_request_ = nullptr;
	temp_connection_path_utilities.clear();

	double new_start_time = 0; 
	while (new_start_time <= theParameters->start_pass_generation || new_start_time > theParameters->stop_pass_generation)
		new_start_time = start_time + theRandomizers[0]->urandom(-300, 300); //The passengers should arrive at stops a little bit randomly, otherwise they might face the exact same situation every day
	start_time = new_start_time;

	end_time = 0;
	nr_boardings = 0;
	AWT_first_leg_boarding = 0;
	this_is_the_last_stop = false;
	memory_projected_RTI.clear();
	arrival_time_at_stop = 0;
	OD_stop = original_origin->get_stop_od_as_origin_per_stop (OD_stop->get_destination());

	selected_path_stops.clear();
	selected_path_trips.clear();
	experienced_crowding_levels.clear();
	waiting_time_due_denied_boarding.clear();

	if (theParameters->drt)
	{
        disconnect(this, nullptr, nullptr, nullptr); //disconnect all signals of passenger
	}
}

void Passenger::init ()
{
	RTI_network_level = theRandomizers[0]->brandom(theParameters->share_RTI_network);

	if (theParameters->pass_day_to_day_indicator == 1)
	{
		anticipated_waiting_time = OD_stop->get_anticipated_waiting_time(); //Changed by Jens 2014-06-27, here a local object with the same name was initialized, so the Passenger object was not updated
		/*for (map<pair<Busstop*, Busline*>,double>::iterator stopline_iter = anticipated_waiting_time.begin(); stopline_iter != anticipated_waiting_time.end(); stopline_iter++)
		{
			pair<Busstop*, Busline*> stopline = (*stopline_iter).first;
			anticipated_waiting_time[stopline] = (*stopline_iter).second;
		}*/
		alpha_RTI = OD_stop->get_alpha_RTI();
		/*for (map<pair<Busstop*, Busline*>,double>::iterator stopline_iter = alpha_RTI.begin(); stopline_iter != alpha_RTI.end(); stopline_iter++)
		{
			pair<Busstop*, Busline*> stopline = (*stopline_iter).first;
			alpha_RTI[stopline] = (*stopline_iter).second;
		}*/
		alpha_exp = OD_stop->get_alpha_exp();
		/*for (map<pair<Busstop*, Busline*>,double>::iterator stopline_iter = alpha_exp.begin(); stopline_iter != alpha_exp.end(); stopline_iter++)
		{
			pair<Busstop*, Busline*> stopline = (*stopline_iter).first;
			alpha_exp[stopline] = (*stopline_iter).second;
		}*/
	}
	if (theParameters->in_vehicle_d2d_indicator == 1)
	{
		anticipated_ivtt = OD_stop->get_anticipated_ivtt();
	}
}

void Passenger::init_zone (int pass_id, double start_time_, ODzone* origin_, ODzone* destination_)
{
	passenger_id = pass_id;
	start_time = start_time_;
	end_time = 0;
	nr_boardings = 0;
	o_zone = origin_;
	d_zone = destination_;
	boarding_decision = false;
	already_walked = false;
	this_is_the_last_stop = false;
}

void Passenger::set_memory_projected_RTI (Busstop* stop, Busline* line, double projected_RTI)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	if (memory_projected_RTI.count(stopline) == 0) // don't override previously projected values
	{
		memory_projected_RTI[stopline] = projected_RTI;
	}
}

void Passenger::set_AWT_first_leg_boarding(Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;

	// So should this be done for every line that potentially pass at this stop upon arrival at the stop

	// consider with and without RTI
	// need to calc projected RTI and PK
	
	// AWT_first_leg_boarding = alpha_exp[stopline] * get_anticipated_waiting_time(stop,line) + alpha_RTI[stopline] * wt_rti + (1-alpha_RTI[stopline]-alpha_exp[stopline])*wt_pk; 	
}

double Passenger::get_memory_projected_RTI (Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	if (memory_projected_RTI.count(stopline) == 0)
	{
		return 0;
	}
	return memory_projected_RTI[stopline];
}

void Passenger::set_anticipated_waiting_time (Busstop* stop, Busline* line, double anticipated_WT)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	anticipated_waiting_time[stopline] = anticipated_WT;
}

void Passenger::set_alpha_RTI (Busstop* stop, Busline* line, double alpha)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	alpha_RTI[stopline] = alpha;
}

void Passenger::set_alpha_exp (Busstop* stop, Busline* line, double alpha)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	alpha_exp[stopline] = alpha;
}

double Passenger::get_anticipated_waiting_time (Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	return anticipated_waiting_time[stopline] / 60; //in minutes
}

double Passenger::get_alpha_RTI (Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	return alpha_RTI[stopline];
}

double Passenger::get_alpha_exp (Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	return alpha_exp[stopline];
}

bool Passenger::any_previous_exp_ODSL (Busstop* stop, Busline* line)
{
	pair<Busstop*, Busline*> stopline;
	stopline.first = stop;
	stopline.second = line;
	return alpha_exp.count(stopline);
}

void Passenger::set_anticipated_ivtt (Busstop* stop, Busline* line, Busstop* leg, double anticipated_ivt)
{
	SLL stoplineleg;
	stoplineleg.stop = stop;
	stoplineleg.line = line;
	stoplineleg.leg = leg;
	anticipated_ivtt[stoplineleg] = anticipated_ivt;
}

double Passenger::get_anticipated_ivtt (Busstop* stop, Busline* line, Busstop* leg)
{
	SLL stoplineleg;
	stoplineleg.stop = stop;
	stoplineleg.line = line;
	stoplineleg.leg = leg;
	return anticipated_ivtt[stoplineleg];
}

double Passenger::get_ivtt_alpha_exp (Busstop* stop, Busline* line, Busstop* leg)
{
	SLL stoplineleg;
	stoplineleg.stop = stop;
	stoplineleg.line = line;
	stoplineleg.leg = leg;
	return ivtt_alpha_exp[stoplineleg];
}

bool Passenger::any_previous_exp_ivtt (Busstop* stop, Busline* line, Busstop* leg)
{
	SLL stoplineleg;
	stoplineleg.stop = stop;
	stoplineleg.line = line;
	stoplineleg.leg = leg;
	return anticipated_ivtt.count(stoplineleg);
}

bool Passenger::execute(Eventlist *eventlist, double time)
{
	if (already_walked == false) // just book the event of arriving at the next stop (origin)
	{
		already_walked = true;
		start(eventlist, time);
		//eventlist->add_event(time, this);
	}
	else
	{
		walk(time);
		//already_walked = false;
	}
return true;
}

void Passenger::walk (double time)
// called every time passengers choose to walk to another stop (origin/transfer)
// puts the passenger at the waiting list at the right timing
// passengers are already assigned with the connection stop they choose as their origin
{
	if (this_is_the_last_stop == true ||  this->get_OD_stop()->check_path_set() == false || OD_stop->get_origin()->get_id() == OD_stop->get_destination()->get_id()) 
	// this may happend if the passenger walked to his final stop or final destination (zonal)
	{
		end_time = time;
		//pass_recycler.addPassenger(this); // terminate passenger
	}
	else // push passengers at the waiting list of their OD
	{
		arrival_time_at_stop = time;
		OD_stop->add_pass_waiting (this);
		if (RTI_network_level == true || OD_stop->get_origin()->get_rti() > 0)
		{
			vector<Busline*> lines_at_stop = OD_stop->get_origin()->get_lines();
			for (vector <Busline*>::iterator line_iter = lines_at_stop.begin(); line_iter < lines_at_stop.end(); line_iter++)
			{
				pair<Busstop*, Busline*> stopline;
				stopline.first = OD_stop->get_origin();
				stopline.second = (*line_iter);
				if (memory_projected_RTI.count(stopline) == 0)
				{
					this->set_memory_projected_RTI(OD_stop->get_origin(),(*line_iter),(*line_iter)->time_till_next_arrival_at_stop_after_time(OD_stop->get_origin(),time));
					//this->set_AWT_first_leg_boarding();
				}
			}
		}
	}
}


/**
 * Notes:
 *	start_time is different from time function is called (via Passenger::execute), think for the most part it should be identical however (Eventlist->add_event(Pass*, time)) is always the same as start time in ODstops
*		-> Just use start_time everywhere...
*	already_walked has been set to true when this is called
* 
 *	Passenger makes a connection decision
 *  Connected stop is set as new origin
 *  Calculates expected arrival to stop, adds a passenger event to walk to the next stop
 *  Passenger output collectors collect decision data
 *    
 *  @todo
 *		Need to connect the traveler to the Controlcenter of their new origin directly after the transitmode decision, rather than when added to waiting queue....check that this makes 
 *		Need to move request sending to before the traveler walks (i.e. directly after the event has been added)
 *		
 * 
 */
void Passenger::start (Eventlist* eventlist, double time)
{
		pair<Busstop*,double> stop_time;
		stop_time.first = OD_stop->get_origin();
		stop_time.second = start_time;
		add_to_selected_path_stop(stop_time);
		Busstop* connection_stop = make_connection_decision(start_time);

		stop_time.first = connection_stop;
		double arrival_time_to_connected_stop = start_time;

		if (connection_stop->get_id() != OD_stop->get_origin()->get_id()) // if the pass. walks to another stop
		{
			// set connected_stop as the new origin
			if (connection_stop->check_stop_od_as_origin_per_stop(OD_stop->get_destination()) == false)
			{
				ODstops* od_stop = new ODstops (connection_stop,OD_stop->get_destination());
				connection_stop->add_odstops_as_origin(OD_stop->get_destination(), od_stop);
				OD_stop->get_destination()->add_odstops_as_destination(connection_stop, od_stop);
			}
			set_ODstop(connection_stop->get_stop_od_as_origin_per_stop(OD_stop->get_destination())); // set this stop as his new origin (new OD)
			
			arrival_time_to_connected_stop += get_walking_time(connection_stop,start_time);
			eventlist->add_event(arrival_time_to_connected_stop, this);

            pair<Busstop*,double> stop_time;
			stop_time.first = connection_stop;
			stop_time.second = arrival_time_to_connected_stop;
			add_to_selected_path_stop(stop_time);
		}
		else // if the pass. stays at the same stop
		{
			OD_stop->add_pass_waiting(this); // store the new passenger at the list of waiting passengers with this OD

			set_arrival_time_at_stop(start_time);
			add_to_selected_path_stop(stop_time);
			if (get_pass_RTI_network_level() == true || OD_stop->get_origin()->get_rti() > 0)
			{
				vector<Busline*> lines_at_stop = OD_stop->get_origin()->get_lines();
				for (vector <Busline*>::iterator line_iter = lines_at_stop.begin(); line_iter < lines_at_stop.end(); line_iter++)
				{
					pair<Busstop*, Busline*> stopline;
					stopline.first = OD_stop->get_origin();
					stopline.second = (*line_iter);
					set_memory_projected_RTI(OD_stop->get_origin(),(*line_iter),(*line_iter)->time_till_next_arrival_at_stop_after_time(OD_stop->get_origin(),start_time));
					//set_AWT_first_leg_boarding();
				}
			}
		}

		if (theParameters->drt) // if drt there are mode choice options following connection decision
		{
			TransitModeType chosen_mode = make_transitmode_decision(connection_stop, start_time); //also sets chosen mode...
			set_chosen_mode(chosen_mode); //important that this is set before dropoff_decision call

			if (chosen_mode == TransitModeType::Flexible)
			{
				Busstop* dropoff_stop = make_dropoff_decision(connection_stop, start_time);

				Controlcenter* CC = connection_stop->get_CC();
				assert(CC != nullptr);
				CC->connectPassenger(this); //connect passenger to the CC of the stop they decided to stay at/walk to, send a request to this CC	

				Request* req = createRequest(connection_stop, dropoff_stop, 1, arrival_time_to_connected_stop, start_time); //create request with load 1 at current time 
				if (req != nullptr) //if a connection, or partial connection was found within the CC service of origin stop
				{
					assert(this->get_curr_request() == nullptr); // @note RequestHandler responsible for resetting this to nullptr if request is rejected or after a pass has boarded a bus and request is removed
					set_curr_request(req); 
					emit sendRequest(req, time); //send request to any controlcenter that is connected
				}
				else
					DEBUG_MSG_V("WARNING - Passenger::start() - failed request creation for stops "<< connection_stop->get_id() << "->" << dropoff_stop->get_id() << " with desired departure time " << arrival_time_to_connected_stop << " at time " << time);
			}
		}
}

Request* Passenger::createRequest(Busstop* origin_stop, Busstop* dest_stop, int load, double desired_departure_time, double time)
{
	assert(load > 0);
    assert(desired_departure_time >= 0);
	assert(time >= 0);

	Request* req = nullptr;

    Controlcenter* cc = origin_stop->get_CC();
    if (cc)
    {
        if (!cc->isInServiceArea(dest_stop)) //if destination of passenger is not within range of the controlcenter of origin stop
        {
            DEBUG_MSG("DEBUG: Passenger::start : passenger decided to stay at " << OD_stop->get_origin()->get_id() << ", but destination " <<
                OD_stop->get_destination()->get_id() << " is outside of service area of controlcenter, searching for transfer stop...");
            Busstop* transferstop = nullptr;
            vector<Pass_path*> paths = OD_stop->get_path_set(); //check the path set of current ODstop pair

            for (const Pass_path* path : paths)
            {
                if (path->get_number_of_transfers() != 0) //find first path that includes transfers
                {
                    //check if the first transfer point of this path is included within the drt service area
                    transferstop = path->get_first_transfer_stop();
                    if (transferstop != nullptr && cc->isInServiceArea(transferstop))
                        break;
                }
            }

            if (transferstop != nullptr) //create request for transfer stop of first path instead of final destination TODO: what to do when there are several transfer stops i.e. several paths
            {
                DEBUG_MSG("DEBUG: Passenger::start : Transfer stop found! Sending request to travel to transfer stop " << transferstop->get_id());
                req = new Request(this, this->get_id(), OD_stop->get_origin()->get_id(), transferstop->get_id(), load, desired_departure_time, time);
            }
        }
        else
        {
            req = new Request(this, this->get_id(), OD_stop->get_origin()->get_id(), dest_stop->get_id(), load, desired_departure_time, time); //create request with load 1 at current time 
        }
    }
	return req;
}

vector<Pass_path*> Passenger::get_first_leg_flexible_paths(const vector<Pass_path*>& path_set) const
{
	vector<Pass_path*> flex_paths; //paths where the first leg is flexible
	for (Pass_path* path : path_set)
	{
		if (path->is_first_transit_leg_flexible())
		{
			flex_paths.push_back(path);
		}
	}
	return flex_paths;
}

vector<Pass_path*> Passenger::get_first_leg_fixed_paths(const vector<Pass_path*>& path_set) const
{
	vector<Pass_path*> fix_paths; //paths where the first leg is fixed
	for (Pass_path* path : path_set)
	{
		if (path->is_first_transit_leg_fixed())
		{
			fix_paths.push_back(path);
		}
	}
	return fix_paths;
}

bool Passenger:: make_boarding_decision (Bustrip* arriving_bus, double time) 
{
	/*Busstop* curr_stop = selected_path_stops.back().first;
	ODstops* od = curr_stop->get_stop_od_as_origin_per_stop(OD_stop->get_destination());*/
//	Busstop* curr_stop = OD_stop->get_origin(); //2014-04-14 Jens West changed this, because otherwise the passengers would board lines and then not know what to do
	double boarding_prob;

	if (theParameters->drt && chosen_mode_ == TransitModeType::Flexible) // if passenger is a flexible transit user
	{
		if (curr_request_->assigned_veh == nullptr) // if we are not associating requests with specific vehicles
		{
			Busstop* dest_stop = arriving_bus->stops.back()->first; // get final destination of arriving bus
			if (arriving_bus->is_flex_trip() && curr_request_->dstop_id == dest_stop->get_id()) // @todo for now passenger always boards the first available flexible trip that is finishing trip at passengers destination
			{
				//always attempt to board flexible buses with destination of the passenger
				OD_stop->set_staying_utility(::large_negative_utility); //we set these utilities just to record boarding output
				OD_stop->set_boarding_utility(::large_positive_utility);
				boarding_prob = 1.0;
				boarding_decision = true;
			}
			else // always stay and wait otherwise
			{
				OD_stop->set_staying_utility(::large_positive_utility);
				OD_stop->set_boarding_utility(::large_negative_utility);
				boarding_prob = 0.0;
				boarding_decision = false;
			}
		}
		else
			abort(); //should never happen at the moment, not assigning vehicles anything
	}
	else
	{
		// use the od based on last stop on record (in case of connections)
		boarding_prob = OD_stop->calc_boarding_probability(arriving_bus->get_line(), time, this);
		boarding_decision = theRandomizers[0]->brandom(boarding_prob);
	}
	if (boarding_decision == true)
	{
		rejected_lines.clear();
		//int level_of_rti_upon_decision = curr_stop->get_rti(); //Everything moved to other places by Jens
		//if (RTI_network_level == 1)
		//{
		//	level_of_rti_upon_decision = 3;
		//}
		////ODstops* passenger_od = original_origin->get_stop_od_as_origin_per_stop(OD_stop->get_destination()); //Jens changed this 2014-06-24, this enables remembering waiting time not only for the first stop of trip
		////passenger_od->record_waiting_experience(this, arriving_bus, time, level_of_rti_upon_decision,this->get_memory_projected_RTI(curr_stop,arriving_bus->get_line()),AWT_first_leg_boarding);
		////OD_stop->record_waiting_experience(this, arriving_bus, time, level_of_rti_upon_decision,this->get_memory_projected_RTI(curr_stop,arriving_bus->get_line()),AWT_first_leg_boarding);
	}
	else
	{
		rejected_lines.push_back(arriving_bus->get_line()->get_id());
	}

	OD_stop->record_passenger_boarding_decision(this, arriving_bus, time, boarding_prob, boarding_decision);

	if (boarding_decision == true)
	{
		nr_boardings++;
	}
	return boarding_decision;
}

void Passenger::record_waiting_experience(Bustrip* arriving_bus, double time)
{
	if(theParameters->demand_format == 3)
	{
		Busstop* curr_stop = OD_stop->get_origin();
		bool left_behind_before = !empty_denied_boarding() && curr_stop->get_id() == get_last_denied_boarding_stop_id();
		double experienced_WT;
		if (left_behind_before)
		{
			double first_bus_arrival_time = waiting_time_due_denied_boarding.back().second;
			/*for (vector<pair<Busstop*,double> >::iterator denied_boarding = waiting_time_due_denied_boarding.end(); denied_boarding > waiting_time_due_denied_boarding.begin();)
			{
				denied_boarding--;
				if (denied_boarding->first->get_id() != curr_stop->get_id())
					break;
				first_bus_arrival_time = denied_boarding->second;
			}*/
			experienced_WT = (first_bus_arrival_time - get_arrival_time_at_stop()) + 3.5 * (time - first_bus_arrival_time);
		}
		else
		{
			experienced_WT = time - get_arrival_time_at_stop();
		}
		
		ODstops* passenger_od = original_origin->get_stop_od_as_origin_per_stop(OD_stop->get_destination());
		passenger_od->record_waiting_experience(this, arriving_bus, time, experienced_WT, curr_stop->get_rti(), this->get_memory_projected_RTI(curr_stop,arriving_bus->get_line()), AWT_first_leg_boarding, static_cast<int>(waiting_time_due_denied_boarding.size()));
		//OD_stop->record_waiting_experience(this, arriving_bus, time, experienced_WT, curr_stop->get_rti(), this->get_memory_projected_RTI(curr_stop,arriving_bus->get_line()), AWT_first_leg_boarding, waiting_time_due_denied_boarding.size());
		//left_behind_before = false;
	}
}

Busstop* Passenger::make_alighting_decision (Bustrip* boarding_bus, double time) // assuming that all passenger paths involve only direct trips
{
	if (theParameters->drt && chosen_mode_ == TransitModeType::Flexible)
	{
		assert(curr_request_->assigned_veh == nullptr); // have not started assigning vehicles to requests yet, otherwise can use this instead
		
		Busstop* final_stop = boarding_bus->stops.back()->first;
		assert(curr_request_->dstop_id == final_stop->get_id()); //otherwise the traveler has boarded a bus they should not have boarded, final stop of boarded bus should be guarantee match chosen dropoff stop of traveler

		if (final_stop->get_id() != OD_stop->get_destination()->get_id()) // in case the alighting stop is not the passengers final destination
		{
			ODstops* left_od_stop;
			if (final_stop->check_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination()) == false) 
			{
				left_od_stop = new ODstops(final_stop, this->get_OD_stop()->get_destination());
				final_stop->add_odstops_as_origin(this->get_OD_stop()->get_destination(), left_od_stop);
				this->get_OD_stop()->get_destination()->add_odstops_as_destination(final_stop, left_od_stop);
			}
			else
			{
				left_od_stop = final_stop->get_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination());
			}
			left_od_stop->set_staying_utility(::large_positive_utility); // not sure if necessary, but doing to match behavior for fixed buses via left_od_stop->calc_combined_set_utility_for_alighting. A fluffy teddy bear
		}

		map<Busstop*, pair<double, double> > alighting_MNL; // utility followed by probability per stop
		alighting_MNL[final_stop].first = ::large_positive_utility;
		alighting_MNL[final_stop].second = 1.0;
		OD_stop->record_passenger_alighting_decision(this, boarding_bus, time, final_stop, alighting_MNL);



		return final_stop;
	}

	// assuming that a pass. boards only paths from his path set
	map <Busstop*, double> candidate_transfer_stops_u; // the double value is the utility associated with the respective stop
	map <Busstop*, double> candidate_transfer_stops_p; // the double value is the probability associated with the respective stop
	vector<Pass_path*> path_set = OD_stop->get_path_set();
	for (vector <Pass_path*>::iterator path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
	{
		vector<vector<Busline*> > alt_lines = (*path_iter)->get_alt_lines();
		if (alt_lines.empty() == false) // in case it is not a walking-only alternative
		{
			vector<vector<Busline*> >::iterator first_leg = alt_lines.begin();
			for (vector <Busline*>::iterator first_leg_lines = (*first_leg).begin(); first_leg_lines < (*first_leg).end(); first_leg_lines++)
			{
				/*
				// currently alights at the first possible transfer stop
				if (boarding_bus->get_line()->get_id() == (*first_leg_lines)->get_id())
				{
					vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer__stops();
					vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin();
					stops_iter++;
					return (*stops_iter).front();
				}
				*/
				if (boarding_bus->get_line()->get_id() == (*first_leg_lines)->get_id())
				{
					vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer_stops();
					vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin() + 2; // pointing to the third place - the first transfer stop
					if (path_set.size() == 1 && (*stops_iter).size() == 1)
					{
						candidate_transfer_stops_u[(*stops_iter).front()] = ::large_positive_utility; // in case it is the only option
					}
					for (vector<Busstop*>::iterator first_transfer_stops = (*stops_iter).begin(); first_transfer_stops < (*stops_iter).end(); first_transfer_stops++)
					{
						if ((*first_transfer_stops)->get_id() == OD_stop->get_destination()->get_id())
						// in case it is the final destination for this passeneger
						{
							candidate_transfer_stops_u[(*first_transfer_stops)] = theParameters->in_vehicle_time_coefficient * ((boarding_bus->get_line()->calc_curr_line_ivt(OD_stop->get_origin(),OD_stop->get_destination(),OD_stop->get_origin()->get_rti(), time))/60);
							// the only utility component is the IVT till the destination
						} 
						else
						// in case it is an intermediate transfer stop
						{
							ODstops* left_od_stop;
							if ((*first_transfer_stops)->check_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination()) == false)
							{
								left_od_stop = new ODstops ((*first_transfer_stops),this->get_OD_stop()->get_destination());
								(*first_transfer_stops)->add_odstops_as_origin(this->get_OD_stop()->get_destination(), left_od_stop);
								this->get_OD_stop()->get_destination()->add_odstops_as_destination((*first_transfer_stops), left_od_stop);
							}
							else
							{
								left_od_stop = (*first_transfer_stops)->get_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination());	
							}
							candidate_transfer_stops_u[(*first_transfer_stops)] = left_od_stop->calc_combined_set_utility_for_alighting (this, boarding_bus, time);
							// the utility is combined for all paths from this transfer stop (incl. travel time till there and transfer penalty)
						}
						// note - this may be called several times, but result with the same calculation
					}
				}
			}
		}
	}
	// calc MNL probabilities
	double MNL_denominator = 0.0;
	for (map <Busstop*, double>::iterator transfer_stops = candidate_transfer_stops_u.begin(); transfer_stops != candidate_transfer_stops_u.end(); transfer_stops++)
	{
		// calc denominator value
		MNL_denominator += exp((*transfer_stops).second);
	}
	for (map <Busstop*, double>::iterator transfer_stops = candidate_transfer_stops_u.begin(); transfer_stops != candidate_transfer_stops_u.end(); transfer_stops++)
	{
		candidate_transfer_stops_p[(*transfer_stops).first] = exp(candidate_transfer_stops_u[(*transfer_stops).first]) / MNL_denominator;
	}
	// perform choice
	vector<double> alighting_probs;
	for (map <Busstop*, double>::iterator stops_probs = candidate_transfer_stops_p.begin(); stops_probs != candidate_transfer_stops_p.end(); stops_probs++)
	{
		alighting_probs.push_back((*stops_probs).second);
	}
	int transfer_stop_position = theRandomizers[0]->mrandom(alighting_probs);
	int iter = 0;
	for (map <Busstop*, double>::iterator stops_probs = candidate_transfer_stops_p.begin(); stops_probs != candidate_transfer_stops_p.end(); stops_probs++)
	{
		iter++;
		if (iter == transfer_stop_position)
		{
			// constructing a structure for output
			map<Busstop*,pair<double,double> > alighting_MNL; // utility followed by probability per stop
			for (map <Busstop*, double>::iterator iter_u = candidate_transfer_stops_u.begin(); iter_u != candidate_transfer_stops_u.end(); iter_u++)
			{
				alighting_MNL[(*iter_u).first].first = (*iter_u).second;
			}
			for (map <Busstop*, double>::iterator iter_p = candidate_transfer_stops_p.begin(); iter_p != candidate_transfer_stops_p.end(); iter_p++)
			{
				alighting_MNL[(*iter_p).first].second = (*iter_p).second;
			}
			OD_stop->record_passenger_alighting_decision(this, boarding_bus, time, (*stops_probs).first, alighting_MNL);
			return ((*stops_probs).first); // rerurn the chosen stop by MNL choice model
		}
	}
	return candidate_transfer_stops_p.begin()->first; // arbitary choice in case something failed
}

Busstop* Passenger::make_connection_decision (double time)
{
	assert(chosen_mode_ == TransitModeType::Null); // traveler should not be waiting for fixed nor flexible yet, should be set to Null before making a connection decision
	temp_connection_path_utilities.clear(); //make sure this is always cleared before its filled in by calls from this method. Used later in make_transitmode_decision and make_dropoff_decision

	map <Busstop*, double> candidate_connection_stops_u; // the double value is the utility associated with the respective stop
	map <Busstop*, double> candidate_connection_stops_p; // the double value is the probability associated with the respective stop
	//Busstop* bs_o = OD_stop->get_origin();
	//Busstop* bs_d = OD_stop->get_destination();
	//vector<Pass_path*> path_set = bs_o->get_stop_od_as_origin_per_stop(bs_d)->get_path_set();
	vector<Pass_path*> path_set = OD_stop->get_path_set();

	// 1. if the path set is empty, passenger moves to an arbitrary walkable stop (first in map of distances for walkable stops)? If no distances are available then the traveler stays at this stop and moves nowhere?
	if (path_set.empty() == true) // move to a nearby stop in case needed
	{
		map<Busstop*,double> & stops = OD_stop->get_origin()->get_walking_distances();
		if (stops.begin()->first->get_id() == OD_stop->get_origin()->get_id())
		{
			map<Busstop*,double>::iterator stops_iter = stops.begin();
			stops_iter++;
			if (stops_iter == stops.end())
			{
				return stops.begin()->first;
			}
			return stops_iter->first;
		}
		else
		{
			return stops.begin()->first;
		}
	}	

	// 2. loops through each path for the current OD of the traveler and all stops within the second pair of alt stops (the connection stop set)
	for (vector <Pass_path*>::iterator path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
	{
		vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer_stops();
		vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin()+1; //start from the set of stops for this path that we can connect to

		for (vector<Busstop*>::iterator connected_stop = (*stops_iter).begin(); connected_stop < (*stops_iter).end(); connected_stop++) // loop through all the potential connection stops of a specific path
		// going over all the stops at the second (connected) set
		{
            if ( (candidate_connection_stops_u.count(*connected_stop) == 0) && ((*connected_stop)->check_destination_stop(this->get_OD_stop()->get_destination()) == true) )
				// only if it wasn't done already and there exists an OD for the remaining part (i.e. the utility has not been calculated already for this connection stop, and their are paths available to the final destination via the connection stop)
			{
				ODstops* left_od_stop = nullptr;
				if ((*connected_stop)->check_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination()) == false) // need to register each stop as a destination stop if not done previously for some reason, isnt this equivalent to the previous check_destination_stop????
				{
					ODstops* left_od_stop = new ODstops ((*connected_stop),this->get_OD_stop()->get_destination());
					(*connected_stop)->add_odstops_as_origin(this->get_OD_stop()->get_destination(), left_od_stop);
					this->get_OD_stop()->get_destination()->add_odstops_as_destination((*connected_stop), left_od_stop); // added to both the origin stop and the destination stop....
				}
				else
				{
					left_od_stop = (*connected_stop)->get_stop_od_as_origin_per_stop(this->get_OD_stop()->get_destination()); // get the OD between the connection stop and the final destination
				}
				if ((*connected_stop)->get_id() == OD_stop->get_destination()->get_id()) // if the final destination of the passenger is within walking distance, then the utility is based only on walking (also no choice will be made)
				// in case it is the final destination for this passeneger
				{
					candidate_connection_stops_u[(*connected_stop)] = theParameters->walking_time_coefficient * (*path_iter)->get_walking_distances().front() / theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4);
					// the only utility component is the CT (walking time) till the destination
				}	 
				else
				// in case it is an intermediate transfer stop, basically anything that isnt the final destination
				{
					assert(left_od_stop); // want to actually see what case we are checking for when/if a nullptr is being returned here... There seems to be a great deal of code attempting to ensure this earlier in this function...
					if(left_od_stop)
					{ 
						//DEBUG_MSG("Passenger::make_connection_decision - Passenger " << this->get_id() << " at stop " << left_od_stop->get_origin()->get_id() << " checking utility for path " << (*path_iter)->get_id());
						candidate_connection_stops_u[(*connected_stop)] = left_od_stop->calc_combined_set_utility_for_connection ((*path_iter)->get_walking_distances().front(), time, this); 
						// walking distances front taken since this is the amount of time it takes to reach the connection stop via walking, so to access the 'set utility'
						// the utility is combined for all paths from this transfer stop (incl. walking time to the connected stop)
					}
					else
					{
						DEBUG_MSG("WARNING Passenger::make_connection_decision - left_od_stop is null");
					}
				}
			}
		}
	}
	// calc MNL probabilities
	double MNL_denominator = 0.0;
	for (map <Busstop*, double>::iterator transfer_stops = candidate_connection_stops_u.begin(); transfer_stops != candidate_connection_stops_u.end(); transfer_stops++)
	{
		// calc denominator value
		MNL_denominator += exp((*transfer_stops).second);
	}
	for (map <Busstop*, double>::iterator transfer_stops = candidate_connection_stops_u.begin(); transfer_stops != candidate_connection_stops_u.end(); transfer_stops++)
	{
		candidate_connection_stops_p[(*transfer_stops).first] = exp(candidate_connection_stops_u[(*transfer_stops).first]) / MNL_denominator;
		//DEBUG_MSG("\t Stop "<< (*transfer_stops).first->get_id() << " prob: " << 100 * candidate_connection_stops_p[(*transfer_stops).first] << "%");
	}
	// perform choice
	vector<double> connecting_probs;
	for (map <Busstop*, double>::iterator stops_probs = candidate_connection_stops_p.begin(); stops_probs != candidate_connection_stops_p.end(); stops_probs++)
	{
		connecting_probs.push_back((*stops_probs).second);
	}
	int transfer_stop_position = theRandomizers[0]->mrandom(connecting_probs);
	int iter = 0;
	for (map <Busstop*, double>::iterator stops_probs = candidate_connection_stops_p.begin(); stops_probs != candidate_connection_stops_p.end(); stops_probs++)
	{
		iter++;
		if (iter == transfer_stop_position)
		{
			// constructing a structure for output
			map<Busstop*,pair<double,double> > connecting_MNL; // utility followed by probability per stop
			for (map <Busstop*, double>::iterator iter_u = candidate_connection_stops_u.begin(); iter_u != candidate_connection_stops_u.end(); iter_u++)
			{
				connecting_MNL[(*iter_u).first].first = (*iter_u).second;
			}
			for (map <Busstop*, double>::iterator iter_p = candidate_connection_stops_p.begin(); iter_p != candidate_connection_stops_p.end(); iter_p++)
			{
				connecting_MNL[(*iter_p).first].second = (*iter_p).second;
			}
			OD_stop->record_passenger_connection_decision(this, time, (*stops_probs).first, connecting_MNL);
			return ((*stops_probs).first); // return the chosen stop by MNL choice model
		}
	}
	return candidate_connection_stops_p.begin()->first; // arbitary choice in case something failed
}

//OBS: Called only in ODzone->execute, the whole zone thing doesnt seem to be used anymore
Busstop* Passenger::make_first_stop_decision (double time)
{
	map <Busstop*, double> candidate_origin_stops_u; // the double value is the utility associated with the respective stop
	map <Busstop*, double> candidate_origin_stops_p; // the double value is the probability associated with the respective stop	
	already_walked = true;
	// going over all relevant origin and destination stops combinations
	if (origin_walking_distances.size() == 1) // in case there is only one possible origin stop
	{
		map<Busstop*,double>::iterator iter_stops = origin_walking_distances.begin();
		return (*iter_stops).first;
	}
	for (map<Busstop*,double>::iterator o_stop_iter = origin_walking_distances.begin(); o_stop_iter != origin_walking_distances.end(); o_stop_iter++)
	{
		if ((*o_stop_iter).second <= theParameters->max_walking_distance)
		{
			candidate_origin_stops_u[(*o_stop_iter).first] = 0;
			for (map<Busstop*,double>::iterator d_stop_iter = destination_walking_distances.begin(); d_stop_iter != destination_walking_distances.end(); d_stop_iter++)
			{
				ODstops* possible_od = (*o_stop_iter).first->get_stop_od_as_origin_per_stop((*d_stop_iter).first);
				vector<Pass_path*> path_set = possible_od->get_path_set();
				for (vector <Pass_path*>::iterator path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
				{
					vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer_stops();
					vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin();
                    Q_UNUSED (stops_iter)
					// taking into account the walking distances from the origin to the origin stop and from the last stop till the final destination
					candidate_origin_stops_u[(*o_stop_iter).first] += exp(theParameters->walking_time_coefficient * origin_walking_distances[(*o_stop_iter).first]/ theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4) + theParameters->walking_time_coefficient * destination_walking_distances[(*d_stop_iter).first]/ theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4) + possible_od->calc_combined_set_utility_for_connection ((*path_iter)->get_walking_distances().front(), time, this));
				}
			}
			candidate_origin_stops_u[(*o_stop_iter).first] = log (candidate_origin_stops_u[(*o_stop_iter).first]);
		}
	}
	// calc MNL probabilities
	double MNL_denominator = 0.0;
	for (map <Busstop*, double>::iterator origin_stops = candidate_origin_stops_u.begin(); origin_stops != candidate_origin_stops_u.end(); origin_stops++)
	{
		// calc denominator value
		MNL_denominator += exp((*origin_stops).second);
	}
	for (map <Busstop*, double>::iterator origin_stops = candidate_origin_stops_u.begin(); origin_stops != candidate_origin_stops_u.end(); origin_stops++)
	{
		candidate_origin_stops_p[(*origin_stops).first] = exp(candidate_origin_stops_u[(*origin_stops).first]) / MNL_denominator;
	}
	// perform choice
	vector<double> origin_probs;
	for (map <Busstop*, double>::iterator stops_probs = candidate_origin_stops_p.begin(); stops_probs != candidate_origin_stops_p.end(); stops_probs++)
	{
		origin_probs.push_back((*stops_probs).second);
	}
	int origin_stop_position = theRandomizers[0]->mrandom(origin_probs);
	int iter = 0;
	for (map <Busstop*, double>::iterator stops_probs = candidate_origin_stops_p.begin(); stops_probs != candidate_origin_stops_p.end(); stops_probs++)
	{
		iter++;
		if (iter == origin_stop_position)
		{
			// constructing a structure for output
			map<Busstop*,pair<double,double> > origin_MNL; // utility followed by probability per stop
			for (map <Busstop*, double>::iterator iter_u = candidate_origin_stops_u.begin(); iter_u != candidate_origin_stops_u.end(); iter_u++)
			{
				origin_MNL[(*iter_u).first].first = (*iter_u).second;
			}
			for (map <Busstop*, double>::iterator iter_p = candidate_origin_stops_p.begin(); iter_p != candidate_origin_stops_p.end(); iter_p++)
			{
				origin_MNL[(*iter_p).first].second = (*iter_p).second;
			}
			return ((*stops_probs).first); // return the chosen stop by MNL choice model
		}
	}
	return candidate_origin_stops_p.begin()->first; // arbitary choice in case something failed

}

TransitModeType Passenger::make_transitmode_decision(Busstop* pickup_stop, double time)
{
	assert(chosen_mode_ == TransitModeType::Null); //!< The chosen mode of the traveler should be Null before a decision has been made
	assert(pickup_stop->check_stop_od_as_origin_per_stop(OD_stop->get_destination()) != false); //!< checked already in make_connection_decision which should always be called
    //DEBUG_MSG("Passenger::make_transitmode_decision() - pass " << this->get_id() << " choosing mode for pickup stop " << pickup_stop->get_id() << " at time " << time);

	//Q_UNUSED(time);
	TransitModeType chosen_mode = TransitModeType::Null;

	if (pickup_stop != OD_stop->get_destination()) // the walking only case.., default is to return TransitModeType::Null then, no decision made
	{
		map<TransitModeType, pair<double, double> > mode_MNL; //!< for output collector at ODstops level
        
		ODstops* targetOD = pickup_stop->get_stop_od_as_origin_per_stop(OD_stop->get_destination());

		double fixed_u = 0.0; //utility of choosing fixed
		double fixed_p = 0.0; //probability of fixed
		double flex_u = 0.0; //utility of choosing flexible
		double flex_p = 0.0; //probability of flex
		bool fixed_available = false;
		bool flex_available = false;

		vector<Pass_path*> paths = targetOD->get_path_set();
		if (temp_connection_path_utilities.count(targetOD) == 0) //should be at least one path, unless traveler walked to an arbitrary stop due to no paths being available
			return TransitModeType::Null;

		for (const auto& path : paths) //utilities of accessing the pickup stop should already be included in temp_connection_path_utilities
		{
			if (path->is_first_transit_leg_fixed()) 
			{
				if (temp_connection_path_utilities[targetOD].count(path) != 0) //should ignore paths that include walking to access targetOD, access to pickup stop set utility already included
				{
					//DEBUG_MSG("\t getting utilities for fixed path " << path->get_id());
					fixed_u += temp_connection_path_utilities[targetOD][path];
					fixed_available = true;
				}
			}
			else if (path->is_first_transit_leg_flexible())
			{
				if(temp_connection_path_utilities[targetOD].count(path) != 0) //should ignore paths that include walking to access targetOD, access to pickup stop set utility already included
				{
					//DEBUG_MSG("\t getting utilities for flex path " << path->get_id());
					flex_u += temp_connection_path_utilities[targetOD][path];
					flex_available = true;
				}
			}
		}

		fixed_u = fixed_available ? log(fixed_u) : ::large_negative_utility; //logsum of path-set utilities associated with each action, default large negative utility otherwise
		flex_u = flex_available ? log(flex_u) : ::large_negative_utility;

		double MNL_denom = exp(fixed_u) + exp(flex_u);
		fixed_p = exp(fixed_u) / MNL_denom;
		flex_p = exp(flex_u) / MNL_denom;
		assert(AproxEqual(fixed_p + flex_p, 1.0));

		mode_MNL[TransitModeType::Fixed].first = fixed_u; 
		mode_MNL[TransitModeType::Fixed].second = fixed_p;
		mode_MNL[TransitModeType::Flexible].first = flex_u; 
		mode_MNL[TransitModeType::Flexible].second = flex_p;
		//DEBUG_MSG("\t Fixed prob: " << fixed_p*100 << "%");
		//DEBUG_MSG("\t Flex prob : " << flex_p*100 << "%");

		// Make choice, if neither fixed or flex is available then default is to return TransitModeType::Null
		if (fixed_available && !flex_available)
		{
			chosen_mode = TransitModeType::Fixed;
		}
		else if (flex_available && !fixed_available)
		{
			chosen_mode = TransitModeType::Flexible;
		}
		else if (!fixed_available && !flex_available)
		{
			chosen_mode = TransitModeType::Null;
		}
		else
		{
			vector<double> mode_probs = { fixed_p, flex_p };
			vector<TransitModeType> modes = { TransitModeType::Fixed, TransitModeType::Flexible };

			chosen_mode = theRandomizers[0]->randomchoice(modes, mode_probs); 
		}
		targetOD->record_passenger_transitmode_decision(this, time, pickup_stop, chosen_mode, mode_MNL); // store decision result at ODstop level
    }
	
	if (chosen_mode == TransitModeType::Null)
		DEBUG_MSG_V("WARNING - Passenger::make_transitmode_decision() - " << "returning TransitModeType::Null" << " at time " << time << " for passenger " << this->get_id());
	else if (chosen_mode == TransitModeType::Flexible)
		assert(pickup_stop->get_CC() != nullptr); // should only choose flexible mode if there is actually a service for the pickup-stop of the traveler

	return chosen_mode;
}

Busstop* Passenger::make_dropoff_decision(Busstop* pickup_stop, double time)
{
    /**
     * @todo
     *	 Only called after set_chosen_mode(TransitModeType::Flexible)
     *  Want to loop through the paths starting from the chosen pickup stop with first leg flexible
     *  and look at each of the first leg dropoff stops and the path set utility for this
     */
    assert(chosen_mode_ == TransitModeType::Flexible); //dropoff decision really only makes sense for on-demand/flexible modes
    Busstop* dropoff_stop = nullptr;

    assert(pickup_stop->check_stop_od_as_origin_per_stop(OD_stop->get_destination()) != false); //!< checked already in make_connection_decision which should always be called
    //DEBUG_MSG("Passenger::make_dropoff_decision() - pass " << this->get_id() << " choosing dropoff stop for pickup stop " << pickup_stop->get_id() << " at time " << time);

    if (pickup_stop != OD_stop->get_destination() || chosen_mode_ != TransitModeType::Flexible) // the walking only case or called for a traveler using a non-flexible mode, default is to return nullptr
    {
        ODstops* targetOD = pickup_stop->get_stop_od_as_origin_per_stop(OD_stop->get_destination());
        if (temp_connection_path_utilities.count(targetOD) == 0) //should be at least one path, unless traveler walked to an arbitrary stop due to no paths being available
            return nullptr;

        map <Busstop*, double> accum_dropoff_stops_u; // accumulated utilities before calculating logsum per stop
        vector<Pass_path*> flex_paths = targetOD->get_flex_first_paths(); // only care about paths with first leg flexible, flexible mode chosen for next leg already

        for (const auto& path : flex_paths) //utilities of accessing the pickup stop should already be included in temp_connection_path_utilities
        {
            if (temp_connection_path_utilities[targetOD].count(path) != 0) //should ignore paths that include walking to access targetOD, access to pickup stop set utility already included
            {
                Busstop* candidate_stop = path->get_first_dropoff_stop();
                //DEBUG_MSG("\t getting utilities for path " << path->get_id() << " with next leg dropoff stop " << candidate_stop->get_id());
                accum_dropoff_stops_u[candidate_stop] += temp_connection_path_utilities[targetOD][path]; 	//sort the path utilities by dropoff stop
            }
        }
        assert(!accum_dropoff_stops_u.empty()); // if empty, a flexible first transit leg was chosen with no available destination stops or no flexible paths available from the target OD (i.e. should never happen)

        map <Busstop*, double> dropoff_stops_u; // the double value is the utility associated with the respective stop
        map <Busstop*, double> dropoff_stops_p; // the double value is the probability associated with the respective stop
        vector<double> stop_probs; // probability distribution of dropoff stops
        vector<Busstop*> candidate_stops; // dropoff stops to choose from

        map<Busstop*, pair<double, double> > dropoff_MNL; //!< for output collector at ODstops level, first double utility of choosing dropoff stop, second probability
        double MNL_denom = 0.0;
        //calculate probabilities
        for (const auto& stop_u : accum_dropoff_stops_u) //calculate the MNL denominator
        {
            dropoff_stops_u[stop_u.first] = (stop_u.second == 0.0) ? -DBL_INF : log(stop_u.second);
            MNL_denom += exp(dropoff_stops_u[stop_u.first]); //maybe redundant directly after log but to stay consistent with logit formulation
        }
        //assert(!AproxEqual(MNL_denom, 0.0));

        for (const auto& stop_u : dropoff_stops_u) //calculate probabilities
        {
            Busstop* candidate_dropoff_stop = stop_u.first;
            double stop_prob = exp(stop_u.second) / MNL_denom; //probability of choosing stop as dropoff point

            dropoff_stops_p[candidate_dropoff_stop] = stop_prob;
            candidate_stops.push_back(candidate_dropoff_stop);
            stop_probs.push_back(stop_prob);

            dropoff_MNL[candidate_dropoff_stop] = make_pair(stop_u.second, stop_prob); // fill in output structure
            //DEBUG_MSG("\t Stop " << candidate_dropoff_stop->get_id() << " prob: " << 100 * stop_prob << "%");
        }

		assert(AproxEqual(accumulate(stop_probs.begin(), stop_probs.end(), 0.0),1.0));

        dropoff_stop = theRandomizers[0]->randomchoice(candidate_stops, stop_probs); // make choice
        targetOD->record_passenger_dropoff_decision(this, time, pickup_stop, dropoff_stop, dropoff_MNL);
    }

    return dropoff_stop;
}

map<Busstop *, double, ptr_less<Busstop *> > Passenger::sample_walking_distances(ODzone* zone)
{
    map<Busstop*,double, ptr_less<Busstop*>> walking_distances;
    map <Busstop*,pair<double,double> , ptr_less<Busstop*> > stop_distances = zone->get_stop_distances();
    for (map <Busstop*,pair<double,double>, ptr_less<Busstop*> >::iterator stop_iter = stop_distances.begin(); stop_iter != stop_distances.end(); stop_iter++)
	{
			walking_distances[(*stop_iter).first] = theRandomizers[0]->nrandom((*stop_iter).second.first, (*stop_iter).second.second);
	}
	return walking_distances;
}

double Passenger::calc_boarding_probability_zone (Busline* arriving_bus, Busstop* o_stop, double time)
{
	// initialization
	double boarding_utility = 0.0;
	double staying_utility = 0.0;
	double path_utility = 0.0;
	vector<Busline*> first_leg_lines;
	bool in_alt = false; // indicates if the current arriving bus is included 
	// checks if the arriving bus is included as an option in a path set of at least ONE of relevant OD pair 
    auto d_stops = d_zone->get_stop_distances();
    for (auto iter_d_stops = d_stops.begin(); iter_d_stops != d_stops.end(); iter_d_stops++)
	{
		vector <Pass_path*> path_set = o_stop->get_stop_od_as_origin_per_stop((*iter_d_stops).first)->get_path_set();
		for (vector <Pass_path*>::iterator path = path_set.begin(); path < path_set.end(); path ++)
		{
			if (in_alt == true)
			{
				break;
			}
			if ((*path)->get_alt_lines().empty() == false) // in case it is not a walk-only alternative
			{
				vector <vector <Busline*> > alt_lines = (*path)->get_alt_lines();
				vector <Busline*> first_lines = alt_lines.front(); // need to check only for the first leg
				for (vector <Busline*>::iterator line = first_lines.begin(); line < first_lines.end(); line++)
				{
					if ((*line)->get_id() == arriving_bus->get_id())
					{
						in_alt = true;
						break;
					}
				}
			}
		}
	}
	// if this bus line can be rellevant
	if (in_alt == true)
	{
		vector<Pass_path*> arriving_paths;
		for (map <Busstop*,pair<double,double> >::iterator iter_d_stops = d_stops.begin(); iter_d_stops != d_stops.end(); iter_d_stops++)
		{
			vector <Pass_path*> path_set = o_stop->get_stop_od_as_origin_per_stop((*iter_d_stops).first)->get_path_set();
			for (vector<Pass_path*>::iterator iter_paths = path_set.begin(); iter_paths < path_set.end(); iter_paths++)
			{
				(*iter_paths)->set_arriving_bus_rellevant(false);
				if ((*iter_paths)->get_alt_lines().empty() == false) //  in case it is not a walking-only alternative
				{
					first_leg_lines = (*iter_paths)->get_alt_lines().front();
					for(vector<Busline*>::iterator iter_first_leg_lines = first_leg_lines.begin(); iter_first_leg_lines < first_leg_lines.end(); iter_first_leg_lines++)
					{
						if ((*iter_first_leg_lines)->get_id() == arriving_bus->get_id()) // if the arriving bus is a possible first leg for this path alternative
						{
						path_utility = (*iter_paths)->calc_arriving_utility(time, this) + theParameters->waiting_time_coefficient * (destination_walking_distances[(*iter_d_stops).first] / theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4));
							// including the walking time from the last stop till the final destination
							boarding_utility += exp(path_utility); 
							arriving_paths.push_back((*iter_paths));
							(*iter_paths)->set_arriving_bus_rellevant(true);
							break;
						}
					}
				}
			}
		}
		boarding_utility = log (boarding_utility);
		for (map <Busstop*,pair<double,double> >::iterator iter_d_stops = d_stops.begin(); iter_d_stops != d_stops.end(); iter_d_stops++)
		{
			vector <Pass_path*> path_set = o_stop->get_stop_od_as_origin_per_stop((*iter_d_stops).first)->get_path_set();
			for (vector<Pass_path*>::iterator iter_paths = path_set.begin(); iter_paths < path_set.end(); iter_paths++)
			{
				if ((*iter_paths)->get_arriving_bus_rellevant() == false)
				{
					// logsum calculation
				path_utility = (*iter_paths)->calc_waiting_utility((*iter_paths)->get_alt_transfer_stops().begin(), time, false, this) + theParameters->walking_time_coefficient * (destination_walking_distances[(*iter_d_stops).first] / theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4));
					// including the walking time from the last stop till the final destination
					staying_utility += exp(path_utility);
				}
			}
		} // NOTE: NO dynamic dominance check yet in the zone case
		if (staying_utility == 0.0) 
		{
			boarding_utility = 2.0;
			staying_utility = -2.0;
		}
		else
		{
			staying_utility = log (staying_utility);
		}
		// calculate the probability to board
		return (exp(boarding_utility) / (exp(boarding_utility) + exp (staying_utility)));
		o_zone->set_boarding_u(boarding_utility);
		o_zone->set_staying_u(staying_utility);
	}
	// what to do if the arriving bus is not included in any of the alternatives?
	// currently - will not board it
	else 
	{	
		boarding_utility = -2.0;
		staying_utility = 2.0;
		return 0;
	}
}

Busstop* Passenger::make_alighting_decision_zone (Bustrip* boarding_bus, double time)
{
	// assuming that a pass. boards only paths from his path set
	map <Busstop*, double> candidate_transfer_stops_u; // the double value is the utility associated with the respective stop
	map <Busstop*, double> candidate_transfer_stops_p; // the double value is the probability associated with the respective stop
    auto d_stops = d_zone->get_stop_distances();
    for (auto iter_d_stops = d_stops.begin(); iter_d_stops != d_stops.end(); iter_d_stops++)
	{
		vector<Pass_path*> path_set = OD_stop->get_origin()->get_stop_od_as_origin_per_stop((*iter_d_stops).first)->get_path_set();
		for (vector <Pass_path*>::iterator path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
		{
			vector<vector<Busline*> > alt_lines = (*path_iter)->get_alt_lines();
			if (alt_lines.empty() == false) // in case it is not a walking-only alternative
			{
				vector<vector<Busline*> >::iterator first_leg = alt_lines.begin();
				for (vector <Busline*>::iterator first_leg_lines = (*first_leg).begin(); first_leg_lines < (*first_leg).end(); first_leg_lines++)
				{
					if (boarding_bus->get_line()->get_id() == (*first_leg_lines)->get_id())
					{
						vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer_stops();
						vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin() + 2; // pointing to the third place - the first transfer stop
						for (vector<Busstop*>::iterator first_transfer_stops = (*stops_iter).begin(); first_transfer_stops < (*stops_iter).end(); first_transfer_stops++)
						{
							ODstops* left_od_stop = (*first_transfer_stops)->get_stop_od_as_origin_per_stop((*iter_d_stops).first);	
							candidate_transfer_stops_u[(*first_transfer_stops)] = left_od_stop->calc_combined_set_utility_for_alighting_zone (this, boarding_bus, time);	// the utility is combined for all paths from this transfer stop (incl. travel time till there and transfer penalty)
							// note - this may be called several times, but result with the same calculation
						}
					}
				}
			}
		}
	}
	// calc MNL probabilities
	double MNL_denominator = 0.0;
	for (map <Busstop*, double>::iterator transfer_stops = candidate_transfer_stops_u.begin(); transfer_stops != candidate_transfer_stops_u.end(); transfer_stops++)
	{
		// calc denominator value
		MNL_denominator += exp((*transfer_stops).second);
	}
	for (map <Busstop*, double>::iterator transfer_stops = candidate_transfer_stops_u.begin(); transfer_stops != candidate_transfer_stops_u.end(); transfer_stops++)
	{
		candidate_transfer_stops_p[(*transfer_stops).first] = exp(candidate_transfer_stops_u[(*transfer_stops).first]) / MNL_denominator;
	}
	// perform choice
	vector<double> alighting_probs;
	for (map <Busstop*, double>::iterator stops_probs = candidate_transfer_stops_p.begin(); stops_probs != candidate_transfer_stops_p.end(); stops_probs++)
	{
		alighting_probs.push_back((*stops_probs).second);
	}
	int transfer_stop_position = theRandomizers[0]->mrandom(alighting_probs);
	int iter = 0;
	for (map <Busstop*, double>::iterator stops_probs = candidate_transfer_stops_p.begin(); stops_probs != candidate_transfer_stops_p.end(); stops_probs++)
	{
		iter++;
		if (iter == transfer_stop_position)
		{
			// constructing a structure for output
			map<Busstop*,pair<double,double> > alighting_MNL; // utility followed by probability per stop
			for (map <Busstop*, double>::iterator iter_u = candidate_transfer_stops_u.begin(); iter_u != candidate_transfer_stops_u.end(); iter_u++)
			{
				alighting_MNL[(*iter_u).first].first = (*iter_u).second;
			}
			for (map <Busstop*, double>::iterator iter_p = candidate_transfer_stops_p.begin(); iter_p != candidate_transfer_stops_p.end(); iter_p++)
			{
				alighting_MNL[(*iter_p).first].second = (*iter_p).second;
			}
			o_zone->record_passenger_alighting_decision_zone(this, boarding_bus, time, (*stops_probs).first, alighting_MNL);
			return ((*stops_probs).first); // rerurn the chosen stop by MNL choice model
		}
	}
	return candidate_transfer_stops_p.begin()->first; // arbitary choice in case something failed
}

Busstop* Passenger::make_connection_decision_zone (double time)
{
	// called with the new transfer stop as the origin stop	
	map <Busstop*, double> candidate_connection_stops_u; // the double value is the utility associated with the respective stop
	map <Busstop*, double> candidate_connection_stops_p; // the double value is the probability associated with the respective stop
	double u_walk_directly = -10.0;
	if (destination_walking_distances.count(OD_stop->get_origin()) > 0) // in case this stop belongs to the destination zone
	{
		if (destination_walking_distances[OD_stop->get_origin()] < theParameters->max_walking_distance)
		{
			u_walk_directly = theParameters->walking_time_coefficient * destination_walking_distances[OD_stop->get_origin()] / theRandomizers[0]->nrandom(theParameters->average_walking_speed, theParameters->average_walking_speed/4);
		}
	}
    auto distances = d_zone->get_stop_distances();
    for (auto iter_d_stops = distances.begin(); iter_d_stops != distances.end(); iter_d_stops++)
	{	
		vector<Pass_path*> path_set = OD_stop->get_origin()->get_stop_od_as_origin_per_stop((*iter_d_stops).first)->get_path_set();
		for (vector <Pass_path*>::iterator path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
		{
			vector<vector<Busstop*> > alt_stops = (*path_iter)->get_alt_transfer_stops();
			vector<vector<Busstop*> >::iterator stops_iter = alt_stops.begin()+1;
			for (vector<Busstop*>::iterator connected_stop = (*stops_iter).begin(); connected_stop < (*stops_iter).end(); connected_stop++)
			// going over all the stops at the second (connected) set
			{
                if ( (candidate_connection_stops_u.count(*connected_stop) == 0) && ((*connected_stop)->check_destination_stop(this->get_OD_stop()->get_destination()) == true))
				// only if it wasn't done already and there exists an OD for the remaining part
				{
					ODstops* left_od_stop = (*connected_stop)->get_stop_od_as_origin_per_stop((*iter_d_stops).first);
					// at every stop you can try to walk from it till the final destination or continue by transit
					candidate_connection_stops_u[(*connected_stop)] = left_od_stop->calc_combined_set_utility_for_connection_zone(this,(*path_iter)->get_walking_distances().front(), time);
					// the utility is combined for all paths from this transfer stop (incl. walking time to the connected stop)
				}
			}
		}
	}
	// calc MNL probabilities
	double MNL_denominator = 0.0;
	for (map <Busstop*, double>::iterator transfer_stops = candidate_connection_stops_u.begin(); transfer_stops != candidate_connection_stops_u.end(); transfer_stops++)
	{
		// calc denominator value
		MNL_denominator += exp((*transfer_stops).second);
	}
	double u_continue_transit_trip = log(MNL_denominator);
	for (map <Busstop*, double>::iterator transfer_stops = candidate_connection_stops_u.begin(); transfer_stops != candidate_connection_stops_u.end(); transfer_stops++)
	{
		candidate_connection_stops_p[(*transfer_stops).first] = exp(candidate_connection_stops_u[(*transfer_stops).first]) / MNL_denominator;
	}
	// perform choice

	// first - decide whether to walk directly to the destination or continue the trnasit trip
	this_is_the_last_stop = theRandomizers[0] ->brandom(OD_stop->calc_binary_logit(u_walk_directly,u_continue_transit_trip)); 
	if (this_is_the_last_stop == true)
	{
		pair<Busstop*,double> stop_time;
		stop_time.first = OD_stop->get_origin();
		stop_time.second = time;
		selected_path_stops.push_back(stop_time);
		return OD_stop->get_origin();
	}
	// second - if we continue by transit, than at which stop to transfer
	vector<double> connecting_probs;
	for (map <Busstop*, double>::iterator stops_probs = candidate_connection_stops_p.begin(); stops_probs != candidate_connection_stops_p.end(); stops_probs++)
	{
		connecting_probs.push_back((*stops_probs).second);
	}
	int transfer_stop_position = theRandomizers[0]->mrandom(connecting_probs);
	int iter = 0;
	for (map <Busstop*, double>::iterator stops_probs = candidate_connection_stops_p.begin(); stops_probs != candidate_connection_stops_p.end(); stops_probs++)
	{
		iter++;
		if (iter == transfer_stop_position)
		{
			// constructing a structure for output
			map<Busstop*,pair<double,double> > alighting_MNL; // utility followed by probability per stop
			for (map <Busstop*, double>::iterator iter_u = candidate_connection_stops_u.begin(); iter_u != candidate_connection_stops_u.end(); iter_u++)
			{
				alighting_MNL[(*iter_u).first].first = (*iter_u).second;
			}
			for (map <Busstop*, double>::iterator iter_p = candidate_connection_stops_p.begin(); iter_p != candidate_connection_stops_p.end(); iter_p++)
			{
				alighting_MNL[(*iter_p).first].second = (*iter_p).second;
			}
			pair<Busstop*,double> stop_time;
			stop_time.first = (*stops_probs).first;
			stop_time.second = time;
			selected_path_stops.push_back(stop_time);
			return ((*stops_probs).first); // return the chosen stop by MNL choice model
		}
	}
	pair<Busstop*,double> stop_time;
	stop_time.first = candidate_connection_stops_p.begin()->first;
	stop_time.second = time;
	selected_path_stops.push_back(stop_time);
	return candidate_connection_stops_p.begin()->first; // arbitary choice in case something failed
}

bool Passenger::stop_is_in_d_zone (Busstop* stop)
{
	if (destination_walking_distances.count(stop)>0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

double Passenger::calc_total_waiting_time()
{
	double total_waiting_time = 0.0;
	vector <pair<Busstop*,double> >::iterator iter_stop=selected_path_stops.begin();
	iter_stop++;
	for (vector <pair<Bustrip*,double> >::iterator iter_trip=selected_path_trips.begin(); iter_trip<selected_path_trips.end(); iter_trip++)
	{
		total_waiting_time += (*iter_trip).second - (*iter_stop).second; 
		iter_stop++;
		iter_stop++;
	}
	return total_waiting_time;
}

double Passenger::calc_total_IVT()
{
	/*
	double IVT = 0.0;
	for (vector <pair<double,double> >::iterator iter = experienced_crowding_levels.begin(); iter < experienced_crowding_levels.end(); iter++)
	{
		IVT += (*iter).first;
	}
	return IVT;
	*/
	double total_IVT;
	total_IVT = 0.0;
	vector <pair<Busstop*,double> >::iterator iter_stop=selected_path_stops.begin();
	iter_stop++;
	iter_stop++;
	for (vector <pair<Bustrip*,double> >::iterator iter_trip=selected_path_trips.begin(); iter_trip<selected_path_trips.end(); iter_trip++)
	{
		total_IVT += (*iter_stop).second - (*iter_trip).second; 
		iter_stop++;
		iter_stop++;
	}
	return total_IVT;
}

double Passenger::calc_IVT_crowding()
{
	double VoT_crowding = 0.0;
	for (vector <pair<double,double> >::iterator iter = experienced_crowding_levels.begin(); iter < experienced_crowding_levels.end(); iter++)
	{
		VoT_crowding += (*iter).first * (*iter).second;
	}
	return VoT_crowding;
}

double Passenger::calc_total_walking_time()
{
	double total_walking_time = 0.0;
	for (vector <pair<Busstop*,double> >::iterator iter_stop=selected_path_stops.begin(); iter_stop<selected_path_stops.end(); iter_stop++)
	{
		iter_stop++;
		total_walking_time += (*iter_stop).second - (*(iter_stop-1)).second; 
	}
	return total_walking_time;
}

double Passenger::calc_total_waiting_time_due_to_denied_boarding()
{
	double total_walking_time_due_to_denied_boarding = 0.0;
	for (vector <pair<Busstop*,double> >::iterator iter_denied=waiting_time_due_denied_boarding.begin(); iter_denied < waiting_time_due_denied_boarding.end(); iter_denied++)
	{
		vector <pair<Busstop*,double> >::iterator iter_stop=selected_path_stops.begin();
		iter_stop++;
		for (vector <pair<Bustrip*,double> >::iterator iter_trip=selected_path_trips.begin(); iter_trip<selected_path_trips.end(); iter_trip++)
		{
			if ((*iter_stop).first->get_id() == (*iter_denied).first->get_id())
			{
				total_walking_time_due_to_denied_boarding += (*iter_trip).second - (*iter_denied).second; 
				break;
			}
			iter_stop++;
			iter_stop++;
		}
		
	}
	return total_walking_time_due_to_denied_boarding;
}

bool Passenger::line_is_rejected(int id) 
{
	vector<int>::iterator it = find(rejected_lines.begin(),rejected_lines.end(), id); 
	return it != rejected_lines.end();
}

void Passenger::write_selected_path(ostream& out)
{
	// claculate passenger travel time components
	double total_waiting_time = calc_total_waiting_time();
	double total_IVT = calc_total_IVT();
	double total_IVT_crowding = calc_IVT_crowding();
	double total_walking_time = calc_total_walking_time();
	double total_waiting_time_due_to_denied_boarding = calc_total_waiting_time_due_to_denied_boarding();
	int nr_transfers = (static_cast<int>(selected_path_stops.size()) - 4) / 2; // given path definition (direct connection - 4 elements, 1 transfers - 6 elements, 2 transfers - 8 elements, etc.

	this->set_GTC(theParameters->walking_time_coefficient * total_walking_time + theParameters->waiting_time_coefficient * total_waiting_time + theParameters->waiting_time_coefficient * 3.5 *total_waiting_time_due_to_denied_boarding + theParameters->in_vehicle_time_coefficient * total_IVT_crowding + theParameters->transfer_coefficient * nr_transfers);

	out << passenger_id << '\t'
		<< original_origin->get_id() << '\t'
		<< original_origin->get_name() << '\t'
		<< OD_stop->get_destination()->get_id() << '\t'
		<< OD_stop->get_destination()->get_name() << '\t'
		<< start_time << '\t'
		<< nr_transfers << '\t'
		<< total_walking_time << '\t'
		<< total_waiting_time << '\t'
		<< total_waiting_time_due_to_denied_boarding << '\t'
		<< total_IVT << '\t'
		<< total_IVT_crowding << '\t'
		<< end_time << '\t'
		<< '{';

	for (vector <pair<Busstop*,double> >::iterator stop_iter = selected_path_stops.begin(); stop_iter < selected_path_stops.end(); stop_iter++)
	{
		out << (*stop_iter).first->get_id() << '\t';
	}

	out << '}' << '\t' 
			
		<< '{' << '\t';
	for (vector <pair<Bustrip*,double> >::iterator trip_iter = selected_path_trips.begin(); trip_iter < selected_path_trips.end(); trip_iter++)
	{
		out << (*trip_iter).first->get_id() << '\t';
	}	
	out << '}' << endl;
}

void Passenger::write_passenger_trajectory(ostream& out)
{
	// find passenger time-location stamps
	out << passenger_id << '\t'
		<< '{' << '\t';
	vector <Busstop*> stop_stamps;
	vector <pair<Bustrip*, double> >::iterator trip_iter = selected_path_trips.begin();
	vector <pair<Busstop*, double> >::iterator stop_iter = selected_path_stops.begin();
	
	while (trip_iter < selected_path_trips.end() && stop_iter < selected_path_stops.end())
	{
		stop_stamps.push_back((*stop_iter).first);
		out << (*stop_iter).second << '\t'; // pass. arrival time at stop
		stop_iter++; // forward to pairing transit stop
		stop_stamps.push_back((*stop_iter).first);
		out << (*stop_iter).second << '\t'; // pass. arrival time at stop
		stop_stamps.push_back((*stop_iter).first);
		out << (*trip_iter).second << '\t'; // pass. boarding time at stop
		trip_iter++;
		stop_iter++;
	}
	stop_stamps.push_back((*stop_iter).first);
	out << (*stop_iter).second << '\t'; // pass. arrival time at last stop
	stop_iter++; // forward to pairing transit stop
	stop_stamps.push_back((*stop_iter).first);
	out << (*stop_iter).second << '\t' // pass. arrival time at destination
		<< '}' << '\t' << '{';
	for (vector <Busstop*>::iterator stop_iter = stop_stamps.begin(); stop_iter < stop_stamps.end(); stop_iter++)
	{
		out << (*stop_iter)->get_id() << '\t';
	}
	out << '}' << endl;
}

int Passenger::get_selected_path_last_line_id ()
{
	return selected_path_trips.back().first->get_line()->get_id();
}

int Passenger::get_last_denied_boarding_stop_id ()
{
	Busstop* stop = waiting_time_due_denied_boarding.back().first;
	return stop->get_id();
}

bool Passenger::empty_denied_boarding ()
{
	return waiting_time_due_denied_boarding.empty();
}

 // PassengerRecycler procedures

PassengerRecycler::	~PassengerRecycler()
{
 	for (list <Passenger*>::iterator iter=pass_recycled.begin();iter!=pass_recycled.end();)
	{			
		delete (*iter); // calls automatically destructor
		iter=pass_recycled.erase(iter);	
	}
}


double Passenger::get_walking_time(Busstop* busstop_dest_ptr, double curr_time)
{
    Busstop* busstop_orig_ptr = OD_stop->get_origin();

    return busstop_orig_ptr->get_walking_time(busstop_dest_ptr, curr_time);
}
