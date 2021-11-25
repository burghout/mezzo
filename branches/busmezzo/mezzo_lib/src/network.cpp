

#include "gettime.h"
#include <cassert>
#include <string>
#include <algorithm>
#include <sstream>
#include <set>
#include <math.h>
#include <utility>



#include "network.h"
#include "od.h"


//using namespace std;

// initialise the global variables and objects
double drt_first_rep_max_headway = 0.0;
double drt_first_rep_waiting_utility = 10.0; //default is to evaluate waiting utility for drt service positively in the first rep
int drt_min_occupancy = 0;
double drt_first_rebalancing_time = 0.0;

bool fwf_wip::day2day_drt_no_rti = false;

bool fwf_wip::write_all_pass_experiences = false; //set manually
bool fwf_wip::randomize_pass_arrivals = true; //set manually
bool fwf_wip::day2day_no_convergence_criterium = false; //set manually
bool fwf_wip::drt_enforce_strict_boarding = false; //set manually

bool PARTC::drottningholm_case = false;
Busstop* PARTC::transfer_stop = nullptr;

long int randseed=0;
int vid=0;
int pid=0;
static long pathid=0;
VehicleRecycler recycler;    // Global vehicle recycler
double time_alpha=0.2; // smoothing factor for the output link times (uses hist_time & avg_time),
// 1 = only new times, 0= only old times.

Parameters* theParameters = new Parameters();

// compare is a helper functor (function object) to be used in STL algorithms as
// predicate (for instance in find_if(start_iterator,stop_iterator, Predicate))
// class T is the 'thing' Object of which the id is compared. get_id() should be defined
// for T

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



struct compareod
{
    compareod(odval val_):val(val_) {}
    bool operator () (ODpair* thing)

    {
        return (thing->odids()==val);
    }

    odval val;
};


template<class T>
struct equalmembers
        // Tests if the two containers of class T have equal members
        // requires class T to have a function bool equals(T);
{
    equalmembers(T base_):base(base_) {}
    bool operator () (T* thing)

    {
        return (thing->equals(base));
    }

    T base;
};



struct compareroute
{
    compareroute(odval odvalue_):odvalue(odvalue_) {}
    bool operator () (Route* route)
    {
        return (route->check(odvalue.first, odvalue.second)==true);
    }
    odval odvalue;
};

bool route_less_than (Route* r1, Route* r2)
// returns true if origin_id of route 1 < origin_id of route 2, or, if they are equal,
// if destination_id1 < destination_id2
{
    return r1->less_than(r2);
}

bool od_less_than (ODpair* od1, ODpair* od2)
{
    return od1->less_than(od2);
}

bool route_id_less_than (Route* r1, Route* r2)
{
    return ( r1->get_id() < r2->get_id() );
}

template <typename T>
string To_String(T val)
{
    stringstream stream;
    stream << val;
    return stream.str();
}

namespace fwf_outputs {
    bool finished_trip_within_pass_generation_interval(Passenger* pass)
    {
        if (pass->get_end_time() <= 0)
            return false;
        if (pass->get_end_time() > theParameters->stop_pass_generation)
            return false;
        if (pass->get_start_time() < theParameters->start_pass_generation)
            return false;
        return true;
    }

    //!< @brief write out vkt results for each replication, should be one row per replication @note Check Link::is_dummylink definition whether these are skipped or not for the current case
    void writeVKT(ostream& out, const FWF_vehdata& fix_vehdata, const FWF_vehdata& drt_vehdata)
    {
        out << drt_vehdata.total_vkt << "\t" << drt_vehdata.total_occupied_vkt << "\t" << drt_vehdata.total_empty_vkt << "\t"
            << fix_vehdata.total_vkt << "\t" << fix_vehdata.total_occupied_vkt << "\t" << fix_vehdata.total_empty_vkt << endl;

        /*out << "\nDRT VKT                     : " << drt_vehdata.total_vkt;
    out << "\nDRT occupied VKT            : " << drt_vehdata.total_occupied_vkt;
    out << "\nDRT empty VKT               : " << drt_vehdata.total_empty_vkt;
    out << "\nDRT occupied time           : " << drt_vehdata.total_occupied_time;
    out << "\nDRT empty time              : " << drt_vehdata.total_empty_time;
    out << "\nDRT driving time            : " << drt_vehdata.total_driving_time;
    out << "\nDRT idle time               : " << drt_vehdata.total_idle_time;
    out << "\nDRT oncall time             : " << drt_vehdata.total_oncall_time;*/
    }

    //!< @brief write out time and vkt spent in different states for a DRT vehicle for e.g. analysis of distributions. Corresponds to one row of "o_fwf_drtvehicle_states.dat"
    void writeDRTVehicleState_row(ostream& out, int bus_id, double init_time, const FWF_vehdata& drt_vehdata)
    {
        out << std::fixed;
        out.precision(5);
        out << bus_id << "\t"
            << init_time << "\t"
            << drt_vehdata.total_vkt << "\t" << drt_vehdata.total_occupied_vkt << "\t" << drt_vehdata.total_empty_vkt << "\t"
            << drt_vehdata.total_occupied_time << "\t" << drt_vehdata.total_empty_time << "\t"
            << drt_vehdata.total_driving_time << "\t" << drt_vehdata.total_idle_time << "\t" << drt_vehdata.total_oncall_time << endl;
    }
    void writeDRTVehicleState_header(ostream& out)
    {
        out << "Bus_ID" << '\t'
            << "Init_Time" << '\t'
            << "Total_VKT" << '\t'
            << "Total_Occ_VKT" << '\t'
            << "Total_Emp_VKT" << '\t'
            << "Total_Occ_Time" << '\t'
            << "Total_Emp_Time" << '\t'
            << "Total_Driving_Time" << '\t'
            << "Total_Idle_Time" << '\t'
            << "Total_OnCall_Time" << endl;
    }

    //!< @brief write out total time any vehicle has spent in any state at each stop. Corresponds to one row of "o_time_in_state_at_stop.dat" @todo currently only time spent in oncall state is output
    void writeDRTVehicleStateAtStop_row(ostream& out, Busstop* stop)
    {
        out << std::fixed;
        out.precision(5);
        out << stop->get_id() << "\t"
            << stop->get_total_time_oncall() << endl;
    }
    void writeDRTVehicleStateAtStop_header(ostream& out)
    {
        out << "Busstop_ID" << '\t'
            << "Total_OnCall_Time" << endl;
    }
}

FWF_vehdata operator+(const FWF_vehdata& lhs, const FWF_vehdata& rhs)
{
    FWF_vehdata sum;
    sum.total_vkt = lhs.total_vkt + rhs.total_vkt;
    sum.total_occupied_vkt = lhs.total_occupied_vkt + rhs.total_occupied_vkt;
    sum.total_empty_vkt = lhs.total_empty_vkt + rhs.total_empty_vkt;

    sum.total_occupied_time = lhs.total_occupied_time + rhs.total_occupied_time;
    sum.total_empty_time = lhs.total_empty_time + rhs.total_empty_time;

    sum.total_driving_time = lhs.total_driving_time + rhs.total_driving_time;
    sum.total_idle_time = lhs.total_idle_time + rhs.total_idle_time;
    sum.total_oncall_time = lhs.total_oncall_time + rhs.total_oncall_time;

    sum.num_vehdata = lhs.num_vehdata + rhs.num_vehdata;

    return sum;
}

//FWF_tripdata operator+(const FWF_tripdata& lhs, const FWF_tripdata& rhs)
//{
//    FWF_tripdata sum;
//    sum.total_trips = lhs.total_trips + rhs.total_trips;
//    sum.total_empty_trips = lhs.total_empty_trips + rhs.total_empty_trips;
//    sum.total_pass_carrying_trips = lhs.total_pass_carrying_trips + rhs.total_pass_carrying_trips;
//
//    sum.total_pass_boarding = lhs.total_pass_boarding + rhs.total_pass_boarding;
//    sum.total_pass_alighting = lhs.total_pass_alighting + rhs.total_pass_alighting;
//    // @todo combined mean and combined stdev calculations
//    // sum.avg_boarding_per_trip = lhs.avg_boarding_per_trip + rhs.avg_boarding_per_trip;
//
//    return sum;
//}

// End of helper functions

Network::Network()
{
#ifndef _NO_GUI
    drawing=new Drawing();
#endif //_NO_GUI
    //eventhandle=new Eventhandle(*drawing);
#ifdef _PVM
    communicator=new PVM ("Mezzo", MSG_TAG_ZOOM_MITSIM, MSG_TAG_ZOOM_MEZZO);
#endif // _NO_PVM

#ifdef _VISSIMCOM
    communicator= new VISSIMCOM("vissimconf.dat");
#endif //_VISSIMCOM
    linkinfo=new LinkTimeInfo();
    eventlist=new Eventlist;
    no_ass_links=0;
    Random::create(1);
}

Network::~Network()
{
    delete linkinfo;
    delete eventlist;

#ifdef _MIME
    delete communicator;
#endif //_MIME
    // OLD way of cleaning up stuff
    /*
    for (map <int, Origin*>::iterator iter=originmap.begin();iter!=originmap.end();)
    {
        delete (iter->second); // calls automatically destructor
        iter=originmap.erase(iter);
    }
    for (map <int, Destination*>::iterator iter1=destinationmap.begin();iter1!=destinationmap.end();)
    {
        delete (iter1->second); // calls automatically destructor
        iter1=destinationmap.erase(iter1);
    }
    for (map <int, Junction*>::iterator iter2=junctionmap.begin();iter2!=junctionmap.end();)
    {
        delete (iter2->second); // calls automatically destructor
        iter2=junctionmap.erase(iter2);
    }
    for (map <int,Node*>::iterator iter3=nodemap.begin();iter3!=nodemap.end();)
    {
        iter3=nodemap.erase(iter3);
    }
    */
    // New way
    for (map <int, Origin*>::iterator iter=originmap.begin();iter!=originmap.end();++iter)
    {
        if (nullptr != iter->second) {
            delete (iter->second); // calls automatically destructor
        }
    }
    originmap.clear();
    for (map <int, Destination*>::iterator iter1=destinationmap.begin();iter1!=destinationmap.end();++iter1)
    {
        if (nullptr != iter1->second) {
            delete (iter1->second); // calls automatically destructor
        }
    }
    destinationmap.clear();
    for (map <int, Junction*>::iterator iter2=junctionmap.begin();iter2!=junctionmap.end();++iter2)
    {
        if (nullptr!=iter2->second) {
            delete (iter2->second); // calls automatically destructor
        }
    }
    junctionmap.clear();
    nodemap.clear();
    /** OLD WAY
    for (map <int,Link*>::iterator iter4=linkmap.begin();iter4!=linkmap.end();)
    {
        delete (iter4->second); // calls automatically destructor
        iter4=linkmap.erase(iter4);
    }
    for (vector <Incident*>::iterator iter5=incidents.begin();iter5<incidents.end();)
    {
        delete (*iter5); // calls automatically destructor
        iter5=incidents.erase(iter5);
    }
*/
    for (map <int,Link*>::iterator iter4=linkmap.begin();iter4!=linkmap.end();++iter4)
    {
        if (nullptr != iter4->second) {
            delete (iter4->second); // calls automatically destructor
        }
    }
    linkmap.clear();
    for (vector <Incident*>::iterator iter5=incidents.begin();iter5<incidents.end();++iter5)
    {
        if (nullptr != *iter5) {
            delete (*iter5); // calls automatically destructor
        }
    }
    incidents.clear();
    // for now keep OD pairs in vector
    /****** OLD
    for (vector <ODpair*>::iterator iter6=odpairs.begin();iter6!=odpairs.end();)
    {
        delete (*iter6);
        iter6=odpairs.erase(iter6);
    }
    for (multimap <odval, Route*>::iterator iter7=routemap.begin();iter7!=routemap.end();)
    {
        delete (iter7->second); // calls automatically destructor
        iter7=routemap.erase(iter7);
    }
    for (map <int, Sdfunc*>::iterator iter8=sdfuncmap.begin();iter8!=sdfuncmap.end();)
    {
        delete (iter8->second); // calls automatically destructor
        iter8=sdfuncmap.erase(iter8);
    }
    for (map <int, Server*>::iterator iter9=servermap.begin();iter9!=servermap.end();)
    {
        delete (iter9->second); // calls automatically destructor
        iter9=servermap.erase(iter9);
    }
    ***********/
    for (vector <ODpair*>::iterator iter6=odpairs.begin();iter6!=odpairs.end();++iter6)
    {
        if (nullptr != *iter6) {
            delete (*iter6);
        }
    }
    odpairs.clear();
    for (multimap <odval, Route*>::iterator iter7=routemap.begin();iter7!=routemap.end();++iter7)
    {
        if (nullptr != iter7->second) {
            delete (iter7->second); // calls automatically destructor
        }
    }
    routemap.clear();
    for (map <int, Sdfunc*>::iterator iter8=sdfuncmap.begin();iter8!=sdfuncmap.end();++iter8)
    {
        if (nullptr != iter8->second) {
            delete (iter8->second); // calls automatically destructor
        }
    }
    sdfuncmap.clear();
    for (map <int, Server*>::iterator iter9=servermap.begin();iter9!=servermap.end();++iter9)
    {
        if (nullptr != iter9->second) {
            delete (iter9->second); // calls automatically destructor
        }
    }
    servermap.clear();

    /******* OLD
    for (map <int, Turning*>::iterator iter10=turningmap.begin();iter10!=turningmap.end();)
    {
        delete (iter10->second); // calls automatically destructor
        iter10=turningmap.erase(iter10);
    }
    for (vector <Vtype*>::iterator iter11=vehtypes.vtypes.begin();iter11<vehtypes.vtypes.end();)
    {
        delete (*iter11); // calls automatically destructor
        iter11=vehtypes.vtypes.erase(iter11);
    }
    for (vector <TurnPenalty*>::iterator iter12= turnpenalties.begin(); iter12 < turnpenalties.end();)
    {
        delete (*iter12);
        iter12=turnpenalties.erase(iter12);
    }
    ***********/
    for (map <int, Turning*>::iterator iter10=turningmap.begin();iter10!=turningmap.end();++iter10)
    {
        if (nullptr != iter10->second) {
            delete (iter10->second); // calls automatically destructor
        }
    }
    turningmap.clear();
    for (vector <Vtype*>::iterator iter11=vehtypes.vtypes.begin();iter11<vehtypes.vtypes.end();++iter11)
    {
        if (nullptr != *iter11) {
            delete (*iter11); // calls automatically destructor
        }
    }
    vehtypes.vtypes.clear();
    for (vector <TurnPenalty*>::iterator iter12= turnpenalties.begin(); iter12 < turnpenalties.end();++iter12)
    {
        if (nullptr != *iter12) {
            delete (*iter12);
        }
    }
    turnpenalties.clear();

    for (auto& controlcenter : ccmap)
    {
        delete controlcenter.second;
    }
    ccmap.clear();
    // TODO: check if Stage SignalPlan and SignalControl need to be cleaned up now (in Trunk they are cleaned up here)
}

int Network::reset()
{
    time=0.0;
    vid = 0;
    // reset eventlist
    eventlist->reset();

    // reset all the network objects
    //Origins
    for (map <int, Origin*>::iterator iter1=originmap.begin();iter1!=originmap.end();iter1++)
    {
        (iter1->second)->reset();
    }
    //Destinations
    for (map <int, Destination*>::iterator iter2=destinationmap.begin();iter2!=destinationmap.end();iter2++)
    {
        (iter2->second)->reset();
    }
    //Junctions
    for (map <int,Junction*>::iterator iter3=junctionmap.begin();iter3!=junctionmap.end();iter3++)
    {
        (iter3->second)->reset();
    }
    //Links
    for (map <int, Link*>::iterator iter4=linkmap.begin();iter4!=linkmap.end();iter4++)
    {
        (iter4->second)->reset();
    }
    //Routes
    for (multimap <odval, Route*>::iterator iter5=routemap.begin();iter5!=routemap.end();iter5++)
    {
        (iter5->second)->reset();
    }
    //OD pairs
    for (vector <ODpair*>::iterator iter6=odpairs.begin();iter6!=odpairs.end();iter6++)
    {
        (*iter6)->reset();
    }
    // OD Matrix rates : re-book all MatrixActions
    odmatrix.reset(eventlist,&odpairs);
    // turnings
    for (map<int,Turning*>::iterator iter7=turningmap.begin(); iter7!=turningmap.end(); iter7++)
    {
        (iter7->second)->reset();
    }

    for (map <int, Server*>::iterator sv_iter=servermap.begin(); sv_iter!=servermap.end(); sv_iter++)
    {
        (*sv_iter).second->reset();
    }
    for (vector <ChangeRateAction*>::iterator cr_iter=changerateactions.begin(); cr_iter != changerateactions.end(); cr_iter++)
    {
        (*cr_iter)->reset();
    }
    //traffic signals
    for (vector <SignalControl*>::iterator sc_iter = signalcontrols.begin(); sc_iter != signalcontrols.end(); sc_iter++)
    {
        (*sc_iter)->reset();
    }

    // vehicle types
    vehtypes.initialize();

    // buslines
    for (vector<Busline*>::iterator lines_iter = buslines.begin(); lines_iter < buslines.end(); lines_iter++)
    {
        (*lines_iter)->reset();
    }

    // bustrips
    for (vector<Bustrip*>::iterator trips_iter = bustrips.begin(); trips_iter < bustrips.end(); trips_iter++)
    {
        (*trips_iter)->reset();
    }

    // busstops
    for (vector<Busstop*>::iterator stops_iter = busstops.begin(); stops_iter < busstops.end(); stops_iter++)
    {
        (*stops_iter)->reset();
    }

    // busroutes
    for (vector<Busroute*>::iterator route_iter = busroutes.begin(); route_iter < busroutes.end(); route_iter++)
    {
        (*route_iter)->reset();
    }

    // ODstops, passengers, paths
    for (vector<ODstops*>::iterator odstops_iter = odstops.begin(); odstops_iter < odstops.end(); odstops_iter++)
    {
        (*odstops_iter)->reset();
    }

    // busvehicles
    for (vector<Bus*>::iterator bus_iter = busvehicles.begin(); bus_iter < busvehicles.end(); bus_iter++)
    {
        (*bus_iter)->reset();
    }

    //controlcenters (and all their initial drt vehicles)
    for (pair<const int, Controlcenter*>& controlcenter : ccmap)
    {
        controlcenter.second->reset();
    }

    //TO DO

    // incidents

    // all the Hybrid functions: BoundaryIn, BoundaryOut etc.

    // AND FINALLY Init the next run
    init();

    return runtime;
}



void Network::delete_passengers()
{
    day = 1;
    day2day->update_day(1);
    for (vector<ODstops*>::iterator odstops_iter = odstops.begin(); odstops_iter < odstops.end(); odstops_iter++)
    {
        (*odstops_iter)->delete_passengers();
    }
}

unsigned int Network::count_generated_passengers()
{
    unsigned int count = 0;
    for (const auto& odstop : odstops)
    {
        count += static_cast<unsigned int>(odstop->get_passengers_during_simulation().size());
    }

    return count;
}

vector<Passenger*> Network::get_all_generated_passengers() 
{
    vector<Passenger*> passengers;
    for (const auto& odstop : odstops)
    {
        vector<Passenger*> odpass = odstop->get_passengers_during_simulation();
        passengers.insert(passengers.end(), odpass.begin(), odpass.end());
    }

    return passengers;
}

void Network::end_of_simulation()
{
    for (map<int,Link*>::iterator iter=linkmap.begin();iter != linkmap.end();iter++)
    {
        (*iter).second->end_of_simulation();
    }
}


multimap<odval, Route*>::iterator Network::find_route (int id, odval val)
{
    multimap<odval, Route*>::iterator upper;
    multimap<odval, Route*>::iterator lower;
    multimap<odval, Route*>::iterator r_iter;
    lower = routemap.lower_bound(val);
    upper = routemap.upper_bound(val);
    for (r_iter=lower; r_iter!=upper; r_iter++)
    {
        if ((*r_iter).second->get_id() == id)
            return r_iter;
    }
    return routemap.end(); // in case none found
}

bool Network::exists_route (int id, odval val)
{
    return find_route(id,val) != routemap.end();
}

bool Network::exists_same_route (Route* route)
{
    multimap<odval, Route*>::iterator upper;
    multimap<odval, Route*>::iterator lower;
    multimap<odval, Route*>::iterator r_iter;
    odval val = route->get_oid_did();
    lower = routemap.lower_bound(val);
    upper = routemap.upper_bound(val);
    for (r_iter=lower; r_iter!=upper; r_iter++)
    {
        if ((*r_iter).second->equals(*route))
            return true;
    }
    return false;
}

ODpair* Network::find_odpair (const int origin_id, const int dest_id)
{
    odval odid (origin_id, dest_id);
    vector <ODpair*>::iterator od_it= (find_if (odpairs.begin(),odpairs.end(), compareod (odid) ));
    if (od_it == odpairs.end())
        return nullptr;
    else
        return *od_it;
}

// define all the inputfunctions according to the rules in the
// grammar . Each production rule has its own function

bool Network::readnodes(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="nodes:")
        return false;
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readnode(in))
            return false;
    }
    return true;
}

bool Network::readnode(istream& in)
{
#ifndef _NO_GUI
    stringstream os; // for the formatting of the icon text (such as "j:42" for junction with id 42)
    const int sz=8; // allows for 6-digit id numbers.
    char t[sz];
#endif //_NO_GUI
    char bracket;
    int nid;
    int type;
    double x;
    double y;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readnodes scanner jammed at " << bracket;
        return false;
    }
    in  >> nid >> type >> x >> y;
    // check nid, type; assert ( !exists (nid) && exists(type) )
    assert ((type < 6) && (type > 0) );
#ifndef _UNSAFE
    //assert  ( (find_if (nodes.begin(),nodes.end(), compare <Node> (nid))) == nodes.end() ); // no node with nid exists
    assert (!nodemap.count(nid));
#endif // _UNSAFE
    if (type==1)
    {
        Origin* optr=new Origin(nid);
#ifndef _NO_GUI
        os << "o:"<< nid << endl;
        os.get(t,sz);
        optr->setxy(x,y);
        NodeIcon* niptr=new NodeIcon(static_cast<int>(x),static_cast<int>(y), optr);
        niptr->settext(t);
        optr->set_icon(niptr);
        drawing->add_icon(niptr);
#endif // _NO_GUI
        nodemap [nid] = optr; // later on take out the vectors. Now we use both map and old vectors
        originmap [nid] = optr;
#ifdef _DEBUG_NETWORK
        cout << " origin " << nid;
#endif //_DEBUG_NETWORK
    }
    if (type==2)
    {
        int sid;
        in >>  sid;
        Destination* dptr=nullptr;
        if (sid < 0) // why was this clause again? check what server == -1 means...
            dptr=new Destination(nid);
        else
        {
            assert (servermap.count(sid));
            //Server* sptr=(*(find_if (servers.begin(),servers.end(), compare <Server> (sid)))) ;
            Server* sptr = servermap [sid];
            if (sptr!=nullptr)
                dptr=new Destination(nid,sptr);
        }
        if (dptr==nullptr)
        {

            cout << "Read nodes: scanner jammed at destination " << nid << endl;
            return false;
        }
#ifndef _NO_GUI
        os << "d:"<< nid << endl;
        os.get(t,sz);
        dptr->setxy(x,y);
        NodeIcon* niptr=new NodeIcon(static_cast<int>(x),static_cast<int>(y),dptr);
        niptr->settext(t);
        dptr->set_icon(niptr);
        drawing->add_icon(niptr);
#endif //_NO_GUI
        nodemap [nid] = dptr; // later on take out the vectors. Now we use both map and old vectors
        destinationmap [nid] = dptr;
#ifdef _DEBUG_NETWORK
        cout << " destination " << nid ;

#endif //_DEBUG_NETWORK
    }
    if (type==3)     // JUNCTION
    {
        Junction* jptr=new Junction(nid);
#ifndef _NO_GUI
        os << "j:"<< nid << endl;
        os.get(t,sz);
        jptr->setxy(x,y);
        NodeIcon* niptr=new NodeIcon(static_cast<int>(x),static_cast<int>(y),jptr);
        niptr->settext(t);
        jptr->set_icon(niptr);
        drawing->add_icon(niptr);
#endif // _NO_GUI
        nodemap [nid] = jptr; // later on take out the vectors. Now we use both map and old vectors
        junctionmap [nid] = jptr;

#ifdef _DEBUG_NETWORK
        cout << " junction " << nid ;
#endif //_DEBUG_NETWORK
    }
    if (type==4)
    {
        BoundaryIn* biptr=new BoundaryIn(nid);
#ifndef _NO_GUI
        os << "bi:"<< nid << endl;
        os.get(t,sz);
        biptr->setxy(x,y);
        NodeIcon* niptr=new NodeIcon(static_cast<int>(x),static_cast<int>(y),biptr);
        niptr->settext(t);
        biptr->set_icon(niptr);
        drawing->add_icon(niptr);
#endif //_NO_GUI
        nodemap [nid] = biptr; // later on take out the vectors. Now we use both map and old vectors
        boundaryinmap [nid] = biptr;
        originmap [nid] = biptr;
        boundaryins.insert(boundaryins.begin(),biptr);
#ifdef _DEBUG_NETWORK
        cout << " boundaryin "  << nid;
#endif //_DEBUG_NETWORK
    }

    if (type==5) // BOUNDARY OUT
    {
        BoundaryOut* jptr=new BoundaryOut(nid);
#ifndef _NO_GUI
        os << "bo:"<< nid << endl;
        os.get(t,sz);
        jptr->setxy(x,y);
        NodeIcon* niptr=new NodeIcon(static_cast<int>(x),static_cast<int>(y),jptr);
        niptr->settext(t);
        jptr->set_icon(niptr);
        drawing->add_icon(niptr);
#endif //_NO_GUI
        nodemap [nid] = jptr; // later on take out the vectors. Now we use both map and old vectors
        boundaryoutmap [nid] = jptr;
        junctionmap [nid] = jptr;
        boundaryouts.insert(boundaryouts.begin(),jptr);
#ifdef _DEBUG_NETWORK
        cout << " boundaryout " << nid ;
#endif //_DEBUG_NETWORK
    }

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readnodes scanner jammed at " << bracket;
        return false;
    }

#ifdef _DEBUG_NETWORK
    cout << "read"<<endl;
#endif //_DEBUG_NETWORK
    // Set up mapping for shortest path graph.
    int new_id = graphnode_to_node.size(); // numbered from 0.
    node_to_graphnode [nid] = new_id;
    graphnode_to_node [new_id] = nid;
    return true;
}


bool Network::readsdfuncs(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="sdfuncs:")
        return false;
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readsdfunc(in))
            return false;
    }
    return true;
}

bool Network::readsdfunc(istream& in)

{
    char bracket;
    int sdid=0;
    int type=0;
    double vmax=0;
    double vmin=0;
    double romax=0;
    double romin=0;
    double alpha=0.0;
    double beta=0.0;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsdfuncs scanner jammed at " << bracket;
        return false;
    }
    in  >> sdid >> type >> vmax >> vmin >> romax >> romin;
    if ((type==1) ||(type ==2))
        in >> alpha >> beta;
    assert (!sdfuncmap.count(sdid));
    assert ( (vmin>0) && (vmax>=vmin) && (romin >= 0) && (romax>=romin) );
    assert ( (type==0) || (type==1) || (type==2));
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsdfuncs scanner jammed at " << bracket;
        return false;
    }
    Sdfunc* sdptr = nullptr;
    if (type==0)
    {
        sdptr = new Sdfunc(sdid,vmax,vmin,romax, romin);
    }	else if ((type==1)	|| (type == 2))
    {
        sdptr = new DynamitSdfunc(sdid,vmax,vmin,romax,romin,alpha,beta);
    }
    assert (sdptr);
    sdfuncmap [sdid] = sdptr;

#ifdef _DEBUG_NETWORK
    cout << " read a sdfunc"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::readlinks(istream& in)
{

    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="links:")
        return false;
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readlink(in))
            return false;
    }
    return true;
}



bool Network::readlink(istream& in)
{
    char bracket;
    int innode;
    int outnode;
    int length;
    int nrlanes;
    int sdid;
    int lid;
    string name;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readlinks scanner jammed at " << bracket;
        return false;
    }
    in  >> lid >> innode >> outnode >> length >> nrlanes >> sdid >> name;

    if (name == "}")
    {
        bracket = '}';
        name="";
    }
    else
    {
        in >> bracket;
    }
    if (bracket != '}')
    {
        cout << "readfile::readlinks scanner jammed at " << bracket;
        return false;
    }
    // find the nodes and sdfunc pointers

    assert ( (length>0) && (nrlanes > 0) );           // check that the length and nrlanes are positive
#ifndef _UNSAFE
    assert (!linkmap.count(lid));
#endif // _UNSAFE

    assert (nodemap.count(innode));
    Node* inptr = nodemap [innode];

    assert (nodemap.count(outnode));
    Node* outptr = nodemap [outnode];

    assert (sdfuncmap.count(sdid));
    Sdfunc* sdptr = sdfuncmap [sdid];
    // make the drawable icon for the link
#ifndef _NO_GUI
    Coord st=inptr->getxy();
    int startx=static_cast <int> (st.x+theParameters->node_radius);
    int starty=static_cast <int> (st.y+theParameters->node_radius);
    st=outptr->getxy();
    int stopx=static_cast <int> (st.x+theParameters->node_radius);
    int stopy=static_cast <int> (st.y+theParameters->node_radius);
    LinkIcon* icon=new LinkIcon(startx, starty ,stopx, stopy);
    stringstream os; // for the formatting of the icon text (such as "j:42" for junction with id 42)
    const int sz=8; // allows for 6-digit id numbers.
    char t[sz];
    os << lid << endl;
    os.get(t,sz);
    icon->settext(t);
    // register the icon in the drawing
    drawing->add_icon(icon);
#endif // _NO_GUI
    // create the link
    Link* link=new Link(lid, inptr, outptr, length,nrlanes,sdptr);
    link->set_name(name);
    // register the icon in the link
#ifndef _NO_GUI
    link->set_icon(icon);
    icon->set_link(link);
#endif //_NO_GUI
    linkmap [lid] = link;
    //links.insert(links.end(),link);
#ifdef _DEBUG_NETWORK
    cout << " read a link"<<endl;
#endif //_DEBUG_NETWORK
    // Set up mapping for shortest path graph.
    int new_id = graphlink_to_link.size(); // numbered from 0.
    link_to_graphlink [lid] = new_id;
    graphlink_to_link [new_id] = lid;

    return true;
}

bool Network::readvirtuallinks(string name)
{
    ifstream in(name.c_str());
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="virtuallinks:")
    {
        in.close();
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readvirtuallink(in))
        {
            in.close();
            return false;
        }
    }
    in.close();
    return true;
}

bool Network::readvirtuallink(istream& in)
{
    char bracket;
    int lid;
    int innode;
    int outnode;
    int length;
    int nrlanes;
    int sdid;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readvirtuallinks scanner jammed at " << bracket;

        return false;
    }
    in  >> lid >> innode >> outnode >> length >> nrlanes >> sdid;
    // check lid, vmax, vmin, romax;
    // assert (!exists (lid) &&exists(innode) && exists(outnode) && length >0 && 0<nrlanes && exists(sdid) )
#ifdef _VISSIMCOM
    // read the virtual path link ids and parking place as well
    long enterparkinglot, lastlink, nr_v_nodes, v_node;
    vector <long> ids;
    in >> enterparkinglot >> lastlink >> nr_v_nodes;
    in >> bracket;

    if (bracket != '{')
    {
        cout << "readfile::readvirtuallinks scanner jammed at " << bracket;
        return false;
    }
    for (long i=0; i<nr_v_nodes; i++)
    {
        in >> v_node;
        ids.push_back(v_node);
    }
    in >> bracket;

    if (bracket != '}')
    {
        cout << "readfile::readvirtuallinks scanner jammed at " << bracket;
        return false;
    }
#endif //_VISSIMCOM

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readvirtuallinks scanner jammed at " << bracket;
        return false;
    }
    // find the nodes and sdfunc pointers
    assert ( (length>0) && (nrlanes > 0) );           // check that the length and nrlanes are positive
    assert (!virtuallinkmap.count(lid));
    assert (nodemap.count(innode));
    Node* inptr = nodemap [innode];
    assert (nodemap.count(outnode));
    Node* outptr = nodemap [outnode];
    assert (boundaryinmap.count(outnode));
    BoundaryIn* biptr = boundaryinmap [outnode];
    assert (boundaryoutmap.count(innode));
    BoundaryOut* boptr = boundaryoutmap [innode];
    assert (sdfuncmap.count(sdid));
    Sdfunc* sdptr = sdfuncmap [sdid];
    // make the drawable icon for the link
#ifndef _NO_GUI
    Coord st=inptr->getxy();
    int startx=static_cast <int> (st.x+theParameters->node_radius);
    int starty=static_cast <int> (st.y+theParameters->node_radius);
    st=outptr->getxy();
    int stopx=static_cast <int> (st.x+theParameters->node_radius);
    int stopy=static_cast <int> (st.y+theParameters->node_radius);

    VirtualLinkIcon* icon=new VirtualLinkIcon(startx, starty ,stopx, stopy);
    // register the icon in the drawing
    drawing->add_icon(icon);
#endif // _NO_GUI
    // create the link
    VirtualLink* link=new VirtualLink(lid, inptr, outptr, length,nrlanes,sdptr);

#ifdef _VISSIMCOM
    // add the id tags
    link->parkinglot = enterparkinglot;
    link->lastlink = lastlink;
    link->set_v_path_ids(ids);
#endif //_VISSIMCOM

    // register the icon in the link
#ifndef _NO_GUI
    link->set_icon(icon);
#endif //_NO_GUI
    linkmap [lid] = link;
    virtuallinkmap [lid] = link;
    virtuallinks.insert(virtuallinks.end(),link);
    biptr->register_virtual(link);
    boptr->register_virtual(link);

#ifdef _DEBUG_NETWORK
    cout << " read a virtual link"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}


bool Network::readservers(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="servers:")
        return false;
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readserver(in))
            return false;
    }
    return true;
}


bool Network::readserver(istream& in)
{
    char bracket;
    int sid;
    int stype;
    double mu;
    double sd;
    double delay;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readservers scanner jammed at " << bracket;
        return false;
    }
    in  >> sid >> stype;

    assert (!servermap.count(sid));
    assert ( (stype > -1) && (stype<5));
    in >> mu >> sd >> delay;
    assert ((mu>=0.0) && (sd>=0.0) && (delay>=0.0)); // to be updated when more server types are added
    // check id, vmax, vmin, romax;
    // type 0= dummy server: Const server
    // type 1=standard N(mu,sd) sever
    // type 2=deterministic (mu) server
    // type 3=stochastic delay server: min_time(mu) + LN(delay, std_delay)
    // type 4=stochastic delay server: LogLogistic(alpha/scale=mu, beta/shape=sd)
    // type -1 (internal) = OD server
    // type -2 (internal)= Destination server
    Server* sptr = nullptr;
    if (stype==0)
        sptr = new ConstServer(sid,stype,mu,sd,delay);
    if (stype==1)
        sptr = new Server(sid,stype,mu,sd,delay);
    if (stype==2)
        sptr = new DetServer(sid,stype,mu,sd,delay);
    if (stype==3)
    {
        sptr = new LogNormalDelayServer (sid,stype,mu,sd*theParameters->sd_server_scale,delay);
    }
    if (stype==4)
    {
        sptr = new LogLogisticDelayServer (sid,stype,mu,sd,delay);
    }
    assert (sptr);
    servermap [sid] = sptr;

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readservers scanner jammed at " << bracket;
        return false;
    }
#ifdef _DEBUG_NETWORK
    cout << " read a server"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}


bool Network::readturnings(string name)
{
    ifstream inputfile(name.c_str());
    assert (inputfile);
    string keyword;
    inputfile >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="turnings:")
    {
        inputfile.close();
        return false;
    }
    int nr;
    inputfile >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readturning(inputfile))
        {
            inputfile.close();
            return false;
        }
    }
    readgiveways(inputfile);
    inputfile.close();
    return true;
}


bool Network::readturning(istream& in)
{

    char bracket;
    int tid;
    int nid;
    int sid;
    int size;
    int inlink;
    int outlink;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readturnings scanner jammed at " << bracket;
        return false;
    }

    in  >> tid >> nid >> sid >> inlink >> outlink >>size;
    // check
    assert (size>0);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readturnings scanner jammed at " << bracket;
        return false;
    }
    map <int, Node*>::iterator node_iter;
    node_iter=nodemap.find(nid);
    assert (node_iter != nodemap.end());
    Node* nptr = (*node_iter).second;
    map <int, Link*>::iterator link_iter;
    link_iter=linkmap.find(inlink);
    assert (link_iter != linkmap.end());
    Link* inlinkptr = (*link_iter).second;
    link_iter=linkmap.find(outlink);
    assert (link_iter != linkmap.end());
    Link* outlinkptr = (*link_iter).second;
    if (sid < 0) // special case: this means a turning prohibitor
    {
        TurnPenalty* tptr=new TurnPenalty();
        tptr->from_link=inlink;
        tptr->to_link=outlink;
        tptr->cost=theParameters->turn_penalty_cost;
        turnpenalties.insert(turnpenalties.begin(),tptr);
        return true;
    }
    assert (servermap.count(sid));
    Server* sptr = servermap[sid];
#ifndef _UNSAFE
    assert (!turningmap.count(tid));
#endif // _UNSAFE
    Turning* tptr = new Turning(tid, nptr, sptr, inlinkptr, outlinkptr,size);
    turningmap [tid] = tptr;
#ifdef _DEBUG_NETWORK
    cout << " read a turning"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

void Network::create_turnings()
/*
creates automatically new turnings for all junctions, using server nr 0 from the servers list
*/
{
    cout << "network::create turnings :" << endl;
    int tid=static_cast<int>(turningmap.size());
    int size= theParameters->default_lookback_size;
    vector<Link*> incoming;
    vector<Link*> outgoing;
    Server* sptr = (*servermap.begin()).second; // safest way, since servermap [0] may not exist (if someone starts numbering their servers at 1 for instance)
    // for all junctions
    for (map <int, Junction*>::iterator iter1=junctionmap.begin();iter1!=junctionmap.end();iter1++)
    {
        cout << " junction id " << (*iter1).second->get_id() << endl;
        incoming=(*iter1).second->get_incoming();
        cout << " nr incoming links "<< incoming.size() << endl;

        outgoing=(*iter1).second->get_outgoing();
        cout << " nr outgoing links "<< outgoing.size() << endl;
        // for all incoming links
        for (vector<Link*>::iterator iter2=incoming.begin();iter2<incoming.end();iter2++)
        {
            cout << "incoming link id "<< (*iter2)->get_id() << endl;
            //for all outgoing links
            for (vector<Link*>::iterator iter3=outgoing.begin();iter3<outgoing.end();iter3++)
            {
                cout << "outcoming link id "<< (*iter3)->get_id() << endl;
                cout << "turning id "<< tid << endl;

                map<int,Turning*>::iterator t_iter;
                t_iter=	turningmap.find(tid);
                //assert (t_iter != turningmap.end());
                Turning* t_ptr= new Turning(tid, (*iter1).second, sptr, (*iter2), (*iter3),size);
                turningmap [tid]=t_ptr;
                tid++;
            }
        }
    }
}


bool Network::writeturnings(string name)
{
    ofstream out(name.c_str());
    assert(out);
    out << "turnings: " << turningmap.size() << endl;
    for (map<int,Turning*>::iterator iter=turningmap.begin();iter!=turningmap.end();iter++)
    {
        (*iter).second->write(out);
    }
    out << endl << "giveways: 0" << endl;
    return true;
}

bool Network::readgiveway(istream& in)
{
    char bracket;
    int nid;
    int tin;
    int tcontr; // node id, turn in, controlling turning
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readgiveway scanner jammed at " << bracket;
        return false;
    }

    in  >>  nid >> tin >> tcontr;
    // check
    assert (nodemap.count(nid));
    // Node* node = nodemap [nid];
    assert (turningmap.count(tin));
    Turning * t_in = turningmap [tin];
    assert (turningmap.count(tcontr));
    Turning * t_contr = turningmap [tcontr];

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readgiveway scanner jammed at " << bracket;
        return false;
    }

    t_in->register_controlling_turn(t_contr);
    return true;
}
bool Network::readgiveways(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="giveways:")
    {
        cout << " readgiveways: no << giveways: >> keyword " << endl;
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readgiveway(in))
        {
            cout << " readgiveways: readgiveway returned false for line nr " << (i+1) << endl;
            return false;
        }
    }


    return true;
}

bool Network::readroutes(istream& in)

{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="routes:")
    {
        cout << " readroutes: no << routes: >> keyword " << endl;
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readroute(in))
        {
            cout << " readroutes: readroute returned false for line nr " << (i+1) << endl;
            return false;
        }
    }

    // TO DO: Check out why ALL routes are registered at the boundaryIn nodes!
    for (vector<BoundaryIn*>::iterator iter=boundaryins.begin(); iter < boundaryins.end(); iter++)
    {
        (*iter)->register_routes(&routemap);
    }
    return true;
}

bool Network::readroute(istream& in)
{
    char bracket;
    int rid;
    int oid;
    int did;
    int lnr;
    int lid;
    vector<Link*> rlinks;
    map <int,Link*>::iterator link_iter;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readroutes scanner jammed at " << bracket;
        return false;
    }
    in  >> rid >> oid >> did >> lnr;
#ifndef _UNSAFE
    assert (!exists_route(rid,odval(oid,did)));
#endif // _UNSAFE
    // check
    in >> bracket;

    if (bracket != '{')
    {
        cout << "readfile::readroutes scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i<lnr; i++)
    {
        in >> lid;

        link_iter = linkmap.find(lid);
        assert (link_iter != linkmap.end());
        Link* linkptr = (*link_iter).second;
        rlinks.insert(rlinks.end(),linkptr);
#ifdef _DEBUG_NETWORK
        cout << " inserted link " << lid << " into route " << rid << endl;
#endif //_DEBUG_NETWORK

    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readroutes scanner jammed at " << bracket;
        return false;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readroutes scanner jammed at " << bracket;
        return false;
    }

    map <int, Origin*>::iterator o_iter;
    o_iter = originmap.find(oid);
    assert (o_iter != originmap.end());
    Origin* optr = o_iter->second;

    map <int, Destination*>::iterator d_iter;
    d_iter = destinationmap.find(did);
    assert (d_iter != destinationmap.end());
    Destination* dptr = d_iter->second;
#ifdef _DEBUG_NETWORK
    cout << "found o&d for route" << endl;
#endif //_DEBUG_NETWORK
    Route* rptr = new Route(rid, optr, dptr, rlinks);
    routemap.insert(pair <odval, Route*> (odval(oid,did),rptr));
#ifdef _DEBUG_NETWORK
    cout << " read a route"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::readcontrolcenters(const string& name)
{
    ifstream in(name.c_str());
    assert(in);
    string keyword;
    in >> keyword;
    if (keyword != "drt_first_rep_max_headway:")
    {
        DEBUG_MSG("readcontrolcenters:: no drt_first_rep_max_headway keyword, read: " << keyword);
        in.close();
        return false;
    }
    in >> ::drt_first_rep_max_headway;

    in >> keyword;
    if (keyword != "drt_first_rep_waiting_utility:")
    {
        DEBUG_MSG("readcontrolcenters:: no drt_first_rep_waiting_utility keyword, read: " << keyword);
        in.close();
        return false;
    }
    in >> ::drt_first_rep_waiting_utility;

    in >> keyword;
    if (keyword != "drt_min_occupancy:")
    {
        DEBUG_MSG("readcontrolcenters:: no drt_min_occupancy keyword, read: " << keyword);
        in.close();
        return false;
    }
    in >> ::drt_min_occupancy;

    in >> keyword;
    if (keyword != "drt_first_rebalancing_time:")
    {
        DEBUG_MSG("readcontrolcenters:: no drt_first_rebalancing_time keyword, read: " << keyword);
        in.close();
        return false;
    }
    in >> ::drt_first_rebalancing_time;

    //Create Controlcenters here or somewhere else. OBS: currently a pointer to this CC is given to Busstop via its constructor
    in >> keyword;
    if (keyword != "controlcenters:")
    {
        DEBUG_MSG("readcontrolcenters:: no controlcenters keyword, read: " << keyword);
        in.close();
        return false;
    }
    int num_control_centers;
    in >> num_control_centers;

    if(num_control_centers == 0)
        DEBUG_MSG("Warning: drt activated but no control centers defined in controlcenters.dat");

    for (int i = 0; i < num_control_centers; ++i)
    {
        int id; //id of control center
        int tg_strategy; //id of trip generation strategy
        int ev_strategy; //id of empty vehicle strategy
        int tvm_strategy; //id of trip vehicle matching strategy
        int vs_strategy; //id of vehicle scheduling strategy
        int rb_strategy; //id of the rebalancing strategy
        double rebalancing_interval; // time elapsed between calls to rebalancing on-call vehicles
        int generate_direct_routes; // 1 if all direct routes between should be added as service routes to this cc and 0 otherwise

        int nr_stops; //number of stops in the control center's service area
        int nr_collection_stops; //number of collection stops in the control center's service area used for rebalancing strategies, @note each collection stop must also be a member of the control center's service area
        int nr_lines; //number of lines (given as input) in the control center's service area

        char bracket;
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '{', read: " << bracket << endl;
            in.close();
            return false;
        }
        in >> id >> tg_strategy >> ev_strategy >> tvm_strategy >> vs_strategy >> rb_strategy >> rebalancing_interval >> generate_direct_routes;

        auto* cc = new Controlcenter(eventlist, this, id, tg_strategy, ev_strategy, tvm_strategy, vs_strategy, rb_strategy, rebalancing_interval);

        //read stops associated with this cc
        in >> nr_stops;
        bracket = ' ';
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '{', read: " << bracket << endl;
            in.close();
            return false;
        }
        for (int i = 0; i < nr_stops; ++i)
        {
            int stopid;
            Busstop* stop;
            in >> stopid;
            stop = (*find_if(busstops.begin(), busstops.end(), compare<Busstop>(stopid)));

            cc->addStopToServiceArea(stop);
            stop->add_CC(cc);
        }
        bracket = ' ';
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '}', read: " << bracket << endl;
            in.close();
            return false;
        }

        //read collection stops associated with this cc used for rebalancing
        in >> nr_collection_stops;
        bracket = ' ';
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '{', read: " << bracket << endl;
            in.close();
            return false;
        }
        for (int i = 0; i < nr_collection_stops; ++i)
        {
            int stopid;
            Busstop* stop;
            in >> stopid;
            stop = (*find_if(busstops.begin(), busstops.end(), compare<Busstop>(stopid)));
            if(cc->isInServiceArea(stop))
                cc->add_collection_stop(stop);
            else
                qDebug() << "Warning - ignoring adding collection stop " << stopid << " to controlcenter " << id << ", collection stop does not exist in service area";
        }
        bracket = ' ';
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '}', read: " << bracket << endl;
            in.close();
            return false;
        }

        //create and add all direct lines to cc
        if (generate_direct_routes == 1)
        {
            //@todo PARTC temporary change create direct lines with intermediate stops
            /*cout << "readcontrolcenters:: generating direct lines for control center " << cc->getID() << endl;
            if (!createAllDRTLinesWithIntermediateStops(cc))
            {
                cout << "readcontrolcenters:: problem generating direct lines for control center " << id;
                in.close();
                return false;
            }
            cc->setGeneratedDirectRoutes(generate_direct_routes);*/
            cout << "readcontrolcenters:: generating direct lines for control center " << cc->getID() << endl;
            if (!createAllDRTLines(cc))
            {
                cout << "readcontrolcenters:: problem generating direct lines for control center " << id;
                in.close();
                return false;
            }
            cc->setGeneratedDirectRoutes(generate_direct_routes);
        }
        
        //read lines associated with this cc 
        in >> nr_lines;
        bracket = ' ';
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '{', read: " << bracket;
            in.close();
            return false;
        }
        for (int i = 0; i < nr_lines; ++i)
        {
            int line_id;
            Busline* line;
            in >> line_id;
            line = (*find_if(buslines.begin(), buslines.end(), compare<Busline>(line_id)));

            cc->addServiceRoute(line);
        }
        bracket = ' ';
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '}', read: " << bracket;
            in.close();
            return false;
        }

        ccmap[id] = cc; //add to network map of control centers

        bracket = ' ';
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readcontrolcenters:: controlcenter scanner expected '}', read: " << bracket << endl;
            in.close();
            return false;
        }
    }

    in.close();
    return true;
}

// read BUS routes
bool Network::readtransitroutes(string name) // reads the busroutes, similar to readroutes
{
    ifstream in(name.c_str());
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="routes:")
    {
        cout << " readBusroutes: no << routes: >> keyword " << endl;
        in.close();
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readbusroute(in))
        {
            cout << " readbusroutes: readbusroute returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    for (auto iter=boundaryins.begin(); iter < boundaryins.end(); iter++)
    {
        (*iter)->register_busroutes(&busroutes);
    }
    in.close();
    return true;

}


bool Network::readbusroute(istream& in)
{
    char bracket;
    int rid;
    int oid;
    int did;
    int lnr;
    int lid;
    vector<Link*> rlinks;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readbusroutes scanner jammed at " << bracket;
        return false;
    }
    in  >> rid >> oid >> did >> lnr;
#ifndef _UNSAFE
    assert ( (find_if (busroutes.begin(),busroutes.end(), compare <Busroute> (rid))) == busroutes.end() ); // no route exists  with rid
#endif // _UNSAFE
    // check
    in >> bracket;

    if (bracket != '{')
    {
        cout << "readfile::readbusroutes scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i<lnr; i++)
    {
        in >> lid;

        map <int, Link*>::iterator l_iter;
        l_iter = linkmap.find(lid);
        assert (l_iter != linkmap.end());

        rlinks.insert(rlinks.end(),l_iter->second);
#ifdef _DEBUG_NETWORK
        cout << " inserted link " << lid << " into busroute " << rid << endl;
#endif //_DEBUG_NETWORK

    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusroutes scanner jammed at " << bracket;
        return false;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusroutes scanner jammed at " << bracket;
        return false;
    }
    // find the origin & dest  pointers
    map <int, Origin*>::iterator o_iter;
    o_iter = originmap.find(oid);
    assert (o_iter != originmap.end());

    map <int, Destination*>::iterator d_iter;
    d_iter = destinationmap.find(did);
    assert (d_iter != destinationmap.end());

#ifdef _DEBUG_NETWORK
    cout << "found o&d for route" << endl;
#endif //_DEBUG_NETWORK
    busroutes.insert(busroutes.end(),new Busroute(rid, o_iter->second, d_iter->second, rlinks));
#ifdef _DEBUG_NETWORK
    cout << " read a route"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::readtransitnetwork(string name) //!< reads the stops, distances between stops, lines, trips and travel disruptions
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    int format;

    // First read the busstops
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="stops:")
    {
        cout << " readbuslines: no << stops: >> keyword " << endl;
        in.close();
        return false;
    }
    int nr= 0;
    in >> nr;
    int i=0;
    int limit;
    for (; i<nr;i++)
    {
        if (!readbusstop(in))
        {
            cout << " readbuslines: readbusstop returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }

    // in case of passenger route choice - read walking distances between stops
    if (theParameters->demand_format == 3)
    {
        in >> keyword;
        if (keyword!="stops_distances:")
        {
            cout << " readbuslines: no << busstops_distances: >> keyword " << endl;
            in.close();
            return false;
        }
        in >> nr;
        limit = i + nr;
        in >> keyword;
        if (keyword!="format:")
        {
            cout << " readbusstops_distances: no << format: >> keyword " << endl;
            return false;
        }
        in >> format; // Give an indication for time-table format
        for (; i<limit;i++)
        {
            if (format == 1 )
            {
                if (!readbusstops_distances_format1(in))
                {
                    cout << " readbuslines: readbusstops_distances returned false for line nr " << (i+1) << endl;
                    in.close();
                    return false;
                }
            }
            if (format == 2 )
            {
                if (!readbusstops_distances_format2(in))
                {
                    cout << " readbuslines: readbusstops_distances returned false for line nr " << (i+1) << endl;
                    in.close();
                    return false;
                }
            }
            // set distnaces between busstops to stops
        }
        
        in >> keyword;
        if (keyword!="stops_walking_times:")
        {
            cout << " readbuslines: no << stops_walking_times: >> keyword " << endl;
            in.close();
            return false;
        }
        in >> nr;
        limit = i + nr;
        
        for (; i<limit;i++)
        {
            
            //read walking time distributions
            if (!readwalkingtimedistribution(in))
            {
                cout << " readbuslines: readwalkingtimedistribution returned false for line nr " << (i+1) << endl;
                in.close();
                return false;
            }
            
        }

        
    }

    // Second read the buslines
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="lines:")
    {
        cout << " readbuslines: no << buslines: >> keyword " << endl;
        in.close();
        return false;
    }
    in >> nr;
    limit = i + nr;
    for (; i<limit;i++)
    {
        if (!readbusline(in))
        {
            cout << " readbuslines: readbusline returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    /*
    for (vector<Busline*>::iterator line_iter = buslines.begin(); line_iter < buslines.end(); line_iter++)
    {
        (*line_iter)->set_opposite_line(*(find_if(buslines.begin(), buslines.end(), compare <Busline> ((*line_iter)->get_opposite_id()) )));
    }
    */
    // Third read the trips
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="trips:")
    {
        cout << " readbuslines: no << bustrips: >> keyword " << endl;
        in.close();
        return false;
    }
    in >> nr;
    limit = i + nr;
    in >> keyword;
    if (keyword!="format:")
    {
        cout << " readbuslines: no << format: >> keyword " << endl;
        return false;
    }
    in >> format; // Give an indication for time-table format
    for (; i<limit;i++)
    {
        if (format == 1 )
        {
            if (!readbustrip_format1(in))
            {
                cout << " readbuslines: readbustrip returned false for line nr " << (i+1) << endl;
                in.close();
                return false;
            }
        }
        if (format == 2 )
        {
            if (!readbustrip_format2(in))
            {
                cout << " readbuslines: readbustrip returned false for line nr " << (i+1) << endl;
                in.close();
                return false;
            }
        }
        if (format == 3 )
        {
            if (!readbustrip_format3(in))
            {
                cout << " readbuslines: readbustrip returned false for line nr " << (i+1) << endl;
                in.close();
                return false;
            }
        }
        // set busline to trip
    }
    in >> keyword;
    if (keyword!="travel_time_disruptions:")
    {
        cout << " readbuslines: no << travel_time_disruptions: >> keyword " << endl;
        in.close();
        return false;
    }
    in >> nr;
    limit = i + nr;
    for (; i<limit;i++)
    {
        if (!read_travel_time_disruptions(in))
        {
            cout << " readbuslines: read_travel_time_disruptions returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    return true;
}

bool Network::readbusstop (istream& in) // reads a busstop
{
    char bracket;
    int stop_id;
    int link_id;
    int RTI_stop;
    double position;
    double length;
    double min_DT;
    string name;
    bool has_bay;
    bool can_overtake;
    bool non_Ramdon_Pass_Generation;
    bool ok= true;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop scanner jammed at " << bracket;
        return false;
    }
    in >> stop_id >> name >> link_id >> position >> length >> has_bay >> can_overtake >> min_DT >> RTI_stop >> non_Ramdon_Pass_Generation;
    
    if (linkmap.find(link_id) == linkmap.end())
    {
        cout << "readfile::readsbusstop error at stop " << stop_id << ". Link " << link_id << " does not exist." << endl;
    }

    Busstop* st = new Busstop(stop_id, name, link_id, position, length, has_bay, can_overtake, min_DT, RTI_stop, non_Ramdon_Pass_Generation);

    st->add_distance_between_stops(st,0.0);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusstop scanner jammed at " << bracket;
        return false;
    }
    busstops.push_back (st);
    add_busstop_to_name_map(name, st);
    busstopsmap[stop_id] = st;

#ifdef _DEBUG_NETWORK
    cout << " read busstop"<< stop_id <<endl;
#endif //_DEBUG_NETWORK
    return ok;
}

void Network::add_busstop_to_name_map(string bus_stop_name,Busstop* bus_stop_ptr){
    //cout << "adding bus stop " << bus_stop_name << " to the map.\n";
    busstop_name_map.insert(pair<string,Busstop*> (bus_stop_name,bus_stop_ptr) );
}

Busstop* Network::get_busstop_from_name(string bus_stop_name) {
    
    if ( busstop_name_map.find(bus_stop_name) == busstop_name_map.end()) {
        //key not found
        cout << "Unknown stop name " << bus_stop_name << endl;
        return nullptr;
    }
    
    
        return busstop_name_map.at(bus_stop_name);
    
    
}

bool Network::readbusline(istream& in) // reads a busline
{
    char bracket;
    int busline_id;
    int opposite_busline_id;
    int ori_id;
    int dest_id;
    int route_id;
    int vehtype;
    int holding_strategy;
    int nr_stops;
    int stop_id;
    int nr_tp;
    int tp_id;
    int nr_stops_init_occup;
    double max_headway_holding;
    double init_occup_per_stop;
    string name;
    vector <Busstop*> stops;
    vector <Busstop*> line_timepoint;
    Busstop* stop;
    Busstop* tp;

    //transfer related variables
    int tr_line_id;	//!< id of line that 'this' busline synchronizes transfers with, 0 if line is not synchronizing transfers
    int nr_tr_stops;	//!< number of transfer stops for this pair of lines
    int tr_stop_id;	//!< ids of each transfer stop
    vector <Busstop*> tr_stops;
    Busstop* tr_stop;

    //DRT related variables
    bool flex_line = false; //true if trips can be dynamically created for this line via a control center

    bool ok= true;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusline scanner jammed at " << bracket << ", expected {";
        return false;
    }
    bracket = ' ';
    in >> busline_id >> opposite_busline_id >> name >> ori_id >> dest_id >> route_id >> vehtype >> holding_strategy >> max_headway_holding >> init_occup_per_stop >> nr_stops_init_occup;
    if(theParameters->transfer_sync)
    {
        in >> tr_line_id;
    }
    if (theParameters->drt)
    {
        in >> flex_line;
    }
    in >> nr_stops;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusline scanner jammed when reading stop points at " << bracket << ", expected {";
        return false;
    }

    for (int i=0; i < nr_stops; i++)
    {
        in >> stop_id;
        stop = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) ))); // find the stop
        stops.push_back(stop); // and add it
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusline scanner jammed when reading stop point at " << bracket << ", expected }";
        return false;
    }

    // find OD pair, route, vehicle type
    odval odid (ori_id, dest_id);
    ODpair* odptr=(*(find_if (odpairs.begin(),odpairs.end(), compareod (odid) )));
    Busroute* br=(*(find_if(busroutes.begin(), busroutes.end(), compare <Route> (route_id) )));
    Vtype* vt= (*(find_if(vehtypes.vtypes.begin(), vehtypes.vtypes.end(), compare <Vtype> (vehtype) )));
    Busline* bl= new Busline (busline_id, opposite_busline_id, name, br, stops, vt, odptr, holding_strategy, max_headway_holding, init_occup_per_stop, nr_stops_init_occup, flex_line);

    for (vector<Busstop*>::iterator stop_iter = bl->stops.begin(); stop_iter < bl->stops.end(); stop_iter++)
    {
        (*stop_iter)->add_lines(bl);
        (*stop_iter)->add_line_nr_waiting(bl, 0);
        (*stop_iter)->add_line_nr_boarding(bl, 0);
        (*stop_iter)->add_line_nr_alighting(bl, 0);
        (*stop_iter)->set_had_been_visited(bl, false);
        if (theParameters->real_time_info == 0)
        {
            (*stop_iter)->add_real_time_info(bl,0);
        }
        else
        {
            (*stop_iter)->add_real_time_info(bl,1);
        }
    }

    // reading time point stops
    in >> nr_tp;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusline scanner jammed when reading time point stops at " << bracket << ", expected {";
        return false;
    }
    for (int i=0; i < nr_tp; i++)
    {
        in >> tp_id;
        tp = (*(find_if(stops.begin(), stops.end(), compare <Busstop> (tp_id) )));
        // search for it in the stops route of the line - 'line_timepoint' is a subset of 'stops'
        //	  assert (tp != *(stops.end())); // assure tp exists
        line_timepoint.push_back(tp); // and add it
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusline scanner jammed when reading time point stops at " << bracket << ", expected }";
        return false;
    }
    bl->add_timepoints(line_timepoint);

    // reading transfer stops
    if(theParameters->transfer_sync)
    {
        bl->add_tr_line_id(tr_line_id);
        if(tr_line_id != 0) //if we are synchronizing transfers with another line
        {
            //read transfer stops and add to busline
            in >> nr_tr_stops;
            in >> bracket;
            if (bracket != '{')
            {
                cout << "readfile::readsbusline scanner jammed when reading transfer stops at " << bracket << ", expected {";
                return false;
            }
            for (int i=0; i < nr_tr_stops; i++)
            {
                in >> tr_stop_id;
                tr_stop = (*(find_if(stops.begin(), stops.end(), compare <Busstop> (tr_stop_id) )));
                tr_stops.push_back(tr_stop);
            }
            in >> bracket;
            if (bracket != '}')
            {
                cout << "readfile::readsbusline scanner jammed when reading transfer stops at " << bracket << ", expected }";
                return false;
            }
            bl->add_tr_stops(tr_stops);
        }
    }

    bracket = ' ';
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusline scanner jammed at " << bracket << ", expected }";
        return false;
    }

    if (flex_line) //if flexible vehicle scheduling is allowed for this line then let the start and end stops of the line know of their origin and destination nodes for use in shortest path methods
    {
        assert(theParameters->drt);
        Origin* origin_node = bl->get_odpair()->get_origin();
        Destination* dest_node = bl->get_odpair()->get_destination();
        Busstop* firststop = bl->stops.front();
        Busstop* laststop = bl->stops.back();

        if (firststop->get_origin_node() == nullptr)
        {
            firststop->set_origin_node(origin_node);
        }
        if (laststop->get_dest_node() == nullptr)
        {
            laststop->set_dest_node(dest_node);
        }

        //ccmap[1]->addServiceRoute(bl); Note: moved the whole process of adding service routes to the readcontrolcenter instead
    }

    // add to buslines vector
    buslines.push_back (bl);
#ifdef _DEBUG_NETWORK
    cout << " read busline"<< stop_id <<endl;
#endif //_DEBUG_NETWORK
    return ok;
}


Busline* Network::create_busline(
    int                     busline_id,            //!< unique identification number
    int                     opposite_busline_id,   //!< identification number of the line that indicates the opposite direction (relevant only when modeling passenger route choice)
    string                  name,                  //!< a descriptive name
    Busroute*               broute,                //!< bus route
    vector <Busstop*>       stops,                 //!< stops on line
    Vtype*                  vtype,                 //!< Vehicle type of this line (TODO 2018-10-23: Currently completely unusued but removing will invalidate all current network inputs)
    ODpair*                 odptr,                 //!< OD pair
    int                     holding_strategy,      //!< indicates the type of holding strategy used for line
    double                  max_headway_holding,   //!< threshold parameter relevant in case holding strategies 1 or 3 are chosen or max holding time in [sec] in case of holding strategy 6
    double                  init_occup_per_stop,   //!< average number of passengers that are on-board per prior upstream stops (scale of a Gamma distribution)
    int                     nr_stops_init_occup,   //!< number of prior upstream stops resulting with initial occupancy (shape of a Gamma distribution)
    bool                    flex_line              //!< true if this line allows for dynamically scheduled trips
)
{
    Busline* bl = new Busline(busline_id, opposite_busline_id, name, broute, stops, vtype, odptr, holding_strategy, max_headway_holding, init_occup_per_stop, nr_stops_init_occup, flex_line);

    for (auto stop_iter = bl->stops.begin(); stop_iter < bl->stops.end(); stop_iter++)
    {
        (*stop_iter)->add_lines(bl);
        (*stop_iter)->add_line_nr_waiting(bl, 0);
        (*stop_iter)->add_line_nr_boarding(bl, 0);
        (*stop_iter)->add_line_nr_alighting(bl, 0);
        (*stop_iter)->set_had_been_visited(bl, false);
        if (theParameters->real_time_info == 0)
        {
            (*stop_iter)->add_real_time_info(bl,0);
        }
        else
        {
            (*stop_iter)->add_real_time_info(bl,1);
        }
    }

	vector<pair<Busstop*,double> > interstopIVT = calc_interstop_freeflow_ivt(broute, stops); 
	bl->set_delta_at_stops(interstopIVT); //add ivt based on freeflow as 'scheduled/expected' ivt between stops for busline

    if (flex_line) //if flexible vehicle scheduling is allowed for this line then let the start and end stops of the line know of their origin and destination nodes for use in shortest path methods
    {
        assert(theParameters->drt);
        Origin* origin_node = bl->get_odpair()->get_origin();
        Destination* dest_node = bl->get_odpair()->get_destination();
        Busstop* firststop = bl->stops.front();
        Busstop* laststop = bl->stops.back();

        if (firststop->get_origin_node() == nullptr)
        {
            firststop->set_origin_node(origin_node);
        }
        if (laststop->get_dest_node() == nullptr)
        {
            laststop->set_dest_node(dest_node);
        }

    }
    return bl;
}

bool Network::createAllDRTLines(Controlcenter* cc)
{
    assert(theParameters->drt);
    assert(cc);

    if (cc)
    {
        set<Busstop*, ptr_less<Busstop*>> serviceArea = cc->getServiceArea();
        vector <Busstop*> stops;
        vector<Busroute*> routesFound;
        vector<Busline*>  buslinesFound;

        //*** begin dummy values
        Vtype* vtype = new Vtype(888, "octobus", 1.0, 20.0);
        int routeIdCounter = 10000; // TODO: update to find max routeId from busroutes vector
        int busLineIdCounter = 10000; //  TODO: update later
       //*** end of dummy values

        ODpair* od_pair = nullptr;
        Origin* ori = nullptr;
        Destination* dest = nullptr;

        for (auto startstop : serviceArea)
        {
            for (auto endstop : serviceArea)
            {
                if (startstop != endstop)
                {
                    // find best origin for startstop if it does not exist
                    if (startstop->get_origin_node() == nullptr)
                    {
                        // find  origin node
                        ori = findNearestOriginToStop(startstop);
                        if (ori != nullptr)
                            startstop->set_origin_node(ori);
                    }
                    // find best destination for endstop if it does not exist
                    if (endstop->get_dest_node() == nullptr)
                    {
                        // find  destination node
                        dest = findNearestDestinationToStop(endstop);
                        if (dest != nullptr)
                            endstop->set_dest_node(dest);
                    }
                
                    // find best odpair
                    int ori_id = startstop->get_origin_node()->get_id();
                    int dest_id = endstop->get_dest_node()->get_id();
                    odval odid(ori_id, dest_id);
                    auto od_it = find_if(odpairs.begin(), odpairs.end(), compareod(odid));
                    if (od_it != odpairs.end())
                        od_pair = *od_it;
                    else // create new OD pair
                    {
                        od_pair = new ODpair(startstop->get_origin_node(), endstop->get_dest_node(), 0.0, &vehtypes);
                        odpairs.push_back(od_pair);
                        /*qDebug() << "createAllDRTLines:----Missing OD pair, creating for Origin " << ori_id <<
                            ", destination " << dest_id;*/
                    }

                    stops.clear();
                    stops.push_back(startstop);
                    stops.push_back(endstop);

                    Busroute* newRoute = create_busroute_from_stops(routeIdCounter, od_pair->get_origin(), od_pair->get_destination(), stops);
                    if (newRoute != nullptr)
                    {
                        //do not add busroutes that already exist
                        auto existing_route_it = find_if(busroutes.begin(), busroutes.end(), [newRoute](Busroute* broute) -> bool { return (Route*)newRoute->equals((Route)*broute); });
                        if (existing_route_it == busroutes.end())
                        {
                            routesFound.push_back(newRoute);
                            routeIdCounter++;
                        }
                        else {
                            newRoute = *existing_route_it;
                        }

                        // create busLine
                        Busline* newLine = create_busline(busLineIdCounter, 0, "DRT Line", newRoute, stops, vtype, od_pair, 0, 0.0, 0.0, 0, true);
                        if (newLine != nullptr)
                        {
                            newLine->set_planned_headway(::drt_first_rep_max_headway); //add a planned headway (associated with CC) for this line. Used when applying dominancy rules in CSGM and for prior knowledge calculations in pass decisions
                            buslinesFound.push_back(newLine);
                            busLineIdCounter++;
                            od_pair->add_route(newRoute); //add route to OD pair so it does not get deleted in Network::init
                        }
                    }
                    else
                        qDebug() << "DTR create buslines: no route found from stop " << startstop->get_id() << " to " << endstop->get_id();

                }
            }
        }
        // add the routes found to the busroutes
        busroutes.insert(busroutes.end(), routesFound.begin(), routesFound.end());
        // add the buslines
        buslines.insert(buslines.end(), buslinesFound.begin(), buslinesFound.end());

        //add buslines to cc
        for (auto line : buslinesFound)
        {
            cc->addServiceRoute(line);
        }
    } //if cc

    return true;
}


bool Network::createAllDRTLinesWithIntermediateStops(Controlcenter* cc)
{
    assert(theParameters->drt);
    assert(cc);

    if (cc)
    {
        set<Busstop*, ptr_less<Busstop*>> serviceArea = cc->getServiceArea();
        vector <Busstop*> stops;
        vector<Busroute*> routesFound;
        vector<Busline*>  buslinesFound;

        //*** begin dummy values
        Vtype* vtype = new Vtype(888, "octobus", 1.0, 20.0);
        int routeIdCounter = 10000; // TODO: update to find max routeId from busroutes vector
        int busLineIdCounter = 10000; //  TODO: update later
       //*** end of dummy values

        ODpair* od_pair = nullptr;
        Origin* ori = nullptr;
        Destination* dest = nullptr;

        for (auto startstop : serviceArea)
        {
            for (auto endstop : serviceArea)
            {
                if (startstop != endstop)
                {
                    // find best origin for startstop if it does not exist
                    if (startstop->get_origin_node() == nullptr)
                    {
                        // find  origin node
                        ori = findNearestOriginToStop(startstop);
                        if (ori != nullptr)
                            startstop->set_origin_node(ori);
                    }
                    // find best destination for endstop if it does not exist
                    if (endstop->get_dest_node() == nullptr)
                    {
                        // find  destination node
                        dest = findNearestDestinationToStop(endstop);
                        if (dest != nullptr)
                            endstop->set_dest_node(dest);
                    }
                
                    // find best odpair
                    int ori_id = startstop->get_origin_node()->get_id();
                    int dest_id = endstop->get_dest_node()->get_id();
                    odval odid(ori_id, dest_id);
                    auto od_it = find_if(odpairs.begin(), odpairs.end(), compareod(odid));
                    if (od_it != odpairs.end())
                        od_pair = *od_it;
                    else // create new OD pair
                    {
                        od_pair = new ODpair(startstop->get_origin_node(), endstop->get_dest_node(), 0.0, &vehtypes);
                        odpairs.push_back(od_pair);
                        /*qDebug() << "createAllDRTLinesWithIntermediateStops:----Missing OD pair, creating for Origin " << ori_id <<
                            ", destination " << dest_id;*/
                    }

                    // start and end stops included only.... but now we also grab intermediate ones from the generated busroute...
                    stops.clear();
                    stops.push_back(startstop);
                    stops.push_back(endstop);

                    Busroute* newRoute = create_busroute_from_stops(routeIdCounter, od_pair->get_origin(), od_pair->get_destination(), stops);

                    if (newRoute != nullptr)
                    {
                        //do not add busroutes that already exist
                        auto existing_route_it = find_if(busroutes.begin(), busroutes.end(), [newRoute](Busroute* broute) -> bool { return (Route*)newRoute->equals((Route)*broute); });
                        if (existing_route_it == busroutes.end())
                        {
                            routesFound.push_back(newRoute);
                            routeIdCounter++;
                        }
                        else {
                            newRoute = *existing_route_it;
                        }

                        // grab all the intermediate busstops including the start and end stops
                        vector<Busstop*> allstops = get_busstops_on_busroute(newRoute); // all stops that we happen to pass by on the route found.
                        assert(allstops.front() == startstop);
                        assert(allstops.back() == endstop);

                        // create busLine
                        Busline* newLine = create_busline(busLineIdCounter, 0, "DRT Line", newRoute, allstops, vtype, od_pair, 0, 0.0, 0.0, 0, true);
                        if (newLine != nullptr)
                        {
                            newLine->set_planned_headway(::drt_first_rep_max_headway); //add a planned headway (associated with CC) for this line. Used when applying dominancy rules in CSGM and for prior knowledge calculations in pass decisions
                            buslinesFound.push_back(newLine);
                            busLineIdCounter++;
                            od_pair->add_route(newRoute); //add route to OD pair so it does not get deleted in Network::init
                        }
                    }
                    else
                        qDebug() << "DTR create buslines: no route found from stop " << startstop->get_id() << " to " << endstop->get_id();

                }
            }
        }
        // add the routes found to the busroutes
        busroutes.insert(busroutes.end(), routesFound.begin(), routesFound.end());
        // add the buslines
        buslines.insert(buslines.end(), buslinesFound.begin(), buslinesFound.end());

        //add buslines to cc
        for (auto line : buslinesFound)
        {
            cc->addServiceRoute(line);
        }
    } //if cc

    return true;
}

Origin* Network::findNearestOriginToStop(Busstop* stop)
{
    int upstreamNodeId = linkmap[stop->get_link_id()]->get_in_node_id();
    if (originmap.count(upstreamNodeId)) // if upstream node is origin, found the nearest and return
      return originmap [upstreamNodeId];
    if (junctionmap.count(upstreamNodeId))
    {
        Junction* upstreamNode = junctionmap [upstreamNodeId];
        std::map <double, Origin*> nearestOrigins;
        for (Link* il:upstreamNode->get_incoming())
        {
            if (originmap.count(il->get_in_node_id()))
               nearestOrigins [il->get_cost(0.0)] = originmap [il->get_in_node_id()];
        }
        if (!nearestOrigins.empty())
            return nearestOrigins.begin()->second;
    }

    // Otherwise do shortest path from every origin to upstreamNodeId

    std::map <double, Origin*> costMap;
    for (auto o:originmap)
    {
        for (auto rlink : o.second->get_links())
        {
            auto sp = shortest_path_to_node(rlink->get_id(),upstreamNodeId,0.0);
            costMap [graph->costToNode(node_to_graphnode[upstreamNodeId]) ] = o.second;
        }
    }
    if (!costMap.empty())
        return costMap.begin()->second;
    // if control reaches here, there is no reachable downstream destination for the stop
    qDebug() << "ERROR: Network::findNearestOriginFromStop(Busstop* stop) : cannot find Origin node for stop "
             << stop->get_id();
    return nullptr;
}

Destination* Network::findNearestDestinationToStop(Busstop* stop)
{
    int downstreamNodeId = linkmap[stop->get_link_id()]->get_out_node_id();
    if (destinationmap.count(downstreamNodeId)) // downstream Node is Destination
        return destinationmap [downstreamNodeId];
    else
    {
        if (junctionmap.count(downstreamNodeId))
        {
            Junction* downstreamNode = junctionmap [downstreamNodeId];
            std::map <double, Destination*> nearestDestinations;
            for (Link* ol:downstreamNode->get_outgoing())
            {
                if (destinationmap.count(ol->get_out_node_id()))
                   nearestDestinations [ol->get_cost(0.0)] = destinationmap [ol->get_out_node_id()];
            }
            if (!nearestDestinations.empty())
                return nearestDestinations.begin()->second;
        }
    }

    // Otherwise do shortest path to every destination

    std::map <double, Destination*> costMap;
    for (auto d:destinationmap)
    {
        auto sp = shortest_path_to_node(stop->get_link_id(),d.first,0.0);
        costMap [graph->costToNode(node_to_graphnode [d.first] )] = d.second;
    }
    if (!costMap.empty())
        return costMap.begin()->second;

    // if control reaches here, there is no reachable downstream destination for the stop
    qDebug() << "ERROR: Network::findNearestDestinationToStop(Busstop* stop) : cannot find Destination node for stop "
             << stop->get_id();
    return nullptr;

}

vector<pair<Busstop*,double> > Network::calc_interstop_freeflow_ivt(const Busroute* route, const vector<Busstop*>& stops) const
{
    //TODO only considers free-flow travel times, adjust for dynamic ones instead that are dependent on histtimes or current simulation time
	assert(route);
	vector<pair<Busstop*, double> > deltas_at_stops; //vector of inter stop times, for first stop ivt from origin node of route

    if (route)
    {
        const vector<Link*> routelinks = route->get_links();
        if (!stops.empty() && !routelinks.empty())
        {
            double total_length; //total length of current link
            double relative_length; //percentage of current link to calculate IVT for
            double current_stop_pos; //position of stop on current link in meters from upstream node
            double time_to_stop = 0.0; //travel time to stop from closest upstream node

            auto current_link = routelinks.cbegin(); //iterator that always points to the next link on the route to be traversed when calculating ivts

            for (auto next_stop = stops.cbegin(); next_stop != stops.cend(); ++next_stop)
            {
                double expected_ivt = 0.0;
                auto next_stop_link = find_if(current_link, routelinks.cend(), compare<Link>((*next_stop)->get_link_id())); //find iterator to link of next stop on busroute
                if (next_stop_link == routelinks.cend()) //stop does not exist on this route
                {
                    return deltas_at_stops; //return all the stop deltas found so far
                }

                //calculate ivt from current position to next stop position
                for (; current_link != next_stop_link; ++current_link) //iterate through links in route up until link of stop
                {
                    expected_ivt += (*current_link)->get_freeflow_time();
                }

                expected_ivt -= time_to_stop; //subtract the travel time for length of link that has already been traversed

                //current_link now points to the same link of the next stop, add the ivt to the position of the next stop on the link
                total_length = (*current_link)->get_length();
                current_stop_pos = (*next_stop)->get_position();
                relative_length = current_stop_pos / total_length;
                time_to_stop = round(relative_length * (*current_link)->get_freeflow_time());

                expected_ivt += time_to_stop;
                deltas_at_stops.push_back(make_pair((*next_stop), expected_ivt));
            }
        }
    }
    return deltas_at_stops;
}

bool Network::readbustrip_format1(istream& in) // reads a trip
{
    if (theParameters->drt){
        DEBUG_MSG_V("DRT currently not available with trip format 1. Aborting...");
        abort();
    }

    char bracket;
    int trip_id;
    int busline_id;
    int nr_stops;
    int stop_id;
    double start_time;
    double pass_time;
    vector <Visit_stop*> stops;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    in >> trip_id >> busline_id >> start_time >> nr_stops;
    for (int i=0; i < nr_stops; i++)
    {
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::readsbustrip scanner jammed at " << bracket;
            return false;
        }
        in >> stop_id >> pass_time;
        // create the Visit_stop
        // find the stop in the list


        Busstop* bs = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
        auto* vs = new Visit_stop (bs, pass_time);
        stops.push_back(vs);

        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::readsbustrip scanner jammed at " << bracket;
            return false;
        }
    }

    // find busline
    Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
    auto* trip= new Bustrip (trip_id, start_time,bl);
    trip->add_stops(stops);
    bl->add_trip(trip,start_time);
    bl->reset_curr_trip();

    trip->convert_stops_vector_to_map();
    // add to bustrips vector
    bustrips.push_back (trip);

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusstop scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::readbustrip_format2(istream& in) // reads a trip
{
    if (theParameters->drt) {
        DEBUG_MSG_V("DRT currently not available with trip format 2. Aborting...");
        abort();
    }

    char bracket;
    int busline_id;
    int nr_stops;
    int nr_trips;
    double arrival_time_at_stop;
    double dispatching_time;
    vector <Visit_stop*> delta_at_stops;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    in >> busline_id >>  nr_stops;
    Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
    auto stops_iter = bl->stops.begin();
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i < nr_stops; i++)
    {
        in >> arrival_time_at_stop;
        // create the Visit_stop
        // find the stop in the list
        auto* vs = new Visit_stop ((*stops_iter), arrival_time_at_stop);
        delta_at_stops.push_back(vs);
        bl->add_stop_delta((*stops_iter), arrival_time_at_stop); //add expected travel times between stops in the order of the stops visited for this line
        stops_iter++;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }

    in >> nr_trips;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    for (int i=1; i < nr_trips+1; i++)
    {
        in >> dispatching_time;
        vector <Visit_stop*> curr_trip;
        double acc_time_table = dispatching_time;
        int tripid;

        for (auto iter = delta_at_stops.begin(); iter < delta_at_stops.end(); iter++)
        {
            acc_time_table += (*iter)->second;
            auto* vs_ct = new Visit_stop ((*iter)->first, acc_time_table);
            curr_trip.push_back(vs_ct);
        }
        tripid = bl->generate_new_trip_id(); //get a new trip_id for this line and increment trip counter
        auto* trip= new Bustrip (tripid, dispatching_time, bl); // e.g. line 2, 3rd trip: trip_id = 203
        trip->add_stops(curr_trip);
        bl->add_trip(trip,dispatching_time);
        bl->reset_curr_trip();
        trip->convert_stops_vector_to_map();
        bustrips.push_back (trip); // add to bustrips vector
    }
    bl->set_static_trips(bl->get_trips()); //save trips to static_trips for resets
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusstop scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::readbustrip_format3(istream& in) // reads a trip
{
    char bracket;
    int busline_id;
    int nr_stops;
    int nr_trips;
    double arrival_time_at_stop;
    double initial_dispatching_time;
    double headway;
    vector <Visit_stop*> delta_at_stops;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    in >> busline_id >>  nr_stops;
    Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
    auto stops_iter = bl->stops.begin();
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i < nr_stops; i++)
    {
        in >> arrival_time_at_stop;
        // create the Visit_stop
        // find the stop in the list
        auto* vs = new Visit_stop ((*stops_iter), arrival_time_at_stop);
        delta_at_stops.push_back(vs);
        bl->add_stop_delta((*stops_iter), arrival_time_at_stop); //add expected travel times between stops in the order of the stops visited for this line
        stops_iter++;
    }

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbustrip scanner jammed at " << bracket;
        return false;
    }
    in >>  initial_dispatching_time >> headway >> nr_trips ;
    for (int i=1; i < nr_trips+1; i++)
    {
        double trip_acc_time = initial_dispatching_time;
        vector <Visit_stop*> curr_trip;
        int tripid;
        for (auto iter = delta_at_stops.begin(); iter < delta_at_stops.end(); iter++)
        {
            trip_acc_time = trip_acc_time + (*iter)->second;
            auto* vs_ct = new Visit_stop ((*iter)->first, trip_acc_time);
            curr_trip.push_back(vs_ct);
        }
        tripid = bl->generate_new_trip_id(); //get a new trip_id for this line and increment trip counter
        auto* trip= new Bustrip (tripid, initial_dispatching_time,bl); // e.g. line 2, 3rd trip: trip_id = 203
        trip->add_stops(curr_trip);
        bl->add_trip(trip,curr_trip.front()->second);
        bl->reset_curr_trip();
        trip->convert_stops_vector_to_map();
        trip->set_last_stop_visited(trip->stops.front()->first);
        bustrips.push_back (trip); // add to bustrips vector
        initial_dispatching_time = initial_dispatching_time + headway;
    }

    bl->set_static_trips(bl->get_trips()); //save trips to static_trips for resets
    bl->set_planned_headway(headway); //store planned headway, assumed the same between resets

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusstop scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::readtransitdemand (string name)
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    int format;
    int limit;
    int nr= 0;
    int i=0;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="passenger_rates:")
    {
        cout << "readtransitdemand: no << passenger_rates: >> keyword " << endl;
        return false;
    }
    in >> nr;
    limit = i + nr;

    in >> keyword;
    if (keyword!="format:")
    {
        cout << "readtransitdemand: no << format: >> keyword " << endl;
        return false;
    }
    in >> format; // Give an indication for demand matrix format
    /*
    if (format == 3)
    {
        generate_stop_ods();
    }
    */
    for (; i<limit;i++)
    {
        if (format == 1)
        {
            if (!read_passenger_rates_format1(in))
            {
                cout << "readtransitdemand: read_passenger_rates returned false for line nr " << (i+1) << endl;
                return false;
            }
        }
        if (format == 10)
        {
            if (!read_passenger_rates_format1_TD_basic(in, nr))
            {
                cout << "readtransitdemand: read_passenger_rates returned false" << endl;
                return false;
            }
            i = limit;
        }
        if (format == 2)
        {
            if (!read_passenger_rates_format2(in))
            {
                cout << "readtransitdemand: read_passenger_rates returned false for line nr " << (i+1) << endl;
                return false;
            }
        }
        if (format == 3)
        {
            if (theParameters->demand_format != 3)
            {
                cout << "readtransitdemand: format " << format << " read in " << name << " incompatible with parameter demand_format= " << theParameters->demand_format << endl;
                return false;
            }
            if (!read_passenger_rates_format3(in))
            {
                cout << "readtransitdemand: read_passenger_rates returned false for line nr " << (i+1) << endl;
                return false;
            }
        }
        if (format!=1 && format!=2 && format!=3 && format!=10)
        {
            cout << "readtransitdemand: read_passenger_rates returned false for wrong format coding " << (i+1) << endl;
            return false;
        }
    }

    return true;
}

bool Network::readtransitdemand_empirical(const string& name)
{
    assert(theParameters->demand_format == 3); //currently assumes demand_format 3 is being used
    ifstream in{ name.c_str() }; // open input file
    if (!in)
    {
        cout << "Problem reading empirical demand: " << name << endl;
        return false;
    }

    string line;
    int counter = 0;
    while (getline(in, line))
    {
        if (line.empty()) //skip empty lines
            continue;

        ++counter;
        istringstream iss(line);
        int origin_stop_id, destination_stop_id;
        double arrival_time; //arrival time in seconds after start passenger generation time

        if (!(iss >> origin_stop_id >> destination_stop_id >> arrival_time))
        {
            cout << "readtransitdemand_empirical: scanner jammed at line nr " << counter << "." << endl
                << "\tRead: " << line << endl;
            return false;
        }

        if (arrival_time + theParameters->start_pass_generation < 0)
        {
            cout << "readtransitdemand_empirical: scanner jammed at line nr " << counter << ". Passenger arrival time " << arrival_time << " seconds after start_pass_generation " << theParameters->start_pass_generation << " is negative." << endl;
            return false;
        }

        vector<Busstop*>::iterator bs_o_it = find_if(busstops.begin(), busstops.end(), compare <Busstop>(origin_stop_id));
        if (bs_o_it == busstops.end())
        {
            cout << "readtransitdemand_empirical: scanner jammed at line nr " << counter << ". Bus stop " << origin_stop_id << " not found." << endl;
            return false;
        }
        Busstop* bs_o = *bs_o_it;
        vector<Busstop*>::iterator bs_d_it = find_if(busstops.begin(), busstops.end(), compare <Busstop>(destination_stop_id));
        if (bs_d_it == busstops.end())
        {
            cout << "readtransitdemand_empirical: scanner jammed at line nr " << counter << ". Bus stop " << destination_stop_id << " not found." << endl;
            return false;
        }
        Busstop* bs_d = *bs_d_it;

        ODstops* od_stop = nullptr;
        //Check if this ODstops has already been created
        if (bs_o->check_stop_od_as_origin_per_stop(bs_d) == false) //check if OD stop pair exists with this stop as origin to the destination of the passenger
        {
            od_stop = new ODstops(bs_o, bs_d, 0.0); //arrival rate set to zero, if non-zero should not have passed check. Note: means this must be called AFTER readtransitdemand
            bs_o->add_odstops_as_origin(bs_d, od_stop);
            bs_d->add_odstops_as_destination(bs_o, od_stop);
            odstops.push_back(od_stop); //used for resets, as well as when passengers are deleted between resets
            odstops_demand.push_back(od_stop); //if arrival rate is zero, should never be initialized via ODstops->execute. Used also for writing outputs for each odstop with at least one passenger generated
        }
        else 
        {
            vector<ODstops*>::iterator od_stop_it;
            od_stop_it = find_if(odstops.begin(), odstops.end(), [bs_o, bs_d](const ODstops* od_stop) -> bool
                {
                    return (od_stop->get_origin() == bs_o) && (od_stop->get_destination() == bs_d);
                }
            );
            if (od_stop_it == odstops.end()) //if true something is wrong with check_stop_od_as_origin_per_stop
            {
                cout << "readtransitdemand_empirical: scanner jammed at line nr " << counter << ". ODstop not found." << endl
                    << "\tRead: " << line << endl;
                return false;
            }
            else
                od_stop = *od_stop_it;
        }
        od_stop->set_empirical_arrivals(true); //flags that this odstop is generating empirical arrival events (needed to keep track between resets in Network::init for both day2day and without)

        empirical_passenger_arrivals.push_back(make_pair(od_stop, arrival_time + theParameters->start_pass_generation));
    }

    cout << "readtransitdemand_empirical: read " << counter << " passengers." << endl;
    return true;

}

bool Network::read_od_pairs_for_generation (string name)
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    int nr= 0;
    in >> keyword;
    if (keyword!="ODpairs:")
    {
        cout << " readbuslines: no << ODpairs: >> keyword " << endl;
        return false;
    }
    in >> nr;
    for (int i=0; i<nr; i++)
    {
        int origin_id;
        int destination_id;
        in >> origin_id >> destination_id;
        Busstop* bs_o=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_id) )));
        Busstop* bs_d=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_id) )));
        pair<Busstop*,Busstop*> odpair;
        odpair.first = bs_o;
        odpair.second = bs_d;
        od_pairs_for_generation.push_back(odpair);
    }
    return true;
}

bool Network::read_passenger_rates_format1 (istream& in) // reads the passenger rates in the format of arrival rate and alighting fraction per line and stop combination
{
    char bracket;
    int origin_stop_id;
    int busline_id;
    double arrival_rate;
    double alighting_fraction;
    bool ok= true;

    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_passenger_rates1 scanner jammed at " << bracket;
        return false;
    }

    in >> origin_stop_id >> busline_id >> arrival_rate >> alighting_fraction ;
    Busstop* bs_o=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
    bs_o->add_line_nr_boarding (bl, arrival_rate * theParameters->demand_scale);
    bs_o->add_line_nr_alighting (bl, alighting_fraction);

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_passenger_rates1 scanner jammed at " << bracket;
        return false;
    }

#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return ok;
}

bool Network::read_passenger_rates_format1_TD_basic (istream& in, int nr_rates) // reads the passenger rates in the format of arrival rate and alighting fraction per line and stop combination
{
    char bracket;
    int slices;
    int origin_stop_id;
    int busline_id;
    double scale;
    double arrival_rate;
    double alighting_fraction;

    string keyword;
    in >> keyword;
    if (keyword!="scale:")
    {
        cout << "readfile::read_passenger_rates10 no << scale: >> keyword " << endl;
        return false;
    }
    in >> scale;
    for (int i=0; i < nr_rates; i++)
    {
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::read_passenger_rates10 scanner jammed at " << bracket;
            return false;
        }
        in >> origin_stop_id >> busline_id >> arrival_rate >> alighting_fraction;
        Busstop* bs_o=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
        Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
        bs_o->add_line_nr_boarding (bl, arrival_rate * theParameters->demand_scale);
        bs_o->add_line_nr_alighting (bl, alighting_fraction);
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::read_passenger_rates10 scanner jammed at " << bracket;
            return false;
        }
    }
    in >> keyword;
    if (keyword!="slices:")
    {
        cout << "readfile::read_passenger_rates10 no << slices: >> keyword " << endl;
        return false;
    }
    in >> slices;
    for (int i=0; i < slices; i++)
    {
        read_passenger_rates_format1_TD_slices (in);
    }
#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::read_passenger_rates_format1_TD_slices (istream& in) // reads the passenger rates in the format of arrival rate and alighting fraction per line and stop combination
{
    char bracket;
    int nr_rates;
    int origin_stop_id;
    int busline_id;
    double scale;
    double loadtime;
    double arrival_rate;
    double alighting_fraction;

    string keyword;
    in >> keyword;
    if (keyword!="passenger_rates:")
    {
        cout << "readfile::read_passenger_rates10_slices no << passenger_rates: >> keyword " << endl;
        return false;
    }
    in >> nr_rates;
    in >> keyword;
    if (keyword!="scale:")
    {
        cout << "readfile::read_passenger_rates10_slices no << scale: >> keyword " << endl;
        return false;
    }
    in >> scale;
    in >> keyword;
    if (keyword!="loadtime:")
    {
        cout << "readfile::read_passenger_rates10_slices no << loadtime: >> keyword " << endl;
        return false;
    }
    in >> loadtime;
    auto* car = new Change_arrival_rate (loadtime);
    car->book_update_arrival_rates (eventlist, loadtime);
    for (int i=0; i < nr_rates; i++)
    {
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::read_passenger_rates10_slices scanner jammed at " << bracket;
            return false;
        }
        in >> origin_stop_id >> busline_id >> arrival_rate >> alighting_fraction;
        Busstop* bs_o=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
        Busline* bl=(*(find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) )));
        car->add_line_nr_boarding_TD (bs_o, bl, arrival_rate * theParameters->demand_scale);
        car->add_line_nr_alighting_TD (bs_o, bl, alighting_fraction);
        bs_o->add_line_update_rate_time(bl, loadtime);
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::read_passenger_rates10_slices scanner jammed at " << bracket;
            return false;
        }
    }
#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::read_passenger_rates_format2 (istream& in) // reads the passenger rates in the format of arrival rate per line, origin stop and destination stop combination
{
    char bracket;
    int origin_stop_id;
    int destination_stop_id;
    int busline_id;
    int nr_stops_info;
    double arrival_rate;
    bool ok= true;

    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected {";
        return false;
    }

    in >> busline_id;
    auto bl_it=find_if(buslines.begin(), buslines.end(), compare <Busline> (busline_id) );
    if (bl_it == buslines.end())
    {
        cout << "Bus line " << busline_id << " not found.";
        return false;
    }
    Busline* bl = *bl_it;

    for (unsigned long i=0; i < bl->stops.size()-1; i++)
    {
        stop_rate stop_rate_d;
        stops_rate stops_rate_d;
        multi_rate multi_rate_d;
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected {";
            return false;
        }
        in >> origin_stop_id >> nr_stops_info;
        auto bs_o_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) );
        if (bs_o_it == busstops.end())
        {
            cout << "Bus stop " << origin_stop_id << " not found.";
            return false;
        }
        Busstop* bs_o = *bs_o_it;
        for (int j=0; j< nr_stops_info; j++)
        {
            in >> bracket;
            if (bracket != '{')
            {
                cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected {";
                return false;
            }
            in >> destination_stop_id >> arrival_rate;
            auto bs_d_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) );
            if (bs_d_it == busstops.end())
            {
                cout << "Bus stop " << destination_stop_id << " not found.";
                return false;
            }
            Busstop* bs_d = *bs_d_it;
            stop_rate_d.first = bs_d;
            stop_rate_d.second = arrival_rate* theParameters->demand_scale;
            stops_rate_d.insert(stop_rate_d);
            multi_rate_d.first = bl;
            multi_rate_d.second = stops_rate_d;
            in >> bracket;
            if (bracket != '}')
            {
                cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected }, (at origin " << origin_stop_id << ", destination " << destination_stop_id << ")";
                return false;
            }
        }
        in >> bracket;
        bs_o->multi_arrival_rates.insert(multi_rate_d);
        if (bracket != '}')
        {
            cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected }, (at origin " << origin_stop_id << ")";
            return false;
        }
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_passenger_rates2 scanner jammed at " << bracket << ", expected }";
        return false;
    }

#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return ok;
}

bool Network::read_passenger_rates_format3 (istream& in) // reads the passenger rates in the format of arrival rate per OD in terms of stops (no path is pre-determined)
{
    char bracket;
    int origin_stop_id;
    int destination_stop_id;
    double arrival_rate;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_passenger_rates3 scanner jammed at " << bracket;
        return false;
    }
    in >> origin_stop_id >> destination_stop_id >> arrival_rate; //
    //Busstop* bs_o=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    auto bs_o_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) );
    if (bs_o_it == busstops.end())
    {
        cout << "Bus stop " << origin_stop_id << " not found.";
        return false;
    }
    Busstop* bs_o = *bs_o_it;
    //Busstop* bs_d=(*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    auto bs_d_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) );
    if (bs_d_it == busstops.end())
    {
        cout << "Bus stop " << destination_stop_id << " not found.";
        return false;
    }
    Busstop* bs_d = *bs_d_it;

    auto* od_stop = new ODstops (bs_o, bs_d, arrival_rate);
    //ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_stop->set_arrival_rate(arrival_rate* theParameters->demand_scale);
    odstops.push_back(od_stop);
    odstops_demand.push_back(od_stop);
    bs_o->add_odstops_as_origin(bs_d, od_stop);
    bs_d->add_odstops_as_destination(bs_o, od_stop);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_passenger_rates3 scanner jammed at " << bracket;
        return false;
    }

#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::readbusstops_distances_format1 (istream& in)
{
    char bracket;
    int from_stop_id;
    int to_stop_id;
    int nr_stops;
    double distance;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    in >> from_stop_id >> nr_stops;
    auto from_bs_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (from_stop_id) );
    if (from_bs_it == busstops.end())
    {
        cout << "Bus stop " << from_stop_id << " not found.";
        return false;
    }
    Busstop* from_bs = *from_bs_it;
    for (int i=0; i < nr_stops; i++)
    {
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
            return false;
        }
        in >> to_stop_id >> distance;
        // create the Visit_stop
        // find the stop in the list
        auto to_bs_it = find_if(busstops.begin(), busstops.end(), compare <Busstop> (to_stop_id) );
        if (to_bs_it == busstops.end())
        {
            cout << "Bus stop " << to_stop_id << " not found.";
            return false;
        }
        Busstop* to_bs = *to_bs_it;
        from_bs->add_distance_between_stops(to_bs,distance);
        to_bs->add_distance_between_stops(from_bs,distance);
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
            return false;
        }
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::readbusstops_distances_format2 (istream& in)
{
    char bracket;
    int from_stop_id;
    int nr_stops;
    double distance;
    string name; //name of hub (as opposed to individual bus shelter/platform)
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    in >> name >> nr_stops;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    vector <Busstop*> stops;
    for (int i=0; i < nr_stops; i++)
    {
        in >> from_stop_id;
        Busstop* from_bs = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (from_stop_id) )));
        stops.push_back(from_bs);
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket << ", expected } in stop list at " << name;
        return false;
    }
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    for (auto from_stop_iter = stops.begin(); from_stop_iter < stops.end(); from_stop_iter++)
    {

        for (auto to_stop_iter = stops.begin(); to_stop_iter < stops.end(); to_stop_iter++)
        {
            in >> distance;
            (*from_stop_iter)->add_distance_between_stops((*to_stop_iter),distance);
            (*to_stop_iter)->add_distance_between_stops((*from_stop_iter),distance);
        }
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket << ", expected } after matrix at " << name;
        return false;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket << ", expected } at end at " << name;
        return false;
    }
    return true;
}

bool Network::read_travel_time_disruptions (istream& in)
{
    char bracket;
    int line;
    int from_stop;
    int to_stop;
    double start_time;
    double end_time;
    double cap_reduction;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    in >> line >> from_stop >> to_stop >> start_time >> end_time >> cap_reduction;
    Busline* d_line = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line) )));
    Busstop* from_bs = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (from_stop) )));
    Busstop* to_bs = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (to_stop) )));
    d_line->add_disruptions(from_bs, to_bs, start_time, end_time, cap_reduction);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusstop_distances scanner jammed at " << bracket;
        return false;
    }
    return true;
}

/////////////// Transit path-set generation functions: start

void Network::generate_consecutive_stops()
{
    for (auto iter_stop = busstops.begin(); iter_stop < busstops.end(); iter_stop++)
    {
        vector<Busline*> lines = (*iter_stop)->get_lines();
        for (auto iter_line = lines.begin(); iter_line < lines.end(); iter_line++)
        {
            bool prior_this_stop = true;
            for (auto iter_consecutive_stops = (*iter_line)->stops.begin(); iter_consecutive_stops < (*iter_line)->stops.end(); iter_consecutive_stops++)
            {
                if (!prior_this_stop)
                {
                    // int origin_id = (*iter_stop)->get_id();
                    // int destination_id = (*iter_consecutive_stops)->get_id();
                    // map <int, vector<Busline*>> origin_map = direct_lines[origin_id];
                    // origin_map[destination_id].push_back(*iter_line);
                    // direct_lines[origin_id] = origin_map;
                    consecutive_stops[(*iter_stop)].push_back((*iter_consecutive_stops));
                }
                if ((*iter_consecutive_stops)->get_id() == (*iter_stop)->get_id())
                {
                    prior_this_stop = false;
                }
            }
        }
    }
    // omitting all repetitions (caused by several direct lines connecting two stops)
    for (auto iter_stop = busstops.begin(); iter_stop < busstops.end(); iter_stop++)
    {
        vector<Busstop*> updated_cons;
        map<Busstop*,bool, ptr_less<Busstop*>> already_exist;
        vector<Busstop*> cons_stops = consecutive_stops[(*iter_stop)];
        for (auto cons_stops_iter = cons_stops.begin(); cons_stops_iter < cons_stops.end(); cons_stops_iter++)
        {
            already_exist[(*cons_stops_iter)] = false;
        }
        for (auto cons_stops_iter = cons_stops.begin(); cons_stops_iter < cons_stops.end(); cons_stops_iter++)
        {
            if (!already_exist[(*cons_stops_iter)])
            {
                updated_cons.push_back((*cons_stops_iter));
                already_exist[(*cons_stops_iter)] = true;
            }
        }
        consecutive_stops[(*iter_stop)].swap(updated_cons);
        /*
        for (vector <vector<Busstop*>::iterator>::iterator iter_drop = drop_stops.begin(); iter_drop < drop_stops.end(); iter_drop++)
        {
            consecutive_stops[(*iter_stop)].erase((*iter_drop));
        }
        */
    }
}

bool Network::check_consecutive (Busstop* first, Busstop* second)
{
    for (auto stop_iter = consecutive_stops[first].begin(); stop_iter < consecutive_stops[first].end(); stop_iter++)
    {
        if ((*stop_iter)->get_id() == second->get_id())
        {
            return true;
        }
    }
    return false;
}

bool Network::find_direct_paths (Busstop* bs_origin, Busstop* bs_destination)
// finds if there is a direct path between a given pair of stops, generate new direct paths
{
    vector <Busline*> lines_o = bs_origin->get_lines(); //!> @todo Check how buslines are being added to stops after autogen of direct DRT lines
    for (auto bl_o = lines_o.begin(); bl_o < lines_o.end(); bl_o++)
    {
        vector <Busline*> lines_d = bs_destination->get_lines();
        for (auto bl_d = lines_d.begin(); bl_d < lines_d.end(); bl_d++)
        {
            if ((*bl_o)->get_id() == (*bl_d)->get_id())
            {
                for (auto stop = (*bl_o)->stops.begin(); stop < (*bl_o)->stops.end(); stop++)
                {
                    if ((*stop) == (bs_origin)) // if this condition is met first - it means that this is a possible path for this OD (origin preceeds destination)
                    {
                        bool original_path = true;
                        vector<Busline*> last_line;
                        vector<vector<Busline*> > lines_sequence;
                        vector<vector<Busstop*> > stops_sequence;
                        vector<double> walking_distances_sequence;
                        // in any case - add this specific direct path
                        vector<Busstop*> o_stop;
                        o_stop.push_back(bs_origin);
                        stops_sequence.push_back(o_stop);
                        stops_sequence.push_back(o_stop);
                        vector<Busstop*> d_stop;
                        d_stop.push_back(bs_destination);
                        stops_sequence.push_back(d_stop);
                        stops_sequence.push_back(d_stop);
                        last_line.push_back(*bl_o);
                        lines_sequence.push_back(last_line);
                        walking_distances_sequence.push_back(0);
                        walking_distances_sequence.push_back(0);
                        Pass_path* direct_path = new Pass_path(pathid, lines_sequence, stops_sequence, walking_distances_sequence);
                        pathid++;

                        ODstops* od_stop;
                        if (!bs_origin->check_destination_stop(bs_destination))
                        {
                            od_stop = new ODstops (bs_origin,bs_destination);
                            bs_origin->add_odstops_as_origin(bs_destination, od_stop);
                            bs_destination->add_odstops_as_destination(bs_origin, od_stop);
                        }
                        else
                        {
                            od_stop = bs_origin->get_stop_od_as_origin_per_stop(bs_destination);
                        }

                        vector <Pass_path*> current_path_set = od_stop->get_path_set();
                        for (vector <Pass_path*>::iterator iter = current_path_set.begin(); iter != current_path_set.end(); iter++)
                        {
                            if (compare_same_lines_paths(direct_path,(*iter)))
                            {
                                original_path = false;
                            }
                        }
                        // add the direct path if it is an original one and it fulfills the constraints
                        if (original_path && check_constraints_paths(direct_path))
                        {
                            bs_origin->get_stop_od_as_origin_per_stop(bs_destination)->add_paths(direct_path);
                            od_direct_lines[bs_origin->get_stop_od_as_origin_per_stop(bs_destination)].push_back(*bl_o);
                            if (direct_path->get_number_of_transfers() < bs_origin->get_stop_od_as_origin_per_stop(bs_destination)->get_min_transfers())
                                // update the no. of min transfers required if decreased
                            {
                                bs_origin->get_stop_od_as_origin_per_stop(bs_destination)->set_min_transfers(direct_path->get_number_of_transfers());
                            }
                        }
                        else
                        {
                            bs_origin->clear_odstops_as_origin(bs_destination);
                            bs_destination->clear_odstops_as_destination(bs_origin);
                        }

                    }
                    if ((*stop) == bs_destination) // if this condition is met first - it means that this is not a possible path (destination preceeds origin)
                    {
                        break;
                    }
                }
            }
        }
    }
    //vector<Busline*> dir_lines = get_direct_lines(bs_origin->get_stop_od_as_origin_per_stop(bs_destination));
    //if  (dir_lines.size() != 0)
    //{
    //	return true;
    //}
#ifdef _DEBUG_NETWORK
#endif //_DEBUG_NETWORK
    return false;
}

void Network::generate_indirect_paths()
{
    vector<Pass_path*> updated_paths;
    vector<Pass_path*> paths;
    vector<vector<Busstop*> > stops_sequence = compose_stop_sequence(); // compose the list of intermediate stops

    /* count how many combinations of paths you can make (multiply number of direct lines on segments) - don't need it
    int nr_path_combinations = 1;
    for (vector<Busstop*>::iterator im_iter = collect_im_stops.begin(); im_iter < (collect_im_stops.end()-1); im_iter++)
    {
        map <Busstop*, ODstops*> od_as_origin = (*im_iter)->get_stop_as_origin();
        int origin_id = (*im_iter)->get_id();
        int destination_id = (*(im_iter+1))->get_id();
        map <int, vector<Busline*> > origin_map = direct_lines[origin_id];
        nr_path_combinations = nr_path_combinations * origin_map[destination_id].size();
    }
    */
    vector<vector<Busline*> > lines_sequence = compose_line_sequence(collect_im_stops.back());
    // ODstops* od = collect_im_stops.front()->get_stop_od_as_origin_per_stop(collect_im_stops.back());
    /*
    if (totaly_dominancy_rule(od,lines_sequence, stops_sequence) == true)
    // in case it is dominated by an existing path - don't generate the path
    {
        return;
    }
    */
    Pass_path* indirect_path = new Pass_path(pathid, lines_sequence, stops_sequence, collect_walking_distances);
    pathid++;
    paths.push_back(indirect_path);
    /*
    int stop_leg = 0;
    vector<vector<Busline*> >::iterator lines_sequence_iter = lines_sequence.begin();
    for (vector<Busstop*>::iterator im_iter = collect_im_stops.begin()+1; im_iter < collect_im_stops.end()-1; im_iter = im_iter+2)
    {
        stop_leg++;
        vector <Busline*> d1_lines = get_direct_lines((*im_iter)->get_stop_od_as_origin_per_stop((*(im_iter+1))));
        // generating all the permutations from the possible direct lines between consecutive transfer stops
        for (vector<Busline*>::iterator d_lines = d1_lines.begin() ; d_lines < d1_lines.end(); d_lines++)
        {
            vector <Busline*> p_lines;
            p_lines.push_back((*d_lines));
            for (vector<Pass_path*>::iterator existing_paths = paths.begin(); existing_paths < paths.end(); existing_paths++)
            {
                vector<vector<Busline*> > permute_path_lines;
                vector<vector<Busline*> > path_lines = (*existing_paths)->get_alt_lines();
                int line_leg = 0;
                for (vector<vector<Busline*> >::iterator path_lines_iter = path_lines.begin(); path_lines_iter < path_lines.end(); path_lines_iter++)
                {
                    line_leg++;
                    if (stop_leg != line_leg)
                    {
                        permute_path_lines.push_back((*path_lines_iter));
                    }
                    else
                    {
                        permute_path_lines.push_back(p_lines);
                    }
                }
                Pass_path* test_path = new Pass_path(pathid, permute_path_lines , stops_sequence, collect_walking_distances);
                pathid++;
                updated_paths.push_back(test_path);
            }
            for (vector<Pass_path*>::iterator additional_paths = updated_paths.begin(); additional_paths < updated_paths.end(); additional_paths++)
            {
                paths.push_back((*additional_paths));
            }
            updated_paths.clear();
        }
    }
    */
    for (auto paths_iter = paths.begin(); paths_iter < paths.end(); paths_iter++)
    {
        // check if this path was already generated
        bool original_path = true;
        vector <Pass_path*> current_path_set = collect_im_stops.front()->get_stop_od_as_origin_per_stop(collect_im_stops.back())->get_path_set();
        for (vector <Pass_path*>::iterator iter = current_path_set.begin(); iter != current_path_set.end(); iter++)
        {
            if (compare_same_lines_paths((*paths_iter),(*iter)) && compare_same_stops_paths((*paths_iter),(*iter)))
            {
                delete *paths_iter;
                original_path = false;
                break;
            }
        }
        // add the indirect path if it is an original one and it fulfills the constraints
        if (original_path && check_constraints_paths ((*paths_iter)))
        {
            collect_im_stops.front()->get_stop_od_as_origin_per_stop(collect_im_stops.back())->add_paths((*paths_iter));
            if ((*paths_iter)->get_number_of_transfers() < collect_im_stops.front()->get_stop_od_as_origin_per_stop(collect_im_stops.back())->get_min_transfers())
                // update the no. of min transfers required if decreased
            {
                collect_im_stops.front()->get_stop_od_as_origin_per_stop(collect_im_stops.back())->set_min_transfers((*paths_iter)->get_number_of_transfers());
            }
        }
    }
}


Busroute* Network::create_busroute_from_stops(int id, Origin* origin_node, Destination* destination_node, const vector<Busstop*>& stops, double time)
{
    assert(origin_node);
    assert(destination_node);
    if (stops.empty()) 
    {
        return nullptr;
    }

    // get path from origin to first stop ... to last stop; to destination
    vector <Link*> rlinks;
    vector <Link*> segment;
    int rootlink = origin_node->get_links().front()->get_id(); // TODO(MrLeffler): for all outgoing links from origin
    rlinks.push_back(linkmap[rootlink]); //rootlink of origin will always be included in potential route

   //1. find path to upstream node of next stop, 2. include all links of path besides rootlink, 3. set rootlink to location of next stop 4. add rootlink to rlinks, 5. increment next stop and return to step 1
    for (auto s : stops)
    {
        if (s->get_link_id() != rootlink) // if the stop is not already on current rootlink
        {
            int usnode = linkmap[s->get_link_id()]->get_in_node_id(); //id of closest node upstream from stop
            segment = shortest_path_to_node(rootlink, usnode, time);

            if (segment.empty()) // if one of the stops in the sequence is not reachable, return nullptr
            { 
                return nullptr;
            }
            rlinks.insert(rlinks.end(), segment.begin() + 1, segment.end()); // add segment to rlinks, always exclude rootlink
            rootlink = s->get_link_id();
            rlinks.push_back(linkmap[rootlink]); //add link that stop is located on to the end of route links
        }
    }
    // add segment to destination if needed
    if (linkmap[rootlink]->get_out_node_id() != destination_node->get_id())
    {
        segment = shortest_path_to_node(rootlink, destination_node->get_id(), time);
        if (segment.empty()) // if one of the stops in the sequence is not reachable, return nullptr
        { 
            return nullptr;
        }
        rlinks.insert(rlinks.end(), segment.begin() + 1, segment.end()); // add segment to rlinks excluding rootlink for last stop
    }

    return new Busroute(id, origin_node, destination_node, rlinks);
}


vector<vector<Busline*> > Network::compose_line_sequence (Busstop* destination)
{
    // compose the list of direct lines between each pair of intermediate stops
    vector<vector<Busline*> > lines_sequence;
    for (auto im_iter = collect_im_stops.begin()+1; im_iter < collect_im_stops.end()-1; im_iter = im_iter+2)
    {
        vector<Busline*> d_lines = get_direct_lines((*im_iter)->get_stop_od_as_origin_per_stop((*(im_iter+1))));
        if ((*(im_iter+1))->get_id() != destination->get_id())
        {
            map<Busline*,bool> delete_lines;
            for (auto line_iter = d_lines.begin(); line_iter < d_lines.end(); line_iter++)
            {
                delete_lines[(*line_iter)] = false;
            }
            if ((*im_iter)->check_destination_stop(destination))
            {
                vector<Busline*> des_lines = get_direct_lines ((*im_iter)->get_stop_od_as_origin_per_stop(destination));
                for (auto line_iter = d_lines.begin(); line_iter < d_lines.end(); line_iter++)
                {
                    for (auto line_iter1 = des_lines.begin(); line_iter1 < des_lines.end(); line_iter1++)
                    {
                        if ((*line_iter)->get_id() == (*line_iter1)->get_id()) // if there is a direct line to the destination in the set
                        {
                            delete_lines[(*line_iter)] = true;
                        }
                    }
                }
                for (map<Busline*,bool>::iterator iter = delete_lines.begin(); iter != delete_lines.end(); iter++)
                    // delete those lines that reach directly the destination and therefore should not be included in the transfer alternative
                {
                    vector <Busline*>::iterator line_to_delete;
                    for (auto iter1 = d_lines.begin(); iter1 < d_lines.end(); iter1++)
                    {
                        if ((*iter).first->get_id() == (*iter1)->get_id())
                        {
                            line_to_delete = iter1;
                            break;
                        }
                    }
                    if ((*iter).second)
                    {
                        d_lines.erase(line_to_delete);
                    }
                }
            }
        }
        lines_sequence.push_back(d_lines);
    }
    return lines_sequence;
}

vector<vector<Busstop*> > Network::compose_stop_sequence ()
{
    vector<vector<Busstop*> > stop_seq;
    vector<Busstop*> stop_vec;
    for (auto stop_iter = collect_im_stops.begin(); stop_iter < collect_im_stops.end(); stop_iter++)
    {
        stop_vec.clear();
        stop_vec.push_back((*stop_iter));
        stop_seq.push_back(stop_vec);
    }
    return stop_seq;
}

void Network::generate_stop_ods()
{
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            /*
            bool od_with_demand = false;
            for (vector <ODstops*>::iterator od = odstops_demand.begin(); od< odstops_demand.end(); od++)
            {
                if ((*od)->get_origin()->get_id() == (*basic_origin)->get_id() && (*od)->get_destination()->get_id() == (*basic_destination)->get_id())
                {
                    od_with_demand = true;
                    break;
                }
            }
            */
            //if (od_with_demand == false) // if it is not an od with a non-zero demand
            //{
            //	if (check_stops_opposing_directions((*basic_origin),(*basic_destination)) == false)
            //	{
            auto* od_stop = new ODstops ((*basic_origin),(*basic_destination));
            //odstops_map [(*basic_origin)].push_back(od_stop);
            //odstops.push_back(od_stop);
            (*basic_origin)->add_odstops_as_origin((*basic_destination), od_stop);
            (*basic_destination)->add_odstops_as_destination((*basic_origin), od_stop);
            //	}
            //}
        }
    }
}

bool Network::check_stops_opposing_directions (Busstop* origin, Busstop* destination)
{
    vector<Busline*> lines_at_origin = origin->get_lines();
    vector<Busline*> lines_at_destination = destination->get_lines();
    if (lines_at_origin.size() == lines_at_destination.size()) // if there is a diff. in number of passing lines then there are some non-opposing lines
    {
        vector<bool> opposite;
        for (auto line_iter_o = lines_at_origin.begin(); line_iter_o < lines_at_origin.end(); line_iter_o++)
        {
            for (auto line_iter_d = lines_at_destination.begin(); line_iter_d < lines_at_destination.end(); line_iter_d++)
            {
                if((*line_iter_o)->get_opposite_id() == (*line_iter_d)->get_id())
                {
                    opposite.push_back(true);
                }
            }
        }
        if (opposite.size() == lines_at_origin.size()) // if all the lines are opposing directions
        {
            return true;
        }
    }
    return false;
}

void Network::find_all_paths ()
// goes over all OD pairs for a given origin to generate their path choice set
{
    assert(!theParameters->drt); //currently untested with DRT
    generate_consecutive_stops();
    // first - generate all direct paths
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            if ((*basic_origin)->get_id() != (*basic_destination)->get_id())
            {
                find_direct_paths ((*basic_origin), (*basic_destination));
            }
        }
    }

    // second - generate indirect paths with walking distances
    cout << "Generating indirect paths:" << endl;
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            cout << (*basic_origin)->get_name() << " - " << (*basic_destination)->get_name() << endl;
            if (!(*basic_origin)->check_destination_stop(*basic_destination))
            {
                auto* od_stop = new ODstops ((*basic_origin),(*basic_destination));
                (*basic_origin)->add_odstops_as_origin((*basic_destination), od_stop);
                (*basic_destination)->add_odstops_as_destination((*basic_origin), od_stop);
            }
            if ((*basic_origin)->get_id() != (*basic_destination)->get_id() && (*basic_origin)->check_destination_stop(*basic_destination))
            {
                collect_im_stops.push_back((*basic_origin));
                find_recursive_connection_with_walking (((*basic_origin)), (*basic_destination));
                collect_im_stops.clear();
                collect_walking_distances.clear();
            }
            merge_paths_by_stops((*basic_origin),(*basic_destination));
            merge_paths_by_common_lines((*basic_origin),(*basic_destination));
        }
        //write_path_set_per_stop (workingdir + "path_set_generation.dat", (*basic_origin));
    }
    // apply static filtering rules
    cout << "Filtering paths..." << endl;
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        static_filtering_rules(*basic_origin);
    }
    // apply dominancy rules
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        dominancy_rules(*basic_origin);
    }

    // report generated choice-sets
    cout << "Saving paths..." << endl;

    write_path_set (workingdir + "o_path_set_generation.dat");
    cout << "Path generation finished!" << endl;
}

void Network::find_all_paths_fast ()
// goes over all OD pairs for a given origin to generate their path choice set
{
    generate_consecutive_stops();
    // first - generate all direct paths
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            if ((*basic_origin)->get_id() != (*basic_destination)->get_id())
            {
                find_direct_paths ((*basic_origin), (*basic_destination));
            }
        }
    }

    // second - generate indirect paths with walking distances
    cout << "Generating indirect paths:" << endl;
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        cout << (*basic_origin)->get_name() << endl;

        collect_im_stops.push_back((*basic_origin));
        find_recursive_connection_with_walking (*basic_origin);
        collect_im_stops.clear();
        collect_walking_distances.clear();

        if (!fwf_wip::day2day_drt_no_rti) // @todo temporary fix, do not merge paths
        {
            for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
            {
                merge_paths_by_stops((*basic_origin), (*basic_destination));
                merge_paths_by_common_lines((*basic_origin), (*basic_destination));
            }
            //write_path_set_per_stop (workingdir + "path_set_generation.dat", (*basic_origin));
        }
    }
    if (!fwf_wip::day2day_drt_no_rti)
    {
        // apply static filtering rules
        cout << "Filtering paths..." << endl;
        for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
        {
            cout << (*basic_origin)->get_name() << endl;
            static_filtering_rules(*basic_origin);
        }
        // apply dominancy rules
        cout << "Applying dominancy rules..." << endl;
        for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
        {
            dominancy_rules(*basic_origin);
        }
    }

    // report generated choice-sets
    cout << "Saving paths..." << endl;

    write_path_set (workingdir + "o_path_set_generation.dat");
    cout << "Path generation finished!" << endl;
}

void Network::find_all_paths_with_OD_for_generation ()
// goes over all OD pairs for a given origin to generate their path choice set
{
    generate_consecutive_stops();
    // first - generate all direct paths
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            if ((*basic_origin)->get_id() != (*basic_destination)->get_id())
            {
                find_direct_paths ((*basic_origin), (*basic_destination));
            }
        }
    }

    // second - generate indirect paths with walking distances
    for (auto basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
        {
            bool relevant_od = false;
            for (auto iter = od_pairs_for_generation.begin(); iter < od_pairs_for_generation.end(); iter ++)
            {
                if ((*basic_origin)->get_id() == (*iter).first->get_id() && (*basic_destination)->get_id() == (*iter).second->get_id())
                {
                    relevant_od = true;
                    break;
                }
            }
            if (relevant_od)
            {
                if (!(*basic_origin)->check_destination_stop(*basic_destination))
                {
                    auto* od_stop = new ODstops ((*basic_origin),(*basic_destination));
                    (*basic_origin)->add_odstops_as_origin((*basic_destination), od_stop);
                    (*basic_destination)->add_odstops_as_destination((*basic_origin), od_stop);
                }
                if ((*basic_origin)->get_id() != (*basic_destination)->get_id() && (*basic_origin)->check_destination_stop(*basic_destination))
                {
                    collect_im_stops.push_back((*basic_origin));
                    find_recursive_connection_with_walking (((*basic_origin)), (*basic_destination));
                    collect_im_stops.clear();
                    collect_walking_distances.clear();
                }
                merge_paths_by_stops((*basic_origin),(*basic_destination));
                merge_paths_by_common_lines((*basic_origin),(*basic_destination));
            }
        }
        write_path_set_per_stop (workingdir + "path_set_generation.dat", (*basic_origin));
    }
    // apply static filtering rules
    /*
    for (vector <Busstop*>::iterator basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        static_filtering_rules(*basic_origin);
    }
    // apply dominancy rules
    for (vector <Busstop*>::iterator basic_origin = busstops.begin(); basic_origin < busstops.end(); basic_origin++)
    {
        dominancy_rules(*basic_origin);
    }
    */
    // report generated choice-sets
    //write_path_set (workingdir + "path_set_generation.dat");
}

/*
void Network:: find_recursive_connection (Busstop* origin, Busstop* destination)
// search recursively for a path (forward - from origin to destination) - without walking links
{
    map <Busstop*, ODstops*> od_as_origin = origin->get_stop_as_origin();
    map <Busstop*, ODstops*> original_od_as_origin = collect_im_stops.front()->get_stop_as_origin();
    vector <Busstop*> cons_stops = consecutive_stops[origin];
    if (cons_stops.size() > 0)
    {
        if ((collect_im_stops.size() - 1) <= original_od_as_origin[destination]->get_min_transfers() + theParameters->max_nr_extra_transfers)
        {
            //for (vector<Busstop*>::iterator stop = cons_stops.begin(); stop < cons_stops.end(); stop++)
            for (map<Busstop*, ODstops*>::iterator iter = od_as_origin.begin(); iter != od_as_origin.end(); iter++)
            {
                //Busstop* intermediate_destination = (*stop);
                Busstop* intermediate_destination = (*iter).first;

                int intermediate_id = intermediate_destination->get_id();
                int previous_stop_id = collect_im_stops.back()->get_id();
                map <int, vector<Busline*> > origin_map = direct_lines[previous_stop_id];
                if (origin_map[intermediate_id].size() != 0)
                {
                    collect_im_stops.push_back(intermediate_destination);
                        if (intermediate_destination->get_id() != destination->get_id())
                        {
                            int destination_id = destination->get_id();
                            map <int, vector<Busline*> > origin_map = direct_lines[intermediate_id];
                            if (origin_map[destination_id].size() != 0)
                            {
                                collect_im_stops.push_back(destination);
                                generate_indirect_paths();
                                collect_im_stops.pop_back();
                            }
                            find_recursive_connection (intermediate_destination, destination);
                        }
                    collect_im_stops.pop_back();
                }
            }
        }
    }
    return;
}
*/

void Network:: find_recursive_connection_with_walking (Busstop* origin, Busstop* destination)
// search recursively for a path (forward - from origin to destination) with walking links
{
    map <Busstop*, double, ptr_less<Busstop*>> possible_origins = origin->get_walking_distances();
    vector <Busstop*> cons_stops = get_cons_stops(origin);
    //	if (cons_stops.size() > 0)
    //	{
    // find the number of expected transfers in this path search
    int nr_im_stop_elements = 0;
    for (auto iter_count = collect_im_stops.begin(); iter_count < collect_im_stops.end(); iter_count++)
    {
        nr_im_stop_elements++;
    }
    if (collect_im_stops.front()->check_destination_stop(destination))
    {
        if (((nr_im_stop_elements-1)/2) <= collect_im_stops.front()->get_stop_od_as_origin_per_stop(destination)->get_min_transfers() + theParameters->max_nr_extra_transfers || collect_im_stops.back()->check_walkable_stop(destination))
        {
            for (auto & possible_origin : possible_origins)
            {
                if (possible_origin.second < 10000)
                {
                    collect_walking_distances.push_back(possible_origin.second);
                    collect_im_stops.push_back(possible_origin.first);
                    bool already_visited = false;
                    for (auto collected_stops = collect_im_stops.begin(); collected_stops < collect_im_stops.end()-2; collected_stops++)
                    {
                        if ((*collected_stops)->get_id() == possible_origin.first->get_id())
                        {
                            already_visited = true;
                            break;
                        }
                    }
                    if (!already_visited)
                    {
                        if (collect_im_stops.back()->get_id() == destination->get_id())
                        {
                            generate_indirect_paths();
                            break;
                        }
                        //???	if ((check_consecutive((*iter),destination) == false && check_consecutive(destination,(*iter)) == true) == false) // in case destination preceeds origin on a given line but not vice versa - then do not continue this branch (heuristic)

                        cons_stops = get_cons_stops(collect_im_stops.back());
                        // applying dominancy rules of transfers and max transfers constraint
                        if (collect_im_stops.front()->get_stop_od_as_origin_per_stop(destination)->get_min_transfers() + theParameters->max_nr_extra_transfers >= static_cast<int>((collect_im_stops.size()-2)/2))
                        {
                            for (auto intermediate_destination : cons_stops)
                            {
                                Busstop* previous_stop = collect_im_stops.back();
                                collect_im_stops.push_back(intermediate_destination);
                                // check path constraints and apply dominancy rules of IVT
                                if (check_path_constraints(destination))
                                {
                                    if (check_consecutive(previous_stop,intermediate_destination))
                                    {
                                        if (destination->check_walkable_stop(intermediate_destination))
                                        {
                                            collect_im_stops.push_back(destination);
                                            collect_walking_distances.push_back(destination->get_walking_distance_stop(intermediate_destination));
                                            if (collect_im_stops.size() >= 4 || possible_origin.first->get_id() != origin->get_id() || intermediate_destination->get_id() != destination->get_id()) { //Added by Jens 2014-10-15, direct paths should not be added again
                                                generate_indirect_paths();
}
                                            collect_im_stops.pop_back();
                                            collect_walking_distances.pop_back();
                                        }
                                        else
                                        {
                                            // decide if there is a point in searching further deep
                                            if (theParameters->absolute_max_transfers >= ((nr_im_stop_elements+1)/2) || intermediate_destination->check_walkable_stop(destination))
                                                // only if this alternative doesn'tincludes too many transfers already or it can be completed without an extra transit leg
                                                // note that nr_elements is calculated at the beginging of the function and since then there are two extra stops
                                            {
                                                find_recursive_connection_with_walking (intermediate_destination, destination);
                                            }
                                        }
                                    }
                                }
                                collect_im_stops.pop_back();
                            }
                        }
                    }
                    collect_im_stops.pop_back();
                    collect_walking_distances.pop_back();
                }
            }
        }
    }
    //	}
}

void Network:: find_recursive_connection_with_walking (Busstop* origin)
// search recursively for a path (forward - from origin) with walking links
{
    map <Busstop*, double, ptr_less<Busstop*>> possible_origins = origin->get_walking_distances();
    vector <Busstop*> cons_stops = get_cons_stops(origin);

    int nr_im_stop_elements = 0;
    for (auto iter_count = collect_im_stops.begin(); iter_count < collect_im_stops.end(); iter_count++)
    {
        nr_im_stop_elements++;
    }

    for (map <Busstop*, double>::iterator poss_origin = possible_origins.begin(); poss_origin != possible_origins.end(); poss_origin++)
    {
        if ((*poss_origin).second < 10000)
        {
            collect_walking_distances.push_back((*poss_origin).second);
            Busstop* intermediate_stop = (*poss_origin).first;
            collect_im_stops.push_back(intermediate_stop);
            bool already_visited = false;
            for (auto collected_stops = collect_im_stops.begin(); collected_stops < collect_im_stops.end()-2; collected_stops++)
            {
                if ((*collected_stops)->get_id() == intermediate_stop->get_id())
                {
                    already_visited = true;
                    break;
                }
            }
            if (!already_visited)
            {
                if (!collect_im_stops.front()->check_destination_stop(poss_origin->first))
                {
                    auto* od_stop = new ODstops (collect_im_stops.front(), intermediate_stop);
                    collect_im_stops.front()->add_odstops_as_origin(intermediate_stop, od_stop);
                    intermediate_stop->add_odstops_as_destination(collect_im_stops.front(), od_stop);
                }

                if (check_path_constraints(intermediate_stop) && collect_im_stops.front()->get_stop_od_as_origin_per_stop(intermediate_stop)->get_min_transfers() + theParameters->max_nr_extra_transfers >= ((static_cast<int>(collect_im_stops.size())-4)/2))
                {
                    if ((collect_im_stops.size() > 2 || collect_im_stops.front()->get_id() != collect_im_stops.back()->get_id()) && (collect_im_stops.size() > 4 || intermediate_stop->get_id() != origin->get_id() || collect_im_stops.front()->get_id() != (*(collect_im_stops.begin() + 1))->get_id())) { //Added by Jens 2014-10-15, direct paths should not be added again
                        generate_indirect_paths();
}

                    if (theParameters->absolute_max_transfers >= ((nr_im_stop_elements+1)/2))
                        // only if this alternative doesn'tincludes too many transfers already or it can be completed without an extra transit leg
                        // note that nr_elements is calculated at the beginging of the function and since then there are two extra stops
                    {
                        cons_stops = get_cons_stops(intermediate_stop);

                        for (auto intermediate_destination : cons_stops)
                        {
                            Busstop* previous_stop = collect_im_stops.back();
                            collect_im_stops.push_back(intermediate_destination);

                            if (check_consecutive(previous_stop,intermediate_destination))
                            {
                                find_recursive_connection_with_walking (intermediate_destination);
                            }
                            collect_im_stops.pop_back();
                        }
                    }
                }
            }
            collect_im_stops.pop_back();
            collect_walking_distances.pop_back();
        }
    }
}

bool Network::check_path_constraints(Busstop* destination)
{
    vector<vector<Busline*> > lines = compose_line_sequence(destination);
    for (auto lines_iter = lines.begin(); lines_iter < lines.end(); lines_iter++)
    {
        if ((*lines_iter).empty())
        {
            return false;
        }
    }
    vector<vector<Busstop*> > stops = compose_stop_sequence();
    if (check_sequence_no_repeating_stops(collect_im_stops))
    {
        if (check_path_no_repeating_lines(lines,stops))
        {
            if (check_path_no_opposing_lines(lines))
            {
                return true;
            }
        }
    }
    return false;
}

void Network::merge_paths_by_stops (Busstop* origin_stop, Busstop* destination_stop) // merge paths with same lines for all legs (only different transfer stops)
// only if the route between this set of possible transfer stops is identical for the two leg sets
{
    //map <Busstop*, ODstops*> od_as_origin = stop->get_stop_as_origin();
    //for (map <Busstop*, ODstops*>::iterator odpairs = od_as_origin.begin(); odpairs != od_as_origin.end(); odpairs++)
    if (origin_stop->get_id() != destination_stop->get_id() && origin_stop->check_destination_stop(destination_stop))
    {
        ODstops* odpairs = origin_stop->get_stop_od_as_origin_per_stop(destination_stop);
        vector <Pass_path*> path_set = odpairs->get_path_set();
        // go over all the OD pairs of stops that have more than a single alternative
        if (path_set.size() > 1)
        {
            map <Pass_path*,bool> paths_to_be_deleted;
            vector <Pass_path*> merged_paths_to_be_added;
            map <Pass_path*,bool> flagged_paths;
            for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
            {
                flagged_paths[(*path_iter)] = false;
                paths_to_be_deleted[(*path_iter)] = false;
            }
            for (auto path1 = path_set.begin(); path1 < path_set.end()-1; path1++)
            {
                bool perform_merge = false;
                vector <vector <Busstop*> > stops1 = (*path1)->get_alt_transfer_stops();
                if (!flagged_paths[(*path1)])
                {
                    for (auto path2 = path1 + 1; path2 < path_set.end(); path2++)
                    {
                        bool fulfilled_conditions = compare_same_lines_paths ((*path1), (*path2));
                        if (fulfilled_conditions)
                        {
                            // both have exactly the same lines for all legs
                            vector<vector<Busline*> > path2_lines = (*path2)->get_alt_lines();
                            auto path2_line = path2_lines.begin();
                            vector<vector<Busline*> > path1_lines = (*path1)->get_alt_lines();
                            vector<vector<Busstop*> > path1_set_stops = (*path1)->get_alt_transfer_stops();
                            auto start_stops = path1_set_stops.begin()+1;
                            for(auto path1_line = path1_lines.begin(); path1_line < path1_lines.end(); path1_line++)
                            {
                                vector<Busline*> line1 = (*path1_line);
                                vector<Busline*> line2 = (*path2_line);
                                vector<Busstop*> start_stop = (*start_stops);
                                vector<Busstop*> end_stop = (*(start_stops+1));
                                // do they have the same routes between stops?
                                if (!compare_common_partial_routes (line1.front(),line2.front(),start_stop.front(),end_stop.front()))
                                {
                                    fulfilled_conditions = false;
                                    break;
                                }
                                start_stops = start_stops + 2;
                                path2_line++;
                            }
                            if (!fulfilled_conditions)
                            {
                                break;
                            }
                        }
                        if (fulfilled_conditions )
                            // both have exactly the same lines for all legs AND the same route for lines on leg 1 and lines on leg 2 between stops
                        {
                            vector <vector <Busstop*> > stops2 = (*path2)->get_alt_transfer_stops();
                            auto stops2_iter = stops2.begin();
                            for (auto stops1_iter = stops1.begin(); stops1_iter < stops1.end(); stops1_iter++)
                            {
                                for (auto stops2_leg = (*stops2_iter).begin(); stops2_leg < (*stops2_iter).end(); stops2_leg++)
                                {
                                    // search for the stops that you want to copy (union of two sets)
                                    bool no_identical = true;
                                    for (auto stops1_leg = (*stops1_iter).begin(); stops1_leg < (*stops1_iter).end(); stops1_leg++)
                                    {
                                        if ((*stops1_leg)->get_id() == (*stops2_leg)->get_id())
                                        {
                                            no_identical = false;
                                            break;
                                        }
                                    }
                                    if (no_identical)
                                        // add those non-shared stops to the stops set
                                    {
                                        (*stops1_iter).push_back((*stops2_leg));
                                        flagged_paths[(*path2)] = true;
                                        //perform_merge = true;
                                    }
                                }
                                stops2_iter++;
                            }
                            paths_to_be_deleted[*path1] = true;
                            paths_to_be_deleted[*path2] = true;
                            perform_merge = true; // Moved by Jens 2014-10-14
                        }
                    }
                }
                if (perform_merge)
                    // generate a new path with the joined stops set
                {
                    Pass_path* merged_path = new Pass_path(pathid, (*path1)->get_alt_lines(), stops1, (*path1)->get_walking_distances());
                    pathid++;
                    merged_paths_to_be_added.push_back(merged_path);
                }
            }
            for (map <Pass_path*,bool>::iterator delete_iter = paths_to_be_deleted.begin(); delete_iter != paths_to_be_deleted.end(); delete_iter++)
                // delete all the paths that were used as source for the merged paths
            {
                vector<Pass_path*>::iterator path_to_delete;
                for (auto paths_iter = path_set.begin(); paths_iter < path_set.end(); paths_iter++)
                {
                    if ((*delete_iter).first->get_id() == (*paths_iter)->get_id() && (*delete_iter).second)
                    {
                        path_to_delete = paths_iter;
                        break;
                    }
                }
                if ((*delete_iter).second)
                {
                    delete *path_to_delete;
                    path_set.erase(path_to_delete);
                }
            }
            for (auto adding_iter = merged_paths_to_be_added.begin(); adding_iter < merged_paths_to_be_added.end(); adding_iter++)
                // add the new merged paths to the path set
            {
                path_set.push_back((*adding_iter));
            }
        }
        odpairs->set_path_set(path_set);
    }
}

void Network::merge_paths_by_common_lines (Busstop* origin_stop, Busstop* destination_stop)  // merge paths with lines that have identical route between consecutive stops
{
    if (origin_stop->get_id() != destination_stop->get_id() && origin_stop->check_destination_stop(destination_stop))
    {
        ODstops* odpairs = origin_stop->get_stop_od_as_origin_per_stop(destination_stop);
        vector <Pass_path*> path_set = odpairs->get_path_set();
        if (path_set.size() > 1)
        {
            // go over all the OD pairs of stops that have more than a single alternative
            map <Pass_path*,bool> paths_to_be_deleted;
            vector <Pass_path*> merged_paths_to_be_added;
            map <Pass_path*,bool> flagged_paths;
            for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
            {
                flagged_paths[(*path_iter)] = false;
                paths_to_be_deleted[(*path_iter)] = false;
            }
            for (auto path1 = path_set.begin(); path1 < path_set.end()-1; path1++)
            {
                bool perform_merge = false;
                vector<vector<Busline*> > transfer_lines1_collect = (*path1)->get_alt_lines();
                if (!flagged_paths[(*path1)])
                {
                    for (auto path2 = path1 + 1; path2 < path_set.end(); path2++)
                    {
                        vector<vector<Busline*> > transfer_lines1_ = (*path1)->get_alt_lines();
                        auto transfer_lines1 = transfer_lines1_.begin();
                        auto transfer_lines1_collect_iter = transfer_lines1_collect.begin();
                        vector<vector<Busstop*> > transfer_stops1_ = (*path1)->get_alt_transfer_stops();
                        if (transfer_lines1_.empty()) // a walking-only path - there is no need in merging
                        {
                            break;
                        }
                        vector<vector<Busstop*> > transfer_stops2_ = (*path2)->get_alt_transfer_stops();
                        vector<vector<Busline*> > transfer_lines2_ = (*path2)->get_alt_lines();
                        auto transfer_stops2 = transfer_stops2_.begin()+1;
                        auto transfer_lines2 = transfer_lines2_.begin();
                        if (transfer_lines2_.empty()) // a walking-only path - there is no need in merging
                        {
                            break;
                        }
                        for (auto transfer_stops1 = transfer_stops1_.begin()+1; transfer_stops1 < transfer_stops1_.end()-1; transfer_stops1 = transfer_stops1 + 2)
                        {
                            int counter_shared_current = 0;
                            int coutner_shared_next = 0;
                            // check if the two paths share stops in two consecutive boarding-alighting stops
                            for (auto transfer_stop1 = (*transfer_stops1).begin(); transfer_stop1 < (*(transfer_stops1)).end(); transfer_stop1++)
                            {
                                for (auto transfer_stop2 = (*transfer_stops2).begin(); transfer_stop2 < (*(transfer_stops2)).end(); transfer_stop2++)
                                {
                                    if ((*transfer_stop1)->get_id() == (*transfer_stop2)->get_id())
                                    {
                                        counter_shared_current++;
                                        break;
                                    }
                                }
                            }
                            for (auto transfer_stop1 = (*(transfer_stops1+1)).begin(); transfer_stop1 < (*(transfer_stops1+1)).end(); transfer_stop1++)
                            {
                                for (auto transfer_stop2 = (*(transfer_stops2+1)).begin(); transfer_stop2 < (*(transfer_stops2+1)).end(); transfer_stop2++)
                                {
                                    if ((*transfer_stop1)->get_id() == (*transfer_stop2)->get_id())
                                    {
                                        coutner_shared_next++;
                                        break;
                                    }
                                }
                            }
                            // if ALL these stops are shared and the two lines have the same route between the stops - merge
                            if ((counter_shared_current == static_cast<int>((*transfer_stops1).size()) && counter_shared_current == static_cast<int>((*transfer_stops2).size()) && coutner_shared_next == static_cast<int>((*(transfer_stops1+1)).size()) && coutner_shared_next == static_cast<int>((*(transfer_stops2+1)).size())) && // there are two consecutive identical sets of transfer stops
                                    ((*(transfer_lines1)).front()->get_id() == (*(transfer_lines2)).front()->get_id() || compare_common_partial_routes((*(transfer_lines1)).front(), (*(transfer_lines2)).front(), (*transfer_stops1).front(), (*(transfer_stops1 + 1)).front()) ))
                            {
                                for (auto transfer_line2 = (*(transfer_lines2)).begin(); transfer_line2 < (*(transfer_lines2)).end(); transfer_line2++)
                                {
                                    bool identical_line = false;
                                    for (auto transfer_line1 = (*(transfer_lines1)).begin(); transfer_line1 < (*(transfer_lines1)).end(); transfer_line1++)
                                    {
                                        // check if the identical route is because it is really the same line...
                                        if ((*transfer_line1)->get_id() == (*transfer_line2)->get_id())
                                        {
                                            identical_line = true;
                                        }
                                    }
                                    if (!identical_line)
                                    {
                                        bool line_included = false;
                                        for (auto line_iter = (*transfer_lines1_collect_iter).begin(); line_iter < (*transfer_lines1_collect_iter).end(); line_iter++)
                                        {
                                            if ((*line_iter)->get_id() == (*transfer_line2)->get_id())
                                            {
                                                line_included = true;
                                                break;
                                            }
                                        }
                                        if (!line_included)
                                        {
                                            (*transfer_lines1_collect_iter).push_back(*transfer_line2);
                                        }
                                        //perform_merge = true;
                                        flagged_paths[(*path2)] = true;
                                    }
                                    paths_to_be_deleted[*path1] = true;
                                    paths_to_be_deleted[*path2] = true;
                                    perform_merge = true; //Moved by Jens 2014-10-14
                                }
                            }
                            else //Added by Jens 2014-10-14, paths need to have all segments in common to be merged
                            {
                                paths_to_be_deleted[*path1] = false;
                                paths_to_be_deleted[*path2] = false;
                                perform_merge = false;
                                break;
                            }

                            // progress all the iterators
                            transfer_stops2++;
                            transfer_stops2++;
                            if (transfer_stops2 >= transfer_stops2_.end()-1 || transfer_lines1 >= transfer_lines1_.end() || transfer_lines2 == transfer_lines2_.end())
                            {
                                break;
                            }
                            if (transfer_stops1 != transfer_stops1_.end())
                            {
                                transfer_lines1++;
                                transfer_lines1_collect_iter++;
                                transfer_lines2++;
                            }
                        }
                        if (perform_merge && path2 == path_set.end()-1)
                            // only after all the required merging took place
                        {
                            Pass_path* merged_path = new Pass_path(pathid, transfer_lines1_collect, (*path1)->get_alt_transfer_stops(), (*path1)->get_walking_distances());
                            pathid++;
                            merged_paths_to_be_added.push_back(merged_path);
                        }
                    }
                }
            }
            for (map <Pass_path*,bool>::iterator delete_iter = paths_to_be_deleted.begin(); delete_iter != paths_to_be_deleted.end(); delete_iter++)
                // delete all the paths that were used as source for the merged paths
            {
                vector<Pass_path*>::iterator path_to_delete;
                for (auto paths_iter = path_set.begin(); paths_iter < path_set.end(); paths_iter++)
                {
                    if ((*delete_iter).first->get_id() == (*paths_iter)->get_id() && (*delete_iter).second)
                    {
                        path_to_delete = paths_iter;
                        break;
                    }
                }
                if ((*delete_iter).second)
                {
                    delete *path_to_delete;
                    path_set.erase(path_to_delete);
                }
            }
            for (auto adding_iter = merged_paths_to_be_added.begin(); adding_iter < merged_paths_to_be_added.end(); adding_iter++)
                // add the new merged paths to the path set
            {
                path_set.push_back((*adding_iter));
            }
        }
        odpairs->set_path_set(path_set);
    }
}

void Network::static_filtering_rules (Busstop* stop)
{
    // includes the following filtering rules: (1) max walking distance; (2) max IVT ratio; (3) maybe worthwhile to way.
    for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
    {
        if (stop->get_id() != (*basic_destination)->get_id() && stop->check_destination_stop(*basic_destination))
        {
            ODstops* odpairs = stop->get_stop_od_as_origin_per_stop(*basic_destination);
            vector <Pass_path*> path_set = odpairs->get_path_set();
            if (!path_set.empty())
            {
                map <Pass_path*,bool> paths_to_be_deleted;
                // calculate the minimum total IVT for this OD pair
                double min_total_scheduled_in_vehicle_time = path_set.front()->calc_total_scheduled_in_vehicle_time(0.0);
                for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
                {
                    min_total_scheduled_in_vehicle_time = min (min_total_scheduled_in_vehicle_time, (*path_iter)->calc_total_scheduled_in_vehicle_time(0.0));
                    paths_to_be_deleted[(*path_iter)] = false;
                }
                for (auto path = path_set.begin(); path < path_set.end(); path++)
                {
                    // more than max. transfers
                    if ((*path)->get_number_of_transfers() > odpairs->get_min_transfers() + theParameters->max_nr_extra_transfers)
                    {
                        paths_to_be_deleted[(*path)] = true;
                    }
                    // max walking distance
                    else if ((*path)->calc_total_walking_distance() > theParameters->max_walking_distance)
                    {
                        paths_to_be_deleted[(*path)] = true;
                    }
                    // max IVT ratio
                    else if ((*path)->calc_total_scheduled_in_vehicle_time(0.0) > min_total_scheduled_in_vehicle_time * theParameters->max_in_vehicle_time_ratio)
                    {
                        paths_to_be_deleted[(*path)] = true;
                    }
                    // eliminating dominated stops by upstream transfer stops (avoiding further downstream transfer stops
                    // downstream_dominancy_rule(*path);
                    // maybe worthwhile to wait (with worst case headway)
                    vector<vector<Busline*> > alt_lines = (*path)->get_alt_lines();
                    for (auto line_set_iter = alt_lines.begin(); line_set_iter < alt_lines.end(); line_set_iter++)
                    {
                        map <Busline*,bool> lines_to_be_deleted;
                        for (auto line_iter = (*line_set_iter).begin(); line_iter < (*line_set_iter).end(); line_iter++)
                        {
                            lines_to_be_deleted[(*line_iter)] = false;
                        }
                        vector<vector<Busstop*> > alt_stops = (*path)->get_alt_transfer_stops();
                        map<Busline*, bool> lines_to_include = (*path)->check_maybe_worthwhile_to_wait((*path)->get_alt_lines().front(), alt_stops.begin(), 0);
                        for (map<Busline*, bool>::iterator lines_to_include_iter = lines_to_include.begin(); lines_to_include_iter != lines_to_include.end(); lines_to_include_iter++)
                        {
                            if (!(*lines_to_include_iter).second)
                                // delete the lines that are not worthwhile to wait for
                            {
                                lines_to_be_deleted[(*lines_to_include_iter).first] = true;
                            }
                        }
                        for (map <Busline*,bool>::iterator delete_iter = lines_to_be_deleted.begin(); delete_iter != lines_to_be_deleted.end(); delete_iter++)
                            // delete all the lines that did not fulfill the maybe worthwhile to wait rule
                        {
                            auto line_to_delete = (*line_set_iter).end();
                            for (auto lines_iter = (*line_set_iter).begin(); lines_iter < (*line_set_iter).end(); lines_iter++)
                            {
                                if ((*delete_iter).first->get_id() == (*lines_iter)->get_id() && (*delete_iter).second)
                                {
                                    line_to_delete = lines_iter;
                                    break;
                                }
                            }
                            if ((*delete_iter).second && line_to_delete != (*line_set_iter).end())
                            {
                                (*line_set_iter).erase(line_to_delete);
                            }
                        }
                    }
                }
                /*
                // apply dominancy rule at stop level
                vector<vector<Busstop*> > alt_stops = (*path)->get_alt_transfer__stops();
                for (vector<vector<Busstop*> >::iterator stop_set_iter = alt_stops.begin()+1; stop_set_iter < alt_stops.end(); stop_set_iter+2)
                    // goes over connected stops (even locations - 2,4,...)
                {
                    map <Busstop*,bool> stops_to_be_deleted;
                    for (vector <Busstop*>::iterator stop_iter = (*stop_set_iter).begin(); stop_iter < (*stop_set_iter).end(); stop_iter++)
                    {
                        stops_to_be_deleted[(*stop_iter)] = false;
                    }
                    for (vector <Busstop*>::iterator stop_iter = (*stop_set_iter).begin(); stop_iter < (*stop_set_iter).end()-1; stop_iter++)
                    {
                        for (vector <Busstop*>::iterator stop_iter1 = stop_iter + 1; stop_iter < (*stop_set_iter).end(); stop_iter1++)
                        {
                            if
                        }
                    }
                    for (map<Busline*, bool>::iterator lines_to_include_iter = lines_to_include.begin(); lines_to_include_iter != lines_to_include.end(); lines_to_include_iter++)
                    {
                        if ((*lines_to_include_iter).second == false)
                        // delete the stop that is been dominated
                        {
                            stops_to_be_deleted[(*stops_to_include_iter).first] = true;
                        }
                    }
                    for (map <Busstop*,bool>::iterator delete_iter = stops_to_be_deleted.begin(); delete_iter != stops_to_be_deleted.end(); delete_iter++)
                    // delete all the stops that are been dominated
                    {
                        vector<Busstop*>::iterator stop_to_delete;
                        for (vector<Busstop*>::iterator stops_iter = (*stop_set_iter).begin(); stops_iter < (*stop_set_iter).end(); stops_iter++)
                        {
                            if ((*delete_iter).first->get_id() == (*stops_iter)->get_id() && (*delete_iter).second == true)
                            {
                                stop_to_delete = stops_iter;
                                break;
                            }
                        }
                        if ((*delete_iter).second == true)
                        {
                            (*stop_set_iter).erase(stop_to_delete);
                        }
                    }
                }
                */
                for (map <Pass_path*,bool>::iterator delete_iter = paths_to_be_deleted.begin(); delete_iter != paths_to_be_deleted.end(); delete_iter++)
                    // delete all the paths that did not fulfill the filtering rules
                {
                    vector<Pass_path*>::iterator path_to_delete;
                    for (auto paths_iter = path_set.begin(); paths_iter < path_set.end(); paths_iter++)
                    {
                        if ((*delete_iter).first->get_id() == (*paths_iter)->get_id() && (*delete_iter).second)
                        {
                            path_to_delete = paths_iter;
                            break;
                        }
                    }
                    if ((*delete_iter).second)
                    {
                        delete *path_to_delete;
                        path_set.erase(path_to_delete);
                    }
                }
            }
            odpairs->set_path_set(path_set);
        }
    }
}

void Network::dominancy_rules (Busstop* stop)
{
    // applying static dominancy rules on the transit path choice set
    // relevant aspects: number of transfers, total IVT, total walking distance and further downstream transfer stop
    for (auto basic_destination = busstops.begin(); basic_destination < busstops.end(); basic_destination++)
    {
        if (stop->get_id() != (*basic_destination)->get_id() && stop->check_destination_stop(*basic_destination))
        {
            ODstops* odpairs = stop->get_stop_od_as_origin_per_stop(*basic_destination);
            vector <Pass_path*> path_set = odpairs->get_path_set();
            if (path_set.size() > 1)
            {
                // go over all the OD pairs of stops that have more than a single alternative
                map <Pass_path*,bool> paths_to_be_deleted;
                for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
                {
                    paths_to_be_deleted[(*path_iter)] = false;
                }
                for (auto path1 = path_set.begin(); path1 < path_set.end()-1; path1++)
                {
                    for (auto path2 = path1 + 1; path2 < path_set.end(); path2++)
                    {
                        // check if path1 dominates path2
                        if ((*path1)->find_number_of_transfers() < (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) <= (*path2)->calc_total_scheduled_in_vehicle_time(0.0) && (*path1)->calc_total_walking_distance() <= (*path2)->calc_total_walking_distance())
                        {
                            paths_to_be_deleted[(*path2)] = true;
                            break;
                        }
                        if ((*path1)->find_number_of_transfers() <= (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) * (1 + theParameters->dominancy_perception_threshold) < (*path2)->calc_total_scheduled_in_vehicle_time(0.0) && (*path1)->calc_total_walking_distance() * (1 + theParameters->dominancy_perception_threshold) <= (*path2)->calc_total_walking_distance())
                        {
                            paths_to_be_deleted[(*path2)] = true;
                            break;
                        }
                        if ((*path1)->find_number_of_transfers() <= (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) * (1 + theParameters->dominancy_perception_threshold) <= (*path2)->calc_total_scheduled_in_vehicle_time(0.0)&& (*path1)->calc_total_walking_distance() * (1 + theParameters->dominancy_perception_threshold) < (*path2)->calc_total_walking_distance())
                        {
                            paths_to_be_deleted[(*path2)] = true;
                            break;
                        }
                        // check if path2 dominates path1
                        if ((*path1)->find_number_of_transfers() > (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) >= (*path2)->calc_total_scheduled_in_vehicle_time(0.0) && (*path1)->calc_total_walking_distance() >= (*path2)->calc_total_walking_distance())
                        {
                            paths_to_be_deleted[(*path1)] = true;
                            break;
                        }
                        if ((*path1)->find_number_of_transfers() >= (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) > (*path2)->calc_total_scheduled_in_vehicle_time(0.0) * (1+ theParameters->dominancy_perception_threshold)&& (*path1)->calc_total_walking_distance() >= (*path2)->calc_total_walking_distance() * (1 + theParameters->dominancy_perception_threshold))
                        {

                            paths_to_be_deleted[(*path1)] = true;
                            break;
                        }
                        if ((*path1)->find_number_of_transfers() >= (*path2)->find_number_of_transfers() && (*path1)->calc_total_scheduled_in_vehicle_time(0.0) >= (*path2)->calc_total_scheduled_in_vehicle_time(0.0) * (1+ theParameters->dominancy_perception_threshold)&& (*path1)->calc_total_walking_distance() > (*path2)->calc_total_walking_distance() * (1 + theParameters->dominancy_perception_threshold))
                        {
                            paths_to_be_deleted[(*path1)] = true;
                            break;
                        }
                    }
                }
                for (map <Pass_path*,bool>::iterator delete_iter = paths_to_be_deleted.begin(); delete_iter != paths_to_be_deleted.end(); delete_iter++)
                    // delete all the dominated paths
                {
                    vector<Pass_path*>::iterator path_to_delete;
                    for (auto paths_iter = path_set.begin(); paths_iter < path_set.end(); paths_iter++)
                    {
                        if ((*delete_iter).first->get_id() == (*paths_iter)->get_id() && (*delete_iter).second)
                        {
                            path_to_delete = paths_iter;
                            break;
                        }
                    }
                    if ((*delete_iter).second)
                    {
                        delete *path_to_delete;
                        path_set.erase(path_to_delete);
                    }
                }
            }
            odpairs->set_path_set(path_set);
        }
    }
}

bool Network::totaly_dominancy_rule (ODstops* odstops, vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops) // check if there is already a path with shorter IVT than the potential one
{
    vector <Pass_path*> path_set = odstops->get_path_set();
    if (path_set.empty()) // if it has no othe paths yet - then there is nothing to compare
    {
        return false;
    }
    map <Pass_path*,bool> paths_to_be_deleted;
    for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
    {
        paths_to_be_deleted[(*path_iter)] = false;
    }
    for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
    {
        vector<vector<Busline*> > lines1 = (*path_iter)->get_alt_lines();
        vector<vector<Busstop*> > stops1 = (*path_iter)->get_alt_transfer_stops();
        double total_walk = 0.0;
        for (auto walk_iter = collect_walking_distances.begin(); walk_iter < collect_walking_distances.end(); walk_iter++)
        {
            total_walk += (*walk_iter);
        }
        if (calc_total_in_vechile_time(lines1, stops1) * (1 + theParameters->dominancy_perception_threshold) < calc_total_in_vechile_time(lines,stops))
        {
            if ((*path_iter)->calc_total_walking_distance() <= total_walk)
            {
                if (lines1.size() <= lines.size())
                {
                    // the proposed path is dominated - does not fulfill constraints
                    return true;
                }
            }
        }
        if (lines1.size() < lines.size())
        {
            if (calc_total_in_vechile_time(lines1, stops1) <= calc_total_in_vechile_time(lines,stops))
            {
                if ((*path_iter)->calc_total_walking_distance() <= total_walk )
                {
                    return true;
                }
            }
        }
        if (calc_total_in_vechile_time(lines, stops) * (1 + theParameters->dominancy_perception_threshold) < calc_total_in_vechile_time(lines1,stops1))
            // the proposed path dominates previous paths - delete the dominated path
        {
            if (total_walk <= (*path_iter)->calc_total_walking_distance())
            {
                if (lines.size() <= lines1.size())
                {
                    paths_to_be_deleted[(*path_iter)] = true;
                }
            }
        }
        if (lines.size() < lines1.size())
        {
            if (calc_total_in_vechile_time(lines, stops) <= calc_total_in_vechile_time(lines1,stops1))
            {
                if (total_walk  <= (*path_iter)->calc_total_walking_distance())
                {
                    paths_to_be_deleted[(*path_iter)] = true;
                }
            }
        }
    }
    for (map <Pass_path*,bool>::iterator delete_iter = paths_to_be_deleted.begin(); delete_iter != paths_to_be_deleted.end(); delete_iter++)
        // delete all the dominated paths
    {
        vector<Pass_path*>::iterator path_to_delete;
        for (auto paths_iter = path_set.begin(); paths_iter < path_set.end(); paths_iter++)
        {
            if ((*delete_iter).first->get_id() == (*paths_iter)->get_id() && (*delete_iter).second)
            {
                path_to_delete = paths_iter;
                break;
            }
        }
        if ((*delete_iter).second)
        {
            path_set.erase(path_to_delete);
        }
    }
    odstops->set_path_set(path_set);
    return false;
}

double Network::calc_total_in_vechile_time (vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops)
{
    double sum_in_vehicle_time = 0.0;
    auto iter_stops = stops.begin();
    iter_stops++; // starting from the second stop
    for (auto iter_lines = lines.begin(); iter_lines < lines.end(); iter_lines++)
    {
        sum_in_vehicle_time += (*iter_lines).front()->calc_curr_line_ivt((*iter_stops).front(),(*(iter_stops+1)).front(), stops.front().front()->get_rti(), 0.0);
        iter_stops++;
        iter_stops++;
    }
    return (sum_in_vehicle_time);
}

/*
bool Network::downstream_dominancy_rule (Pass_path* check_path)
{
    vector<vector<Busstop*> > alt_stops = check_path->get_alt_transfer__stops();
    vector<vector<Busline*> > alt_lines = check_path->get_alt_lines();
    vector<vector<Busline*> >::iterator alt_lines_iter = alt_lines.begin();
    for (vector<vector<Busstop*> >::iterator alt_stops_iter = alt_stops.begin()+2; alt_stops_iter < alt_stops.end(); alt_stops_iter = alt_stops_iter + 2)
    {
        if ((*alt_stops_iter).size() > 1) // no need if there is only one transfer stop
        {
            map <Busstop*,bool> curr_stops_to_be_deleted, next_stops_to_be_deleted;
            // initialize all stops in current and next legs
            for (vector <Busstop*>::iterator stop_iter = (*alt_stops_iter).begin(); stop_iter < (*alt_stops_iter).end(); stop_iter++)
            {
                curr_stops_to_be_deleted[(*stop_iter)] = false;
            }
            for (vector <Busstop*>::iterator stop_iter = (*(alt_stops_iter+1)).begin(); stop_iter < (*(alt_stops_iter+1)).end(); stop_iter++)
            {
                next_stops_to_be_deleted[(*stop_iter)] = false;
            }
    !!!		if (what is the right condition here?)
            {
                for (vector<Busstop*>::iterator curr_stops = (*alt_stops_iter).begin()+1; curr_stops < (*alt_stops_iter).end(); curr_stops++)
                {
                    curr_stops_to_be_deleted[(*curr_stops)] = true;
                }
                for (vector<Busstop*>::iterator next_stops = (*(alt_stops_iter+1)).begin()+1; next_stops < (*(alt_stops_iter+1)).end(); next_stops++)
                {
                    next_stops_to_be_deleted[(*next_stops)] = true;
                }
                for (map <Busstop*,bool>::iterator delete_iter = curr_stops_to_be_deleted.begin(); delete_iter != curr_stops_to_be_deleted.end(); delete_iter++)
                // delete all the stops that are been dominated by upstream stops
                {
                    vector<Busstop*>::iterator stop_to_delete;
                    for (vector<Busstop*>::iterator stops_iter = (*alt_stops_iter).begin(); stops_iter < (*alt_stops_iter).end(); stops_iter++)
                    {
                        if ((*delete_iter).first->get_id() == (*stops_iter)->get_id() && (*delete_iter).second == true)
                        {
                            stop_to_delete = stops_iter;
                            break;
                        }
                    }
                    if ((*delete_iter).second == true)
                    {
                        (*alt_stops_iter).erase(stop_to_delete);
                    }
                }
                for (map <Busstop*,bool>::iterator delete_iter = next_stops_to_be_deleted.begin(); delete_iter != next_stops_to_be_deleted.end(); delete_iter++)
                // delete all the stops that are been dominated by upstream stops
                {
                    vector<Busstop*>::iterator stop_to_delete;
                    for (vector<Busstop*>::iterator stops_iter = (*(alt_stops_iter+1)).begin(); stops_iter < (*(alt_stops_iter+1)).end(); stops_iter++)
                    {
                        if ((*delete_iter).first->get_id() == (*stops_iter)->get_id() && (*delete_iter).second == true)
                        {
                            stop_to_delete = stops_iter;
                            break;
                        }
                    }
                    if ((*delete_iter).second == true)
                    {
                        (*(alt_stops_iter+1)).erase(stop_to_delete);
                    }
                }
            }
        }
        alt_lines_iter++;
    }
    return true;
}
*/

bool Network::compare_same_lines_paths (Pass_path* path1, Pass_path* path2)
// checks if two paths are identical in terms of lines
{
    if (path1->get_number_of_transfers() == path2->get_number_of_transfers())
    {
        vector <bool> is_shared;
        vector <vector <Busline*> > lines1 = path1->get_alt_lines();
        vector <vector <Busline*> > lines2 = path2->get_alt_lines();
        if (!lines1.empty() && !lines2.empty())
        {
            auto lines2_iter = lines2.begin();
            for (auto lines1_iter = lines1.begin(); lines1_iter < lines1.end(); lines1_iter++)
            {
                for (auto leg_lines1 = (*lines1_iter).begin(); leg_lines1 < (*lines1_iter).end(); leg_lines1++)
                {
                    is_shared.push_back(false);
                    for (auto leg_lines2 = (*lines2_iter).begin(); leg_lines2 < (*lines2_iter).end(); leg_lines2++)
                    {
                        if ((*leg_lines1)->get_id() == (*leg_lines2)->get_id())
                        {
                            is_shared.pop_back();
                            is_shared.push_back(true); // found the corresponding
                            break;
                        }
                    }
                    if (!is_shared.back()) // this line is not shared - no need to continue
                    {
                        return false;
                    }
                }
                lines2_iter++;
            }
            return true;
        }
        return false; // accounts for walking-only alternatives
    }
    
    
        return false;
    
}

bool Network::compare_same_stops_paths (Pass_path* path1, Pass_path* path2)
// checks if two paths are identical in terms of stops
{
    if (path1->get_number_of_transfers() == path2->get_number_of_transfers())
    {
        vector <bool> is_shared;
        vector <vector <Busstop*> > stops1 = path1->get_alt_transfer_stops();
        vector <vector <Busstop*> > stops2 = path2->get_alt_transfer_stops();
        auto stops2_iter = stops2.begin();
        for (auto stops1_iter = stops1.begin(); stops1_iter < stops1.end(); stops1_iter++)
        {
            for (auto leg_stops1 = (*stops1_iter).begin(); leg_stops1 < (*stops1_iter).end(); leg_stops1++)
            {
                is_shared.push_back(false);
                for (auto leg_stops2 = (*stops2_iter).begin(); leg_stops2 < (*stops2_iter).end(); leg_stops2++)
                {
                    if ((*leg_stops1)->get_id() == (*leg_stops2)->get_id())
                    {
                        is_shared.pop_back();
                        is_shared.push_back(true); // found the corresponding
                        break;
                    }
                }
                if (!is_shared.back()) // this stop is not shared - no need to continue
                {
                    return false;
                }
            }
            stops2_iter++;
        }
        return true;
    }
    
    
        return false;
    
}

bool Network::compare_common_partial_routes (Busline* line1, Busline* line2, Busstop* start_section, Busstop* end_section) // checks if two lines have the same route between two given stops
{
    vector<Busstop*>::iterator iter_stop1;
    vector<Busstop*>::iterator iter_stop2;
    // found the pointer to starting point on both lines
    int stop_on_route_counter = 0;
    for (iter_stop1 = line1->stops.begin(); iter_stop1 < line1->stops.end(); iter_stop1++)
    {
        if ((*iter_stop1)->get_id() == start_section->get_id())
        {
            stop_on_route_counter++;
            break;
        }
    }
    for (iter_stop2 = line2->stops.begin(); iter_stop2 < line2->stops.end(); iter_stop2++)
    {
        if ((*iter_stop2)->get_id() == start_section->get_id())
        {
            stop_on_route_counter++;
            break;
        }
    }
    if (stop_on_route_counter < 2)
    {
        return false;
    }
    
    
        for (; (*iter_stop1)->get_id() != end_section->get_id(); iter_stop1++)
        {
            if ((*iter_stop1)->get_id() != (*iter_stop2)->get_id())
            {
                return false;
            }
            iter_stop2++;
            if (iter_stop2 == line2->stops.end())
            {
                return false;
            }
            if (iter_stop1 == line1->stops.end()-1)
            {
                return false;
            }
        }
        if ((*(iter_stop2))->get_id() != end_section->get_id())
        {
            return false;
        }
    
    return true;
}

bool Network::check_constraints_paths (Pass_path* path) // checks if the path meets all the constraints
{
    if (path->get_alt_transfer_stops().size() <= 4)
    {
        return true;
    }
    if (!check_path_no_repeating_lines(path->get_alt_lines(), path->get_alt_transfer_stops()))
    {
        return false;
    }
    if (!check_path_no_repeating_stops(path))
    {
        return false;
    }
    if (!check_path_no_opposing_lines(path->get_alt_lines()))
    {
        return false;
    }
    return true;
}

bool Network::check_path_no_repeating_lines (vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops_) // checks if the path does not include going on and off the same bus line at the same stop
{
    if (lines.size() < 2)
    {
        return true;
    }
    auto stops = stops_.begin()+1;
    for (auto lines_iter1 = lines.begin(); lines_iter1 < (lines.end()-1); lines_iter1++)
    {
        stops = stops + 2; // start at fourth place and then jumps two every next pair of lines
        for (auto leg_lines1 = (*lines_iter1).begin(); leg_lines1 < (*lines_iter1).end(); leg_lines1++)
        {
            for (auto lines_iter2 = lines_iter1 + 1; lines_iter2 < lines.end(); lines_iter2++)
            {
                int counter_sim = 0;
                // checking all legs
                for (auto leg_lines2 = (*lines_iter2).begin(); leg_lines2 < (*lines_iter2).end(); leg_lines2++)
                {
                    // checking whether the two lines have the same path on the relevant segment (if so - no reason for transfer)
                    if ((*leg_lines1)->stops.size() != (*leg_lines2)->stops.size())
                    {

                    }
                    if ((*leg_lines1)->get_id() == (*leg_lines2)->get_id() || compare_common_partial_routes((*leg_lines1),(*leg_lines2),(*stops).front(), (*(stops+1)).front()))
                    {
                        counter_sim++;
                    }
                }
                if (counter_sim == static_cast<int>((*lines_iter2).size()))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

/*
bool Network::check_sequence_no_repeating_lines(vector<vector<Busline*> > lines, vector<Busstop*> stops_)
{
    vector <Busstop*>::iterator stops = stops_.begin();
    for (vector <vector <Busline*> >::iterator lines_iter1 = lines.begin(); lines_iter1 < (lines.end()-1); lines_iter1++)
    {
        for (vector <Busline*>::iterator leg_lines1 = (*lines_iter1).begin(); leg_lines1 < (*lines_iter1).end(); leg_lines1++)
        {
            for (vector <vector <Busline*> >::iterator lines_iter2 = lines_iter1 + 1; lines_iter2 < lines.end(); lines_iter2++)
            {
                int counter_sim = 0;
                // checking all legs
                for (vector <Busline*>::iterator leg_lines2 = (*lines_iter2).begin(); leg_lines2 < (*lines_iter2).end(); leg_lines2++)
                {
                    if ((*leg_lines1)->get_id() == (*leg_lines2)->get_id() || compare_common_partial_routes((*leg_lines1),(*leg_lines2),(*stops), (*(stops+1))) == true)
                    {
                        counter_sim++;
                    }
                }
                if (counter_sim == ((*lines_iter2).size()))
                {
                    return false;
                }
            }
        }
        stops++;
    }
    return true;
}
*/

bool Network::check_path_no_repeating_stops (Pass_path* path) // chceks if the path does not include going through the same stop more than once
{
    vector <vector <Busstop*> > stops = path->get_alt_transfer_stops();
    for (auto stops_iter1 = stops.begin(); stops_iter1 < stops.end(); stops_iter1++)
    {
        for (auto leg_stops1 = (*stops_iter1).begin(); leg_stops1 < (*stops_iter1).end()-1; leg_stops1++)
        {
            for (auto leg_stops2 = leg_stops1 + 1; leg_stops1 < (*stops_iter1).end(); leg_stops1++)
                // go over the consecutive stops in the same leg
            {
                if ((*leg_stops1)->get_id() == (*leg_stops2)->get_id())
                {
                    return false;
                }
            }
            // go over all the stops in consecutive legs
            for (auto stops_iter2 = stops_iter1+1; stops_iter2 < stops.end(); stops_iter2++)
            {
                for (auto leg_stops2 = (*stops_iter2).begin(); leg_stops1 < (*stops_iter1).end()-1; leg_stops1++)
                {
                    if ((*leg_stops1)->get_id() == (*leg_stops2)->get_id())
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool Network::check_sequence_no_repeating_stops (vector<Busstop*> stops) // chceks if the sequence does not include going through the same stop more than once
{
    for (auto stops_iter1 = stops.begin()+1; stops_iter1 < stops.end()-1; stops_iter1 = stops_iter1+2)
    {
        for (auto stops_iter2 = stops_iter1+1; stops_iter2 < stops.end(); stops_iter2++)
        {
            if ((*stops_iter1)->get_id() == (*stops_iter2)->get_id())
            {
                return false;
            }
        }
    }
    return true;
}

bool Network::check_path_no_opposing_lines (vector<vector<Busline*> > lines)
{
    if (lines.empty()) {
        return true;
}

    for (auto lines_leg = lines.begin(); lines_leg < lines.end()-1; lines_leg++)
    {
        for (auto lines_leg1 = lines_leg + 1; lines_leg1 < lines.end(); lines_leg1++)
        {
            for (auto line_iter = (*lines_leg).begin(); line_iter < (*lines_leg).end(); line_iter++)
            {
                for (auto line_iter1 = (*lines_leg1).begin(); line_iter1 < (*lines_leg1).end(); line_iter1++)
                {
                    if ((*line_iter)->get_opposite_id() == (*line_iter1)->get_id() || (*line_iter)->get_id() == (*line_iter1)->get_opposite_id())
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

unsigned int Network::count_transit_paths()
{
    unsigned int pathcounter = 0;
    for (auto stop_iter = busstops.begin(); stop_iter < busstops.end(); stop_iter++)
    {
        ODs_for_stop odstops = (*stop_iter)->get_stop_as_origin();
        for (const auto& odstop : odstops)
        {
            vector <Pass_path*> path_set = odstop.second->get_path_set();
            for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
            {
                pathcounter++;
            }
        }
    }
    return pathcounter;
}

bool Network::write_path_set (string name1)
{
    ofstream out1(name1.c_str(),ios_base::app);
    assert (out1);
    int pathcounter = 0;
    out1 << "transit_paths:" << '\t' ;
    for (auto stop_iter = busstops.begin(); stop_iter < busstops.end(); stop_iter++)
    {
        ODs_for_stop odstops = (*stop_iter)->get_stop_as_origin();
        for (auto & odstop : odstops)
        {
            vector <Pass_path*> path_set = odstop.second->get_path_set();
            for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
            {
                pathcounter++;
                vector<vector<Busstop*> > stops = (*path_iter)->get_alt_transfer_stops();
                vector<vector<Busline*> > lines = (*path_iter)->get_alt_lines();
                vector<double> walking = (*path_iter)->get_walking_distances();
                out1 << odstop.second->get_origin()->get_id() << '\t'<< odstop.second->get_destination()->get_id() << '\t' << (*path_iter)->get_id()  << '\t' << stops.size() << '\t' << '{' << '\t';
                for (auto stop_leg = stops.begin(); stop_leg < stops.end(); stop_leg++)
                {
                    out1 << (*stop_leg).size() << '\t' << '{' << '\t';
                    for (auto stop_iter = (*stop_leg).begin(); stop_iter < (*stop_leg).end(); stop_iter++)
                    {
                        out1 << (*stop_iter)->get_id() << '\t';
                    }
                    out1 << '}' << '\t' ;
                }
                out1 << '}' << '\t' << lines.size() << '\t' << '{';
                for (auto line_leg = lines.begin(); line_leg < lines.end(); line_leg++)
                {
                    out1 << (*line_leg).size() << '{' << '\t';
                    for (auto line_iter = (*line_leg).begin(); line_iter < (*line_leg).end(); line_iter++)
                    {
                        out1 << (*line_iter)->get_id() << '\t';
                    }
                    out1 << '}' << '\t' ;
                }
                out1 << '}' << '\t' << walking.size() << '\t' << '{';
                for (auto walk_iter = walking.begin(); walk_iter < walking.end(); walk_iter++)
                {
                    out1 << (*walk_iter) << '\t';
                }
                out1 << '}' << endl;
            }
        }
    }
    out1 << "nr_paths:" << '\t' << pathcounter << '\t' <<  endl;
    return true;
}

bool Network::write_path_set_per_stop (string name1, Busstop* stop)
{
    ofstream out1(name1.c_str(),ios_base::app);
    assert (out1);
    ODs_for_stop odstops = stop->get_stop_as_origin();
    for (auto & odstop : odstops)
    {
        vector <Pass_path*> path_set = odstop.second->get_path_set();
        for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
        {
            vector<vector<Busstop*> > stops = (*path_iter)->get_alt_transfer_stops();
            vector<vector<Busline*> > lines = (*path_iter)->get_alt_lines();
            vector<double> walking = (*path_iter)->get_walking_distances();
            out1 << odstop.second->get_origin()->get_id() << '\t'<< odstop.second->get_destination()->get_id() << '\t' << (*path_iter)->get_id()  << '\t' << stops.size() << '\t' << '{' << '\t';
            for (auto stop_leg = stops.begin(); stop_leg < stops.end(); stop_leg++)
            {
                out1 << (*stop_leg).size() << '\t' << '{' << '\t';
                for (auto stop_iter = (*stop_leg).begin(); stop_iter < (*stop_leg).end(); stop_iter++)
                {
                    out1 << (*stop_iter)->get_id() << '\t';
                }
                out1 << '}' << '\t' ;
            }
            out1 << '}' << '\t' << lines.size() << '\t' << '{';
            for (auto line_leg = lines.begin(); line_leg < lines.end(); line_leg++)
            {
                out1 << (*line_leg).size() << '{' << '\t';
                for (auto line_iter = (*line_leg).begin(); line_iter < (*line_leg).end(); line_iter++)
                {
                    out1 << (*line_iter)->get_id() << '\t';
                }
                out1 << '}' << '\t' ;
            }
            out1 << '}' << '\t' << walking.size() << '\t' << '{';
            for (auto walk_iter = walking.begin(); walk_iter < walking.end(); walk_iter++)
            {
                out1 << (*walk_iter) << '\t';
            }
            out1 << '}' << endl;
        }
    }
    return true;
}

bool Network::write_path_set_per_od (string name1, Busstop* origin_stop, Busstop* destination_stop)
{
    ofstream out1(name1.c_str(),ios_base::app);
    assert (out1);
    ODstops* od = origin_stop->get_stop_od_as_origin_per_stop(destination_stop);
    vector<Pass_path*> path_set = od->get_path_set();
    for (auto path_iter = path_set.begin(); path_iter < path_set.end(); path_iter++)
    {
        vector<vector<Busstop*> > stops = (*path_iter)->get_alt_transfer_stops();
        vector<vector<Busline*> > lines = (*path_iter)->get_alt_lines();
        vector<double> walking = (*path_iter)->get_walking_distances();
        out1 << od->get_origin()->get_id() << '\t'<< od->get_destination()->get_id() << '\t' << (*path_iter)->get_id()  << '\t' << stops.size() << '\t' << '{' << '\t';
        for (auto stop_leg = stops.begin(); stop_leg < stops.end(); stop_leg++)
        {
            out1 << (*stop_leg).size() << '\t' << '{' << '\t';
            for (auto stop_iter = (*stop_leg).begin(); stop_iter < (*stop_leg).end(); stop_iter++)
            {
                out1 << (*stop_iter)->get_id() << '\t';
            }
            out1 << '}' << '\t' ;
        }
        out1 << '}' << '\t' << lines.size() << '\t' << '{';
        for (auto line_leg = lines.begin(); line_leg < lines.end(); line_leg++)
        {
            out1 << (*line_leg).size() << '{' << '\t';
            for (auto line_iter = (*line_leg).begin(); line_iter < (*line_leg).end(); line_iter++)
            {
                out1 << (*line_iter)->get_id() << '\t';
            }
            out1 << '}' << '\t' ;
        }
        out1 << '}' << '\t' << walking.size() << '\t' << '{';
        for (auto walk_iter = walking.begin(); walk_iter < walking.end(); walk_iter++)
        {
            out1 << (*walk_iter) << '\t';
        }
        out1 << '}' << endl;
    }
    return true;
}

bool Network::read_transit_path_sets(string name)
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="transit_paths:")
    {
        cout << " read_transit_path_sets: no << transit_paths: >> keyword " << endl;
        in.close();
        return false;
    }
    int nr= 0;
    in >> nr;
    int i=0;
    for (; i<nr;i++)
    {
        if (!read_transit_path(in))
        {
            cout << " read_transit_path_sets: read_transit_path_sets returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    cout << "Reading path set completed" << endl;
    return true;
}

bool Network::read_transit_path(istream& in)
{
    char bracket;
    int origin_id;
    int destination_id;
    int nr_stop_legs;
    int nr_line_legs;
    int nr_stops;
    int nr_lines;
    int nr_walks;
    int stop_id;
    int line_id;
    double walk_dis;
    string path_id;
    vector<vector<Busstop*> > alt_stops;
    vector<vector<Busline*> > alt_lines;
    vector<double> alt_walking;
    in >> origin_id >> destination_id >> path_id >> nr_stop_legs;

    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_id) )));
    // reading all the stops that compose the path
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i<nr_stop_legs; i++)
    {
        in >> nr_stops;
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::read_transit_path scanner jammed at " << bracket;
            return false;
        }
        vector<Busstop*> stops_leg;
        for (int j=0; j<nr_stops; j++)
        {
            in >> stop_id;
            Busstop* bs = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
            stops_leg.push_back(bs);
        }
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::read_transit_path scanner jammed at " << bracket;
            return false;
        }
        alt_stops.push_back(stops_leg);
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }

    // reading all the lines that compose the path
    in >> nr_line_legs >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i<nr_line_legs; i++)
    {
        in >> nr_lines;
        if (nr_lines < 1)
        {
            alt_lines.clear();
        }
        in >> bracket;
        if (bracket != '{')
        {
            cout << "readfile::read_transit_path scanner jammed at " << bracket;
            return false;
        }
        vector<Busline*> lines_leg;
        for (int j=0; j<nr_lines; j++)
        {
            in >> line_id;
            Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));
            lines_leg.push_back(bl);
        }
        in >> bracket;
        if (bracket != '}')
        {
            cout << "readfile::read_transit_path scanner jammed at " << bracket;
            return false;
        }
        alt_lines.push_back(lines_leg);
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }

    // reading the walking distances that compose the path
    in >> nr_walks;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    for (int j=0; j<nr_walks; j++)
    {
        in >> walk_dis;
        alt_walking.push_back(walk_dis);
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }

    Pass_path* path = new Pass_path(pathid, alt_lines , alt_stops, alt_walking);
    pathid++;
    if (!bs_o->check_stop_od_as_origin_per_stop(bs_d))
    {
        auto* od_stop = new ODstops (bs_o,bs_d);
        bs_o->add_odstops_as_origin(bs_d, od_stop);
        bs_d->add_odstops_as_destination(bs_o, od_stop);
        odstops.push_back(od_stop);
    }
    ODstops* od_pair = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_pair->add_paths(path);
    return true;
}

/////////////// Transit path-set generation functions: end

bool Network::read_transitday2day(string name)
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK

    if (keyword == "salience:")
    {
        float v;
        in >> v;
        day2day->set_salience(v);
        in >> keyword;
    }
    if (keyword == "crowding_salience:")
    {
        float v_c;
        in >> v_c;
        day2day->set_crowding_salience(v_c);
        in >> keyword;
    }
    if (keyword == "trust:")
    {
        float v1;
        in >> v1;
        day2day->set_trust(v1);
        in >> keyword;
    }
    if (keyword == "recency:")
    {
        float r;
        in >> r;
        day2day->set_recency(r);
        in >> keyword;
    }
    if (keyword!="ODS:")
    {
        cout << " read_ODs: no << ODs: >> keyword " << endl;
        in.close();
        return false;
    }
    int nr= 0;
    in >> nr;
    int i=0;
    for (; i<nr;i++)
    {
        if (!read_OD_day2day(in))
        {
            cout << " read_OD_day2day: read_OD_day2day returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    return true;
}

bool Network::read_transitday2day(map<ODSL, Travel_time>& wt_map)
{
    if (theParameters->pass_day_to_day_indicator == 1)
    {
        for (auto & row : wt_map)
        {
            read_OD_day2day(row);
        }
    }
    else if (theParameters->pass_day_to_day_indicator == 2)
    {
        for (auto & row : wt_map)
        {
            read_pass_day2day(row);
        }
    }

    return true;
}

bool Network::read_IVTT_day2day(string name)
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="ODS:")
    {
        cout << " read_ODs: no << ODs: >> keyword " << endl;
        in.close();
        return false;
    }
    int nr= 0;
    in >> nr;
    int i=0;
    for (; i<nr;i++)
    {
        if (!read_OD_IVTT(in))
        {
            cout << " read_OD_day2day: read_OD_day2day returned false for line nr " << (i+1) << endl;
            in.close();
            return false;
        }
    }
    return true;
}

bool Network::read_IVTT_day2day(map<ODSLL, Travel_time>& ivt_map)
{
    if (theParameters->in_vehicle_d2d_indicator == 1)
    {
        for (auto & row : ivt_map)
        {
            read_OD_IVTT(row);
        }
    }
    else if (theParameters->in_vehicle_d2d_indicator == 2)
    {
        for (auto & row : ivt_map)
        {
            read_pass_IVTT(row);
        }
    }

    return true;
}

bool Network::read_OD_day2day (istream& in)
{
    char bracket;
    int origin_stop_id;
    int destination_stop_id;
    int stop_id;
    int line_id;
    double anticipated_waiting_time;
    double alpha_RTI;
    double alpha_exp;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    in >> origin_stop_id >> destination_stop_id >> stop_id >> line_id >> anticipated_waiting_time >> alpha_RTI >> alpha_exp;
    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));
    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_stop->set_anticipated_waiting_time(bs_s, bl, anticipated_waiting_time);
    od_stop->set_alpha_RTI(bs_s, bl, alpha_RTI);
    od_stop->set_alpha_exp(bs_s, bl, alpha_exp);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    return true;
}

enum k {EXP, PK, RTI, anticip, anticip_EXP};
enum l {e0, e1, crowding, e3, e4};

bool Network::read_OD_day2day (pair<const ODSL, Travel_time>& wt_row)
{
    const int& origin_stop_id = wt_row.first.orig;
    const int& destination_stop_id = wt_row.first.dest;
    const int& stop_id = wt_row.first.stop;
    const int& line_id = wt_row.first.line;
    double anticipated_waiting_time = wt_row.second.tt[anticip_EXP];
    double alpha_RTI = wt_row.second.alpha[RTI];
    double alpha_exp = wt_row.second.alpha[EXP];

    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));

    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_stop->set_anticipated_waiting_time(bs_s, bl, anticipated_waiting_time);
    od_stop->set_alpha_RTI(bs_s, bl, alpha_RTI);
    od_stop->set_alpha_exp(bs_s, bl, alpha_exp);

    return true;
}

bool Network::read_pass_day2day (pair<const ODSL, Travel_time>& wt_row)
{
    const int& pass_id = wt_row.first.pid;
    const int& origin_stop_id = wt_row.first.orig;
    const int& destination_stop_id = wt_row.first.dest;
    const int& stop_id = wt_row.first.stop;
    const int& line_id = wt_row.first.line;
    double anticipated_waiting_time = wt_row.second.tt[anticip_EXP];
    double alpha_RTI = wt_row.second.alpha[RTI];
    double alpha_exp = wt_row.second.alpha[EXP];

    Busstop* bs_o = *(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) ));
    Busstop* bs_d = *(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) ));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = *(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) ));

    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    vector<Passenger*> passengers = od_stop->get_passengers_during_simulation();
    Passenger* p = *find_if(passengers.begin(), passengers.end(), compare<Passenger>(pass_id));

    p->set_anticipated_waiting_time(bs_s, bl, anticipated_waiting_time);
    p->set_alpha_RTI(bs_s, bl, alpha_RTI);
    p->set_alpha_exp(bs_s, bl, alpha_exp);

    return true;
}

bool Network::read_OD_IVTT (istream& in)
{
    char bracket;
    int origin_stop_id;
    int destination_stop_id;
    int stop_id;
    int line_id;
    int leg_id;
    double anticipated_in_vehicle_time;
    double alpha_exp;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    in >> origin_stop_id >> destination_stop_id >> stop_id >> line_id >> leg_id >> anticipated_in_vehicle_time >> alpha_exp;
    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));
    Busstop* bs_l = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (leg_id) )));
    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_stop->set_anticipated_ivtt(bs_s, bl, bs_l, anticipated_in_vehicle_time);
    od_stop->set_ivtt_alpha_exp(bs_s, bl, bs_l, alpha_exp);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::read_transit_path scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::read_OD_IVTT (pair<const ODSLL, Travel_time>& ivt_row)
{
    const int& origin_stop_id = ivt_row.first.orig;
    const int& destination_stop_id = ivt_row.first.dest;
    const int& stop_id = ivt_row.first.stop;
    const int& line_id = ivt_row.first.line;
    const int& leg_id = ivt_row.first.leg;
    double anticipated_in_vehicle_time = ivt_row.second.tt[anticip];
    // double alpha_exp = ivt_row.second.alpha[EXP];

    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));
    Busstop* bs_l = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (leg_id) )));
    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    od_stop->set_anticipated_ivtt(bs_s, bl, bs_l, anticipated_in_vehicle_time);

    return true;
}

bool Network::read_pass_IVTT (pair<const ODSLL, Travel_time>& ivt_row)
{
    const int& pass_id = ivt_row.first.pid;
    const int& origin_stop_id = ivt_row.first.orig;
    const int& destination_stop_id = ivt_row.first.dest;
    const int& stop_id = ivt_row.first.stop;
    const int& line_id = ivt_row.first.line;
    const int& leg_id = ivt_row.first.leg;
    double anticipated_in_vehicle_time = ivt_row.second.tt[anticip];
    // double alpha_exp = ivt_row.second.alpha[EXP];

    Busstop* bs_o = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (origin_stop_id) )));
    Busstop* bs_d = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (destination_stop_id) )));
    Busstop* bs_s = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (stop_id) )));
    Busline* bl = (*(find_if(buslines.begin(), buslines.end(), compare <Busline> (line_id) )));
    Busstop* bs_l = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop> (leg_id) )));

    ODstops* od_stop = bs_o->get_stop_od_as_origin_per_stop(bs_d);
    vector<Passenger*> passengers = od_stop->get_passengers_during_simulation();
    Passenger* p = *find_if(passengers.begin(), passengers.end(), compare<Passenger>(pass_id));
    p->set_anticipated_ivtt(bs_s, bl, bs_l, anticipated_in_vehicle_time);

    return true;
}

bool Network::readtransitfleet (string name) // !< reads transit vehicle types, vehicle scheduling and dwell time functions
{
    ifstream in(name.c_str()); // open input file
    assert (in);
    string keyword;
    int nr= 0;
    int i=0;
    int limit;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="dwell_time_functions:")
    {
        cout << " readbustypes: no << bustypes: >> keyword " << endl;
        return false;
    }
    in >> nr;
    limit = i + nr;
    for (; i<limit;i++)
    {
        if (!read_dwell_time_function(in))
        {
            cout << " readbustypes: read_bustypes returned false for line nr " << (i+1) << endl;
            return false;
        }
    }
    in >> keyword;
    if (keyword!="vehicle_types:")
    {
        cout << " readbustypes: no << bustypes: >> keyword " << endl;
        return false;
    }
    in >> nr;
    limit = i + nr;
    for (; i<limit;i++)
    {
        if (!read_bustype(in))
        {
            cout << " readbustypes: read_bustypes returned false for line nr " << (i+1) << endl;
            return false;
        }
    }
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="vehicle_scheduling:")
    {
        cout << " readbusvehicle: no << vehicle_scheduling: >> keyword " << endl;
        return false;
    }
    in >> nr;
    limit = i + nr;
    for (; i<limit;i++)
    {
        if (!read_busvehicle(in))
        {
            cout << " readbusvehicles: read_busvehicle returned false for line nr " << (i+1) << endl;
            return false;
        }
    }
    if (theParameters->drt)
    {
        in >> keyword;
        if (keyword != "unassigned_vehicles:")
        {
            DEBUG_MSG("readtransitfleet: no << unassigned_vehicles: >> keyword ");
            return false;
        }
        in >> nr;
        limit = i + nr;
        for (; i < limit; i++)
        {
            if (!read_unassignedvehicle(in))
            {
                DEBUG_MSG("readtransitfleet: read_drtvehicle returned false for line nr" << (i+1));
                return false;
            }
        }
    }
    return true;
}

bool Network::read_dwell_time_function (istream& in)
{
    char bracket;
    int func_id;
    // dwell time parameters
    int dwell_time_function_form;
    // 11 - Linear function of boarding and alighting
    // 12 - Linear function of boarding and alighting + non-linear crowding effect (Weidmann)
    // 13 - Max (boarding, alighting) + non-linear crowding effect (Weidmann)
    // 21 - TCRP(max doors with crowding, boarding from front door, alighting from both doors) + bay + stop capacity
    // 22 - TCRP(max doors with crowding, boarding from x doors, alighting from y doors) + bay + stop capacity
    double dwell_constant;
    double boarding_coefficient;
    double alighting_cofficient;
    double dwell_std_error;

    // in case of TCRP function form
    double share_alighting_front_door;
    double crowdedness_binary_factor;
    int number_boarding_doors;
    int number_alighting_doors;

    // extra delays
    double bay_coefficient;
    double over_stop_capacity_coefficient;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop scanner jammed at " << bracket;
        return false;
    }
    in >> func_id >> dwell_time_function_form >> dwell_constant >> boarding_coefficient >> alighting_cofficient >> dwell_std_error >> bay_coefficient >> over_stop_capacity_coefficient;

    if (dwell_time_function_form == 21)
    {
        in >> share_alighting_front_door >> crowdedness_binary_factor;
        auto* dt= new Dwell_time_function (func_id,dwell_time_function_form,dwell_constant,boarding_coefficient,alighting_cofficient,dwell_std_error,share_alighting_front_door,crowdedness_binary_factor,bay_coefficient,over_stop_capacity_coefficient);
        dt_functions.push_back (dt);
    }
    if (dwell_time_function_form == 22)
    {
        in >> number_boarding_doors >> number_alighting_doors >> share_alighting_front_door >> crowdedness_binary_factor;
        auto* dt= new Dwell_time_function (func_id,dwell_time_function_form,dwell_constant,boarding_coefficient,alighting_cofficient,dwell_std_error,number_boarding_doors,number_alighting_doors,share_alighting_front_door,crowdedness_binary_factor,bay_coefficient,over_stop_capacity_coefficient);
        dt_functions.push_back (dt);
    }
    else
    {
        auto* dt= new Dwell_time_function (func_id,dwell_time_function_form,dwell_constant,boarding_coefficient,alighting_cofficient,dwell_std_error,bay_coefficient,over_stop_capacity_coefficient);
        dt_functions.push_back (dt);
    }

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbustype scanner jammed at " << bracket;
        return false;
    }
    return true;
}

bool Network::read_bustype (istream& in) // reads a bustype
{
    char bracket;
    int type_id;
    int number_seats;
    int capacity;
    int dtf_id;
    double length;
    string bus_type_name;

    vector <Bustype*> types;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusstop scanner jammed at " << bracket;
        return false;
    }
    in >> type_id  >> bus_type_name >> length >> number_seats >> capacity >> dtf_id;
    Dwell_time_function* dtf=(*(find_if(dt_functions.begin(), dt_functions.end(), compare <Dwell_time_function> (dtf_id) )));
    Bustype* bt= new Bustype (type_id, bus_type_name, length, number_seats, capacity,dtf);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbustype scanner jammed at " << bracket;
        return false;
    }
    bustypes.push_back (bt);

#ifdef _DEBUG_NETWORK
    cout << " read bustype"<< type_id <<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::read_busvehicle(istream& in) // reads a bus vehicle
{
    char bracket;
    int bv_id;
    int type_id;
    int nr_trips;
    int trip_id;
    vector <Start_trip*> driving_roster;
    bool ok= true;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusvehicle scanner jammed at " << bracket;
        return false;
    }
    in >> bv_id >> type_id >> nr_trips;

    // find bus type and create bus vehicle
    Bustype* bty=(*(find_if(bustypes.begin(), bustypes.end(), compare <Bustype> (type_id) )));
    // generate a new bus vehicle
    vid++; // increment the veh id counter, buses are vehicles too
    //Bus* bus=recycler.newBus(); // get a bus vehicle
    //bus->set_bus_id(bv_id);
    //bus->set_bustype_attributes(bty);
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsbusvehicle scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i < nr_trips; i++) // for each trip on the chain
    {
        in >> trip_id;
        auto btr_it = find_if(bustrips.begin(), bustrips.end(), compare <Bustrip> (trip_id) ); // find the trip in the list
        if (btr_it == bustrips.end())
        {
            cout << "Bus trip " << trip_id << " not found.";
            return false;
        }
        Bustrip* btr = *btr_it;

        //Moved here by Jens 2014-09-05
        Bus* bus=recycler.newBus(); // get a bus vehicle
        bus->set_flex_vehicle(false); //vehicle can only run pre-planned fixed line, fixed schedule trips
        bus->set_bus_id(bv_id);
        bus->set_bustype_attributes(bty);
        btr->set_busv(bus);
        bus->set_curr_trip(btr);
        if (i>0) // flag as busy for all trip except the first on the chain
        {
            btr->get_busv()->set_on_trip(true);
        }
        btr->set_bustype(bty);
        Start_trip* st = new Start_trip (btr, btr->get_starttime());
        driving_roster.push_back(st); // save the driving roster at the vehicle level
        busvehicles.push_back (bus);
    }
    for (auto trip = driving_roster.begin(); trip < driving_roster.end(); trip++)
    {
        (*trip)->first->add_trips(driving_roster); // save the driving roster at the trip level for each trip on the chain
    }

    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsbusvehicle scanner jammed at " << bracket;
        return false;
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readbusvehicle scanner jammed at " << bracket;
        return false;
    }
#ifdef _DEBUG_NETWORK
    cout << " read busvehicle"<< bv_id <<endl;
#endif //_DEBUG_NETWORK
    return ok;
}

bool Network::read_unassignedvehicle(istream& in) //reads a bus vehicles that are initialized without a trip assigned to them
{
    char bracket;
    int bv_id; //id of bus vehicle
    int type_id; //id of bus vehicle type
    int init_stop_id; //id of stop that bus is initialized at (generated in an idle state)
    Busstop* init_stop;
    int cc_id; //id of control center that this bus is initially connected with
    Controlcenter* init_cc;
    double init_time; //time at which bus is generated
    
    DrtVehicleInit unassignedvehicle; //tuple with vehicle, init stop, init control center, and init time

    in >> bracket;
    if (bracket != '{')
    {
        DEBUG_MSG_V("readfile::read_unassignedvehicle scanner jammed at " << bracket);
        return false;
    }
    in >> bv_id >> type_id >> init_stop_id >> cc_id >> init_time;
    
    bracket = ' ';
    in >> bracket;
    if (bracket != '}')
    {
        DEBUG_MSG_V("readfile::read_unassignedvehicle scanner jammed at " << bracket);
        return false;
    }

    // find bus type and create bus vehicle
    Bustype* bty = (*(find_if(bustypes.begin(), bustypes.end(), compare <Bustype>(type_id))));
    // generate a new bus vehicle
    vid++;
    Bus* bus = recycler.newBus(); // get a bus vehicle
    bus->set_flex_vehicle(true); //vehicle can be assigned dynamically generated trips
    bus->set_bus_id(bv_id);
    bus->set_bustype_attributes(bty);
    bus->set_curr_trip(nullptr); // bus has no trip assigned to it, on_trip should = false

    init_stop = (*(find_if(busstops.begin(), busstops.end(), compare <Busstop>(init_stop_id))));

    if (ccmap.count(cc_id) != 0) {
        init_cc = ccmap[cc_id];
    } else
    {
        DEBUG_MSG("ERROR in readfile::read_unassignedvehicle, Controlcenter with id " << cc_id << " does not exist!");
        return false;
    }

    unassignedvehicle = make_tuple(bus, init_stop, init_cc, init_time);
    drtvehicles.push_back(unassignedvehicle);

    return true;
}

ODstops* Network::get_ODstop_from_odstops_demand(int os_id, int ds_id)
{
    auto odstop_it = find_if(odstops_demand.begin(), odstops_demand.end(), [=](ODstops* odstop) -> bool {
        return (odstop->get_origin()->get_id() == os_id && odstop->get_destination()->get_id() == ds_id); 
        });
    ODstops* target_odstop = odstop_it != odstops_demand.end() ? (*odstop_it) : nullptr;

    return target_odstop;
}

Pass_path* Network::get_pass_path_from_id(int path_id) const
{
    Pass_path* target_path = nullptr;
    for (const auto& odstop : odstops)
    {
        vector<Pass_path*> path_set = odstop->get_path_set();
        auto path_it = find_if(path_set.begin(), path_set.end(), [path_id](Pass_path* path) -> bool { return path->get_id() == path_id; });
        if (path_it != path_set.end())
        {
            target_path = *path_it;
            break;
        }
    }
    
    return target_path;
}

vector<Busstop*> Network::get_busstops_on_link(Link* link) const
{
    vector<Busstop*> stops_on_link;
    // collect all stops with matching link id
    for(auto stop : busstops)
    {
        if(stop->get_link_id() == link->get_id())
            stops_on_link.push_back(stop);
    }
    // sort by the position of the stop on the link (closest to upstream node first)
    sort(stops_on_link.begin(), stops_on_link.end(), [](const Busstop* s1, const Busstop* s2) -> bool
        {
            if (s1->get_position() != s2->get_position())
                return s1->get_position() < s2->get_position();
            else
                return s1->get_id() < s2->get_id(); // if positions are equal the stop with the lowest id is considered earlier on the link
        }
    );

    return stops_on_link;
}

vector<Busstop*> Network::get_busstops_on_busroute(Busroute* route) const
{
    vector<Busstop*> stops_on_route;
    for (auto link : route->get_links())
    {
        vector<Busstop*> stops_on_link = get_busstops_on_link(link);
        stops_on_route.insert(stops_on_route.end(), stops_on_link.begin(), stops_on_link.end());
    }
    return stops_on_route;
}

// read traffic control
bool Network::readsignalcontrols(string name)
{
    ifstream inputfile(name.c_str());
    assert (inputfile);
    string keyword;
    inputfile >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="controls:")
    {
        inputfile.close();
        return false;
    }
    int nr;
    inputfile >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readsignalcontrol(inputfile))
        {
            inputfile.close();
            return false;
        }
    }
    inputfile.close();
    return true;
}


bool Network::readsignalcontrol(istream & in)
{
    char bracket;
    int controlid;
    int nr_plans;
    bool ok= true;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsignalcontrol scanner jammed at " << bracket;
        return false;
    }
    in >> controlid >> nr_plans;
    auto* sc = new SignalControl(controlid);

    for (int i=0; i < nr_plans; i++)
    {
        ok = ok && (readsignalplan (in, sc));
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsignalcontrol scanner jammed at " << bracket;
        return false;
    }
    signalcontrols.push_back(sc);
#ifdef _DEBUG_NETWORK
    cout << " read signalcontrol"<< controlid <<endl;
#endif //_DEBUG_NETWORK
    return ok;
}


bool Network::readsignalplan(istream& in, SignalControl* sc)
{
    char bracket;
    int planid;
    int nr_stages;
    double offset;
    double cycletime;
    double start;
    double stop;
    bool ok= true;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsignalplans scanner jammed at " << bracket;
        return false;
    }
    in >> planid >> start >> stop >> offset >> cycletime >> nr_stages;
    assert ((start >= 0.0) && (stop > 0.0) && (cycletime > 0.0)  && (nr_stages > 0));
    // make a new signalplan
    auto* sp = new SignalPlan(planid, start, stop, offset, cycletime);
    // read & make all stages
    for (int i=0; i<nr_stages; i++)
    {
        ok = ok && (readstage(in, sp)); // same as &= but not sure if &= is bitwise or not...
    }
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readsignalplans scanner jammed at " << bracket;
        return false;
    }
    sc->add_signal_plan(sp);
    signalplans.push_back(sp);
#ifdef _DEBUG_NETWORK
    cout << " read signalplan "<< planid <<endl;
#endif //_DEBUG_NETWORK
    return ok;
}


bool Network::readstage(istream& in, SignalPlan* sp)
{
    char bracket;
    int stageid;
    int nr_turnings;
    int turnid;
    double start;
    double duration;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readstages scanner jammed at " << bracket;
        return false;
    }
    in  >> stageid >> start >> duration >> nr_turnings;
    assert ((start >= 0.0) && (duration > 0.0));

    // make a new stage
    auto* stageptr = new Stage(stageid, start, duration);
    // read all turnings find each turning in the turnings list and add it to the stage
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readsignals scanner jammed at " << bracket;
        return false;
    }
    for (int i=0; i<nr_turnings; i++)
    {
        in >> turnid;
        // find turning in turnings list
        map <int,Turning*>::iterator t_iter;
        t_iter=turningmap.find(turnid);
        assert (t_iter!=turningmap.end());
        Turning* turn=t_iter->second;
        //vector <Turning*>::iterator turn = find_if(turnings.begin(), turnings.end(), compare<Turning>(turnid));
        //assert (turn != turnings.end());
        // add to stage
        stageptr->add_turning(turn);
    }
    in >> bracket;
    if (bracket != '}') // once for the turnings
    {
        cout << "readfile::readstages scanner jammed at " << bracket;
        return false;
    }
    in >> bracket; // once for the stage
    if (bracket != '}')
    {
        cout << "readfile::readstages scanner jammed at " << bracket;
        return false;
    }
    // add stage to stages list
    stages.push_back(stageptr);
    // add stage to signal plan
    sp->add_stage(stageptr);
#ifdef _DEBUG_NETWORK
    cout << " read stage "<< stageid <<endl;
#endif //_DEBUG_NETWORK
    return true;
}



bool Network::readnetwork(string name)
{
    ifstream inputfile(name.c_str());
    assert  (inputfile);
    if (readservers(inputfile) && readnodes(inputfile) && readsdfuncs(inputfile) && readlinks (inputfile) )
    {
        inputfile.close();
        return true;
    }
    
    
        inputfile.close();
        return false;
    
}

bool Network::register_links()
{
    // register all the incoming links @ the destinations
    for (auto & iter : destinationmap)
    {
        iter.second->register_links(linkmap);
    }
    //register all the outgoing links at the origins
    for (auto & iter1 : originmap)
    {
        iter1.second->register_links(linkmap);
    }
    //register all the incoming & outgoing links at the junctions
    for (auto & iter2 : junctionmap)
    {
        iter2.second->register_links(linkmap);
    }
    return true;
}

bool Network::readods(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="od_pairs:")
    {
        cout << "stuck at " << keyword << " instead of : od_pairs: " << endl;
        return false;
    }
    int nr;
    in >> nr;
    in >> keyword;
    if (keyword!="scale:")
    {
        cout << "stuck at " << keyword << " instead of : scale: " << endl;
        return false;
    }
    double scale;
    in >> scale;

    for (int i=0; i<nr;i++)
    {
        if (!readod(in,scale))
        {
            cout << "stuck at od pair " << i << " of " << nr << endl;
            return false;
        }
    }
    for (auto iter=boundaryins.begin();iter<boundaryins.end();iter++)
    {
        (*iter)->register_ods(&odpairs);
    }
    return true;
}



bool Network::readod(istream& in, double scale)
{
    char bracket;
    int oid;
    int did;
    double rate;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readdemandfile::readod scanner jammed at " << bracket;
        return false;
    }
    in  >> oid >> did >> rate;
    // check oid, did, rate;
    // scale up/down the rate
    rate=rate*scale;
    //assert (rate > 0);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readdemandfile::readod scanner jammed at " << bracket;
        return false;
    }
    // find oid, did
    // find the origin & dest  pointers
    map <int, Origin*>::iterator o_iter;
    o_iter = originmap.find(oid);
    assert (o_iter != originmap.end());

    map <int, Destination*>::iterator d_iter;
    d_iter = destinationmap.find(did);
    assert (d_iter != destinationmap.end());

#ifdef _DEBUG_NETWORK
    cout << "found o and d " << oid << "," << did << endl;
#endif //_DEBUG_NETWORK
    // create odpair

    auto* odpair=new ODpair (o_iter->second, d_iter->second, rate,&vehtypes); // later the vehtypes can be defined PER OD

    //add odpair to origin and general list
    odpairs.insert(odpairs.begin(),odpair);
    (o_iter->second)->add_odpair(odpair);
    // set od list
#ifdef _DEBUG_NETWORK
    cout << " read an od"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::add_od_routes()
// adds routes to OD pairs, sorts out unnecessary routes
{
    int nr_deleted =0;
    vector <Route*> deleted_routes;
    vector <Route*> new_del_routes;
    for (auto iter=odpairs.begin(); iter < odpairs.end(); iter++)
    {
        addroutes ((*iter)->get_origin()->get_id(), (*iter)->get_destination()->get_id(), *iter );
        if (theParameters->delete_bad_routes)
        {
            new_del_routes = (*iter)->delete_spurious_routes(runtime/4);
            deleted_routes.insert(deleted_routes.begin(), new_del_routes.begin(), new_del_routes.end());
            //nr_deleted += (*iter)->delete_spurious_routes(runtime/4).size(); // deletes all the spurious routes in the route set.
        }
    }
    nr_deleted = static_cast<int>(deleted_routes.size());
    if (nr_deleted > 0 ) {
        cout << nr_deleted << " routes deleted" << endl;
}

    // write the new routes file.
    auto del=deleted_routes.begin();
    multimap<odval,Route*>::iterator route_m;
    multimap<odval,Route*>::iterator route_l;
    multimap<odval,Route*>::iterator route_u;
    vector <Route*>::iterator route;
    for (; del < deleted_routes.end(); del++)
    {
        odval val=(*del)->get_oid_did();
        route_l  = routemap.lower_bound(val);
        route_u  = routemap.upper_bound(val);
        for (route_m=route_l;route_m != route_u; route_m++) // check all routes  for given odval
        {
            if ((*route_m).second->get_id() == (*del)->get_id())
            {
                routemap.erase(route_m);
                delete (*del);
                break;
            }
        }
    }


    return true;
}

bool Network::addroutes (int oid, int did, ODpair* odpair)
{
    /*
    //find routes from o to d and add
    vector<Route*>::iterator iter=routes.begin();
    odval odvalue(oid, did);
    while (iter!=routes.end())
    {
        iter=(find_if (iter,routes.end(), compareroute(odvalue) )) ;
        if (iter!=routes.end())
        {
            //Route* rptr=*iter;
            odpair->add_route(*iter);
#ifdef _DEBUG_NETWORK
            cout << "added route " << ((*iter)->get_id())<< endl;
#endif //_DEBUG_NETWORK
            iter++;
        }
    }
    */

    //vector <ODpair*>::iterator od_iter=odpairs.begin();
    odval od_v(oid, did);
    multimap <odval,Route*>::iterator r_iter;
    multimap <odval,Route*>::iterator r_lower;
    multimap <odval,Route*>::iterator r_upper;
    r_lower = routemap.lower_bound(od_v); // get lower boundary of all routes with this OD
    r_upper = routemap.upper_bound(od_v); // get upper boundary
    for (r_iter=r_lower; r_iter != r_upper; r_iter++) // add all routes to OD pair
    {
        odpair->add_route((*r_iter).second);
#ifdef _DEBUG_NETWORK
        cout << "added route " << ((*r_iter).first)<< endl;
#endif //_DEBUG_NETWORK
    }


    return true;
}


ODRate Network::readrate(istream& in, double scale)
{
#ifdef _DEBUG_NETWORK
    cout << "read a rate" << endl;
#endif //_DEBUG_NETWORK
    ODRate odrate;
    odrate.odid=odval(0,0);
    odrate.rate=-1;
    char bracket;
    int oid;
    int did;
    int rate;
    in >> bracket;

    if (bracket != '{')
    {
        cout << "readdemandfile::readrate scanner jammed at " << bracket;
        return odrate;
    }
    in  >> oid >> did >> rate;
    // check oid, did, rate;
    // scale up/down the rate
    rate=static_cast<int> (rate*scale);
    //   assert (rate > 0);
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readdemandfile::readrate scanner jammed at " << bracket;
        return odrate;
    }
    odrate.odid=odval(oid,did);
    // find and check od pair)
    odrate.rate=rate;
    return odrate;
}


bool Network::readrates(istream& in)
{
    auto* odslice=new ODSlice();
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="od_pairs:")
    {
        cout << "stuck at " << keyword << " instead of : od_pairs: " << endl;
        return false;
    }
    int nr;
    in >> nr;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="scale:")
    {
        cout << "stuck at " << keyword << " instead of : scale: " << endl;
        return false;
    }
    double scale;
    in >> scale;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="loadtime:")
    {
        cout << "stuck at " << keyword << " instead of : loadtime: " << endl;
        return false;
    }
    double loadtime;
    in >> loadtime;
    for (int i=0; i<nr;i++)
    {
        ODRate odrate=readrate(in,scale);
        if (odrate.rate==-1)
        {
            cout << "stuck at od readrates. load time : "<< loadtime << " od nr " << i << " of " <<nr << endl;

            return false;
        }
         {
            odslice->rates.insert(odslice->rates.end(),odrate);
}
    }
    odmatrix.add_slice(loadtime,odslice);
#ifdef _DEBUG_NETWORK
    cout << " added a slice " << endl;
#endif //_DEBUG_NETWORK
    // MatrixAction* mptr adds itself into the eventlist, and will be cleared up when executed.
    // it is *not* a dangling pointer
    auto* mptr=new MatrixAction(eventlist, loadtime, odslice, &odpairs);
    assert (mptr != nullptr);
#ifdef _DEBUG_NETWORK

    cout << " added a matrixaction " << endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::readserverrates(string name)
{
    ifstream in(name.c_str());
    assert (in);
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="rates:")
    {
        in.close();
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readserverrate(in))
        {
            in.close();
            return false;
        }
    }
    in.close();
    return true;
}

bool Network::readserverrate(istream& in)
{
#ifdef _DEBUG_NETWORK
    cout << "read a rate" << endl;
#endif //_DEBUG_NETWORK
    char bracket;
    int sid;
    double time;
    double mu;
    double sd;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readserverrate::readserverrate scanner jammed at " << bracket;
        return  false;
    }
    in  >> sid >> time >> mu >> sd;
    //assert  ( (find_if (servers.begin(),servers.end(), compare <Server> (sid))) != servers.end() );   // server with sid exists
    assert (servermap.count(sid)); // make sure exists
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readserverrate::readserverrate scanner jammed at " << bracket;
        return false;
    }
    //Server* sptr=*(find_if (servers.begin(),servers.end(), compare <Server> (sid)));
    Server* sptr = servermap[sid];
    auto* cptr=new ChangeRateAction(eventlist,time,sptr,mu,sd);
    assert (cptr != nullptr);
    changerateactions.push_back(cptr);
    return true;
}


bool Network::readwalkingtimedistribution(istream& in) // reads a walking time distribution
{
    
    char bracket;
    
    string orig_name;
    string dest_name;
    
    Busstop *orig_stop_ptr;
    Busstop *dest_stop_ptr;
    
    double interval_start;
    double interval_end;
    
    unsigned int num_quantiles;
    
    
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected {" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    //read origin name, destination name, interval start and end second as well as number of quantiles
    in >> orig_name >> dest_name >> interval_start >> interval_end >> num_quantiles;
    
    //get pointer to origin and destination stop
    orig_stop_ptr = get_busstop_from_name(orig_name);
    dest_stop_ptr = get_busstop_from_name(dest_name);
    
    //containers for quantile position and quantile values
    vector<double> quantiles(num_quantiles);
    vector<double> quantile_values(num_quantiles);
    
    //get quantile positions
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected {" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    for (unsigned int quant = 0; quant < num_quantiles; quant++) {
        in >> quantiles[quant];
    }
    
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected }" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    //get quantile values
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected {" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    for (unsigned int quant = 0; quant < num_quantiles; quant++) {
        in >> quantile_values[quant];
    }
    
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected }" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readwalkingtimedistribution scanner jammed at " << bracket << ", expected }" << endl;
        return false;
    }
    bracket = ' '; //reset bracket
    
    
    //store walking time distribution
    orig_stop_ptr->add_walking_time_quantiles(dest_stop_ptr, quantiles, quantile_values, num_quantiles, interval_start, interval_end);
    
    return true;
}


bool Network::readdemandfile(string name)
{

    ifstream inputfile(name.c_str());
    assert (inputfile);
    if (readods(inputfile))
    {
        string keyword;
        inputfile >> keyword;
#ifdef _DEBUG_NETWORK
        cout << keyword << endl;
#endif //_DEBUG_NETWORK
        if (keyword!="slices:")
        {
            inputfile.close();
            return false;
        }
        int nr;
        inputfile >> nr;
        for (int i=0; i<nr; i++)
        {
            if (!readrates(inputfile))
            {
                inputfile.close();
                return false;
            }
        }
        inputfile.close();
        return true;
    }
    
    
        inputfile.close();
        return false;
    
}

bool Network::readvtype (istream & in)
{
    char bracket;
    int id;
    string label;
    double prob;
    double length;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readvtypes scanner jammed at " << bracket;
        return false;
    }
    in  >> id >> label >> prob >> length;

    assert ( (prob >= 0.0) && (prob<=1.0) && (length>0.0) ); // 0.0 means unused in the vehicle mx
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readvtypes scanner jammed at " << bracket;
        return false;
    }
    vehtypes.vtypes.insert(vehtypes.vtypes.end(), new Vtype (id,label,prob,length));
    return true;
}


bool Network::readvtypes (string name)
{
    ifstream in(name.c_str());
    assert (in);

    string keyword;

    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="vtypes:")
    {
        in.close();
        return false;
    }
    int nr;
    in >> nr;
    for (int i=0; i<nr;i++)
    {
        if (!readvtype(in))
        {
            in.close();
            return false;
        }
    }
    vehtypes.initialize();
    in.close();
    return true;
}



bool Network::writeoutput(string name)
// !!!!!!! changed to write the OD output!!!!!!!!!
{
    ofstream out(name.c_str());
    //assert(out);
    bool ok=true;
    auto iter=odpairs.begin();
    (*iter)->writefieldnames(out);
    for (;iter<odpairs.end();iter++)
    {
        ok=ok && (*iter)->write(out);
    }
    out.close();
    return ok;
}

bool Network::writesummary(string name)
// !!!!!!! writes the OD pair output!!!!!!!!!
{
    ofstream out(name.c_str());
    //assert(out);
    bool ok=true;
    out << "Origin\tDestination\tNrRoutes\tNrArrived\tTotalTravelTime(s)\tTotalMileage(m)" << endl;
    for (auto iter=odpairs.begin();iter<odpairs.end();iter++)
    {
        ok=ok && (*iter)->writesummary(out);
    }
    out.close();
    return ok;
}

bool Network::writeFWFsummary(
    ostream& out,
    const FWF_passdata& total_passdata,
    const FWF_passdata& fix_passdata,
    const FWF_passdata& drt_passdata,
    const FWF_vehdata& total_vehdata,
    const FWF_vehdata& fix_vehdata,
    const FWF_vehdata& drt_vehdata,
    const FWF_ccdata& cc_data,
    const FWF_tripdata& drt_tripdata,
    int pass_ignored
)
{
    assert(out);
    Q_UNUSED(drt_passdata)
    Q_UNUSED(fix_passdata)

    out << std::fixed;
    out.precision(2);
    //ofstream out(filename.c_str());
    /*
    Collect statistics from other output files relevant for fixed with flexible implementation output
    */
    out << "### Total passenger summary ###";
    out << "\nPassenger journeys completed: " << total_passdata.pass_completed;

    out << "\n\nAverage GTC                    : " << total_passdata.avg_gtc;
    out << "\nStdev GTC                      : " << total_passdata.std_gtc;

    //out << "\n\nTotal walking time             : " << total_passdata.total_wlkt;
    out << "\n\nAverage walking time           : " << total_passdata.avg_total_wlkt;
    out << "\nStdev walking time             : " << total_passdata.std_total_wlkt;

    //out << "\n\nTotal waiting time             : " << total_passdata.total_wt;
    out << "\n\nAverage waiting time           : " << total_passdata.avg_total_wt;
    out << "\nStdev waiting time             : " << total_passdata.std_total_wt;
    //out << "\nMinimum waiting time           : " << total_passdata.min_wt;
    out << "\nMaximum waiting time           : " << total_passdata.max_wt;
    out << "\nMedian waiting time            : " << total_passdata.median_wt;
    out << "\nCV waiting time                : " << total_passdata.cv_wt;

    //out << "\n\nTotal denied waiting time      : " << total_passdata.total_denied_wt;
    out << "\n\nAverage denied waiting time    : " << total_passdata.avg_denied_wt;
    out << "\nStdev denied waiting time      : " << total_passdata.std_denied_wt;

    //out << "\n\nTotal in-vehicle time          : " << total_passdata.total_ivt;
    out << "\n\nAverage in-vehicle time        : " << total_passdata.avg_total_ivt;
    out << "\nStdev in-vehicle time          : " << total_passdata.std_total_ivt;

    //out << "\n\nTotal crowded in-vehicle time  : " << total_passdata.total_crowded_ivt;
    out << "\n\nAverage crowded in-vehicle time: " << total_passdata.avg_total_crowded_ivt;
    out << "\nStdev crowded in-vehicle time  : " << total_passdata.std_total_crowded_ivt;

    out << "\n\nTotal PKT      : " << total_passdata.total_pass_vkt;
    out << "\nTotal Fixed PKT: " << total_passdata.total_pass_fix_vkt;
    out << "\nTotal DRT PKT  : " << total_passdata.total_pass_drt_vkt;
    out << "\nPKT mode split (FIX / Total, DRT / Total)     : " << total_passdata.total_pass_fix_vkt / total_passdata.total_pass_vkt << ", " << total_passdata.total_pass_drt_vkt / total_passdata.total_pass_vkt;

    out << "\n\nTotal passengers ignored (trip out of pass-generation start-stop interval): " << pass_ignored;    
    out << "\nstart_pass_generation= " << theParameters->start_pass_generation;
    out << "\nstop_pass_generation= " << theParameters->stop_pass_generation;
    out << "\nstoptime= " << runtime;
    /*       out << "\n\n### Fixed passenger summary ###";
           out << "\n\nTotal walking time             : " << fix_passdata.total_wlkt;
           out << "\nAverage walking time           : " << fix_passdata.avg_total_wlkt;
           ...

           out << "\n\n### DRT passenger summary ###";
           out << "\n\nTotal walking time             : " << drt_passdata.total_wlkt;
           out << "\nAverage walking time           : " << drt_passdata.avg_total_wlkt;
           ...;*/

    out << "\n\n### Total vehicle summary ###";
    out << "\nTotal VKT                   : " << total_vehdata.total_vkt;
    out << "\nTotal occupied VKT          : " << total_vehdata.total_occupied_vkt;
    out << "\nTotal empty VKT             : " << total_vehdata.total_empty_vkt;
    out << "\nTotal occupied time         : " << total_vehdata.total_occupied_time;
    out << "\nTotal empty time            : " << total_vehdata.total_empty_time;
    out << "\nTotal driving time          : " << total_vehdata.total_driving_time;
    out << "\nTotal idle time             : " << total_vehdata.total_idle_time;
    out << "\nTotal oncall time           : " << total_vehdata.total_oncall_time;

    out << "\n\n### Fixed vehicle summary ###";
    out << "\nFixed VKT                   : " << fix_vehdata.total_vkt;
    out << "\nFixed occupied VKT          : " << fix_vehdata.total_occupied_vkt;
    out << "\nFixed empty VKT             : " << fix_vehdata.total_empty_vkt;
    out << "\nFixed occupied time         : " << fix_vehdata.total_occupied_time;
    out << "\nFixed empty time            : " << fix_vehdata.total_empty_time;
    out << "\nFixed driving time          : " << fix_vehdata.total_driving_time;
    out << "\nFixed idle time             : " << fix_vehdata.total_idle_time;
    out << "\nFixed oncall time           : " << fix_vehdata.total_oncall_time; //should always be zero...fixed schedule buses are currently always assigned a trip
    out << "\nFixed occ/total VKT ratio   : " << fix_vehdata.total_occupied_vkt / fix_vehdata.total_vkt;
    out << "\nFixed PKT/VKT ratio         : " << total_passdata.total_pass_fix_vkt / fix_vehdata.total_vkt; //should match average occupancy somewhat

    out << "\n\n### DRT vehicle summary ###";
    out << "\nDRT VKT                     : " << drt_vehdata.total_vkt;
    out << "\nDRT occupied VKT            : " << drt_vehdata.total_occupied_vkt;
    out << "\nDRT empty VKT               : " << drt_vehdata.total_empty_vkt;
    out << "\nDRT occupied time           : " << drt_vehdata.total_occupied_time;
    out << "\nDRT empty time              : " << drt_vehdata.total_empty_time;
    out << "\nDRT driving time            : " << drt_vehdata.total_driving_time;
    out << "\nDRT idle time               : " << drt_vehdata.total_idle_time;
    out << "\nDRT oncall time             : " << drt_vehdata.total_oncall_time;
    out << "\nDRT occ/total VKT ratio     : " << drt_vehdata.total_occupied_vkt / drt_vehdata.total_vkt;
    out << "\nDRT PKT/VKT ratio           : " << total_passdata.total_pass_drt_vkt / drt_vehdata.total_vkt; //should match average occupancy somewhat


    out << "\n\n### DRT trip summary ###";    
    out << "\nTotal trips                            : " << drt_tripdata.total_trips;
    out << "\nTotal pass-carrying trips              : " << drt_tripdata.total_pass_carrying_trips;
    out << "\nTotal empty trips                      : " << drt_tripdata.total_empty_trips;
    out << "\nTotal pass boarding                    : " << drt_tripdata.total_pass_boarding;
    out << "\nTotal pass alighting                   : " << drt_tripdata.total_pass_alighting;

    out << "\n\nAverage boarding per pass-carrying trip: " << drt_tripdata.avg_boarding_per_trip;
    out << "\nStdev boarding per pass-carrying trip  : " << drt_tripdata.std_boarding_per_trip;
    out << "\nMinimum boarding per pass-carrying trip: " << drt_tripdata.min_boarding_per_trip;
    out << "\nMaximum boarding per pass-carrying trip: " << drt_tripdata.max_boarding_per_trip;
    out << "\nMedian boarding per pass-carrying trip : " << drt_tripdata.median_boarding_per_trip;

    out << "\n\n### ControlCenter summary ###";
    out << "\nRequests recieved           : " << cc_data.total_requests_recieved;
    out << "\nRequests rejected           : " << cc_data.total_requests_rejected;
    out << "\nRequests accepted           : " << cc_data.total_requests_accepted;
    out << "\nRequests served             : " << cc_data.total_requests_served;

    out << "\n\n------------------------------------------------------------------\n\n";

    return true;
}

bool Network::write_day2day_passenger_waiting_experience_header(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_waiting_experience.dat"
    out << "day" << '\t'
        << "pass_id" << '\t'
        << "origin_id" << '\t'
        << "destination_id" << '\t'
        << "line_id" << '\t'
        << "trip_id" << '\t'
        << "stop_id" << '\t'
        << "boarding_time" << '\t'
        << "generation_time" << '\t'
        << "expected_wt_pk" << '\t'
        << "rti_level_available" << '\t'
        << "projected_wt_rti" << '\t'
        << "experienced_wt" << '\t'
        << "anticipated_wt" << '\t'
        << "nr_missed" << endl;
    return true;
}
bool Network::write_day2day_passenger_waiting_experience(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_waiting_experience.dat"
    for (auto stop_iter = busstops.begin(); stop_iter != busstops.end(); ++stop_iter)
    {
        ODs_for_stop stop_as_origin = (*stop_iter)->get_stop_as_origin();
        for (auto od_iter = stop_as_origin.begin(); od_iter != stop_as_origin.end(); ++od_iter)
        {
            map <Passenger*, list<Pass_waiting_experience>, ptr_less<Passenger*> > waiting_experience = od_iter->second->get_waiting_output();
            for (auto pass_iter1 = waiting_experience.begin(); pass_iter1 != waiting_experience.end(); ++pass_iter1)
            {
                out << day << '\t';
                od_iter->second->write_waiting_exp_output(out, (*pass_iter1).first);
            }
        }
    }
    return true;
}
bool Network::write_day2day_passenger_onboard_experience_header(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_onboard_experience.dat"
    out << "day" << '\t'
        << "pass_id" << '\t'
        << "origin_id" << '\t'
        << "destination_id" << '\t'
        << "line_id" << '\t'
        << "trip_id" << '\t'
        << "stop_id" << '\t'
        << "leg_id" << '\t'
        << "expected_ivt" << '\t'
        << "experienced_ivt" << '\t'
        << "experienced_ivt_crowding" << endl;
    return true;
}
bool Network::write_day2day_passenger_onboard_experience(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_onboard_experience.dat"
    out << std::fixed;
    for (auto stop_iter = busstops.begin(); stop_iter != busstops.end(); ++stop_iter)
    {
        ODs_for_stop stop_as_origin = (*stop_iter)->get_stop_as_origin();
        for (auto od_iter = stop_as_origin.begin(); od_iter != stop_as_origin.end(); ++od_iter)
        {
            map <Passenger*, list<Pass_onboard_experience>, ptr_less<Passenger*> > onboard_experience = od_iter->second->get_onboard_output();
            for (auto pass_iter1 = onboard_experience.begin(); pass_iter1 != onboard_experience.end(); ++pass_iter1)
            {
                out.precision(0);
                out << day << '\t';
                od_iter->second->write_onboard_exp_output(out, (*pass_iter1).first);
            }
        }
    }
    return true;
}
bool Network::write_day2day_passenger_transitmode_header(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_transitmode.dat"
    out << "day" << '\t'
        << "pass_id" << '\t'
        << "original_origin_id" << '\t'
        << "destination_id" << '\t'
        << "pickupstop_id" << '\t'
        << "time" << '\t'
        << "generation_time" << '\t'
        << "chosen_transitmode_id" << '\t'
        << "fixed_mode_id" << '\t'
        << "u_fixed" << '\t'
        << "prob_fixed" << '\t'
        << "drt_mode_id" << '\t'
        << "u_drt" << '\t'
        << "prob_drt" << endl;
    return true;
}
bool Network::write_day2day_passenger_transitmode(string filename)
{
    ofstream out(filename.c_str(), ios_base::app); //"o_fwf_day2day_passenger_transitmode.dat"
    out << std::fixed;
    for (const auto& od : odstops_demand)
    {
        vector<Passenger*> pass_vec = od->get_passengers_during_simulation();
        for (const auto& pass : pass_vec)
        {
            /*if (pass->get_end_time() > 0)
            {*/
            out.precision(0);
            out << day << '\t';
            od->write_transitmode_output(out, pass);
            /*}*/
        }
    }
    return true;
}


bool Network::write_day2day_boardings_header(string filename)
{
    ofstream out(filename.c_str(),ios_base::app); //"o_fwf_day2day_boardings.dat"
            out << "line_id" << '\t'
            << "total_pass_boarded" << '\t'
            << "day" << endl;
    return true;
}

bool Network::write_day2day_boardings(string filename)
{
    ofstream out(filename.c_str(),ios_base::app); //"o_fwf_day2day_boardings.dat"
    map<int,int> line_total_boarding; // total number of boardings per line

    // collect all trips to calculate number of boardings per line
    vector<Bustrip*> all_trips; // will contain all activated fixed trips and all completed 
    std::copy_if(begin(bustrips), end(bustrips), back_inserter(all_trips), [](const Bustrip* trip) { return trip->is_activated(); }); // all the fixed trips
    vector<Bustrip*> drt_trips; // all completed drt trip from any control center
    for (const auto& cc : ccmap)
    {
        for (const auto& vehtrip : cc.second->completedVehicleTrips_)
        {
            drt_trips.push_back(vehtrip.second);
        }
        vector<Bustrip*> drt_activated_trips = cc.second->get_activated_trips(); // all activated drt trips that have not completed
        drt_trips.insert(drt_trips.end(), drt_activated_trips.begin(), drt_activated_trips.end());
    }
    all_trips.insert(all_trips.end(), drt_trips.begin(), drt_trips.end());
    for(auto trip : all_trips)
    {
        line_total_boarding[trip->get_line()->get_id()] += trip->get_total_boarding();   
    }
    for(auto line : buslines) // just set all lines with zero trip boardings to zero
    {
        if(line_total_boarding.count(line->get_id()) == 0)
            line_total_boarding[line->get_id()] = 0;
    }

    /*out << "line_id" << '\t' << "total_pass_boarded" << '\t' << "day" << '\t' << endl;*/
    for(auto res : line_total_boarding)
    {
        out << res.first << '\t' << res.second << '\t' << day << endl;    
    }

    return true;
}

bool Network::write_day2day_modesplit_header(string filename)
{
    ofstream out(filename.c_str(),ios_base::app); //"o_fwf_day2day_modesplit.dat"
            out << "day" << '\t'
            << "fix_pkt" << '\t'
            << "drt_pkt" << '\t'
            << "fix_boarding" << '\t'
            << "drt_boarding" << '\t'
            << "fix_chosen" << '\t'
            << "drt_chosen" << '\t'
            << "avg_gtc" << '\t'
            <<  "npass" << '\t'
            <<  "pass_ignored" << endl;
    return true;
}
bool Network::write_day2day_modesplit(string filename)
{
    ofstream out(filename.c_str(),ios_base::app); //"o_fwf_day2day_modesplit.dat"
    FWF_passdata total_passdata;
    FWF_tripdata total_tripdata;
    vector<Passenger*> all_pass_dest_reached;

    //collect all passengers, calculate pkt per mode and total, calculate fix / total and drt/ total pkt percentages
    vector<Passenger*> all_pass = get_all_generated_passengers(); // includes also pass that did not complete their trip, outside pass gen range etc..
    int pass_ignored = 0;
    for (Passenger* pass : all_pass) // only check pass that completed their trip
    {
        /*if (fwf_outputs::finished_trip_within_pass_generation_interval(pass))
        {
            allpass_within_passgen.push_back(pass);
        }*/
        if(pass->get_end_time() > 0) 
        {
            all_pass_dest_reached.push_back(pass);
        }
        else
        {
            ++pass_ignored;
        }
    }
    total_passdata.calc_pass_statistics(all_pass_dest_reached);
    double fix = total_passdata.total_pass_fix_vkt / total_passdata.total_pass_vkt;
    double drt = total_passdata.total_pass_drt_vkt / total_passdata.total_pass_vkt;
    out << std::fixed;
    out.precision(2);
    out << day << '\t';
    out << fix << '\t' << drt << '\t';

    // collect all trips to calculate mode split based on number of boardings instead
    vector<Bustrip*> all_trips; // will contain all activated fixed trips and all completed 
    std::copy_if(begin(bustrips), end(bustrips), back_inserter(all_trips), [](const Bustrip* trip) { return trip->is_activated(); }); // all the fixed trips

    vector<Bustrip*> drt_trips; // all completed drt trip from any control center
    for (const auto& cc : ccmap)
    {
        for (const auto& vehtrip : cc.second->completedVehicleTrips_)
        {
            drt_trips.push_back(vehtrip.second);
        }
        vector<Bustrip*> drt_activated_trips = cc.second->get_activated_trips(); // all activated drt trips that have not completed
        drt_trips.insert(drt_trips.end(), drt_activated_trips.begin(), drt_activated_trips.end());
    }
    all_trips.insert(all_trips.end(), drt_trips.begin(), drt_trips.end());

    total_tripdata.calc_trip_statistics(all_trips);
    if(total_tripdata.total_pass_boarding != 0)
    {
        fix = static_cast<double>(total_tripdata.fix_total_pass_boarding) / static_cast<double>(total_tripdata.total_pass_boarding);
        drt = static_cast<double>(total_tripdata.drt_total_pass_boarding) / static_cast<double>(total_tripdata.total_pass_boarding);
    }
    else
    {
        fix = 0.0;
        drt = 0.0;
    }
    out << fix << '\t' << drt << '\t';
    out << total_passdata.total_fix_chosen << '\t' << total_passdata.total_drt_chosen << '\t';
    out.precision(5);
    out << total_passdata.avg_gtc << '\t';
    out << total_passdata.npass << '\t' << pass_ignored << endl;

    return true;
}

bool Network::writeheadways(string name)
// writes the time headways for the virtual links. to compare with the arrival process in Mitsim
{
    ofstream out(name.c_str());
    assert(out);
    for (auto iter=virtuallinks.begin();iter<virtuallinks.end();iter++)
    {
        out << (*iter)->get_id()<< endl;
        (*iter)->write_in_headways(out);
    }
    out.close();
    return true;
}

/** @ingroup PARTC
* - bunch of stuff used for results output
* @todo remove
*/
namespace PARTC
{
    void writeFWFsummary_odcategories(ostream& out, const FWF_passdata& passdata_b2b, const FWF_passdata& passdata_b2c, const FWF_passdata& passdata_c2c, const FWF_passdata& passdata_total, int pass_ignored)
    {
        assert(out);
        out << std::fixed;
        out.precision(2);
        out << "### Passenger per OD category summary: \tB2B\tB2C\tC2C\tTotal";
        out << "\nPassenger journeys completed                 :\t" << passdata_b2b.pass_completed << "\t" << passdata_b2c.pass_completed << "\t" << passdata_c2c.pass_completed << "\t" << passdata_total.pass_completed;

        out << "\n\nAverage GTC:\t" << passdata_b2b.avg_gtc << "\t" << passdata_b2c.avg_gtc << "\t" << passdata_c2c.avg_gtc << "\t" << passdata_total.avg_gtc;
        out << "\nStdev GTC  :\t" << passdata_b2b.std_gtc << "\t" << passdata_b2c.std_gtc << "\t" << passdata_c2c.std_gtc << "\t" << passdata_total.std_gtc;

        //out << "\n\nTotal walking time             :\t" << passdata_b2b.total_wlkt << "\t" << passdata_b2c.total_wlkt << "\t" << passdata_c2c.total_wlkt << "\t" << passdata_total.total_wlkt;
        //out << "\nAverage walking time           :\t" << passdata_b2b.avg_total_wlkt << "\t" << passdata_b2c.avg_total_wlkt << "\t" << passdata_c2c.avg_total_wlkt << "\t" << passdata_total.avg_total_wlkt;
        //out << "\nStdev walking time             :\t" << passdata_b2b.std_total_wlkt << "\t" << passdata_b2c.std_total_wlkt << "\t" << passdata_c2c.std_total_wlkt << "\t" << passdata_total.std_total_wlkt;

        //out << "\n\nTotal waiting time             :\t" << passdata_b2b.total_wt << "\t" << passdata_b2c.total_wt << "\t" << passdata_c2c.total_wt << "\t" << passdata_total.total_wt;
        out << "\n\nAverage waiting time           :\t" << passdata_b2b.avg_total_wt << "\t" << passdata_b2c.avg_total_wt << "\t" << passdata_c2c.avg_total_wt << "\t" << passdata_total.avg_total_wt;
        out << "\nStdev waiting time             :\t" << passdata_b2b.std_total_wt << "\t" << passdata_b2c.std_total_wt << "\t" << passdata_c2c.std_total_wt << "\t" << passdata_total.std_total_wt;
        //out << "\nMinimum waiting time           :\t" << passdata_b2b.min_wt << "\t" << passdata_b2c.min_wt << "\t" << passdata_c2c.min_wt << "\t" << passdata_total.min_wt;
        out << "\nMaximum waiting time           :\t" << passdata_b2b.max_wt << "\t" << passdata_b2c.max_wt << "\t" << passdata_c2c.max_wt << "\t" << passdata_total.max_wt;
        out << "\nMedian waiting time            :\t" << passdata_b2b.median_wt << "\t" << passdata_b2c.median_wt << "\t" << passdata_c2c.median_wt << "\t" << passdata_total.median_wt;

        //out << "\n\nTotal denied waiting time      :\t" << passdata_b2b.total_denied_wt << "\t" << passdata_b2c.total_denied_wt << "\t" << passdata_c2c.total_denied_wt << "\t" << passdata_total.total_denied_wt;
        out << "\n\nAverage denied waiting time    :\t" << passdata_b2b.avg_denied_wt << "\t" << passdata_b2c.avg_denied_wt << "\t" << passdata_c2c.avg_denied_wt << "\t" << passdata_total.avg_denied_wt;
        out << "\nStdev denied waiting time      :\t" << passdata_b2b.std_denied_wt << "\t" << passdata_b2c.std_denied_wt << "\t" << passdata_c2c.std_denied_wt << "\t" << passdata_total.std_denied_wt;

        //out << "\n\nTotal in-vehicle time          :\t" << passdata_b2b.total_ivt << "\t" << passdata_b2c.total_ivt << "\t" << passdata_c2c.total_ivt << "\t" << passdata_total.total_ivt;
        out << "\n\nAverage in-vehicle time        :\t" << passdata_b2b.avg_total_ivt << "\t" << passdata_b2c.avg_total_ivt << "\t" << passdata_c2c.avg_total_ivt << "\t" << passdata_total.avg_total_ivt;
        out << "\nStdev in-vehicle time          :\t" << passdata_b2b.std_total_ivt << "\t" << passdata_b2c.std_total_ivt << "\t" << passdata_c2c.std_total_ivt << "\t" << passdata_total.std_total_ivt;

        //out << "\n\nTotal crowded in-vehicle time  :\t" << passdata_b2b.total_crowded_ivt << "\t" << passdata_b2c.total_crowded_ivt << "\t" << passdata_c2c.total_crowded_ivt << "\t" << passdata_total.total_crowded_ivt;
        out << "\n\nAverage crowded in-vehicle time:\t" << passdata_b2b.avg_total_crowded_ivt << "\t" << passdata_b2c.avg_total_crowded_ivt << "\t" << passdata_c2c.avg_total_crowded_ivt << "\t" << passdata_total.avg_total_crowded_ivt;
        out << "\nStdev crowded in-vehicle time  :\t" << passdata_b2b.std_total_crowded_ivt << "\t" << passdata_b2c.std_total_crowded_ivt << "\t" << passdata_c2c.std_total_crowded_ivt << "\t" << passdata_total.std_total_crowded_ivt;

        out << "\n\nTotal passengers ignored (trip out of pass-generation start-stop interval):\t" << pass_ignored;
        out << "\n\n------------------------------------------------------------------\n\n";

       /* out << "\n\n### Branch to Corridor passenger summary ###";
        out << "\nPassenger journeys completed: " << passdata_b2c.pass_completed;

        out << "\n\nTotal walking time             : " << passdata_b2c.total_wlkt;
        out << "\nAverage walking time           : " << passdata_b2c.avg_total_wlkt;
        out << "\nStdev walking time             : " << passdata_b2c.std_total_wlkt;

        out << "\n\nTotal waiting time             : " << passdata_b2c.total_wt;
        out << "\nAverage total waiting time     : " << passdata_b2c.avg_total_wt;
        out << "\nStdev total waiting time       : " << passdata_b2c.std_total_wt;
        out << "\nMinimum waiting time           : " << passdata_b2c.min_wt;
        out << "\nMaximum waiting time           : " << passdata_b2c.max_wt;
        out << "\nMedian waiting time            : " << passdata_b2c.median_wt;

        out << "\n\nTotal denied waiting time      : " << passdata_b2c.total_denied_wt;
        out << "\nAverage denied waiting time    : " << passdata_b2c.avg_denied_wt;
        out << "\nStdev denied waiting time      : " << passdata_b2c.std_denied_wt;

        out << "\n\nTotal in-vehicle time          : " << passdata_b2c.total_ivt;
        out << "\nAverage in-vehicle time        : " << passdata_b2c.avg_total_ivt;
        out << "\nStdev in-vehicle time          : " << passdata_b2c.std_denied_wt;

        out << "\n\nTotal crowded in-vehicle time  : " << passdata_b2c.total_crowded_ivt;
        out << "\nAverage crowded in-vehicle time: " << passdata_b2c.avg_total_crowded_ivt;
        out << "\nStdev crowded in-vehicle time  : " << passdata_b2c.std_total_crowded_ivt;

        out << "\n\n### Corridor to Corridor passenger summary ###";*/
    }
}

bool Network::write_busstop_output(string name1, string name2, string name3, string name4, string name5, string name6, string name7, string name8, string name9, string name10, 
    string name11, string name12, string name13, string name14, string name15, string name16, string name17, string name18, string name19, string name20, 
    string name21, string name22, string name23, string name24)
{
    ofstream out1(name1.c_str(),ios_base::app); //"o_transitlog_out.dat"
    ofstream out2(name2.c_str(),ios_base::app); //"o_transitstop_sum.dat"
    ofstream out3(name3.c_str(),ios_base::app); //"o_transitline_sum.dat"
    ofstream out4(name4.c_str(),ios_base::app); //"o_transit_trajectory.dat"
    ofstream out7(name7.c_str(),ios_base::app); //"o_segments_trip_loads.dat"
    ofstream out8(name8.c_str(),ios_base::app); //"o_selected_paths.dat"
    ofstream out9(name9.c_str(),ios_base::app); //"o_segments_line_loads.dat"
    ofstream out10(name10.c_str(),ios_base::app); //"o_od_stops_summary.dat"
    ofstream out11(name11.c_str(),ios_base::app); //"o_trip_total_travel_time.dat"
    ofstream out12(name12.c_str(),ios_base::app); //"o_od_stop_summary_without_paths.dat"
    ofstream out16(name16.c_str(), ios_base::app); //"o_passenger_trajectory.dat"
    ofstream out17(name17.c_str(), ios_base::app); //"o_passenger_welfare_summary.dat"
    ofstream out18(name18.c_str(), ios_base::app); // "o_fwf_summary.dat"
    ofstream out21(name21.c_str(), ios_base::app); // "o_vkt.dat"
    ofstream out24(name24.c_str(), ios_base::app); // "o_time_in_state_at_stop.dat"

  /*  this->write_busstop_output(
        workingdir + "o_transitlog_out.dat",                    out1
        workingdir + "o_transitstop_sum.dat",                   out2
        workingdir + "o_transitline_sum.dat",                   out3
        workingdir + "o_transit_trajectory.dat",                out4
        workingdir + "o_passenger_boarding.dat",                out5
        workingdir + "o_passenger_alighting.dat",               out6   
        workingdir + "o_segments_trip_loads.dat",               out7
        workingdir + "o_selected_paths.dat",                    out8
        workingdir + "o_segments_line_loads.dat",               out9
        workingdir + "o_od_stops_summary.dat",                  out10
        workingdir + "o_trip_total_travel_time.dat",            out11
        workingdir + "o_od_stop_summary_without_paths.dat",     out12
        workingdir + "o_passenger_waiting_experience.dat",      out13
        workingdir + "o_passenger_onboard_experience.dat",      out14
        workingdir + "o_passenger_connection.dat",              out15
        workingdir + "o_passenger_trajectory.dat",              out16
        workingdir + "o_passenger_welfare_summary.dat",         out17
        workingdir + "o_fwf_summary.dat",                       out18
        workingdir + "o_passenger_transitmode.dat",             out19
        workingdir + "o_passenger_dropoff.dat",                 out20
        workingdir + "o_vkt.dat",                               out21
        workingdir + "o_fwf_summary_odcategory.dat",            out22
        workingdir + "o_fwf_drtvehicle_states.dat",             out23
        workingdir + "o_time_spent_in_state_at_stop.dat"        out24
    );*/

    Q_UNUSED(name5)
    Q_UNUSED(name6)
    Q_UNUSED(name13)
    Q_UNUSED(name14)
    Q_UNUSED(name15)
    // passenger decision related, deactivated
    //ofstream out5(name5.c_str(),ios_base::app); // "o_passenger_boarding.dat"
    //ofstream out6(name6.c_str(),ios_base::app); // "o_passenger_alighting.dat"
    //ofstream out13(name13.c_str(),ios_base::app); // "o_passenger_waiting_experience.dat"
    //ofstream out14(name14.c_str(),ios_base::app); // "o_passenger_onboard_experience.dat"
    //ofstream out15(name15.c_str(),ios_base::app); // "o_passenger_connection.dat"
    

    /*FWF/DRT related outputs
@todo Need to either create a calc function for each one of these or fill it in in a different way.
*/
    FWF_passdata total_passdata;
    FWF_passdata fix_passdata;
    FWF_passdata drt_passdata;
    FWF_vehdata  total_vehdata;
    FWF_vehdata  fix_vehdata;
    FWF_vehdata  drt_vehdata;
    FWF_tripdata drt_tripdata;

    FWF_ccdata cc_summarydata;
    vector<Passenger*> allpass_within_passgen;

    /** @ingroup PARTC
    */

    FWF_passdata total_passdata_b2b;
    FWF_passdata total_passdata_b2c;
    FWF_passdata total_passdata_c2c;

    vector<Passenger*> allpass_b2b;
    vector<Passenger*> allpass_b2c;
    vector<Passenger*> allpass_c2c;
    /**@} PARTC */

    // writing the crude data and summary outputs for each bus stop
    write_transitlogout_header(out1);
    write_transitstopsum_header(out2);
    if(theParameters->drt)
    {
        fwf_outputs::writeDRTVehicleStateAtStop_header(out24);    
    }

    for (auto & busstop : busstops)
    {
        busstop->write_output(out1);

        if (theParameters->drt)
        {
            if (busstop->get_CC()) //if the stop is in the service area of a CC
            {
                fwf_outputs::writeDRTVehicleStateAtStop_row(out24, busstop);
            }
        }

        vector<Busline*> stop_lines = busstop->get_lines();
        for (auto & stop_line : stop_lines)
        {
            busstop->calculate_sum_output_stop_per_line(stop_line->get_id());
            busstop->get_output_summary(stop_line->get_id()).write(out2,busstop->get_id(),stop_line->get_id(), busstop->get_name());
            // this summary file contains for each stop a record for each line thats use it
        }
    }
    // writing the assignment results in terms of each segment on individual trips
    write_transittriploads_header(out7);
    for (auto & bustrip : bustrips)
    {
        bustrip->write_assign_segments_output(out7);
    }
    // writing the trajectory output for each bus vehicle (by stops)
    write_transit_trajectory_header(out4);
    for (auto & busvehicle : busvehicles)
    {
        busvehicle->write_output(out4);
    }
    fix_vehdata.calc_total_vehdata(busvehicles);

    // writing the aggregate summary output for each bus line
    write_transitlinesum_header(out3);
    write_transitlineloads_header(out9);
    write_triptotaltraveltime_header(out11);
    for (const auto& busline : buslines)
    {
        busline->calculate_sum_output_line();
        busline->get_output_summary().write(out3,busline->get_id());
        if (theParameters->demand_format == 3)
        {
            //(*iter)->calc_line_assignment();
            busline->write_assign_output(out9);
        }
        busline->write_ttt_output(out11);
    }
    // writing the decisions of passenger and summary per OD pair
    if (theParameters->demand_format == 3)
    {
        double total_pass_GTC = 0.0;
        int pass_counter = 0;
        write_selected_path_header(out8);
        write_od_summary_header(out12);
        for (auto od_iter = odstops_demand.begin(); od_iter < odstops_demand.end(); od_iter++)
        {
            if (!(*od_iter)->get_passengers_during_simulation().empty())
            {
                (*od_iter)->write_od_summary(out10);
                (*od_iter)->write_od_summary_without_paths(out12);
            }
            vector<Passenger*> pass_vec = (*od_iter)->get_passengers_during_simulation();
            for (auto pass_iter = pass_vec.begin(); pass_iter < pass_vec.end(); pass_iter++)
            {
                if ((*pass_iter)->get_end_time() > 0)
                {
                    (*pass_iter)->write_selected_path(out8);
                    (*pass_iter)->write_passenger_trajectory(out16);
                    total_pass_GTC += (*pass_iter)->get_GTC();
                    pass_counter++;
                }
            }
        }
        write_passenger_welfare_summary(out17, total_pass_GTC, pass_counter);
        
        vector<Passenger*> all_pass = get_all_generated_passengers(); // includes also pass that did not complete their trip, outside pass gen range etc..
        int pass_ignored = 0; // passengers ignored from fwf summary output...i.e. did not start and complete trip with pass generation interval

        // FWF passenger output
        if (PARTC::drottningholm_case)
        {
            for (auto pass : all_pass) // only check pass who started and completed trip within the pass generation time interval
            {
                if (fwf_outputs::finished_trip_within_pass_generation_interval(pass))
                {
                    allpass_within_passgen.push_back(pass);
                    PARTC::ODCategory od_category = PARTC::ODCategory::Null;
                    ODstops* od = pass->get_OD_stop();
                    int ostop = pass->get_original_origin()->get_id();
                    int dstop = od->get_destination()->get_id();

                    // divide passengers by od category and print to output file
                    if (PARTC::is_branch_to_branch(ostop, dstop))
                    {
                        od_category = PARTC::ODCategory::b2b;
                    }
                    else if (PARTC::is_branch_to_corridor(ostop, dstop))
                    {
                        od_category = PARTC::ODCategory::b2c;
                    }
                    else if (PARTC::is_corridor_to_corridor(ostop, dstop))
                    {
                        od_category = PARTC::ODCategory::c2c;
                    }
                    assert(od_category != PARTC::ODCategory::Null); // each od should have a category

                    if (od_category == PARTC::ODCategory::b2b)
                    {
                        allpass_b2b.push_back(pass);
                    }
                    else if (od_category == PARTC::ODCategory::b2c)
                    {
                        allpass_b2c.push_back(pass);
                    }
                    else if (od_category == PARTC::ODCategory::c2c)
                    {
                        allpass_c2c.push_back(pass);
                    }
                }
                else
                {
                    ++pass_ignored;
                }
            }
            total_passdata.calc_pass_statistics(allpass_within_passgen);
            total_passdata_b2b.calc_pass_statistics(allpass_b2b);
            total_passdata_b2c.calc_pass_statistics(allpass_b2c);
            total_passdata_c2c.calc_pass_statistics(allpass_c2c);

            ofstream out22(name22.c_str(), ios_base::app); //"o_fwf_summary_odcategory.dat"
            PARTC::writeFWFsummary_odcategories(out22, total_passdata_b2b, total_passdata_b2c, total_passdata_c2c, total_passdata, pass_ignored);
        }
        else
        {
            Q_UNUSED(name22);
            for (auto pass : all_pass) // only check pass who started and completed trip within the pass generation time interval
            {
                if (fwf_outputs::finished_trip_within_pass_generation_interval(pass))
                {
                    allpass_within_passgen.push_back(pass);
                }
                else
                {
                    ++pass_ignored;
                }
            }
            total_passdata.calc_pass_statistics(allpass_within_passgen);
            //total_passdata.calc_pass_statistics(all_pass);
        }

        //int pass_ignored = 0; @todo uncomment to return to original pass output collection without PARTC specific passenger grouping
        //for (auto pass : all_pass) // only check pass who started and completed trip within the pass generation time interval
        //{
        //    if (fwf_outputs::finished_trip_within_pass_generation_interval(pass))
        //    {
        //        allpass_within_passgen.push_back(pass);
        //    }
        //    else
        //    {
        //        ++pass_ignored;
        //    }
        //}
        //total_passdata.calc_pass_statistics(allpass_within_passgen);

        if (theParameters->drt)
        {
            Q_UNUSED(name19)
            Q_UNUSED(name20)
            //ofstream out19(name19.c_str(), ios_base::app); //o_passenger_transitmode_choices.dat
            //ofstream out20(name20.c_str(), ios_base::app); //o_passenger_dropoff_choices.dat

            //for (const auto& od : odstops_demand)
            //{
            //    vector<Passenger*> pass_vec = od->get_passengers_during_simulation();
            //    for (const auto& pass : pass_vec)
            //    {
            //        /*if (pass->get_end_time() > 0)
            //        {*/
            //        od->write_transitmode_output(out19, pass);
            //        //od->write_dropoff_output(out20, pass);
            //        /*}*/
            //    }
            //}

            //write outputs for objects owned by control centers
            vector<Bustrip*> all_completed_trips; // all completed drt trip from any control center
            vector<Bus*> all_drtvehicles; //!< vector of all generated vehicles (including cloned vehicles)
            map<int, vector<Bus*> > drt_vehdata_per_vehicle; // disaggregate drt vehicle based on unique vehicle (or more specifically bus vehicle) ids, each vector corresponds to vehicle data for one vehicle id
            for (const auto& cc : ccmap) //writing trajectory output for each drt vehicle and calculating trip summary stats
            {
                cc_summarydata.total_requests_recieved += cc.second->summarydata_.requests_recieved;
                cc_summarydata.total_requests_rejected += cc.second->summarydata_.requests_rejected;
                cc_summarydata.total_requests_accepted += cc.second->summarydata_.requests_accepted;
                cc_summarydata.total_requests_served += cc.second->summarydata_.requests_served;

                for (const auto& vehtrip : cc.second->completedVehicleTrips_)
                {
                    all_drtvehicles.push_back(vehtrip.first);
                    all_completed_trips.push_back(vehtrip.second);

                    drt_vehdata_per_vehicle[vehtrip.first->get_bus_id()].push_back(vehtrip.first); //sort bus objects into bus id buckets, recall that there may be multiple vehicles per ID due to bus cloning

                    vehtrip.first->write_output(out4); //write trajectory output for each bus vehicle that completed a trip
                    vehtrip.second->write_assign_segments_output(out7); // writing the assignment results in terms of each segment on individual trips
                }

                for (const auto& veh : cc.second->connectedVeh_)
                {
                    drt_vehdata_per_vehicle[veh.second->get_bus_id()].push_back(veh.second);
                    all_drtvehicles.push_back(veh.second);

                    veh.second->write_output(out4); //write trajectory output for each bus vehicle that has not completed a trip
                }
            }
            drt_vehdata.calc_total_vehdata(all_drtvehicles);
            drt_tripdata.calc_trip_statistics(all_completed_trips);

            if (!drt_vehdata_per_vehicle.empty()) //only generate output if we have drt vehicles
            {
                ofstream out23(name23.c_str(), ios_base::app); // "o_fwf_drtvehicle_states.dat"
                fwf_outputs::writeDRTVehicleState_header(out23);

                for (const auto& vehid_data : drt_vehdata_per_vehicle)
                {
                    FWF_vehdata temp_vehdata;
                    temp_vehdata.calc_total_vehdata(vehid_data.second); // calculate vehdata for vector of bus objects in id bucket
                    fwf_outputs::writeDRTVehicleState_row(out23, vehid_data.first, vehid_data.second.front()->get_init_time(), temp_vehdata); // use the init time of the first bus object associated with id bucket
                    temp_vehdata.clear();
                }
            }
        }
        
        fwf_outputs::writeVKT(out21, fix_vehdata, drt_vehdata);// out21 "o_drtvkt.dat" 

        total_vehdata = fix_vehdata + drt_vehdata;
        writeFWFsummary(out18, total_passdata, fix_passdata, drt_passdata, total_vehdata, fix_vehdata, drt_vehdata, cc_summarydata, drt_tripdata, pass_ignored);
        // deactivated - unneccessary files in most cases
        /*for (auto stop_iter = busstops.begin(); stop_iter != busstops.end(); ++stop_iter)
        {
            auto stop_as_origin = (*stop_iter)->get_stop_as_origin();
            for (auto od_iter = stop_as_origin.begin(); od_iter != stop_as_origin.end(); ++od_iter)
            {
                map <Passenger*,list<Pass_boarding_decision> > boarding_decisions = od_iter->second->get_boarding_output();
                for (auto pass_iter1 = boarding_decisions.begin(); pass_iter1 != boarding_decisions.end(); ++pass_iter1)
                {
                    od_iter->second->write_boarding_output(out5, (*pass_iter1).first);
                }

                map <Passenger*,list<Pass_alighting_decision> > alighting_decisions = od_iter->second->get_alighting_output();
                for (auto pass_iter2 = alighting_decisions.begin(); pass_iter2 != alighting_decisions.end(); ++pass_iter2)
                {
                    od_iter->second->write_alighting_output(out6, (*pass_iter2).first);
                    break;
                }

                map <Passenger*,list<Pass_connection_decision> > connection_decisions = od_iter->second->get_connection_output();
                for (auto pass_iter1 = connection_decisions.begin(); pass_iter1 != connection_decisions.end(); ++pass_iter1)
                {
                    od_iter->second->write_connection_output(out15, (*pass_iter1).first);
                }

                map <Passenger*,list<Pass_waiting_experience> > waiting_experience = od_iter->second->get_waiting_output();
                for (auto pass_iter1 = waiting_experience.begin(); pass_iter1 != waiting_experience.end(); ++pass_iter1)
                {
                    od_iter->second->write_waiting_exp_output(out13, (*pass_iter1).first);
                }

                map <Passenger*,list<Pass_onboard_experience> > onboard_experience = od_iter->second->get_onboard_output();
                for (auto pass_iter1 = onboard_experience.begin(); pass_iter1 != onboard_experience.end(); ++pass_iter1)
                {
                    od_iter->second->write_onboard_exp_output(out14, (*pass_iter1).first);
                }
            }
        }*/
        
    }
    return true;
}

void Network::write_transitlineloads_header(ostream& out)
{
    out << "Line_ID" << '\t'
        << "Upstream_stop_ID" << '\t'
        << "Upstream_stop_name" << '\t'
        << "Downstream_stop_ID" << '\t'
        << "Downstream_stop_name" << '\t'
        << "Passenger_load" << '\t' << endl;
}

void Network::write_transittriploads_header(ostream& out)
{
    out << "Line_ID" << '\t'
        << "Trip_ID" << '\t'
        << "Vehicle_ID" << '\t'
        << "Start_stop_ID" << '\t'
        << "Start_stop_name" << '\t'
        << "End_stop_id" << '\t'
        << "End_stop_name" << '\t'
        << "Passenger_load" << '\t' << endl;
}

void Network::write_transitlogout_header(ostream& out)
{
    out << "Line_ID" << '\t'
        << "Trip_ID" << '\t'
        << "Vehicle_ID" << '\t'
        << "Stop_ID" << '\t'
        << "Stop_name" << '\t'
        << "Entering_time" << '\t'
        << "Scheduled_arrival_time" << '\t'
        << "Dwell_time" << '\t'
        << "Lateness" << '\t'
        << "Exit_time" << '\t'
        << "Riding_time" << '\t'
        << "Riding_pass_time" << '\t'
        << "Time_since_arrival" << '\t'
        << "Time_since_departure" << '\t'
        << "Nr_alighting_pass" << '\t'
        << "Nr_boarding_pass" << '\t'
        << "On-board_occupancy" << '\t'
        << "Nr_waiting" << '\t'
        << "Total_waiting_time" << '\t'
        << "Holding_time" << '\t' << endl;
}

void Network::write_triptotaltraveltime_header(ostream& out)
{
    out << "Trip_ID" << '\t'
        << "Total_travel_time" << '\t' << endl;
}

void Network::write_transitstopsum_header(ostream& out)
{
    out << "Stop_ID" << '\t'
        << "Stop_name" << '\t'
        << "Line_ID" << '\t'
        << "Average_headway" << '\t'
        << "Average_dwell_time" << '\t'
        << "Average_abs_schedule_deviation" << '\t'
        << "Average_waiting_time_per_stop" << '\t'
        << "Total_pass_boarding" << '\t'
        << "Standard_deviation_headway" << '\t'
        << "Standard_deviation_dwell_time" << '\t'
        << "Share_on_time" << '\t'
        << "Share_early" << '\t'
        << "Share_late" << '\t'
        << "Total_pass_riding_time" << '\t'
        << "Total_pass_dwell_time" << '\t'
        << "Total_pass_waiting_time" << '\t'
        << "Total_pass_holding_time" << '\t'
        << "Total_pass_weighted_in_vehicle_time" << '\t' << endl;
}

void Network::write_transitlinesum_header(ostream& out)
{
    out << "Line_ID" << '\t'
        << "Average_headway" << '\t'
        << "Average_dwell_time" << '\t'
        << "Average_abs_schedule_deviation" << '\t'
        << "Average_waiting_time_per_stop" << '\t'
        << "Total_pass_boarding" << '\t'
        << "Standard_deviation_headway(averaged_over_stops)" << '\t'
        << "Standard_deviation_dwell_time(averaged_over_stops)" << '\t'
        << "Share_on_time" << '\t'
        << "Share_early" << '\t'
        << "Share_late" << '\t'
        << "Total_pass_riding_time" << '\t'
        << "Total_pass_dwell_time" << '\t'
        << "Total_pass_waiting_time" << '\t'
        << "Total_pass_holding_time" << '\t'
        << "Control_objective_functon_value" << '\t'
        << "Total_pass_weighted_in_vehicle_time" << '\t' << endl;
}

void Network::write_selected_path_header(ostream& out)
{
    out << "Passenger_ID" << '\t'
        << "Origin_stop_ID" << '\t'
        << "Origin_stop_name" << '\t'
        << "Destination_stop_ID" << '\t'
        << "Destination_stop_name" << '\t'
        << "Start_time" << '\t'
        << "Number_transfers" << '\t'
        << "Total_walking_time" << '\t'
        << "Total_waiting_time" << '\t'
        << "Total_waiting_time_due_to_denied_boarding" << '\t'
        << "Total_in_vehicle_time" << '\t'
        << "Total_weighted_in_vehicle_time" << '\t'
        << "End_time" << '\t' << endl;
}

void Network::write_transit_trajectory_header(ostream& out)
{
    out << "Line_ID" << '\t'
        << "Trip_ID" << '\t'
        << "Stop_ID" << '\t'
        << "Vehicle_ID" << '\t'
        << "Link_ID" << '\t'
        << "Entering?" << '\t'
        << "Arrival/Departure_time" << '\t' << endl;
}

void Network::write_od_summary_header(ostream& out)
{
    out << "Origin_stop_ID" << '\t'
        << "Origin_stop_name" << '\t'
        << "Destination_stop_ID" << '\t'
        << "Destination_stop_name" << '\t'
        << "Nr_pass_completed" << '\t'
        << "Average_travel_time" << '\t'
        << "Average_number_boardings" << '\t' << endl;
}

void Network::write_passenger_welfare_summary(ostream& out, double total_gtc, int total_pass)
{
    unsigned int total_pass_gen = count_generated_passengers();
    out << std::fixed;
    out.precision(2);
    out << "Total generalized travel cost:" << '\t' << total_gtc << endl
        << "Average generalized travel cost per passenger:" << '\t' << total_gtc / total_pass << endl
        << "Number of generated passengers: " << '\t' << total_pass_gen << endl
        << "Number of completed passenger journeys:" << '\t' << total_pass << endl
        << "Number of unserved passengers: " << '\t' << total_pass_gen - total_pass << endl;
}

bool Network::set_freeflow_linktimes()
{
    for (auto & iter : linkmap) {
        iter.second->set_hist_time(iter.second->get_freeflow_time());
}
    return true;
}

bool Network::writelinktimes(string name)
{
    ofstream out(name.c_str());
    // assert(out);
    out << "links:\t" << linkmap.size() << endl;
    out << "periods:\t" << nrperiods << endl;
    out << "periodlength:\t" << periodlength << endl;
    for (auto & iter : linkmap)
    {
        iter.second->write_time(out);
    }
    out.close();
    return true;
}

bool Network::readlinktimes(string name)
{
    ifstream inputfile(name.c_str());
    assert(inputfile);
    if (readtimes(inputfile))
    {
        inputfile.close();
        return true;
    }

    inputfile.close();
    return false;
}

bool Network::readtimes(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "links:") {
        return false;
    }
    int nr;
    in >> nr;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "periods:") {
        return false;
    }
    in >> nrperiods;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "periodlength:") {
        return false;
    }
    in >> periodlength;
    for (int i = 0; i < nr; i++)

    {
        if (!readtime(in))
        {
            cout << " readtimes for link : " << i << " failed " << endl;
            return false;

        }
    }

    if (nr == 0) // create the histtimes from freeflow times
    {
        cout << " creating linktimes from freeflow times " << endl;
#ifdef _DEBUG_NETWORK
        cout << " creating linktimes from freeflow times " << endl;
        cout << " linkmap.size() " << linkmap.size() << endl;
        cout << " virtuallinks.size() " << virtuallinks.size() << endl;
#endif //_DEBUG_NETWORK
        for (auto& iter : linkmap)
        {
            double linktime = iter.second->get_freeflow_time();
            auto* ltime = new LinkTime();
            ltime->periodlength = periodlength;
            ltime->nrperiods = nrperiods;
            ltime->id = iter.second->get_id();
            for (int i = 0; i < nrperiods; i++) {
                //		(ltime->times).push_back(linktime);
                (ltime->times)[i] = linktime;
            }
            iter.second->set_hist_time(linktime);
            iter.second->set_histtimes(ltime);
            linkinfo->times.insert(pair <int, LinkTime*>(iter.second->get_id(), ltime));
        }
    }
    linkinfo->set_graphlink_to_link(graphlink_to_link);

    return true;
}

bool Network::readtime(istream& in)

{
    char bracket;
    int lid;
    double linktime=-1.0; // default that causes assertion failure if unset
    auto* ltime=new LinkTime();
    ltime->periodlength=periodlength;
    ltime->nrperiods=nrperiods;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readtimes scanner jammed at " << bracket;
        return false;
    }
    in  >> lid ;
    ltime->id=lid;
    for (int i=0;i<nrperiods;i++)
    {
        in >> linktime;
        //(ltime->times).push_back(linktime);
        (ltime->times) [i] = linktime;
    }
    map <int,Link*>::iterator l_iter;
    l_iter = linkmap.find(lid);
    //   assert  ( l_iter < links.end() );     // lid exists
    assert (l_iter!=linkmap.end());
    assert ( linktime >= 0.0 );
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readtimes scanner jammed at " << bracket;
        return false;
    }
    (*l_iter).second->set_hist_time(linktime);
    (*l_iter).second->set_histtimes(ltime);
    linkinfo->times.insert(pair<int,LinkTime*> (lid,ltime));
#ifdef _DEBUG_NETWORK
    cout << " read a linktime"<<endl;
#endif //_DEBUG_NETWORK
    return true;
}

bool Network::copy_linktimes_out_in()
{
    bool ok=true;
    auto l_iter=linkmap.begin();
    for (;l_iter!=linkmap.end();l_iter++)
    {
        ok = ok  && ((*l_iter).second->copy_linktimes_out_in());
    }
    return ok;
}

bool Network::readincident (istream & in)
{
    // OPTIMIZE LATER
    char bracket;
    int lid;
    int sid;
    bool blocked;
    double penalty;
    double start;
    double stop;
    double info_start;
    double info_stop;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readincident scanner jammed at " << bracket;
        return false;

    }
    in  >> lid  >> sid >> penalty >> start >> stop >> info_start >> info_stop >> blocked;
    //assert  ( (find_if (links.begin(),links.end(), compare <Link> (lid))) < links.end() );     // lid exists
    assert (linkmap.count(lid));
    // assert  ( (find_if (sdfuncs.begin(),sdfuncs.end(), compare <Sdfunc> (sid))) < sdfuncs.end() );     // sid exists
    assert (sdfuncmap.count(sid));
    assert ( (penalty >= 0.0 ) && (start>0.0) && (stop>start) );
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readincident scanner jammed at " << bracket;
        return false;

    }
    auto* incident = new Incident(lid, sid, start,stop,info_start,info_stop,eventlist,this, blocked);
#ifndef _NO_GUI
    // Set the icon
    Link* link = linkmap[lid];
    int x = link->get_icon()->get_x ();
    int y = link->get_icon()->get_y();
    auto* iptr=new IncidentIcon(x,y);
    incident->set_icon(iptr);
    drawing->add_icon(iptr);

#endif
    incidents.insert(incidents.begin(), incident); // makes the incident and initialises its start in the eventlist
#ifdef _DEBUG_NETWORK
    cout <<"incident from " << start << " to " << stop << " on link nr " << lid << endl;
#endif //_DEBUG_NETWORK
    return find_alternatives_all(lid,penalty,incident);    // make the alternatives
}


bool Network::readincidents(istream& in)
{
    string keyword;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "incidents:")
    {
        return false;
    }
    int nr;
    in >> nr;
    for (int i = 0; i < nr; i++)
    {
        if (!readincident(in))
        {
            return false;
        }
    }

    return true;
}

bool Network::readincidentparams(istream& in)
{
    string keyword;
    in >> keyword;

#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "parameters:")
    {
        return false;
    }
    int nr;
    in >> nr;
    for (int i = 0; i < nr; i++)
    {
        if (!readincidentparam(in))
        {
            return false;
        }
    }

    return true;
}


bool Network::readincidentparam(istream& in)
{
    char bracket;
    double mu;
    double sd;
    in >> bracket;
    if (bracket != '{')
    {
        cout << "readfile::readincidentparam scanner jammed at " << bracket;
        return false;
    }
    in >> mu >> sd;
    in >> bracket;
    if (bracket != '}')
    {
        cout << "readfile::readincidentparam scanner jammed at " << bracket;
        return false;
    }
    incident_parameters.push_back(mu);
    //  cout << "checking: mu " << mu << " first of list " << incident_parameters[0] << endl;
    incident_parameters.push_back(sd);

    return true;
}

bool Network::readx1(istream& in)
{
    string keyword;
    char bracket;
    in >> keyword;
#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword != "X1:") {
        return false;
    }
    double mu;
    double sd;
    in >> bracket >> mu >> sd >> bracket;
    incident_parameters.push_back(mu);
    incident_parameters.push_back(sd);

    return true;
}


bool Network::readincidentfile(string name)
{
    ifstream inputfile(name.c_str());
    assert(inputfile);
    if (readsdfuncs(inputfile) && readincidents(inputfile) && readincidentparams(inputfile) && readx1(inputfile))
    {
        for (auto& incident : incidents)
        {
            incident->set_incident_parameters(incident_parameters);
        }
        inputfile.close();
        return true;
    }

    inputfile.close();
    return false;
}

bool Network::readpathfile(string name)
{
    ifstream inputfile(name.c_str());
    assert(inputfile);
    if (readroutes(inputfile))
    {
        inputfile.close();
        return true;
    }

    inputfile.close();
    return false;
}

bool Network::writepathfile(string name)
{
    ofstream out(name.c_str());
    assert(out);
    out << "routes:" << '\t'<< routemap.size() << endl;
    auto r_iter = routemap.begin();
    for (r_iter=routemap.begin();r_iter!=routemap.end();r_iter++)
    {
        (*r_iter).second->write(out);
    }
    out.close();
    return true;
}

void Network::write_transitroutes(string name)
{
    ofstream out(name.c_str());
    assert(out);
    out << "routes:" << '\t'<< busroutes.size() << endl;

    for (auto r_iter:busroutes)
    {
        r_iter->write(out);
    }
    out.close();

}

bool Network::readassignmentlinksfile(string name)
{
    ifstream in(name.c_str());
    assert(in);
    string keyword;
    string temp;
    in >> keyword;

#ifdef _DEBUG_NETWORK
    cout << keyword << endl;
#endif //_DEBUG_NETWORK
    if (keyword!="no_obs_links:")
    {
        in.close();
        return false;
    }
    int nr;
    int lid;
    in >> nr;
    in >> temp;
    assert (temp=="{");
    map <int, Link*>::iterator iter;
    for (int i=0; i<nr;i++)
    {
        in >> lid;
        iter = linkmap.find(lid);
        assert (iter!=linkmap.end()); // assert it exists
        (*iter).second->set_use_ass_matrix(true);
        no_ass_links++;
    }
    in >> temp;
    assert (temp=="}");
    in.close();
    return true;
}


bool Network::readparameters(string name)
{
    ifstream inputfile(name.c_str());
    assert(inputfile);
    if (theParameters->read_parameters(inputfile))
    {
        if (theParameters->pass_day_to_day_indicator != 0) 
        {
            cout << "theParameters->pass_day_to_day_indicator: " << theParameters->pass_day_to_day_indicator << endl;
        }
        // WILCO : deleted windows specific error code
        //SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        inputfile.close();
        return true;
    }

    inputfile.close();
    return false;
}

bool Network::init_shortest_path()
/* Initialises the shortest path graph with the link costs
*/
{
    int lid;
    int in;
    int out;
    int routenr;
    routenr=static_cast<int>(routemap.size());
    double cost;
    double mu;
    double sd;
    random=new (Random);

    if (randseed != 0) {
        random->seed(randseed);
    } else {
        random->randomize();
}

#ifdef _DEBUG_SP
    cout << "network::init_shortest path, routes.size  " << routenr << ", linkmap.size " << linkmap.size() << ", nodemap.size " << nodemap.size() << endl;
#endif //_DEBUG_SP
    // CREATE THE GRAPH
#ifndef _USE_VAR_TIMES
    graph=new Graph<double, GraphNoInfo<double> > (nodemap.size() /* 50000*/, linkmap.size(), 9999999.0);
#else
   // graph=new Graph<double, LinkTimeInfo > (nodemap.rbegin()->first+1, linkmap.rbegin()->first+1, 9999999.0); // Wilco 2018-03-01 - creates a graph with maxId(nodes) vertices and maxId(links) edges
    graph=new Graph<double, LinkTimeInfo > (nodemap.size()+1, linkmap.size()+1, 9999999.0); // Wilco 2020 - creates a graph at the right size

#endif
    // ADD THE LINKS AND NODES

    for (auto & iter : linkmap) // create the link graph for shortest path
    {
        lid=iter.second->get_id();
        in=iter.second->get_in_node_id();
        out=iter.second->get_out_node_id();
        mu=iter.second->get_hist_time();
        sd=disturbance*mu;
        cost=mu;
#ifdef _DEBUG_SP
        cout << " graph->addlink: link " << lid << ", innode " << in << ", outnode " << out << ", cost " << cost << endl;
#endif //_DEBUG_SP
        //graph->addLink(lid,in,out,cost);
        graph->addLink(link_to_graphlink[lid],node_to_graphnode[in],node_to_graphnode[out],cost);

    }
    // ADD THE TURNPENALTIES;

    // NOTE TODO WILCO--> Check out if we need to complete turn penalties


    // first set all the indices
    graph->set_downlink_indices();

    for(auto iter1=turnpenalties.begin();iter1<turnpenalties.end();iter1++)
    {
       // graph->set_turning_prohibitor((*iter1)->from_link, (*iter1)->to_link);
        int from = link_to_graphlink[(*iter1)->from_link];
        int to = link_to_graphlink[(*iter1)->to_link];
        graph->set_turning_prohibitor(from, to);
    }

    theParameters->shortest_paths_initialised= true;

    return true;
}


vector<Link*> Network::get_path(int destid)  // get the path to destid (original id)
{
#ifdef _DEBUG_SP
    eout << "shortest path to " << destid << endl << " with " ;
#endif //_DEBUG_SP
    int graphdest = node_to_graphnode[destid];
    vector <int> linkids=graph->shortest_path_vector(graphdest);  // get out the shortest path current root link to Destination (*iter3)
#ifdef _DEBUG_SP
    eout << linkids.size() << " links " << endl << "  : " ;
#endif //_DEBUG_SP
    vector <Link*> rlinks;

    if (linkids.empty())
    {
        cout << "Shortest path: get_path : PROBLEM OBTAINING links in path to  dest " << destid << endl;
        return rlinks;
    }
    for (auto iter4=linkids.begin();iter4<linkids.end();iter4++) // find all the links
    {
        //int lid=(*iter4);
        int lid = graphlink_to_link[(*iter4)];
#ifdef _DEBUG_SP
        eout << lid << " , ";
#endif //_DEBUG_SP
        map <int,Link*>::iterator l_iter;
        l_iter = linkmap.find(lid);
        assert (l_iter != linkmap.end());
        rlinks.insert(rlinks.end(),(*l_iter).second);
    }
    assert (!rlinks.empty());
    return rlinks;
}


bool Network::shortest_paths_all()
//calculate the shortest paths for each link emanating from each origin to each destination;
// and saving them if  there is a new path found (i.e. it's not in the routes vector already)
{
    double entrytime = 0.0;    // entry time for time-variant shortest path search
    int nr_reruns = static_cast<int> (runtime / theParameters->update_interval_routes) - 1; // except for last period
    // determines the number of reruns of the shortest path alg.
    int routenr = static_cast<int>(routemap.size());
    for (int i = 0; i < nr_reruns; i++)
    {
        entrytime = i * theParameters->update_interval_routes;
        int lastorigin = -1;
        for (auto iter1 = odpairs.begin(); iter1 < odpairs.end();)
        {
            // OD pairs are sorted by origin, destination
            // For each origin in OD pairs, find the destinations that need another route
            Origin* ori = (*iter1)->get_origin();
            lastorigin = ori->get_id();
            cout << "last origin: " << lastorigin << endl;
            vector <Destination*> dests;
            bool exitloop = false;
            while (!exitloop)
            {
                double od_rate = (*iter1)->get_rate();
                double nr_routes = (*iter1)->get_nr_routes();
                if (((od_rate > theParameters->small_od_rate) && ((od_rate / theParameters->small_od_rate) < nr_routes)) || (nr_routes < 1)) {
                    // if the od pair has not too many routes for its size
                    dests.push_back((*iter1)->get_destination());
                }
                iter1++;
                if (iter1 == odpairs.end()) {
                    exitloop = true;
                }
                else
                    if (((*iter1)->get_origin()->get_id()) != lastorigin) {
                        exitloop = true;
                    }
            }
            cout << " dests size is: " << dests.size() << endl;
            vector<Link*> outgoing = ori->get_links();
            for (auto iter2 = outgoing.begin(); iter2 < outgoing.end(); iter2++)
            {

                //	 #ifdef _DEBUG_SP
                cout << "shortest_paths_all: starting label correcting from root " << (*iter2)->get_id() << endl;
                //	#endif //_DEBUG_SP
#ifndef _USE_VAR_TIMES
                graph->labelCorrecting((*iter2)->get_id());  // find the shortest path from Link (*iter2) to ALL nodes
#else
                if (linkinfo != nullptr) 
                { // if there are link info times
                    graph->labelCorrecting(link_to_graphlink[(*iter2)->get_id()], entrytime, linkinfo);  // find the shortest path from Link (*iter2) to ALL nodes
                }
                else 
                {
                    graph->labelCorrecting(link_to_graphlink[(*iter2)->get_id()]);  // find the shortest path from Link (*iter2) to ALL nodes NO LINKINFO
                }
#endif // _USE_VAR_TIMES
#ifdef _DEBUG_SP
                cout << "finished label correcting for root link " << (*iter2)->get_id() << endl;
#endif //_DEBUG_SP
                for (auto iter3 = dests.begin(); iter3 < dests.end(); iter3++)
                {
#ifdef _DEBUG_SP
                    cout << " see if we can reach destination " << (*iter3)->get_id() << endl;
#endif //_DEBUG_SP
                    if (graph->reachable(node_to_graphnode[(*iter3)->get_id()])) // if the destination is reachable from this link...
                    {
#ifdef _DEBUG_SP
                        cout << " it's reachable.." << endl;
#endif //_DEBUG_SP
                        vector<Link*> rlinks = get_path((*iter3)->get_id());
#ifdef _DEBUG_SP
                        cout << " gotten path" << endl;
#endif //_DEBUG_SP
                        if (!rlinks.empty())
                        {
                            int frontid = (rlinks.front())->get_id();
#ifdef _DEBUG_SP
                            cout << " gotten front " << endl;
#endif //_DEBUG_SP
                            if (frontid != (*iter2)->get_id()) {
                                rlinks.insert(rlinks.begin(), (*iter2)); // add the root link to the path
                            }
                            routenr++;
#ifdef _DEBUG_SP
                            cout << " checking if the routenr does not already exist " << endl;
#endif //_DEBUG_SP
                            odval val = odval(ori->get_id(), (*iter3)->get_id());
                            assert(!exists_route(routenr, val)); // Check that no route exists with same routeid, at least for this OD pair
#ifdef _DEBUG_SP
                            cout << " making route " << endl;
#endif //_DEBUG_SP
                            Route* rptr = new  Route(routenr, ori, (*iter3), rlinks);
                            bool exists = true;
                            if (rptr != nullptr)
                            {
                                exists = exists_same_route(rptr);
                                if (!exists)
                                {
                                    routemap.insert(routemap.end(), pair <odval, Route*>(val, rptr)); // add the newly found route
                                }
                            }
                            else {
                                routenr--;
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool Network::shortest_pathtree_from_origin_link(int lid, double start_time)
{

    if (!theParameters->shortest_paths_initialised) { // initialize shortest path graph if needed
        init_shortest_path(); //sets parameter to initialized if successful
    }
    if (theParameters->shortest_paths_initialised)
    {
        if (linkinfo != nullptr)
        {
            graph->labelCorrecting(link_to_graphlink[lid], start_time, linkinfo);
        }
        else 
        {
            graph->labelCorrecting(link_to_graphlink[lid], start_time);
        }

        return true;
    }
    {
        return false; // could not init graph, so no search done
    }
}

vector<Link*> Network::shortest_path_to_node(int rootlink, int dest_node, double start_time) //!< returns shortest path Links
{

    vector<Link*> rlinks;
    if (shortest_pathtree_from_origin_link(rootlink, start_time))
    {
        if (graph->reachable(node_to_graphnode[dest_node])) {
            rlinks = get_path(dest_node); // get_path requires original node id
        }
        else {
            cout << "shortest_path_to_node : Error: Node " << dest_node << " is not reachable from rootlink "
                << rootlink << std::endl;
        }
    }
    return rlinks;
}

bool Network::find_alternatives_all(int lid, double penalty, Incident* incident)
// Makes sure that each affected link has an alternative
{
    map <int, map <int, Link*> > affected_links_per_dest; // indexed by destination, each destination will have a nr of affected links
    map <int, Origin*> affected_origins; // Simple map of affected origins
    map <int, Link*> affected_links; // simple map of affected links
    map <int, set <int> > links_without_alternative; // all links,dests without a 'ready' alternative. indexed by link_id, dest_id
    // Find all the affected links
    Link* incident_link = linkmap[lid];
    multimap <int, Route*> i_routemap = incident_link->get_routes();// get all routes through incident link
    // unsigned int nr_affected_routes = i_routemap.size();
    auto rmiter = i_routemap.begin();
    // get all affected (links,destinations) from each route, and store the origins as well
    for (; rmiter != i_routemap.end(); rmiter++)
    {
        Route* r = (*rmiter).second;
        int dest = (*rmiter).first;
        vector <Link*> route_affected_links_upstream = r->get_upstream_links(lid);
        vector<Link*>::iterator l_iter;
        for (l_iter = route_affected_links_upstream.begin(); l_iter != route_affected_links_upstream.end(); l_iter++)
        {
            Link* link = (*l_iter);
            int link_id = link->get_id();
            affected_links_per_dest[dest][link_id] = link;  // stores all affected links, per destination
            affected_links[link_id] = link; // stores all affected links, once
        }
        Origin* ori = r->get_origin();
        int oid = ori->get_id();
        affected_origins[oid] = ori;
    }
    // per destination, for all affected links, find out if they have an alternative that does not go through incident link
    auto lm_iter = affected_links_per_dest.begin();
    for (; lm_iter != affected_links_per_dest.end(); lm_iter++)
    {
        int dest = (*lm_iter).first;
        map <int, Link*> thelinks = (*lm_iter).second;
        auto linkiter = thelinks.begin();
        for (; linkiter != thelinks.end(); linkiter++)
        {
            Link* link = linkiter->second;
            link->set_selected(true); // set the affected link icon to 'selected colour'
#ifndef _NO_GUI
            link->set_selected_color(Qt::green);
#endif
            int linkid = link->get_id();
            int nr_alternatives = link->nr_alternative_routes(dest, lid);
            if (nr_alternatives == 0)
            {
                links_without_alternative[linkid].insert(dest);

#ifndef _NO_GUI
                link->set_selected_color(Qt::red); // set red colour for Affected links without alternatives
#endif
            }
            else
            {
                // Store the routes at the link (do this already at the counting)

            }

        }
    }

    // Add the affected links & origins to the Incident
    incident->set_affected_links(affected_links);
    incident->set_affected_origins(affected_origins);
    int found_links = 0;

    // IF affected_links_without_alternative is NOT empty
    if (!(links_without_alternative.empty()))
    {
        // DO a shortest path init, and a shortest path search wih penalty for incident link to create alternatives for each link.
        bool initok = false;
        if (!theParameters->shortest_paths_initialised) {
            initok = init_shortest_path();
        }
        if (initok)
        {
            auto mi = links_without_alternative.begin();
            for (; mi != links_without_alternative.end(); mi++)
            {
                // get shortest path and add.
                double cost = (graph->linkCost(link_to_graphlink[lid])) + penalty;
                int root = mi->first;
                Link* rootlink = linkmap[root];
                set <int> dests = mi->second;
                graph->linkCost(link_to_graphlink[lid], cost);
                graph->labelCorrecting(link_to_graphlink[root]);
                for (auto dest : dests)
                {
                    if (graph->reachable(node_to_graphnode[dest]))
                    {

                        vector<Link*> rlinks = get_path(dest); // original ID
#ifdef _DEBUG_SP
                        eout << " network::shortest_alternatives from link " << root << " to destination " << (*di) << endl;
                        graph->printPathToNode((*di));
#endif //_DEBUG_SP
                        //save the found remainder in the link table
                        int frontid = (rlinks.front())->get_id();
                        // let's makes sure the current link is in the path
                        if (frontid != root) {
                            rlinks.insert(rlinks.begin(), rootlink); // add the rddoot link from the path
                        }
                        rootlink->add_alternative(dest, rlinks);
                        found_links++;
                    }
                }
            }

        }
    }
    //Now select all links that are affected:
    cout << " nr of routes affected by incident " << i_routemap.size() << endl;
    return true;
}

/*
void Network::delete_spurious_routes()
{
}
*/
void Network::renum_routes()
{
    multimap <odval, Route*>::iterator route;
    int counter = 0;
    for (route = routemap.begin(); route != routemap.end(); route++, counter++)
    {
        (*route).second->set_id(counter);

    }

}

void Network::reset_link_icons() // reset the links to normal color and hide the incident icon
{
#ifndef _NO_GUI
    auto link = linkmap.begin();
    for (; link != linkmap.end(); link++)
    {
        link->second->set_selected(false);

        (link->second)->set_selected_color(theParameters->selectedcolor);
    }
    for (auto& incident : incidents)
    {
        incident->set_visible(false);
    }
#endif
}


bool Network::readmaster(string name)
{
    string temp;
    ifstream inputfile(name.c_str());
    //assert (inputfile);
    inputfile >> temp;
    if (temp != "#input_files")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    if (temp != "network=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #0
    inputfile >> temp;
    if (temp != "turnings=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #1
    inputfile >> temp;
    if (temp != "signals=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #2
    inputfile >> temp;
    if (temp != "histtimes=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #3
    inputfile >> temp;
    if (temp != "routes=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #4
    inputfile >> temp;
    if (temp != "demand=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); //  #5
    inputfile >> temp;
    if (temp != "incident=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #6
    inputfile >> temp;
    if (temp != "vehicletypes=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #7
    inputfile >> temp;
    if (temp != "virtuallinks=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #8
    inputfile >> temp;
    if (temp != "serverrates=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #9
    inputfile >> temp;
    if (temp != "#output_files")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    if (temp != "linktimes=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #10
    inputfile >> temp;
    if (temp != "output=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #11
    inputfile >> temp;
    if (temp != "summary=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); //  #12
    inputfile >> temp;
    if (temp != "speeds=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #13
    inputfile >> temp;
    if (temp != "inflows=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #14
    inputfile >> temp;
    if (temp != "outflows=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); //  #15
    inputfile >> temp;
    if (temp != "queuelengths=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #16
    inputfile >> temp;
    if (temp != "densities=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp); // #17
    inputfile >> temp;
    if (temp != "#scenario")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    if (temp != "starttime=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> starttime;
    inputfile >> temp;
    if (temp != "stoptime=") // stoptime==runtime
    {
        inputfile.close();
        return false;
    }
    inputfile >> runtime;
    theParameters->running_time = runtime;
    inputfile >> temp;
    if (temp != "calc_paths=")
    {
        calc_paths = false;
        inputfile.close();
        return true;
    }


    inputfile >> calc_paths;

    inputfile >> temp;
    if (temp != "traveltime_alpha=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> time_alpha;
    inputfile >> temp;
    if (temp != "parameters=")
    {
        inputfile.close();
        return false;
    }
    inputfile >> temp;
    filenames.push_back(workingdir + temp);   //  #18 Parameters

#ifdef _VISSIMCOM
    inputfile >> temp;
    if (temp != "vissimfile=")
    {
        //cout << "No vissimfile specified in masterfile" << endl;
        inputfile.close();
        return true;
    }
    else
    {
        // NOTE: here the full path is specified! (since it may be somewhere else)
        inputfile >> vissimfile; // read in separate var, because this part is only used with VISSIMCOM
    }
#endif //_VISSIMCOM
    inputfile >> temp;
    if (temp != "background=")
    {
        //cout << "No background specified in masterfile" << endl;
        inputfile.close();
        return true;

    }
    if (inputfile >> temp) {
        filenames.push_back(workingdir + temp); //  #19
    }

    inputfile.close();
    return true;
}

#ifndef _NO_GUI
double Network::executemaster(QPixmap* pm_, QMatrix* wm_)
{
    pm = pm_;
    wm = wm_;
    time = 0.0;
    if (!readparameters(filenames[18])) {
        cout << "Problem reading parameters: " << filenames[18] << endl; // read parameters first
    }

    //!< @todo fwf_wip, wip for fixed with drt implementation
    if(theParameters->drt)
    {
        if(theParameters->pass_day_to_day_indicator > 0 || theParameters->in_vehicle_d2d_indicator > 0)
        {
            if(theParameters->real_time_info == 0 && theParameters->share_RTI_network == 0 && theParameters->default_alpha_RTI == 0)
            {
                fwf_wip::day2day_drt_no_rti = true;
            }
        }
    }

    if (!readvtypes(filenames[7])) {
        cout << "Problem reading vtypes: " << filenames[6] << endl; // read the vehicle types first
    }
    if (!readnetwork(filenames[0])) {
        cout << "Problem reading network: " << filenames[0] << endl; // read the network configuration
    }
    if (!readvirtuallinks(filenames[8])) {
        cout << "Problem reading virtuallinks: " << filenames[7] << endl;	//read the virtual links
    }
    if (!readserverrates(filenames[9])) {
        cout << "Problem reading serverrates: " << filenames[8] << endl;	//read the virtual links
    }
    if (!register_links()) {
        cout << "Problem reading registering links at nodes " << endl; // register the links at the destinations, junctions and origins
    }
    if (!(readturnings(filenames[1])))
    {
        cout << "no turnings read, making new ones...." << endl;
        create_turnings(); // creates the turning movements for all junctions    if not read by file
        writeturnings(filenames[1]); // writes the new turnings
    }

    if (!(readlinktimes(filenames[3])))
    {
        cout << "no linktimes read, taking freeflow times... " << endl;
        set_freeflow_linktimes();  //read the historical link times if exist, otherwise set them to freeflow link times.
    }

    // 2005-11-28 put the reading of OD matrix before the paths...
    if (!readdemandfile(filenames[5])) {
        cout << "Problem reading OD matrix " << filenames[5] << endl; // generate the odpairs.
    }
    //Sort the ODpairs
    sort(odpairs.begin(), odpairs.end(), od_less_than);


    if (!(readpathfile(filenames[4]))) // read the known paths
    {
        cout << "no routes read from the pathfile" << endl;
        calc_paths = true; // so that new ones are calculated.
    }
    if (calc_paths)
    {
        if (!init_shortest_path()) {
            cout << "Problem starting init shortest path " << endl; // init the shortest paths
        }
        if (!shortest_paths_all()) {
            cout << "Problem calculating shortest paths for all OD pairs " << endl; // see if there are new routes based on shortest path
        }
    }
    // Sort the routes by OD pair
    // NOTE: Obsolete, routemap is always sorted by OD pair
    // sort(routes.begin(), routes.end(), route_less_than);

    // add the routes to the OD pairs AND delete the 'bad routes'
    add_od_routes();
    //	renum_routes (); // renum the routes
    writepathfile(filenames[4]); // write back the routes.
    // end temporary
    if (calc_paths)
    {
        //delete_spurious_routes();
        writepathfile(filenames[4]); // write back the routes.
    }

    this->readsignalcontrols(filenames[2]);
    // NEW 2007_03_08
#ifdef _BUSES
    // read the transit system input
    this->readtransitroutes(workingdir + "transit_routes.dat"); //FIX IN THE MAIN READ & WRITE
    this->readtransitnetwork(workingdir + "transit_network.dat"); //FIX IN THE MAIN READ & WRITE

    if (theParameters->drt)
    {
        if (!readcontrolcenters(workingdir + "controlcenters.dat")) //Note: currently dependent on being read after transit network (to find stops and lines) and before transit fleet (to find control center) and before path_set_generation
        {
            DEBUG_MSG_V("Problem reading controlcenters.dat. Aborting...");
            abort();
        }
    }

    this->readtransitfleet(workingdir + "transit_fleet.dat");
    this->readtransitdemand(workingdir + "transit_demand.dat");

    if (theParameters->empirical_demand == 1)
    {
        assert(theParameters->demand_format == 3);
        this->readtransitdemand_empirical(workingdir + "transit_demand_empirical.dat"); // needs to be called before path set generation (since odstops with no demand are skipped)
    }

    if (theParameters->demand_format == 3)
    {
        //generate_stop_ods();
        if (theParameters->choice_set_indicator == 0)
        {
            generate_consecutive_stops();
            if (theParameters->od_pairs_for_generation)
            {
                assert(!theParameters->drt); //this call path is currently completely untested with DRT
                read_od_pairs_for_generation(workingdir + "ODpairs_pathset.dat");
                find_all_paths_with_OD_for_generation();
            }
            else
            {
                find_all_paths_fast();
            }
        }
        else if (theParameters->choice_set_indicator == 1)
        {
            this->read_transit_path_sets(workingdir + "path_set_generation.dat");
        }
    }

    day = 1;
    day2day = new Day2day(1);
    if (theParameters->pass_day_to_day_indicator >= 1)
    {
        this->read_transitday2day(workingdir + "transit_day2day.dat");
    }
    if (theParameters->in_vehicle_d2d_indicator >= 1)
    {
        this->read_IVTT_day2day(workingdir + "transit_day2day_onboard.dat");
    }
#endif // _BUSES
    if (!init()) {
        cout << "Problem initialising " << endl;
    }
    if (!readincidentfile(filenames[6])) {
        cout << "Problem reading incident file " << filenames[5] << endl; // reads the incident file   and makes all the alternative routes at all  links
    }
    if (filenames.size() >= 20) {
        drawing->set_background(filenames[19].c_str());
    }
    if (theParameters->use_ass_matrix)
    {
        this->readassignmentlinksfile(workingdir + "assign_links.dat"); //FIX IN THE MAIN READ & WRITE
    }

    return runtime;
}
#endif // _NO_GUI

double Network::executemaster()
{
    time = 0.0;
    if (!readparameters(filenames[18])) {
        cout << "Problem reading parameters: " << filenames[18] << endl; // read parameters first
    }

    //!< @todo fwf_wip, wip for fixed with drt implementation
    if(theParameters->drt)
    {
        if(theParameters->pass_day_to_day_indicator > 0 || theParameters->in_vehicle_d2d_indicator > 0)
        {
            if(theParameters->real_time_info == 0 && theParameters->share_RTI_network == 0 && theParameters->default_alpha_RTI == 0)
            {
                fwf_wip::day2day_drt_no_rti = true;
            }
        }
    }

    if (!readvtypes(filenames[7])) {
        cout << "Problem reading vtypes: " << filenames[6] << endl; // read the vehicle types
    }
    if (!readnetwork(filenames[0])) {
        cout << "Problem reading network: " << filenames[0] << endl; // read the network configuration
    }
    if (!readvirtuallinks(filenames[8])) {
        cout << "Problem reading virtuallinks: " << filenames[7] << endl;	//read the virtual links
    }
    if (!readserverrates(filenames[9])) {
        cout << "Problem reading serverrates: " << filenames[8] << endl;	//read the virtual links
    }
    if (!register_links()) {
        cout << "Problem reading registering links at nodes " << endl; // register the links at the destinations, junctions and origins
    }
    if (!(readturnings(filenames[1])))
    {
        cout << "no turnings read, making new ones...." << endl;
        create_turnings(); // creates the turning movements for all junctions    if not read by file
        writeturnings(filenames[1]); // writes the new turnings
    }
    if (!(readlinktimes(filenames[3])))
    {
        cout << "no linktimes read, taking freeflow times... " << endl;
        set_freeflow_linktimes();  //read the historical link times if exist, otherwise set them to freeflow link times.
    }
    // New 2005-11-28 put the reading of OD matrix before the paths...
    if (!readdemandfile(filenames[5])) {
        cout << "Problem reading OD matrix " << filenames[4] << endl; // generate the odpairs.
    }
    //Sort the ODpairs
    sort(odpairs.begin(), odpairs.end(), od_less_than);


    if (!(readpathfile(filenames[4]))) // read the known paths
    {
        cout << "no routes read from the pathfile" << endl;
        calc_paths = true; // so that new ones are calculated.
    }
    if (calc_paths)
    {
        if (!init_shortest_path()) {
            cout << "Problem starting init shortest path " << endl; // init the shortest paths
        }
        if (!shortest_paths_all()) {
            cout << "Problem calculating shortest paths for all OD pairs " << endl; // see if there are new routes based on shortest path
        }
    }
    // Sort the routes by OD pair
    //sort(routes.begin(), routes.end(), route_less_than);
    // add the routes to the OD pairs
    add_od_routes();
    //	renum_routes ();
    // temporary
    writepathfile(filenames[4]); // write back the routes.
    // end temporary
    if (calc_paths)
    {
        //delete_spurious_routes();
        writepathfile(filenames[4]); // write back the routes.
    }

    readsignalcontrols(filenames[2]);
#ifdef _BUSES
    // read the transit system input
    this->readtransitroutes(workingdir + "transit_routes.dat"); //FIX IN THE MAIN READ & WRITE
    this->readtransitnetwork(workingdir + "transit_network.dat"); //FIX IN THE MAIN READ & WRITE

    if (theParameters->drt)
    {
        if (!readcontrolcenters(workingdir + "controlcenters.dat")) //Note: currently dependent on being read after transit network (to find stops and lines) and before transit fleet (to find control center) and path_set_generation (if direct lines are autogenerated)
        {
            DEBUG_MSG_V("Problem reading controlcenters.dat. Aborting...");
            abort();
        }
    }

    this->readtransitfleet(workingdir + "transit_fleet.dat");
    this->readtransitdemand(workingdir + "transit_demand.dat");

    if (theParameters->empirical_demand == 1)
    {
        assert(theParameters->demand_format == 3);
        this->readtransitdemand_empirical(workingdir + "transit_demand_empirical.dat");
    }

    if (theParameters->demand_format == 3)
    {
        //generate_stop_ods();
        if (theParameters->choice_set_indicator == 0)
        {
            generate_consecutive_stops();
            if (theParameters->od_pairs_for_generation == true)
            {
                read_od_pairs_for_generation(workingdir + "ODpairs_pathset.dat");
                find_all_paths_with_OD_for_generation();
            }
            else
            {
                find_all_paths_fast();
            }
        }
        else if (theParameters->choice_set_indicator == 1)
        {
            this->read_transit_path_sets(workingdir + "path_set_generation.dat");
        }
    }

    day = 1;
    day2day = new Day2day(1);
    if (theParameters->pass_day_to_day_indicator >= 1)
    {
        this->read_transitday2day(workingdir + "transit_day2day.dat");
    }
    if (theParameters->in_vehicle_d2d_indicator >= 1)
    {
        this->read_IVTT_day2day(workingdir + "transit_day2day_onboard.dat");
    }

#endif //_BUSES

    if (!init()) {
        cout << "Problem initialising " << endl;
    }
    if (!readincidentfile(filenames[6])) {
        cout << "Problem reading incident file " << filenames[5] << endl; // reads the incident file   and makes all the alternative routes at all  links
    }
    if (theParameters->use_ass_matrix)
    {
        this->readassignmentlinksfile(workingdir + "assign_links.dat"); // !!! WE NEED TO FIX THIS INTO THE MAIN READ& WRITE
    }

    return runtime;
}


bool Network::writeall(unsigned int repl)
{
    replication = repl;
    string rep;
    string cleantimes;
    end_of_simulation();
    string linktimesfile = filenames[10];
    cleantimes = linktimesfile + ".clean";
    string summaryfile = filenames[12];
    string vehicleoutputfile = filenames[11];
    string allmoesfile = "allmoes.dat";
    string assignmentmatfile = "assign.dat";
    string vqueuesfile = "v_queues.dat";
    if (replication > 0)
    {
        stringstream repstr;
        repstr << "." << replication;
        rep = repstr.str();
        cleantimes = linktimesfile + ".clean" + rep;
        linktimesfile += rep;
        summaryfile += rep;
        vehicleoutputfile += rep;
        allmoesfile += rep;
        assignmentmatfile += rep;
        vqueuesfile += rep;
    }
    writelinktimes(linktimesfile);
    // NEW: Write also the non-smoothed times

    time_alpha = 1.0;
    writelinktimes(cleantimes);

    writesummary(summaryfile); // write the summary first because
    writeoutput(vehicleoutputfile);  // here the detailed output is written and then deleted from memory
    writemoes(rep);
    writeallmoes(allmoesfile);
    //writeheadways("timestamps.dat"); // commented out, since no-one uses them
    writeassmatrices(assignmentmatfile);
    write_v_queues(vqueuesfile);
    this->write_busstop_output(
        workingdir + "o_transitlog_out.dat",
        workingdir + "o_transitstop_sum.dat",
        workingdir + "o_transitline_sum.dat",
        workingdir + "o_transit_trajectory.dat",
        workingdir + "o_passenger_boarding.dat",
        workingdir + "o_passenger_alighting.dat",
        workingdir + "o_segments_trip_loads.dat",
        workingdir + "o_selected_paths.dat",
        workingdir + "o_segments_line_loads.dat",
        workingdir + "o_od_stops_summary.dat",
        workingdir + "o_trip_total_travel_time.dat",
        workingdir + "o_od_stop_summary_without_paths.dat",
        workingdir + "o_passenger_waiting_experience.dat",
        workingdir + "o_passenger_onboard_experience.dat",
        workingdir + "o_passenger_connection.dat",
        workingdir + "o_passenger_trajectory.dat",
        workingdir + "o_passenger_welfare_summary.dat",
        workingdir + "o_fwf_summary.dat",
        workingdir + "o_passenger_transitmode.dat",
        workingdir + "o_passenger_dropoff.dat",
        workingdir + "o_vkt.dat",
        workingdir + "o_fwf_summary_odcategory.dat",
        workingdir + "o_fwf_drtvehicle_states.dat",
        workingdir + "o_time_spent_in_state_at_stop.dat"
    );
    write_transitroutes(workingdir + "o_transit_routes.dat");
    return true;
}

bool Network::writeallmoes(string name)
{
    //cout << "going to write to this file: " << name << endl;
    ofstream out(name.c_str());
    //assert(out);

    //out << "CONVERGENCE" << endl;

    //out << "SumDiff_InputOutputLinkTimes : " << calc_diff_input_output_linktimes () << endl << endl;
    //out << "SumSquare_InputOutputLinkTimes : " << calc_sumsq_input_output_linktimes () << endl << endl;
    //out << "Root Mean Square Linktimes : " <<this->calc_rms_input_output_linktimes() << endl;
    //out << "Root Mean Square Normalized Linktimes : " <<this->calc_rmsn_input_output_linktimes() << endl;
    out << "MOES" << endl;
    /****** TEMPORARY TO CUT OUT THE ALL MOES THAT ARE NOT USED NOW
    int maxindex=0;

    for (map<int, Link*>::iterator iter=linkmap.begin();iter!=linkmap.end();iter++)
    {
    out << (*iter).second->get_id() << "\t\t\t\t\t";
    maxindex=_MAX (maxindex, (*iter).second-> max_moe_size());
    }
    out << endl;
    for (map<int,Link*>::iterator iter1=linkmap.begin();iter1!=linkmap.end();iter1++)
    {
    out << "speed"<< "\t" << "density" << "\t" << "inflow" <<"\t" << "outflow" <<"\t" << "queue" << "\t";
    }
    out << endl;
    for (int index=0;index <maxindex; index++)
    {
    for (map<int,Link*>::iterator iter=linkmap.begin();iter!=linkmap.end();iter++)
    {
    (*iter).second->write_speed(out,index);
    (*iter).second->write_density(out,index);
    (*iter).second->write_inflow(out,index);
    (*iter).second->write_outflow(out,index);
    (*iter).second->write_queue(out,index);
    }
    out << endl;
    }
    */
    out.close();
    return true;
}

double Network::calc_diff_input_output_linktimes()
{
    double total = 0.0;
    for (auto& iter1 : linkmap)
    {
        if (iter1.second->get_nr_passed() > 0) {
            total += iter1.second->calc_diff_input_output_linktimes();
        }
    }
    return total;
}

double Network::calc_sumsq_input_output_linktimes()
{
    double total = 0.0;
    for (auto& iter1 : linkmap)
    {
        if (iter1.second->get_nr_passed() > 0) {
            total += iter1.second->calc_sumsq_input_output_linktimes();
        }
    }
    return total;
}

double Network::calc_mean_input_linktimes()
{
    return this->linkinfo->mean();

}

double Network::calc_rms_input_output_linktimes()
{
    double n = linkmap.size() * nrperiods;
    double ssq = calc_sumsq_input_output_linktimes();
    double result = sqrt(ssq / n);
    return result;
}

double Network::calc_rmsn_input_output_linktimes()
{
    return (calc_rms_input_output_linktimes() / calc_mean_input_linktimes());
}

double Network::calc_mean_input_odtimes()
{
    double n = odpairs.size();
    double sum = 0.0;
    for (auto& odpair : odpairs)
    {
        sum += odpair->get_mean_old_odtimes();
    }
    return sum / n;
}

double Network::calc_rms_input_output_odtimes()
{
    double n = odpairs.size();
    double diff = 0.0;
    double ssq = 0.0;
    for (auto& odpair : odpairs)
    {
        diff = odpair->get_diff_odtimes();
        ssq += diff * diff;
    }

    return sqrt(ssq / n);
}

double Network::calc_rmsn_input_output_odtimes()
{
    return (calc_rms_input_output_odtimes() / calc_mean_input_odtimes());
}

bool Network::writemoes(string ending)
{
    string name = filenames[13] + ending; // speeds
    ofstream out(name.c_str());
    //assert(out);

    for (auto& iter : linkmap)
    {

        iter.second->write_speeds(out, nrperiods);
    }
    out.close();
    name = filenames[14] + ending; // inflows
    out.open(name.c_str());
    //assert(out);

    for (auto& iter1 : linkmap)
    {
        iter1.second->write_inflows(out, nrperiods);
    }
    out.close();
    name = filenames[15] + ending; // outflows
    out.open(name.c_str());
    //assert(out);

    for (auto& iter2 : linkmap)
    {
        iter2.second->write_outflows(out, nrperiods);
    }
    out.close();
    name = filenames[16] + ending; // queues
    out.open(name.c_str());
    //assert(out);

    for (auto& iter3 : linkmap)
    {
        iter3.second->write_queues(out, nrperiods);
    }
    out.close();
    name = filenames[17] + ending; // densities
    out.open(name.c_str());
    //assert(out);

    for (auto& iter4 : linkmap)
    {
        iter4.second->write_densities(out, nrperiods);
    }
    out.close();
    return true;
}


bool Network::write_v_queues(string name)
{
    ofstream out(name.c_str());
    //assert(out);
    for (auto& iter : originmap)
    {
        iter.second->write_v_queues(out);
    }
    out.close();
    return true;
}


bool Network::writeassmatrices(string name)
{
    ofstream out(name.c_str());
    //assert(out);
    out << "no_obs_links: " << no_ass_links << endl;
    int nr_periods = static_cast<int>(runtime / theParameters->ass_link_period);
    out << "no_link_pers: " << nr_periods << endl;
    for (int i = 0; i < nr_periods; i++)
    {
        out << "link_period: " << i << endl;
        for (auto& iter1 : linkmap)
        {
            iter1.second->write_ass_matrix(out, i);
        }
        //out << endl;
    }
    out.close();
    return true;

}


bool Network::init()
{
    // initialise the turning events
    double initvalue = 0.1;
    for (auto& iter : turningmap)
    {
        iter.second->init(eventlist, initvalue);
        initvalue += 0.00001;
    }
    // initialise the signal controllers (they will init the plans and stages)
    for (auto iter1 = signalcontrols.begin(); iter1 < signalcontrols.end(); iter1++)
    {
        (*iter1)->execute(eventlist, initvalue);
        initvalue += 0.000001;
        //	cout << "Signal control initialised " << endl;
    }
#ifdef _BUSES
    // Initialise the buslines
    for (auto iter3 = buslines.begin(); iter3 < buslines.end(); iter3++)
    {
        (*iter3)->execute(eventlist, initvalue);
        initvalue += 0.00001;
    }

    //!< @todo PARTC specific, remove
    for (auto line : buslines)
    {
        if (!line->is_flex_line())
        {
            if (line->stops.back()->get_id() == PARTC::morby_station_id) //all fixed lines end at morby station
            {
                PARTC::drottningholm_case = true;
                PARTC::transfer_stop = busstopsmap[PARTC::transfer_stop_id];
                assert(PARTC::transfer_stop->get_id() == PARTC::transfer_stop_id);
            }
            else
                PARTC::drottningholm_case = false;
        }
    }

    if (theParameters->demand_format == 3)
    {
        if (theParameters->empirical_demand == 1)
        {
            //!< @todo should work with day2day as well as far as I can tell, however this needs to be tested further, seems to work with SF network with day2day in test group
            //assert(theParameters->pass_day_to_day_indicator == 0); 
            //assert(theParameters->in_vehicle_d2d_indicator == 0);

            for (const auto& od_arrival : empirical_passenger_arrivals) //add all empirical passenger arrivals to corresponding OD in terms of stops.
            {
                ODstops* od_stop = od_arrival.first;
                double arrival_time = od_arrival.second;

                if (od_stop->check_path_set() == true) // if no path set available to traveler then skip it
                {
                    if (!od_stop->is_active()) // if non-empty path set and not an initialization call. Active set first time ODstop::execute is called. For resets then the passengers should already have been added to the odstops_demand->execute chain via add_passenger_to_odstop
                    {
                        Passenger* pass = new Passenger(pid, arrival_time, od_stop);
                        od_stop->add_passenger_to_odstop(pass);
                        pid++;
                        pass->init();
                        eventlist->add_event(arrival_time, pass);
                    }
                    else
                    {
                        assert((theParameters->pass_day_to_day_indicator > 0 || theParameters->in_vehicle_d2d_indicator > 0)); // if od stop is active here, this means we have this is not the first call to Network::init after reset AND passengers are not deleted between resets with day2day (they have memory)
                        assert(od_stop->has_empirical_arrivals()); //Empirical passengers will be initialized via ODstops::execute instead ('active' call to this will loop over ODstops::passengers_during_simulation)
                    }
                }
            }
        }
        for (auto iter_odstops = odstops_demand.begin(); iter_odstops < odstops_demand.end(); iter_odstops++)
        {
            if ((*iter_odstops)->get_arrivalrate() != 0.0 || (*iter_odstops)->has_empirical_arrivals())
            {
                if ((*iter_odstops)->check_path_set())
                {
                    (*iter_odstops)->execute(eventlist, initvalue); // adds an event for the generation time of the first passenger per OD in terms of stops
                }
                initvalue += 0.00001;
            }
        }
    }
    //Initialize the DRT vehicles to their starting stop at their starting time
    if (theParameters->drt)
    {
        //Initialize rebalancing calls of controlcenter(s), initvalue is one rebalancing interval after the start pass generation period....
        double rb_init_time = theParameters->start_pass_generation + ::drt_first_rebalancing_time + 0.1;
        for(auto cc : ccmap) // initialize all potential rebalancing events
        {
            if(cc.second->rb_strategy_ != 0) // @todo for now do not initialize any rebalancing events if no strategy is being used, may change this later if rebalancing is triggered dynamically or at a later time
            {
                //double init_time = cc.second->get_next_rebalancing_time(rb_init_time);
                double init_time = rb_init_time;
                eventlist->add_event(init_time, cc.second->rebalancing_action_);
                rb_init_time += 0.00001;
            }
        }

        //Add buses to vector of unassigned vehicles and initial Busstop
        for (const DrtVehicleInit& drt_init : drtvehicles)
        {
            Bus* bus = get<0>(drt_init);
            Busstop* stop = get<1>(drt_init);
            Controlcenter* cc = get<2>(drt_init);
            double init_time = get<3>(drt_init);

            assert(bus->is_flex_vehicle());

            bus->set_curr_trip(nullptr); //unlike non-dynamically generated bus/trips this bus should not have a trip between resets as well
            bus->set_on_trip(false);
            cc->connectVehicle(bus); //connect vehicle to a control center
            cc->addVehicleToAllServiceRoutes(bus); //initially, each vehicle of this control center can be assigned to any service route of the control center
            cc->addInitialVehicle(bus);
            stop->book_unassigned_bus_arrival(eventlist, bus, init_time); //should be in a Null state until their init_time (also adds a Busstop event scheduled for the init_time of vehicle  to switch state of bus to IdleEmpty from Null)
            bus->set_init_time(init_time);
        }
    }
#endif //_BUSES
#ifdef _DEBUG_NETWORK
    cout << "turnings initialised" << endl;
#endif //_DEBUG_NETWORK
    // initialise the od pairs and their events
    for (auto iter0 = odpairs.begin(); iter0 < odpairs.end();)
    {
        if ((*iter0)->get_nr_routes() == 0) //chuck out the OD pairs without paths
        {
            //#ifdef _DEBUG_NETWORK
            cout << "OD pair " << (*iter0)->get_origin()->get_id() << " - " <<
                (*iter0)->get_destination()->get_id() << " does not have any route connecting them. deleting..." << endl;
            //#endif //_DEBUG_NETWORK
            delete* iter0;
            iter0 = odpairs.erase(iter0);
        }
        else // otherwise initialise them
        {
            (*iter0)->execute(eventlist, initvalue);
            initvalue += 0.00001;
            iter0++;
        }
    }

#ifdef _DEBUG_NETWORK
    cout << "odpairs initialised" << endl;
    cout << "number of destinations " << destinations.size() << endl;
#endif //_DEBUG_NETWORK
    // initialise the destination events
    //	register_links();
    for (auto& iter2 : destinationmap)
    {
        iter2.second->execute(eventlist, initvalue);
        initvalue += 0.00001;
    }

    // Initialise the communication
#ifdef _MIME
    if (communicator->is_connected())
    {
#ifdef _PVM
        communicator->initsend();
#endif //_PVM
#ifdef _VISSIMCOM
        communicator->register_virtuallinks(&virtuallinks); // register the virtual links
        communicator->init(vissimfile, runtime);

#endif // _VISSIMCOM
        communicator->register_boundaryouts(&boundaryouts);          // register the boundary nodes that send messages
        communicator->register_boundaryins(&boundaryins);          // register the boundary nodes that receive messages

        communicator->execute(eventlist, initvalue); // book yourself for the first time.
        initvalue += 0.000001;
    }
#endif // _MIME
#ifndef _NO_GUI
    recenter_image();
#endif // _NO_GUI
    return true;
}

bool Network::run(int period)
{
    // This part will be transferred to the GUI

    double t0 = timestamp();
    double tc;
    double next_an_update = t0 + an_step;
#ifndef _NO_GUI
    drawing->draw(pm, wm);
#endif //_NO_GUI
    //eventhandle->startup();
    double time = 0.0;
    while ((time > -1.0) && (time < period))       // the big loop
    {
        time = eventlist->next();
        if (time > (next_an_update - t0) * speedup)  // if the sim has come far enough
        {
            tc = timestamp();
            while (tc < next_an_update)  // wait till the next animation update
            {
                tc = timestamp();
            }
            //	eventhandle->startup();     //update animation
#ifndef _NO_GUI
            drawing->draw(pm, wm);
#endif // _NO_GUI
            next_an_update += an_step;    // update next step.
        }
    }
    //double tstop=timestamp();

    //cout << "running time " << (tstop-t0) << endl;
    return 0;

}

double Network::step(double timestep)
// same as run, but more stripped down. Called every timestep by the GUI
{
    double t0 = timestamp();
#ifndef _NO_GUI
    double tc; // current time
#endif //_NO_GUI
    double next_an_update = t0 + timestep;   // when to exit
    Q_UNUSED(next_an_update)

        if (theParameters->pass_day_to_day_indicator == 0 && theParameters->in_vehicle_d2d_indicator == 0)
        {
            while ((time > -1.0) && (time < runtime))       // the big loop
            {
                time = eventlist->next();
                //cout << time << "\t";
#ifndef _NO_GUI

                tc = timestamp();

                if (tc > next_an_update)  // the time has come for the next animation update
                {
                    drawing->draw(pm, wm);
                    return time;
                }

#endif //_NO_GUI
            }


            if (time >= runtime) // quick and dirty way of updating vehicle state timers after sim run is over
            {
                // update final states of all vehicles to 'Null' for 'time in vehicle state' output
                for (auto bus : this->busvehicles) // fixed vehicles
                {
                    bus->set_state(BusState::Null, runtime);
                }
                if (theParameters->drt)
                {
                    for (auto cc : this->ccmap)
                    {
                        map<int, Bus*> drtvehicles = cc.second->getConnectedVehicles();
                        for (auto veh : drtvehicles)
                        {
                            veh.second->set_state(BusState::Null, runtime);
                        }
                    }
                }
            }

            return time;
        }
        else
        {
            total_pass_boarded_per_line_d2d.clear(); // Never used without day2day, want to clear this between resets but not add it to Network::reset since it collectes data for each day and reset is called between days.
        }


    enum m { wt, ivt };
    float crit[2];
    crit[wt] = 1000.0F;
    crit[ivt] = 1000.0F;
    float theta = theParameters->break_criterium;
    //map<ODSL, Travel_time> wt_rec; //the record of waiting time data
    //map<ODSLL, Travel_time> ivt_rec; //the record of in-vehicle time data
    //day2day->reset();
    bool iter = true;
    while (iter) //day2day
    {
        cout << "Day: " << day << endl;

        crit[wt] = 0.0F;
        crit[ivt] = 0.0F;

        double timer = 1200;
        while ((time > -1.0) && (time < runtime))       // the big loop
        {
            time = eventlist->next();
            //cout << time << "\t";

            if (time >= timer) //Jens 2014
            {
                cout << "Time: " << timer << endl;
                timer += 1200;
            }

#ifndef _NO_GUI

            tc = timestamp();

            if (tc > next_an_update)  // the time has come for the next animation update
            {
                drawing->draw(pm, wm);
                return time;
            }

            if (time >= runtime) // quick and dirty way of updating vehicle state timers after sim run is over
            {
                // update final states of all vehicles to 'Null' for 'time in vehicle state' output
                for (auto bus : this->busvehicles) // fixed vehicles
                {
                    bus->set_state(BusState::Null, runtime);
                }
                if (theParameters->drt)
                {
                    for (auto cc : this->ccmap)
                    {
                        map<int, Bus*> drtvehicles = cc.second->getConnectedVehicles();
                        for (auto veh : drtvehicles)
                        {
                            veh.second->set_state(BusState::Null, runtime);
                        }
                    }
                }
            }
#endif //_NO_GUI
        }


        for (const auto& line : buslines) // collect traveler boarding on each line
        {
            total_pass_boarded_per_line_d2d[line][day] = line->get_total_boarded_pass();
            //total_pass_boarded_per_line_d2d[line][day] = line->count_total_boarded_passengers();
        }

        if (theParameters->pass_day_to_day_indicator > 0)
        {
            crit[wt] = insert(wt_rec, day2day->process_wt_replication(odstops, wt_rec, buslines)); //insert result from day2day learning in data container
        }

        if (theParameters->in_vehicle_d2d_indicator > 0)
        {
            crit[ivt] = insert(ivt_rec, day2day->process_ivt_replication(odstops, ivt_rec, buslines)); //insert result from day2day learning in data container
        }

        string addition = To_String(static_cast<long double>(crit[wt])) + "\t" + To_String(static_cast<long double>(crit[ivt]));
        day2day->write_output(workingdir + "o_convergence.dat", addition); // uses the internal 'wt_day' and 'ivt_day' states that are set/updated in the process_tt_replication methods....
        if (fwf_wip::day2day_drt_no_rti)
        {
            //Write out the ODSL and ODSLL tables here @todo it seems like 'day' doesn't necessarily correspond to the actual Network::day, but rather the first time a network element was experienced
            if (day == 1)
            {
                Day2day::write_wt_alphas_header(workingdir + "o_fwf_wt_alphas.dat");
                Day2day::write_ivt_alphas_header(workingdir + "o_fwf_ivt_alphas.dat");

                write_day2day_modesplit_header(workingdir + "o_fwf_day2day_modesplit.dat");
                write_day2day_boardings_header(workingdir + "o_fwf_day2day_boardings.dat");
                if (fwf_wip::write_all_pass_experiences)
                {
                    write_day2day_passenger_waiting_experience_header(workingdir + "o_fwf_day2day_passenger_waiting_experience.dat");
                    write_day2day_passenger_onboard_experience_header(workingdir + "o_fwf_day2day_passenger_onboard_experience.dat");
                    write_day2day_passenger_transitmode_header(workingdir + "o_fwf_day2day_passenger_transitmode.dat");
                }
            }
            Day2day::write_wt_alphas(workingdir + "o_fwf_wt_alphas.dat", wt_rec);
            Day2day::write_ivt_alphas(workingdir + "o_fwf_ivt_alphas.dat", ivt_rec);

            write_day2day_modesplit(workingdir + "o_fwf_day2day_modesplit.dat"); // write modesplit for the current day
            write_day2day_boardings(workingdir + "o_fwf_day2day_boardings.dat"); // write total boardings per line for the current day

            if (fwf_wip::write_all_pass_experiences)
            {
                write_day2day_passenger_waiting_experience(workingdir + "o_fwf_day2day_passenger_waiting_experience.dat");
                write_day2day_passenger_onboard_experience(workingdir + "o_fwf_day2day_passenger_onboard_experience.dat");
                write_day2day_passenger_transitmode(workingdir + "o_fwf_day2day_passenger_transitmode.dat");
            }
        }

        cout << "Convergence: " << crit[wt] << " " << crit[ivt] << endl;

        day++;
        day2day->update_day(day); // clears the 'wt_day' and 'ivt_day' internal states of day2day, updates the kapas (decaying learning weights) with the current day
        bool convergence_reached = fwf_wip::day2day_no_convergence_criterium ? false : (crit[wt] < theta || crit[ivt] < theta);
        if (!convergence_reached && day <= theParameters->max_days)
        {
            reset();
            read_transitday2day(wt_rec);
            read_IVTT_day2day(ivt_rec);
        }
        else
        {
            iter = false; //break the while loop when one of the criteria has been reached
        }
    }
    wt_rec.clear();
    ivt_rec.clear();

    return time;

}

#ifndef _NO_GUI
// Graphical funcitons

void Network::recenter_image()
{
    wm->reset(); // resets the worldmatrix to unary matrix (neutral zoom and pan)
    // position the drawing in the center of the pixmap
    vector <int> boundaries = drawing->get_boundaries();

    width_x = static_cast<double>(boundaries[2]) - static_cast<double>(boundaries[0]);
    height_y = static_cast<double>(boundaries[3]) - static_cast<double>(boundaries[1]);
    double scale_x = (pm->width()) / width_x;
    double scale_y = (pm->height()) / height_y;

    scale = _MIN(scale_x, scale_y);
    // cout << "scales. x: " << scale_x << " y: " << scale_y <<" scale: " << scale <<  endl;
    wm->translate(boundaries[0], boundaries[1]); // so that (minx,miny)=(0,0)

    // center the image
    if (scale_x > scale_y)// if the Y dimension determines the scale
    {
        double move_x = (pm->width() - (width_x * scale_y)) / 2; //
        wm->translate(move_x, 0);
    }
    else
    {
        double move_y = (pm->height() - (height_y * scale_x)) / 2; //
        wm->translate(0, move_y);
    }
    wm->scale(scale, scale);
}

/**
*	initialize to fit the network boundaries into
*   the standard graphic view:
*	a) transform from model coordinate (O')
*      to standar view coordinate (O)
*		X=sxy*(X'-Xmin')
*		Y=sxy*(Y'-Ymin')
*	b) move the overscaled dimension to centre
*   IMPORTANT: the order of operation is
*	step 1: translate (-Xmin', -Ymin')
*   step 2: scale the coordinate with a factor=min{view_X/model_X', view_Y/model_Y'}
*   step 3: translate the overscaled dimension
*   PROOF BY M=M1*M2*M3
*/
QMatrix Network::netgraphview_init()
{
    // make sure initial worldmatrix is a unit matrix
    initview_wm.reset();

    vector <int> boundaries = drawing->get_boundaries();
    //initview_wm.translate(boundaries[0],boundaries[1]);

    //scale the image and then centralize the image along
    //the overscaled dimension
    width_x = static_cast<double>(boundaries[2]) - static_cast<double>(boundaries[0]);
    height_y = static_cast<double>(boundaries[3]) - static_cast<double>(boundaries[1]);
    double scale_x = (pm->width()) / width_x;
    double scale_y = (pm->height()) / height_y;

    if (scale_x > scale_y) {
        scale = scale_y;
        // the x dimension is overscaled
        double x_adjust = pm->width() / 2 - width_x * scale / 2;
        initview_wm.translate(x_adjust, 0);
        initview_wm.scale(scale, scale);
    }
    else {
        scale = scale_x;
        double y_adjust = pm->height() / 2 - height_y * scale / 2;
        initview_wm.translate(0, y_adjust);
        initview_wm.scale(scale, scale);
    }
    initview_wm.translate(-boundaries[0], -boundaries[1]);
    // make a copy to "wm"
    (*wm) = initview_wm;
    // return the information to the canvas
    return initview_wm;
}


void Network::redraw() // redraws the image
{
    drawing->draw(pm, wm);
}

#endif // _NO_GUI

// INCIDENT FUNCTIONS

void Network::set_incident(int lid, int sid, bool blocked, double blocked_until)
{
    //cout << "incident start on link " << lid << endl;
    //Link* lptr=(*(find_if (links.begin(),links.end(), compare <Link> (lid) ))) ;
    Link* lptr = linkmap[lid];
    //Sdfunc* sdptr=(*(find_if (sdfuncs.begin(),sdfuncs.end(), compare <Sdfunc> (sid) ))) ;
    Sdfunc* sdptr = sdfuncmap[sid];
    lptr->set_incident(sdptr, blocked, blocked_until);
}

void Network::unset_incident(int lid)
{
    //cout << "end of incident on link  "<< lid << endl;
    //Link* lptr=(*(find_if (links.begin(),links.end(), compare <Link> (lid) ))) ;
    Link* lptr = linkmap[lid];
    lptr->unset_incident();
}

void Network::broadcast_incident_start(int lid)
{
    // for all links inform and if received, apply switch algorithm
    //cout << "BROADCAST incident on link  "<< lid << endl;
    for (auto& iter : linkmap)
    {
        iter.second->broadcast_incident_start(lid, incident_parameters);
    }
    // for all origins : start the automatic switiching algorithm
    for (auto& iter1 : originmap)
    {
        iter1.second->broadcast_incident_start(lid, incident_parameters);
    }

}


void Network::broadcast_incident_stop(int lid)
{	//cout << "BROADCAST END of incident on link  "<< lid << endl;
    // for all origins: stop the automatic switching stuff
    for (auto& iter : originmap)
    {
        iter.second->broadcast_incident_stop(lid);
    }
}

void Network::removeRoute(Route* theroute)
{
    odval val = theroute->get_oid_did();

    multimap<odval, Route*>::iterator it;
    multimap<odval, Route*>::iterator lower;
    multimap<odval, Route*>::iterator upper;
    lower = routemap.lower_bound(val);
    upper = routemap.upper_bound(val);
    for (it = lower; it != upper; it++)
    {
        if ((*it).second == theroute)
        {
            routemap.erase(it);
            return;
        }
    }
}

void Network::set_output_moe_thickness(unsigned int val) // sets the output moe for the links
{
    double maxval = 0.0;
    double minval = 999999.0;
    pair <double, double> minmax;
    auto iter = linkmap.begin();
    for (; iter != linkmap.end(); iter++)
    {
        minmax = (*iter).second->set_output_moe_thickness(val);
        minval = min(minval, minmax.first);
        maxval = max(maxval, minmax.second);

    }
    theParameters->min_thickness_value = minval;
    theParameters->max_thickness_value = maxval;
}

void Network::set_output_moe_colour(unsigned int val) // sets the output moe for the links
{
    double maxval = 0.0;
    double minval = 999999.0;
    pair <double, double> minmax;
    auto iter = linkmap.begin();
    for (; iter != linkmap.end(); iter++)
    {
        minmax = (*iter).second->set_output_moe_colour(val);
        minval = min(minval, minmax.first);
        maxval = max(maxval, minmax.second);

    }
    theParameters->min_colour_value = minval;
    theParameters->max_colour_value = maxval;
}



Incident::Incident(int lid_, int sid_, double start_, double stop_, double info_start_, double info_stop_, Eventlist* eventlist, Network* network_, bool blocked_) :start(start_), stop(stop_),
info_start(info_start_), info_stop(info_stop_), lid(lid_), sid(sid_), network(network_), blocked(blocked_)
{
    eventlist->add_event(start, this);
}

bool Incident::execute(Eventlist* eventlist, double time)
{
    //cout << "incident_execute time: " << time << endl;

    // In case no information is Broadcasted:
    if ((info_start < 0.0) || (info_stop < 0.0)) // there is no information broadcast
    {
        // #1: Start the incident on the link
        if (time < stop)
        {
            network->set_incident(lid, sid, blocked, stop); // replace sdfunc with incident sd function
#ifndef _NO_GUI
            icon->set_visible(true);
#endif
            eventlist->add_event(stop, this);
        }
        // #2: End the incident on the link
        else
        {
            network->unset_incident(lid);
#ifndef _NO_GUI
            icon->set_visible(false);
#endif
        }
    }

    else
    {
        // #1: Start the incident on the link
        if (time < info_start)
        {
            network->set_incident(lid, sid, blocked, stop); // replace sdfunc with incident sd function
#ifndef _NO_GUI
            icon->set_visible(true);
#endif
            eventlist->add_event(info_start, this);
        }
        // #2: Start the broadcast of Information
        else if (time < stop)
        {
            broadcast_incident_start(lid);
            // TO DO: Change the broadcast to only include only the affected links and origins
            eventlist->add_event(stop, this);
        }
        // #3: End the incident on the link
        else if (time < info_stop)
        {
            network->unset_incident(lid);
#ifndef _NO_GUI
            icon->set_visible(false);
#endif
            eventlist->add_event(info_stop, this);
        }
        // #4: End the broadcast of Information
        else
        {
            broadcast_incident_stop(lid);
        }
    }
    return true;
}

void Incident::broadcast_incident_start(int lid)
{
    // for all links
    auto linkiter = affected_links.begin();
    for (; linkiter != affected_links.end(); linkiter++)
    {
        (*linkiter).second->broadcast_incident_start(lid, incident_parameters);
    }

    // for all origins
    auto oriter = affected_origins.begin();
    for (; oriter != affected_origins.end(); oriter++)
    {
        (*oriter).second->broadcast_incident_start(lid, incident_parameters);
    }
}

void Incident::broadcast_incident_stop(int lid)
{

    auto oriter = affected_origins.begin();
    for (; oriter != affected_origins.end(); oriter++)
    {
        (*oriter).second->broadcast_incident_stop(lid);
    }
}


// ODMATRIX CLASSES

ODMatrix::ODMatrix() = default;

void ODMatrix::reset(Eventlist * eventlist, vector <ODpair*> *odpairs)
{
    auto s_iter = slices.begin();
    for (; s_iter != slices.end(); s_iter++)
    {
        //create and book the MatrixAction
        double loadtime = (*s_iter).first;
        ODSlice* odslice = (*s_iter).second;
        auto* mptr = new MatrixAction(eventlist, loadtime, odslice, odpairs);
        assert(mptr != nullptr);

    }
}


void ODMatrix::add_slice(double time, ODSlice * slice)
{
    slices.insert(slices.end(), (pair <double, ODSlice*>(time, slice)));
}

// MATRIXACTION CLASSES

MatrixAction::MatrixAction(Eventlist * eventlist, double time, ODSlice * slice_, vector<ODpair*> *ods_)
{
    slice = slice_;
    ods = ods_;
    eventlist->add_event(time, this);
}

bool MatrixAction::execute(Eventlist * eventlist, double   /*time*/)
{
    assert(eventlist != nullptr);
    //cout << time << " : MATRIXACTION:: set new rates "<< endl;
    // for all odpairs in slice

    for (auto iter = slice->rates.begin(); iter < slice->rates.end(); iter++)
    {
        // find odpair
        ODpair* odptr = (*(find_if(ods->begin(), ods->end(), compareod(iter->odid))));
        //init new rate
        odptr->set_rate(iter->rate);
    }
    return true;
}

/* Experienced passenger LoS based on output collectors of passengers */
void FWF_passdata::calc_pass_statistics(const vector<Passenger*>& passengers)
{
    vector<double> waiting_times;
    vector<double> waiting_denied_times;
    vector<double> walking_times;
    vector<double> inveh_times;
    vector<double> inveh_crowded_times;

    vector<double> gtcs;
    vector<double> transfers_per_pass;

    double wlkt = 0.0;
    double wt = 0.0;
    double ivt = 0.0;
    double d_wt = 0.0;
    double c_ivt = 0.0;
    double n_trans = 0.0;
    double gtc = 0.0;

    double vkt = 0.0;
    double drt_vkt = 0.0;
    double fix_vkt = 0.0;

    for (Passenger* pass : passengers)
    {
        if (pass->get_end_time() > 0) // will cause a crash otherwise when searching through incomplete output rows, so for now only passengers that completed their trip will count
        {
            npass++;
            wlkt = pass->calc_total_walking_time();

            wt = pass->calc_total_waiting_time();
            d_wt = pass->calc_total_waiting_time_due_to_denied_boarding();

            ivt = pass->calc_total_IVT();
            c_ivt = pass->calc_IVT_crowding();

            n_trans = static_cast<double>(pass->get_nr_transfers());

            gtc = theParameters->walking_time_coefficient * wlkt
            + theParameters->waiting_time_coefficient * wt
            + theParameters->waiting_time_coefficient * 3.5 * d_wt
            + theParameters->in_vehicle_time_coefficient * c_ivt
            + theParameters->transfer_coefficient * n_trans;

            vkt = pass->get_total_vkt();
            drt_vkt = pass->get_total_drt_vkt();
            fix_vkt = pass->get_total_fix_vkt();
        
            total_pass_vkt += vkt;
            total_pass_drt_vkt += drt_vkt;
            total_pass_fix_vkt += fix_vkt;

            total_drt_chosen += pass->get_num_drt_mode_choice();
            total_fix_chosen += pass->get_num_fix_mode_choice();

            total_wlkt += wlkt;
            total_wt += wt;
            total_denied_wt += d_wt;
            total_ivt += ivt;
            total_crowded_ivt += c_ivt;

            min_wt = Min(min_wt, wt);
            max_wt = Max(max_wt, wt);

            walking_times.push_back(wlkt);
            waiting_times.push_back(wt);
            waiting_denied_times.push_back(d_wt);
            inveh_times.push_back(ivt);
            inveh_crowded_times.push_back(c_ivt);

            gtcs.push_back(gtc);
            transfers_per_pass.push_back(n_trans);
        }
    }
    if (npass != 0)
    {
        median_wt = fwf_stats::findMedian(waiting_times);

        // calc mean and stdevs
        pair<double, double> wlkt_stats = fwf_stats::calcMeanAndStdev(walking_times);
        pair<double, double> wt_stats = fwf_stats::calcMeanAndStdev(waiting_times);
        pair<double, double> wtd_stats = fwf_stats::calcMeanAndStdev(waiting_denied_times);
        pair<double, double> ivt_stats = fwf_stats::calcMeanAndStdev(inveh_times);
        pair<double, double> ivtc_stats = fwf_stats::calcMeanAndStdev(inveh_crowded_times);
        pair<double, double> gtc_stats = fwf_stats::calcMeanAndStdev(gtcs);
        pair<double, double> ntrans_stats = fwf_stats::calcMeanAndStdev(transfers_per_pass);

        avg_total_wlkt = wlkt_stats.first;
        std_total_wlkt = wlkt_stats.second;

        avg_total_wt = wt_stats.first;
        std_total_wt = wt_stats.second;
        cv_wt = std_total_wt / avg_total_wt;

        avg_denied_wt = wtd_stats.first;
        std_denied_wt = wtd_stats.second;

        avg_total_ivt = ivt_stats.first;
        std_total_ivt = ivt_stats.second;

        avg_total_crowded_ivt = ivtc_stats.first;
        std_total_crowded_ivt = ivtc_stats.second;

        avg_ntrans = ntrans_stats.first;
        std_ntrans = ntrans_stats.second;

        avg_gtc = gtc_stats.first;
        std_gtc = gtc_stats.second;
    }
    pass_completed = npass; //!< @todo pass_completed and npass kindof redundant at the moment
}

void FWF_vehdata::calc_total_vehdata(const vector<Bus*>& vehicles)
{
    for(auto* veh : vehicles)
    {
        total_driving_time += veh->get_total_time_driving();
        total_idle_time += veh->get_total_time_idle();
        total_oncall_time += veh->get_total_time_oncall();
        total_empty_time += veh->get_total_time_empty();
        total_occupied_time += veh->get_total_time_occupied();

        total_vkt += veh->get_total_vkt();
        total_empty_vkt += veh->get_total_empty_vkt();
        total_occupied_vkt += veh->get_total_occupied_vkt();
    }
    num_vehdata = vehicles.size();
}

void FWF_tripdata::calc_trip_statistics(const vector<Bustrip*>& trips)
{
    vector<double> trip_boardings;
    double trip_total_boarding;
    for (const Bustrip* trip : trips)
    {
        if (trip->is_activated()) // trip actually started, i.e. a bus was assigned to it and started this particular trip
        {
            trip_total_boarding = trip->get_total_boarding();
            ++total_trips;
            total_pass_boarding += trip_total_boarding;
            if(trip->is_flex_trip())
                drt_total_pass_boarding += trip_total_boarding;
            else
                fix_total_pass_boarding += trip_total_boarding;

            total_pass_alighting += trip->get_total_alighting();

            if (trip_total_boarding == 0)
            {
                ++total_empty_trips;
            }
            else
            {
                trip_boardings.push_back(trip_total_boarding);
                min_boarding_per_trip = Min(min_boarding_per_trip, trip_total_boarding);
                max_boarding_per_trip = Max(max_boarding_per_trip, trip_total_boarding);
                ++total_pass_carrying_trips;
            }
        }
    }
    if (total_pass_carrying_trips != 0)
    {
        median_boarding_per_trip = fwf_stats::findMedian(trip_boardings);
        pair<double, double> boarding_stats = fwf_stats::calcMeanAndStdev(trip_boardings);
        avg_boarding_per_trip = boarding_stats.first;
        std_boarding_per_trip = boarding_stats.second;
    }
}
