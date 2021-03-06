#include "od.h"
#include "network.h"
#include <math.h>
#include "MMath.h"
/*
  TO BE ADDED:
  Change the server in ODaction to specific OD server that is created with proper params

*/



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


bool compare_route_cost(Route* r1, Route* r2)
{
	return (r1->cost(0.0) < r2->cost(0.0));
}


ODaction::ODaction(ODpair* odpair_):odpair(odpair_)
{
#ifdef _DEBUG_OD
	cout << "ODaction::ODaction odpair->get_rate() : " << odpair->get_rate() << endl;
#endif //_DEBUG_OD
	double rate = odpair->get_rate();
	double mu = 0.0;
	active = false;
	if (rate >= 1)
	{
		mu=3600.0/(rate);
		active = true;
	}
	server=new ODServer(1000, 2, mu, theParameters->odserver_sigma);

}

ODaction::~ODaction()
{
	delete server;
}

void ODaction::reset(double rate_)
{
	server->reset();
	if (rate_ < 1)
	{
		active = false;
	}
	else
	{
		server->set_rate((3600/rate_),theParameters->odserver_sigma);
		active=true;
	}
}

bool ODaction::execute(Eventlist* eventlist, double time)
{
  if (active)
  {
	  bool ok=false;
	#ifdef _DEBUG_OD
  			cout << "ODaction @ " << time;
	#endif //_DEBUG_OD
	  // select route(origin, destination) and make a new vehicle
	  Route* route=odpair->select_route(time);
	  Vtype* vehtype=(odpair->vehtypes() )->random_vtype();
	  vid++;
	   // 2002_12_18 recycling the vehicles
	  Vehicle* veh=recycler.newVehicle(); // get a _normal_ vehicle
	  veh->init(vid,vehtype->id, vehtype->length,route,odpair,time);  
	  if ( (odpair->get_origin())->insert_veh(veh,time)) // insert vehicle in the input queue
  		ok=true;
	  else
  		{
  			ok=false; // if there's no room on the inputqueue (should never happen) we just drop the vehicle.
  			cout << "OD action:: dropped a vehicle " << veh->get_id() << endl;
  			//delete veh; // so we're not creating memory leaks...
  			recycler.addVehicle(veh);
  		}	
	  // get next time from now on
	  double new_time=server->next(time);
	#ifdef _DEBUG_OD
 		 cout << " ...next ODaction @ " << new_time << endl;
	#endif //_DEBUG_OD
	  // book myself in eventlist
	  eventlist->add_event(new_time,this);
	  return ok;
  }
  else
  {
	  double temp = odpair->get_random()->urandom (1, 100);
	  eventlist->add_event(time + theParameters->update_interval_routes+temp,this);
	  return true;
  }
 }

void ODaction::book_later(Eventlist* eventlist, double time)
{
	eventlist->add_event(time, this);
}

Origin* ODpair::get_origin()
{
 	return origin;
}

Destination* ODpair::get_destination()
{
	return destination;
}

vector <rateval> ODpair::get_route_rates()
{
 	double totalU=0.0;    // total utility  = sum of all utilities
	double U=0.0;    //one utility
	double Urate=0.0; // rate for route
	vector <rateval> route_rates;
	for (vector<Route*>::iterator iter=routes.begin();iter<routes.end();iter++)
	{
	 	totalU+=1/((*iter)->cost());
	}
	for (vector<Route*>::iterator iter1=routes.begin();iter1<routes.end();iter1++)
	{
	 	U=1/((*iter1)->cost());
		Urate=rate*(U/totalU);
		
		route_rates.push_back( rateval((*iter1),Urate ) ) ;
	}
	return route_rates;
}


void ODpair::add_route(Route* route)
{
	routes.insert(routes.end(),route);
}

 Route* ODpair::get_route(int id)
 { 
   vector <Route*>::iterator rptr=find_if (routes.begin(),routes.end(), compare <Route> (id));
//   Route** rptr=find_if (routes.begin(),routes.end(), compare <Route> (id));
   if (rptr !=routes.end())
   		return *rptr;
   else
   		return NULL;
 }

/**
* a heuristic to delete spurious route
*/
vector <Route*> ODpair::delete_spurious_routes(double time)
// finds routes that are more than 
{

	unsigned int maxroutes=10;
	double threshold =0.0;
	vector <Route*> thrown;
	string reason ="";
	vector <Route*>::iterator iter1=routes.begin();
	vector <Route*>::iterator shortest_route=routes.begin();
	double min_cost = (*iter1)->cost(time);
    for ( ; iter1<routes.end(); iter1++)
	{
	  if ((*iter1)->cost(time) < min_cost)
	  {
		  min_cost = (*iter1)->cost(time);
		  shortest_route = iter1;
	  }
	}
	
	if (rate < theParameters->small_od_rate ) // if the rate is small there should be no route choice
	{
		threshold = 1.5; // all other routes will be deleted
		maxroutes = 5;
		reason = " small OD ";
	}
	else
	{
		
		double r = (rate/theParameters->small_od_rate); // even for larger OD pairs, limit nr of routes by rate
		//if (routes.size() > r)
	//		threshold = 1.50; // only good routes 
	//	else
		maxroutes = static_cast <int> (r+1);	
		threshold = theParameters->max_rel_route_cost;
		reason = " large cost ";
	}

	iter1=routes.begin();
    for ( ; iter1<routes.end(); )
	{
	  if ((*iter1)->cost(time) > (threshold*min_cost))
	  {
		  // remove from route choice set
		  
		  cout << " erased route " << (*iter1)->get_id() << " from route choice set for OD pair ("
			  << odids().first << "," << odids().second << ") because: " << reason << ", cost: "<< (*iter1)->cost(time) << 
			  ", mincost: " << min_cost << ", rate: " << rate << endl;
		  thrown.push_back((*iter1));
		  iter1=routes.erase(iter1);
	  }
	  else iter1++;
	}
	// delete if there are too many routes
	
	sort (routes.begin(), routes.end(), compare_route_cost);
	cout << "cost of sorted routes " << endl;
	reason = " Max nr routes reached ";
	vector <Route*>::iterator r_iter = routes.begin();
    for ( ; r_iter < routes.end(); r_iter++)
	{
		cout << " route " << (*r_iter)->get_id() << " costs " << (*r_iter)->cost(0.0) << endl;
	}	

	if (maxroutes < routes.size())
	{	
		vector<Route*>::iterator r=routes.end();
		for (unsigned int k = routes.size(); k > maxroutes; k--)
		{
			r=routes.end();
			r--;
			cout << " erased route " << (*r)->get_id() << " from route choice set for OD pair ("
			  << odids().first << "," << odids().second << ") because: " << reason << ", cost: "<< (*r)->cost(time) << 
			  ", mincost: " << min_cost << ", rate: " << rate << endl;
			 thrown.push_back((*r));
			 routes.erase(r);
		}

	}
	return thrown;
}


Route* ODpair::select_route(double time)          // to be changed
/* The naive route choice that's implemented is based on the relative cost of a route
	which is defined in the Route class. Based on the utility of a route relative to the
	sum of utilities of all routes, the route in question gets a probability for being chosen
   In this implementation the utility=-ln(cost). (in LOGIT formulation)
*/

{
	vector <double> cost;
	int n=routes.size();
   if (n==0)
   {
      cout << " No Route from Origin " << this->origin->get_id();
      cout << " to destination " << this->destination->get_id() << endl;
   }
  assert (routes.size() > 0); // so the program breaks here if anything goes wrong.
  if (n==1)
		return routes.front();
#ifdef _DEBUG_OD
	cout << " ODpair::select_route(). nr routes: " << n << endl;
#endif //_DEBUG_OD


	totalU=0.0;    // total utility  = sum of all utilities
	subU=0.0;    //subsum 

	utilities.clear(); // empty the utilities vector
	for (vector<Route*>::iterator iter=routes.begin();iter<routes.end();iter++)
	{
	  utilities.push_back((*iter)->utility(time));	
	  totalU+=utilities.back();
	}


	// make the choice!!! STILL RANDOM; HOW TO MAKE IT DETERMINISTIC? (set the seed to the route choice)
	double choice=random->urandom(); // draws random number from 0..1
	// METHOD: All routes have a 'fraction of the pie' equal to their relative utility
	int i= 0;
	for (vector<Route*>::iterator iter1=routes.begin();iter1<routes.end();iter1++, i++)
	{
	 	subU+=utilities[i];
	#ifdef _DEBUG_OD
       cout << " subcost: " << subU << "; choice: " << choice<< endl;
	#endif //_DEBUG_OD	 		 	
	 	if (( (subU) / totalU) >= choice) // 
	 	{
		    return (*iter1);
	 	}	
	}

	// if it exits the loop without selecting a route
	cout << "TROUBLE: No Route selected! Returning the first in list" <<  endl;
	return routes.front(); // to be sure that in any case a route is returned
}
 	
odval ODpair::odids ()
{
	return odval(origin->get_id(), destination->get_id());
}

ODpair::ODpair(Origin* origin_, Destination* destination_, double rate_, Vtypes* vtypes_)
	:origin(origin_), destination(destination_), rate(rate_), vtypes (vtypes_)
{
 	odaction=new ODaction(this);
 	random=new Random();
	start_rate=rate;
#ifndef _DETERMINISTIC_ROUTE_CHOICE
 	if (randseed != 0)
	   random->seed(randseed);
	else
		random->randomize();	
#else
	random->seed(42);
#endif // _DETERMINISTIC_ROUTE_CHOICE
 	const int nr_fields=9;
	string names[nr_fields]={"origin_id","dest_id","veh_id","start_time","end_time","travel_time", "mileage","route_id","switched_route"};
    vector <string> fields(names,names+nr_fields);
	grid=new Grid(nr_fields,fields);	
	oldgrid =new Grid(nr_fields,fields);
}

ODpair::ODpair(): /*id (-1),*/ odaction (NULL), origin (NULL), destination (NULL),
                  rate (-1), start_rate(-1), random (NULL), grid(NULL), oldgrid(NULL)
{

}

ODpair::~ODpair()
{
	if (odaction)
		delete odaction;
	if (random)
		delete random;	
}

void ODpair::reset()
{
	rate=start_rate;
#ifndef _DETERMINISTIC_ROUTE_CHOICE
 	if (randseed != 0)
	   random->seed(randseed);
	else
		random->randomize();	
#else
	random->seed(42);
#endif
	odaction->reset(rate);
	if (oldgrid)
		*oldgrid = *grid;
	grid->reset();
}
 	
bool ODpair::execute(Eventlist* eventlist, double time)
{
	if (rate > 0)
	{
		double temp = 3600 / (rate); // the optimal time to start generating.
		double delay =  (this->random->urandom(1.2, temp));
		odaction->book_later (eventlist, time+delay);
		return true;
	}
	else
		return odaction->execute(eventlist, time);

}

double ODpair::get_rate()
{
	return rate;
}


void ODpair::report (list <double>   collector)
{
	collector.insert(collector.begin(),destination->get_id());
	collector.insert(collector.begin(),origin->get_id());							
	grid->insert_row(collector);
}

bool ODpair::writesummary(ostream& out)
{
	out << origin->get_id() << "\t" << destination->get_id() << "\t" << routes.size() << "\t";
	out << grid->size() << "\t" << grid->sum(6) << "\t" << grid->sum(7) << endl;
  return true;
}

bool  ODpair::writefieldnames(ostream& out)
{
	vector <string> fnames = grid->get_fieldnames();
	for (vector<string>::iterator iter=fnames.begin(); iter < fnames.end(); iter++)
	{
		out << (*iter) << '\t';
	}
	out << endl;
	return true;
}

bool ODpair::less_than(ODpair* od)
	 // returns true if origin_id of this odpair is less than origin_id of the "od" parameter, or
	 // if the origins are equal, if the destination_id is less than that of the "od" parameter provided
 {
	 int id_or1= origin->get_id();
	 int id_or2= od->get_origin()->get_id();
	 if (id_or1 > id_or2 )
		 return false;
	 else
	 {
		if (id_or1 < id_or2)
			return true;
		else // same origins
		{
			if (destination ->get_id() >= od->get_destination()->get_id())
				return false;
			else
				return true;
		}
	 }
 }

Route* ODpair::filteredRoute(int index)
{
	Route* theroute=routes[index];
	filtered_routes_.push_back(theroute);
	routes.erase(routes.begin()+index);
	return theroute;
}
