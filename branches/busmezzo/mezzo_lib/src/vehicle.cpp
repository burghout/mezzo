#include "vehicle.h"
#include "MMath.h"
#include "controlcenter.h"

Vehicle::Vehicle()
{
 id=-1;
 type=-1;
 route=NULL;
 odpair=NULL;
 start_time=0.0;
 length=0;
 exit_time=0.0;
 entered=false;
 switched=0;
 meters=0;
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
}

void Vehicle::set_curr_link(Link* curr_link_)
{
	curr_link=curr_link_;
}

Link* Vehicle::get_curr_link()
{
	return curr_link;
}

Link* Vehicle::nextlink()
{
	if (entered)
		return route->nextlink(curr_link);
	else
		return route->firstlink();
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
}
Bus::Bus(int id_, int type_, double length_, Route* route_, ODpair* odpair_, double time_, bool flex_vehicle, QObject* parent) : QObject(parent),
    Vehicle(id_, type_, length_, route_, odpair_, time_), flex_vehicle_(flex_vehicle)
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
};
Bus::Bus(int bv_id_, Bustype* bty, bool flex_vehicle, QObject* parent) : QObject(parent), flex_vehicle_(flex_vehicle)
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
};

// ***** Special Bus Functions *****
void Bus::reset ()
{
	occupancy = 0;
	on_trip = true;
	type = 4;
	output_vehicle.clear();

	//Controlcenter
	disconnect(this, 0, 0, 0); //disconnect all signal slots (will reconnect to control center in Network::init)
	last_stop_visited_ = nullptr;
	state_ = BusState::Null;
}

Bus::~Bus()
{}

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

void Bus::advance_curr_trip (double time, Eventlist* eventlist) // progresses trip-pointer 
{
    if (theParameters->drt)
    {
        if (flex_vehicle_)
            DEBUG_MSG("----------Bus " << id << " finishing trip " << curr_trip->get_id() << " at time " << time);

        if (flex_vehicle_ && curr_trip->is_flex_trip()) //if the trip that just finished was dynamically scheduled then the controlcenter is in charge of bookkeeping the completed trip and bus for writing outputs
        {
            if (last_stop_visited_->get_dest_node() == nullptr)
            {
                DEBUG_MSG_V("Problem when ending trip in Bus::advance_curr_trip - final stop " << last_stop_visited_->get_id() << " does not have a destination node associated with it. Aborting...");
                abort();
            }

            double trip_time = curr_trip->get_enter_time() - curr_trip->get_starttime();
            DEBUG_MSG("\t Total trip time: " << trip_time);
            Controlcenter* cc = last_stop_visited_->get_CC(); //TODO: what if multiple control centers are associated with this stop?
            curr_trip->get_line()->remove_flex_trip(curr_trip); //remove from set of uncompleted flex trips in busline, control center takes ownership of the trip for deletion
            cc->addCompletedVehicleTrip(this, curr_trip); //save bus - bustrip pair in control center of last stop 
        }
    }

	vector <Start_trip*>::iterator trip1, next_trip; // find the pointer to the current and next trip
	for (vector <Start_trip*>::iterator trip = curr_trip->driving_roster.begin(); trip < curr_trip->driving_roster.end(); trip++)
	{
		if ((*trip)->first == curr_trip)
		{
			trip1 = trip;
			break;
		}
	}
	next_trip = trip1+1;
	on_trip = false;  // the bus is available for its next trip
	if (next_trip != curr_trip->driving_roster.end()) // there are more trips for this bus
	{
		if ((*next_trip)->first->get_starttime() <= time) // if the bus is already late for the next trip
		{
			Busline* line = (*next_trip)->first->get_line();
			// then the trip is activated
			(*next_trip)->first->activate(time, line->get_busroute(), line->get_odpair(), eventlist);
			return;
		}
		// if the bus is early for the next trip, then it will be activated at the scheduled time from Busline
	}

	//busvehicle should be unassigned at opposing stop after completing its trip, then mimic the initialization process for DRT vehicles from network reader
    if (theParameters->drt)
    {
        if (flex_vehicle_ && curr_trip->is_flex_trip())
        {
            assert(occupancy == 0); //no passengers should be remaining on the bus at this point
            vid++;
            Bus* newbus = recycler.newBus(); //want to clone the bus that just finished its trip with its final stop as its 'origin stop'
            assert(newbus->get_state() == BusState::Null);

            //copy over info from the old bus to the new
            newbus->set_flex_vehicle(true);
            newbus->set_bus_id(id);
            newbus->set_bustype_attributes(bus_type);
            newbus->set_curr_trip(nullptr); //bus has no trip assigned to it yet
            newbus->set_on_trip(false);

            Controlcenter* cc = last_stop_visited_->get_CC();
            this->set_state(BusState::Null, time); //set state of old bus to Null to remove it from fleet state map
            cc->disconnectVehicle(this); //disconnect old bus and connect new bus
            cc->connectVehicle(newbus);

            for (const int& sroute_id : sroute_ids_)
            {
                newbus->add_sroute_id(sroute_id);
                cc->removeVehicleFromServiceRoute(sroute_id, this);
                cc->addVehicleToServiceRoute(sroute_id, newbus);
            }

            //if an opposing stop exists, initialize newbus at the opposing stop associated with last stop visited by oldbus with expected arrival time as soon as possible
            if (last_stop_visited_->get_opposing_stop() != nullptr)
            {
                Busstop* opposing_stop = last_stop_visited_->get_opposing_stop();
                if (opposing_stop->get_origin_node() == nullptr)
                {
                    DEBUG_MSG_V("Problem when turning bus in Bus::advance_curr_trip - opposing stop " << opposing_stop->get_id() << " to stop " << last_stop_visited_->get_id() << " does not have an origin node associated with it. Aborting..."); //opposing stop should have an origin node associated with it
                    abort();
                }
                opposing_stop->add_unassigned_bus_arrival(eventlist, newbus, time);
            }
            else //bus stays at the current stop (but cannot currently go anywhere until it has turned around)
            {
                last_stop_visited_->add_unassigned_bus_arrival(eventlist, newbus, time);
            }
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

void Bus::set_state(const BusState newstate, const double time)
{
	if (newstate == BusState::OnCall || newstate == BusState::IdleEmpty || newstate == BusState::DrivingEmpty)
		assert(occupancy == 0); 

	if (state_ != newstate)
	{
		BusState oldstate = state_;
		state_ = newstate;
		print_state();
		emit stateChanged(this, oldstate, state_, time);
	}
}

void Bus::print_state()
{
	cout << endl << "Bus " << bus_id << " is ";
	switch (state_)
	{
	case BusState::OnCall:
		cout << "OnCall";
		break;
	case BusState::IdleEmpty:
		cout << "IdleEmpty";
		break;
	case BusState::IdlePartiallyFull:
		cout << "IdlePartiallyFull";
		break;
	case BusState::IdleFull:
		cout << "IdleFull";
		break;
	case BusState::DrivingEmpty:
		cout << "DrivingEmpty";
		break;
	case BusState::DrivingPartiallyFull:
		cout << "DrivingPartiallyFull";
		break;
	case BusState::DrivingFull:
		cout << "DrivingFull";
		break;
	case BusState::Null:
		cout << "NullBusState";
		break;
	default:
		DEBUG_MSG_V("Something went very wrong");
		abort();
	}
	cout << endl;
	cout << "\t" << "- last stop visited: " << last_stop_visited_->get_id() << endl;
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

