#include "vehicle.h"
#include "MMath.h"
#include "controlcenter.h"

Vehicle::Vehicle()
{
 id=-1;
 type=-1;
 route=nullptr;
 odpair=nullptr;
 start_time=0.0;
 length=0;
 exit_time=0.0;
 entered=false;
 switched=0;
 meters=0;
 this->set_curr_link_route_idx(0);
}

Vehicle::~Vehicle()
{}

Vehicle::Vehicle(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_): id(id_), route(route_), odpair(odpair_), start_time(time_) , type(type_), length(length_), exit_time(0.0)
{
	entered=false;	
	switched=0;
	meters=0;
}

void Vehicle::init (int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_)
{
 id=id_;
 type=type_;
 route=route_;
 odpair=odpair_;
 start_time=time_;
 length=length_;
 exit_time=0.0;
 entered=false;
 switched=0;
 meters=0;
 this->set_curr_link_route_idx(0);
}

void Vehicle::set_curr_link(Link* curr_link_)
{
	if(entered)
		this->advance_curr_link_route_idx();
	curr_link=curr_link_;
}

Link* Vehicle::get_curr_link()
{
	return curr_link;
}

Link* Vehicle::nextlink()
{
	Link* link = nullptr;
	if (entered)
    {
        link = route->nextlink(curr_link_route_idx); // find next link based on index
    }
    else
	{
		link = route->firstlink();
	}

	if (link)
		assert(curr_link->get_out_node_id() == link->get_in_node_id()); //sanity check to make sure the route is connected

	return link;

}

const odval Vehicle::get_odids ()
{return odpair->odids();}

int Vehicle::get_oid()
{
	odval oid=odpair->odids() ;
	return oid.first;	
}

int Vehicle::get_did()
{
	odval did=odpair->odids() ;
	return did.second;	
}
 	

void Vehicle::report(double time)
{
  arrival_time=time;
  list <double> collector;
  collector.push_back(id);
  collector.push_back(start_time);
  collector.push_back(arrival_time);
  collector.push_back(arrival_time - start_time);
  collector.push_back(meters);
  collector.push_back(route->get_id());
  collector.push_back(switched);
  odpair->report(collector);
}

 // VehicleRecycler procedures

VehicleRecycler::	~VehicleRecycler()
{
 	/*
	for (list <Vehicle*>::iterator iter=recycled.begin();iter!=recycled.end();)
	{			
		delete (*iter); // calls automatically destructor
		iter=recycled.erase(iter);	
	}
	for (list <Bus*>::iterator iter1=recycled_busses.begin();iter1!=recycled_busses.end();)
	{			
		delete (*iter1); // calls automatically destructor
		iter1=recycled_busses.erase(iter1);	
	}
	*/

}

QString BusState_to_QString(BusState state)
{
	QString state_s = "";

    switch (state)
    {
    case BusState::Null: state_s = "Null"; break;
    case BusState::IdleEmpty: state_s = "IdleEmpty"; break;
    case BusState::IdlePartiallyFull: state_s = "IdlePartiallyFull"; break;
    case BusState::IdleFull: state_s = "IdleFull"; break;
    case BusState::DrivingEmpty: state_s = "DrivingEmpty"; break;
    case BusState::DrivingPartiallyFull: state_s = "DrivingPartiallyFull"; break;
    case BusState::DrivingFull: state_s = "DrivingFull"; break;
    case BusState::OnCall: state_s = "OnCall"; break;
    }
	return state_s;
}

Bus::Bus(QObject* parent) : QObject(parent), Vehicle()
{
	occupancy = 0;
	on_trip = false;
	number_seats = 45;
	capacity = 70;
	type = 4;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	curr_trip = nullptr;
	last_stop_visited_ = nullptr;
	state_ = BusState::Null;
    flex_vehicle_ = false;
    CC_ = nullptr;
}
Bus::Bus(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_, bool flex_vehicle, Controlcenter* CC, QObject* parent) : QObject(parent),
    Vehicle(id_, type_, length_, route_, odpair_, time_), flex_vehicle_(flex_vehicle), CC_(CC)
{
	occupancy = 0;
	on_trip = false;
	number_seats = 50;
	capacity = 70;
	type = 4;
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	curr_trip = nullptr;
	last_stop_visited_ = nullptr;
	state_ = BusState::Null;
	start_time = time_;
	this->set_curr_link_route_idx(0);
};
Bus::Bus(int bv_id_, Bustype* bty, bool flex_vehicle, Controlcenter* CC, QObject* parent) : QObject(parent), flex_vehicle_(flex_vehicle), CC_(CC)
{
	bus_id = bv_id_;
	type = 4;
	occupancy = 0;
	on_trip = false;
	length = bty->get_length();
	number_seats = bty->get_number_seats();
	capacity = bty->get_capacity();
	random = new (Random);
	if (randseed != 0)
	{
		random->seed(randseed);
	}
	else
	{
		random->randomize();
	}

	curr_trip = nullptr;
	last_stop_visited_ = nullptr;
	state_ = BusState::Null;
	this->set_curr_link_route_idx(0);
};

// ***** Special Bus Functions *****
void Bus::reset ()
{
	occupancy = 0;
	on_trip = true;
	type = 4;
	output_vehicle.clear();
	this->set_curr_link_route_idx(0);
	//set_next_link_route_idx(0);

	//Controlcenter
    if (theParameters->drt && flex_vehicle_)
    {
        disconnect(this, nullptr, nullptr, nullptr); //disconnect all signal slots (will reconnect to control center in Network::init if initially a DRT vehicle)
        CC_ = nullptr; //re-added in Network::init if drt vehicle
        last_stop_visited_ = nullptr;
        state_ = BusState::Null;
        sroute_ids_.clear(); //initial service routes re-added in Network::init
    }

	total_time_spent_in_state.clear();
	total_time_spent_in_state_per_stop.clear();
	time_state_last_entered.clear();
	total_meters_traveled = 0;
	total_empty_meters_traveled = 0;
	total_occupied_meters_traveled = 0;

	init_time = 0.0;
}

void Bus::set_occupancy(int occup)
{
	assert(occup >= 0 && occup <= capacity);
	occupancy = occup;
}

Bus::~Bus()
{
    delete random;
}

Busvehicle_location::~Busvehicle_location()
{}

void Bus::set_bustype_attributes (Bustype* bty) 
// change the fields that are determined by the bustype
{
	type = 4;
	length = bty->get_length();
	number_seats = bty->get_number_seats();
	capacity = bty->get_capacity();
	bus_type = bty;
}

Bustrip* Bus::get_next_trip()
{
    Bustrip* next_trip = nullptr;
    if (curr_trip != nullptr && !curr_trip->driving_roster.empty())
    {
        vector <Start_trip*>::iterator curr_trip_it, next_trip_it; // find the pointer to the current and next trip
        for (vector <Start_trip*>::iterator trip = curr_trip->driving_roster.begin(); trip < curr_trip->driving_roster.end(); trip++)
        {
            if ((*trip)->first == curr_trip)
            {
                curr_trip_it = trip;
                break;
            }
        }
        next_trip_it = curr_trip_it + 1;
        //on_trip = false;  // the bus is available for its next trip
        if (next_trip_it != curr_trip->driving_roster.end()) // there are more trips for this bus
        {
            next_trip = (*next_trip_it)->first;
        }
	}
	return next_trip;
}


Bus* Bus::progressFlexVehicle(Bus* oldbus, double time) 
{
    assert(oldbus->get_occupancy() == 0); //no passengers should be remaining on the bus at this point
    //vid++; // VehicleID is already incremented in Bustrip::advance_next_stop and so many other places, so just let the caller be responsible for it
    Bus* newbus = recycler.newBus(); //want to clone the bus that just finished its trip with its final stop as its 'origin stop'
    assert(newbus->get_state() == BusState::Null);

    //copy over info from the old bus to the new
    newbus->set_flex_vehicle(true);
    newbus->set_bus_id(oldbus->get_bus_id());
    newbus->set_bustype_attributes(oldbus->get_bus_type());
    newbus->set_curr_trip(nullptr); //bus has no trip assigned to it yet
    newbus->set_on_trip(false); //free too be assigned to a trip
	newbus->set_last_stop_visited(oldbus->get_last_stop_visited());
	newbus->set_init_time(oldbus->get_init_time());

	//disconnect and connect oldbus and newbus to/from controlcenter and swap busstates
    BusState oldstate = oldbus->get_state();
	assert(oldstate == BusState::IdleEmpty); //oldbus should have reached their final stop on trip and all passengers should have exited

    Controlcenter* cc = oldbus->get_CC(); //save local pointer to member control center, disconnectVehicle sets member to nullptr
    oldbus->set_state(BusState::Null, time); //set state of old bus to Null to remove it from fleet state map
    cc->disconnectVehicle(oldbus); //disconnect old bus and connect new bus
    cc->connectVehicle(newbus);
    newbus->set_state(oldstate, time); //updates fleet state of CC with this vehicle

	//add newbus to the same service routes as oldbus
    set<int> sroute_ids = oldbus->sroute_ids_; //copy ids of service routes of old bus when it finished its trip, sroute_ids_ will change when removing oldbus from control center service routes
    for (const int& sroute_id : sroute_ids)
    {
        cc->removeVehicleFromServiceRoute(sroute_id, this); //strip oldbus (this) of all of its service routes, both in control center and from its member sroute_ids_
        cc->addVehicleToServiceRoute(sroute_id, newbus); //initiate the newbus with the sroutes of the oldbus
    }
    return newbus;
}

void Bus::assignBusToTrip(Bus* bus, Bustrip* trip)
{
	trip->set_busv(bus); //assign bus to the trip
	bus->set_curr_trip(trip); //assign trip to the bus
	trip->set_bustype(bus->get_bus_type());//set the bus type of the trip (so trip has access to this bustypes dwell time function)
}


void Bus::advance_curr_trip (double time, Eventlist* eventlist) // progresses trip-pointer 
{
    if (theParameters->drt)
    {
        if (flex_vehicle_ && curr_trip->is_flex_trip()) //if the trip that just finished was dynamically scheduled then the controlcenter is in charge of bookkeeping the completed trip and bus for writing outputs
        {
            if (last_stop_visited_->get_dest_node() == nullptr)
            {
                DEBUG_MSG_V("Problem when ending trip in Bus::advance_curr_trip - final stop " << last_stop_visited_->get_id() << " does not have a destination node associated with it. Aborting...");
                abort();
            }
            CC_->addCompletedVehicleTrip(this, curr_trip); //save bus - bustrip pair in control center of last stop 
        }
    }

	Bustrip* next_trip = get_next_trip();
	on_trip = false;  // the bus is available for its next trip

	if (next_trip != nullptr) // there are more trips for this bus
	{
		if (flex_vehicle_ && next_trip->is_flex_trip()) // for dynamically scheduled trip-chains a new bus needs to be dynamically generated and assigned to next trip
		{
			assert(theParameters->drt);
			//vid++ when we have a trip-chain that is continuing, the id is already incremented in Bustrip::advance_next_stop for whatever reason
			Bus* busclone = progressFlexVehicle(this, time); //! - Create the cloned bus, do the disconnect and reconnect with CC
			next_trip->set_starttime(time); // for calculating output as well as the activation check below, this now represents actual starttime and not just the 'expected one'
			assignBusToTrip(busclone, next_trip); //the trip has not started yet however, on_trip set to true in Bustrip::activate
			assert(busclone->get_curr_trip() == next_trip); //should now point to the same once assigned
			next_trip->set_scheduled_for_dispatch(true); // updates the status of the trip a well, should be called after cloned bus is assigned to next trip

		}
		if (next_trip->get_starttime() <= time) // if the bus is already late for the next trip (or dynamically generated trip that starts immediately)
		{
			Busline* line = next_trip->get_line();
			
			// then the trip is activated
			if(!next_trip->is_activated()) // a trip should only be activated (successfully) once
				next_trip->activate(time, line->get_busroute(), line->get_odpair(), eventlist);
			else
			{
				qDebug() << "Warning - Busline::execute ignored double activation of trip " << next_trip->get_id();
			}

			return;
		}
		// if the bus is early for the next trip, then it will be activated at the scheduled time from Busline
	}

	//busvehicle should be unassigned after completing its trip
    if (theParameters->drt)
    {
        if (flex_vehicle_ && curr_trip->is_flex_trip())
        {
			vid++; // still have no clue if this is important but sticking basically to what is done in Network::read_busvehicle
			Bus* busclone = progressFlexVehicle(this, time);

            //initialize busclone as unassigned at the final stop of the trip
            if (last_stop_visited_->get_origin_node() == nullptr) //bus cannot currently be assigned a new trip starting from this stop unless there is an origin node associated with it
            {
                DEBUG_MSG_V("ERROR in Bus::advance_curr_trip - problem re-initializing bus " << busclone->get_bus_id() << " at final stop " << last_stop_visited_->get_id() <<
                                ", Busstop does not have an origin node associated with it. Aborting...");
                abort();
            }

            last_stop_visited_->add_unassigned_bus(busclone, time); // bus is immediately unassigned at the final stop of the trip
            //last_stop_visited_->book_unassigned_bus_arrival(eventlist, busclone, time);

        }
    }
}

void Bus::record_busvehicle_location (Bustrip* trip, Busstop* stop, double time)
{
	output_vehicle.push_back(Busvehicle_location(trip->get_line()->get_id(), trip->get_id() , stop->get_id(), bus_id , stop->get_link_id() , 1, time)); 
	output_vehicle.push_back(Busvehicle_location(trip->get_line()->get_id(), trip->get_id() , stop->get_id(), bus_id , stop->get_link_id() , 0, stop->get_exit_time())); 
}

void Bus::write_output(ostream & out)
{
	for (list <Busvehicle_location>::iterator iter = output_vehicle.begin(); iter!=output_vehicle.end();iter++)
	{
		iter->write(out);
	}
}

BusState Bus::calc_state(const bool assigned_to_trip, const bool bus_exiting_stop, const int occupancy) const
{
	assert(occupancy <= capacity && occupancy >= 0); //occupancy should be a valid load for this vehicle
	
	if (!assigned_to_trip) //if the vehicle is currently not assigned to any passenger carrying or rebalancing trip
		return BusState::OnCall;

	if (!bus_exiting_stop) //if the vehicle has just entered a stop and is standing still
	{
		if (occupancy == 0) //bus is empty
			return BusState::IdleEmpty;
		if (occupancy > 0 && occupancy < capacity) //bus is partially full
			return BusState::IdlePartiallyFull;
		if (occupancy == capacity) //bus is full
			return BusState::IdleFull;
	}
	else //if the vehicle has begun to drive
	{
		if (occupancy == 0) 
			return BusState::DrivingEmpty;
		if (occupancy > 0 && occupancy < capacity)
			return BusState::DrivingPartiallyFull;
		if (occupancy == capacity) 
			return BusState::DrivingFull;
	}

	DEBUG_MSG_V("Null BusState returned by Bus::calc_state");
	return BusState::Null;
}

double Bus::get_time_since_last_in_state(BusState state, double time)
{
	double time_in_state = -1.0;
    if(time_state_last_entered.count(state) != 0)
    {
        time_in_state = time - time_state_last_entered[state];
		if(time_in_state < 0.0)
		{
		    qDebug() << "Warning - calculated negative time in state for bus" << id << "returning zero time in state";
			time_in_state = 0.0;
		}
    }
	return time_in_state;
}

double Bus::get_total_time_in_state(BusState state) const
{
	if (total_time_spent_in_state.count(state) != 0)
		return total_time_spent_in_state.at(state);
	else
		return 0.0;
}

double Bus::get_total_time_empty()
{
	double total_time_empty = 0.0;
	for (const auto& state_time : total_time_spent_in_state)
	{
		switch (state_time.first)
		{
		case BusState::DrivingEmpty:
		case BusState::IdleEmpty:
		case BusState::OnCall:
			total_time_empty += state_time.second;
			break;
		default:
			break;
		}
	}

	return total_time_empty;
}

double Bus::get_total_time_occupied()
{
	double total_time_occupied = 0.0;
	for (const auto& state_time : total_time_spent_in_state)
	{
		switch (state_time.first)
		{
		case BusState::DrivingPartiallyFull:
		case BusState::DrivingFull:
		case BusState::IdlePartiallyFull:
		case BusState::IdleFull:
			total_time_occupied += state_time.second;
			break;
		default:
			break;
		}
	}

	return total_time_occupied;
}

double Bus::get_total_time_driving()
{
	double total_time_driving = 0.0;
	for (const auto& state_time : total_time_spent_in_state)
	{
		switch (state_time.first)
		{
		case BusState::DrivingEmpty:
		case BusState::DrivingPartiallyFull:
		case BusState::DrivingFull:
			total_time_driving += state_time.second;
			break;
		default:
			break;
		}
	}

	return total_time_driving;
}

double Bus::get_total_time_idle()
{
	double total_time_idle = 0.0;
	for (const auto& state_time : total_time_spent_in_state)
	{
		switch (state_time.first)
		{
		case BusState::IdleEmpty:
		case BusState::IdlePartiallyFull:
		case BusState::IdleFull:
			total_time_idle += state_time.second;
			break;
		default:
			break;
		}
	}

	return total_time_idle;
}

double Bus::get_total_time_oncall()
{
	double total_time_oncall = 0.0;
	for (const auto& state_time : total_time_spent_in_state)
	{
		switch (state_time.first)
		{
		case BusState::OnCall:
			total_time_oncall += state_time.second;
			break;
		default:
			break;
		}
	}

	return total_time_oncall;
}

void Bus::update_meters_traveled(int meters_, bool is_empty, double time)
{
	if(time < theParameters->start_pass_generation || time > theParameters->stop_pass_generation) //ignore all meters traveled updates outside of passenger generation period	    
		return;

	total_meters_traveled += meters_;
	if (is_empty)
		total_empty_meters_traveled += meters_;
	else
		total_occupied_meters_traveled += meters_;

	if ( (this->get_curr_trip() != nullptr) && this->get_on_trip() ) // the bus is currently on a trip
	{
		if(get_occupancy() > 0) // if there are passengers onboard, update their meters traveled as well
		{
			bool is_flextrip = this->get_curr_trip()->is_flex_trip();
			map <Busstop*, passengers, ptr_less<Busstop*>> passengers_onboard = this->get_curr_trip()->get_passengers_on_board(); // pass stored by their alighting stop
			for (auto alighting_stop : passengers_onboard)
			{
				vector<Passenger*> pass_with_alighting_stop = alighting_stop.second;
				for (Passenger* pass : pass_with_alighting_stop)
				{
					pass->update_vehicle_meters_traveled(meters_, is_flextrip);
				}
			}
		}
	}
}

double Bus::get_total_vkt() const
{
	return static_cast<double>(total_meters_traveled) / 1000.0;
}
double Bus::get_total_empty_vkt() const
{
	return static_cast<double>(total_empty_meters_traveled) / 1000.0;
}
double Bus::get_total_occupied_vkt() const
{
	return static_cast<double>(total_occupied_meters_traveled) / 1000.0;
}


void Bus::set_state(const BusState newstate, const double time)
{
	if (newstate == BusState::OnCall || newstate == BusState::IdleEmpty || newstate == BusState::DrivingEmpty)
		assert(occupancy == 0); 

	if (state_ != newstate)
	{
		BusState oldstate = state_;
		state_ = newstate;
		assert(theParameters->start_pass_generation < theParameters->stop_pass_generation); // passenger generation interval must be feasible 

		// Only calculate 'time in state' within the passenger generation interval, outside of this is considered warmup and cooldown time for now
	    double timer = 0.0;
		if(time >= theParameters->start_pass_generation && time <= theParameters->stop_pass_generation) // if call within the passenger generation interval, update cumulative time in each busstate
        {
            timer = time;
			double time_in_state = timer - time_state_last_entered[oldstate];
            total_time_spent_in_state[oldstate] += time_in_state; // update cumulative time in each busstate (used for outputs)
            if (oldstate == BusState::OnCall && last_stop_visited_ != nullptr) //store cumulative time in on-call state at each stop
            {
                total_time_spent_in_state_per_stop[last_stop_visited_->get_id()][oldstate] += time_in_state;
				last_stop_visited_->update_total_time_oncall(time_in_state);
            }
        }
        else if (time < theParameters->start_pass_generation) // if call is before pass generation interval, ignore any state update but set timer of newstate to pass start, time_state_last_entered holds this as a 'dummy' time last state entered
		{
		    timer = theParameters->start_pass_generation;
		}
		else if(time > theParameters->stop_pass_generation) // if call is after pass generation interval, set timer to pass stop and update states as normal
		{
		    timer = theParameters->stop_pass_generation;
			double time_in_state = timer - time_state_last_entered[oldstate];
			total_time_spent_in_state[oldstate] += time_in_state; // update cumulative time in each busstate (used for outputs)
			if (oldstate == BusState::OnCall && last_stop_visited_ != nullptr) //store cumulative time in on-call state at each stop
            {
                total_time_spent_in_state_per_stop[last_stop_visited_->get_id()][oldstate] += time_in_state;
				last_stop_visited_->update_total_time_oncall(time_in_state);
            }
		}

		time_state_last_entered[newstate] = timer; // start timer for newstate


#ifdef  _DRTDEBUG
        //print_state();
#endif //  DRTDEBUG_
		emit stateChanged(this, oldstate, state_, time);
	}
}

void Bus::print_state()
{
	qDebug() << "Bus" << bus_id << "is" << BusState_to_QString(state_);
	if(last_stop_visited_)
		qDebug() << "\t" << "- last stop visited: " << last_stop_visited_->get_id() << endl;
	else
		qDebug() << "\t" << "- last stop visited: nullptr" << endl;
	if(curr_trip)
	    qDebug() << "\t" << "- current trip " << curr_trip->get_id() << " status: " <<  BustripStatus_to_QString(curr_trip->get_status()) << endl;
	else
		qDebug() << "\t" << "- current trip: nullptr" << endl;
}

bool Bus::is_idle() const
{
	switch (state_)
	{
	case BusState::IdleEmpty: 
	case BusState::IdleFull: 
	case BusState::IdlePartiallyFull:
		return true;
	default:
		return false;
	}
}

bool Bus::is_driving() const
{
	switch (state_)
	{
	case BusState::DrivingEmpty:
	case BusState::DrivingFull:
	case BusState::DrivingPartiallyFull:
		return true;
	default:
		return false;
	}
}

bool Bus::is_oncall() const
{
	if (state_ == BusState::OnCall)
		return true;
	return false;
}

bool Bus::is_empty() const
{
	return occupancy == 0;
}

bool Bus::is_full() const
{
	return occupancy >= capacity;
}

bool Bus::is_null() const
{
	if (state_ == BusState::Null)
		return true;
	return false;
}

Busstop* Bus::get_next_stop() const
{
	Busstop* next_stop = nullptr;
	if(curr_trip)
		next_stop = (*curr_trip->get_next_stop())->first;

	return next_stop;
}


// ***** Bus-types functions *****
Bustype::Bustype ()
{
}

Bustype::Bustype (int type_id_, string bus_type_name_, double length_, int number_seats_, int capacity_, Dwell_time_function* dwell_time_function_):
	type_id(type_id_), bus_type_name(bus_type_name_), length(length_), number_seats(number_seats_), capacity(capacity_), dwell_time_function(dwell_time_function_)
{

}
Bustype::~Bustype ()
{
}

