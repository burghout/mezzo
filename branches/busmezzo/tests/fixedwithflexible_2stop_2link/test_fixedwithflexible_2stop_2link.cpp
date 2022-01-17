#include <QString>
#include <QtTest/QtTest>
#include "network.h"
#include "MMath.h"
#include "csvfile.h"
#include <algorithm>
#ifdef Q_OS_WIN
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif
//#include <unistd.h>
#include <QFileInfo>

//! DRT Tests BusMezzo
//! Contains tests for testing fixed with flexible choice model implementation with walking links
const std::string network_path = "../networks/FWF_2stops_d2d/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/FWF_2stops_d2d/ExpectedOutputs/";
const QString path_set_generation_filename = "o_path_set_generation.dat";

const vector<QString> d2d_output_filenames = 
{
    "o_convergence.dat",
    "o_fwf_ivt_alphas.dat",
    "o_fwf_wt_alphas.dat",
    "o_fwf_day2day_modesplit.dat",
    "o_fwf_day2day_boardings.dat",
    "o_fwf_day2day_passenger_waiting_experience.dat",
    "o_fwf_day2day_passenger_onboard_experience.dat",
    "o_fwf_day2day_passenger_transitmode.dat"
};
const vector<QString> output_filenames =
{
    "o_od_stop_summary_without_paths.dat",
    "o_od_stops_summary.dat",
    "o_passenger_trajectory.dat",
    "o_passenger_welfare_summary.dat",
    "o_segments_line_loads.dat",
    "o_segments_trip_loads.dat",
    "o_selected_paths.dat",
    "o_transit_routes.dat",
    "o_transit_trajectory.dat",
    "o_transitline_sum.dat",
    "o_transitlog_out.dat",
    "o_transitstop_sum.dat",
    "o_trip_total_travel_time.dat",
    "o_fwf_summary.dat",
    "o_vkt.dat",
    "o_fwf_drtvehicle_states.dat",
    "o_time_spent_in_state_at_stop.dat"
};

const vector<QString> skip_output_filenames =
{
//    "o_od_stop_summary_without_paths.dat",
//    "o_od_stops_summary.dat"
}; //!< Files omitted in testSaveResults.

const long int seed = 42;

class TestFixedWithFlexible_2stop_2link : public QObject
{
    Q_OBJECT

public:
    TestFixedWithFlexible_2stop_2link() = default;
    ~TestFixedWithFlexible_2stop_2link() = default;

private slots:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testInitParameters(); //!< tests that the parameters for the loaded network are as expected
    void testValidRouteInput(); //!< tests if the routes that are read in are actually possible given the configuration of the network...
    void testAutoGenBuslines(); //!< tests with methods for autogenerate buslines between pairs of stops and including intermediate stops
    void testCasePathSet(); //!< tests if setting choice set generation parameters yield expected paths
    void testRunNetwork();
    void testPassAssignment_day2day(); //!< tests of resulting pass assignment between days
    void testPassAssignment_final(); //!< tests of resulting pass assignment on final day
    void testSaveResults();
    void testFinalDay2dayOutput(); //!< tests of final day2day convergence files
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;

    map< Busline*,map<int,int>, ptr_less<Busline*> > total_pass_boarded_per_line_d2d; //!< contains the resulting pass flows without day2day activated. Key1 = busline, Key2 = day, value = passenger load
    vector< Passenger* > all_pass;
};


void TestFixedWithFlexible_2stop_2link::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestFixedWithFlexible_2stop_2link::testInitNetwork()
{
    // remove old output files:
    for (const QString& filename : d2d_output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename); //remove old day2day convergence results
    }
    
    qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    
    ::fwf_wip::autogen_drt_lines_with_intermediate_stops = true; // set manually
    ::fwf_wip::csgm_no_merging_or_filtering_paths = true; //set manually (default false)
    
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);
    
    nt->init();

    // Test if the network is properly read and initialized
    QVERIFY(net->get_links().size() == 14);
    QVERIFY(net->get_nodes().size() == 8);
    QVERIFY(net->get_odpairs().size() == 4);
    QVERIFY(net->get_stopsmap().size() == 4);
    QVERIFY(net->get_busstop_from_name("Stop_1")->get_id() == 1);
    QVERIFY(net->get_busstop_from_name("Stop_21")->get_id() == 21);
    QVERIFY(net->get_busstop_from_name("Stop_2")->get_id() == 2);
    QVERIFY(net->get_busstop_from_name("Stop_22")->get_id() == 22);
    
    // Mezzo parameters
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    // Demand parameters
    QVERIFY2(theParameters->empirical_demand == 1, "Failure, empirical demand not set to 1 in parameters");  
    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    double total_demand = 0.0;
    for(const auto& od : odstops_demand)
    {
        total_demand += od->get_arrivalrate();
    }
    QVERIFY2(AproxEqual(total_demand,0.0), "Failure, total stochastic demand should be 0 pass/h");

    //Static fixed service design
    //QVERIFY2(AproxEqual(net->get_buslines()[0]->get_planned_headway(),360.0), "Failure, buslines should have a 360 second (6 min) planned headway");

    //Control center
    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == true, "Failure, generate direct routes of controlcenter is not set to true");

    //! @todo Fleet composition.... 
    //QVERIFY(ccmap.begin()->second->getConnectedVehicles().size() == 20);

    
    //!< Test if newly generated passenger path sets match expected output
    //! //QVERIFY(theParameters->choice_set_indicator == 1); // Traveler choice set for this network is manually defined
    QVERIFY(theParameters->choice_set_indicator == 0); // Traveler choice set for this network is autogenerated
    qDebug() << "Comparing " + path_set_generation_filename + " with ExpectedOutputs/" + path_set_generation_filename;
    QString ex_path_set_fullpath = expected_outputs_path + path_set_generation_filename;
    QFile ex_path_set_file(ex_path_set_fullpath); //expected o_path_set_generation.dat
    QVERIFY2(ex_path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open ExpectedOutputs/o_path_set_generation.dat");

    QFile path_set_file(path_set_generation_filename); //generated o_path_set_generation.dat
    QVERIFY2(path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open o_path_set_generation.dat");

    QVERIFY2(path_set_file.readAll() == ex_path_set_file.readAll(), "Failure, o_path_set_generation.dat differs from ExpectedOutputs/o_path_set_generation.dat");

    ex_path_set_file.close();
    path_set_file.close();
    
}

void TestFixedWithFlexible_2stop_2link::testInitParameters()
{
    // Manually set global test flags
    ::fwf_wip::write_all_pass_experiences = true; //set manually
    ::fwf_wip::randomize_pass_arrivals = false; //set manually
    ::fwf_wip::day2day_no_convergence_criterium = true; //set manually
    ::fwf_wip::drt_enforce_strict_boarding = true; //set manually
    ::fwf_wip::zero_pk_fixed = true; // set manually
    
    //BusMezzo parameters, drt without RTI
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 0, "Failure, real time info is not set to 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 0.0), "Failure, share RTI network is not 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI, 0.0), "Failure, default alpha RTI is not 0 in parameters");
    
    //! CSGM @todo
    QVERIFY2(theParameters->choice_set_indicator == 0, "Failure, choice set indicator is not set to 0 in parameters");
    //QVERIFY2(net->count_transit_paths() == 27, "Failure, network should have 14 transit paths defined");

    //day2day params
    QVERIFY2(theParameters->pass_day_to_day_indicator == 1, "Failure, waiting time day2day indicator is not activated");
    QVERIFY2(theParameters->in_vehicle_d2d_indicator == 1, "Failure, IVT day2day indicator is not activade");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI,0.0), "Faliure, default credibility coefficient for day2day RTI is not set to 0.0");
    QVERIFY2(AproxEqual(theParameters->break_criterium,0.01),"Failure, break criterium for day2day is not set to 0.01");
    QVERIFY2(theParameters->max_days == 100, "Failure, max days for day2day is not set to 100.");
}

void TestFixedWithFlexible_2stop_2link::testValidRouteInput()
{
    /**
      @todo
        1. Loop through all transit_routes (or routes), every transit route is a route (and is included as such) but not every route is a transit route
        2. Check that all links are connected in the right sequence (innode should match outnode between links)
        3. Check that each node has a turning movement defined for it that matches the sequence of the links and is active
    */
    
    vector<Busroute*> routes = net->get_busroutes();
    map<int,Turning*> turningmap = net->get_turningmap();
    for(Busroute* route : routes)
    {
        //test if route found is a connected sequence of links
        vector<Link*> rlinks = route->get_links();
        for (Link* rlink : rlinks)
        {
            Link* nextlink = route->nextlink(rlink);
            if (nextlink)
            {
                QString msg = QString("Failure, link %1 is not an upstream link to link %2").arg(rlink->get_id()).arg(nextlink->get_id());
                QVERIFY2(rlink->get_out_node_id() == nextlink->get_in_node_id(), qPrintable(msg)); //outnode of preceding link should be innode of succeeding link
                
                //Check that a turning movement is defined for the connected links and is active
                bool turningfound = false;
                for(auto val : turningmap)
                {
                    Turning* turning = val.second;
                    if(turning->is_active())
                    {
                        Link* inlink = turning->get_inlink();
                        Link* outlink = turning->get_outlink();
                        if( (inlink->get_id() == rlink->get_id()) && (outlink->get_id() == nextlink->get_id()) )
                        {
                            //check that the turning node matches the links as well
                            Node* node = turning->get_node();
                            QVERIFY(rlink->get_out_node_id() == node->get_id());
                            QVERIFY(nextlink->get_in_node_id() == node->get_id());
                            turningfound = true;
                        }
                    }
                    
                }
                msg = QString("Failure, no active turning exists between link %1 and link %2").arg(rlink->get_id()).arg(nextlink->get_id());
                QVERIFY2(turningfound, qPrintable(msg));
            }
        }
    }
}

void TestFixedWithFlexible_2stop_2link::testAutoGenBuslines()
{
    /**
      All generated buslines should only include start and end stops even when autogenerate with intermediate stops
      All predefined (i.e. fixed) lines should also only have a start and and end stop
    */
    QVERIFY(::fwf_wip::autogen_drt_lines_with_intermediate_stops);
//    vector<Busline*> lines = net->get_buslines();
//    for(auto line : lines)
//    {
//        QString msg = QString("Failure, line %1 does not visit only 2 stops").arg(line->get_id());
//        QVERIFY2(line->stops.size() == 2,qPrintable(msg));
//    }
}

void TestFixedWithFlexible_2stop_2link::testCasePathSet()
{
    /**
        Checks for the combination of autogenerated direct DRT lines and passenger choice sets match the desired case study
        
        - Currently checks all ODs with stop 1 as an origin (i.e. 1->2, 1->22, 1->21) and stop 1 as a destination (2->1, 22->1, 21->1)
        - No transfers between DRT lines are ever allowed for this case only DRT->FIX
    */
    
    vector<ODstops*> ods = net->get_odstops_demand();
    for(auto od : ods)
    {
        int o = od->get_origin()->get_id();
        int d = od->get_destination()->get_id();
        qDebug() << "Checking od: " << o << d;
        
        vector<Pass_path*> pathset = od->get_path_set();
        QVERIFY2(!pathset.empty(), "Failure, there should be at least one path per OD"); // there should always be a path for each OD with non-zero demand rate for this network

        for(auto path : pathset)
        {
            // general conditions for paths that should always hold true
            vector<vector<Busstop*> > alt_stops = path->get_alt_transfer_stops();
            QVERIFY(!alt_stops.empty()); // no empty paths
            
            Busstop* path_o = path->get_origin();
            Busstop* path_d = path->get_destination();
            
            QVERIFY(path_o->get_id() == o);
            QVERIFY(path_d->get_id() == d);
            
            vector<vector<Busline*> > alt_lines = path->get_alt_lines();
            QVERIFY(!alt_lines.empty()); // no empty paths
            
            // Fixed and Flexible lines are always considered distinct from one another
            QVERIFY(path->has_no_mixed_mode_legs(alt_lines));
            
            // max 2 legs should be available
            size_t n_legs = alt_lines.size();
            QVERIFY(n_legs < 3);
            
            int n_trans = path->get_number_of_transfers();
            QVERIFY(n_trans < 2);
            
            vector<Busline*> leg_1 = alt_lines[0];
            vector<Busline*> leg_2;
            if(n_legs > 1)
                leg_2 = alt_lines[1];
                
            // 1 -> 2
            // DRT->     y
            // FIX->     y
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  x
            // FIX->DRT  x
            if(o == 1 && d == 2)
            {
                QVERIFY(n_legs == 1);
                QVERIFY(n_trans == 0);
            }
            
            // 1 -> 21
            // DRT->     y
            // FIX->     x
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  y
            // FIX->DRT  y
            else if(o == 1 && d == 21)
            {
                QVERIFY(n_legs == 1 || n_legs == 2);
                if(n_trans == 0)
                    QVERIFY(Pass_path::check_all_flexible_lines(leg_1));
                else if (n_trans == 1)
                {
                    QVERIFY(!leg_2.empty());
                    if(Pass_path::check_all_fixed_lines(leg_1)) // if first leg fixed should be followed by flex
                    {
                        QVERIFY(Pass_path::check_all_flexible_lines(leg_2));
                        
                    }
                    else
                    {
                        QVERIFY(Pass_path::check_all_flexible_lines(leg_1)); // if first leg flex should be followed by fix
                        QVERIFY(Pass_path::check_all_fixed_lines(leg_2));
                    }
                }
            }
            else if(o == 1 && d == 22)
            // 1 -> 22
            // DRT->     y
            // FIX->     x
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  y
            // FIX->DRT  y
            {
                QVERIFY(n_legs == 1 || n_legs == 2);
                if(n_trans == 0)
                    QVERIFY(Pass_path::check_all_flexible_lines(leg_1));
                else if (n_trans == 1)
                {
                    QVERIFY(!leg_2.empty());
                    if(Pass_path::check_all_fixed_lines(leg_1)) // if first leg fixed should be followed by flex
                    {
                        QVERIFY(Pass_path::check_all_flexible_lines(leg_2));
                        
                    }
                    else
                    {
                        QVERIFY(Pass_path::check_all_flexible_lines(leg_1)); // if first leg flex should be followed by fix
                        QVERIFY(Pass_path::check_all_fixed_lines(leg_2));
                    }
                }
            }
            else if (o == 2 && d == 1)
            // 2 -> 1
            // DRT->     y
            // FIX->     x
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  x
            // FIX->DRT  x
            {
                QVERIFY(n_legs == 1);
                QVERIFY(n_trans == 0);
                QVERIFY(Pass_path::check_all_flexible_lines(leg_1));
            }
            else if (o == 21 && d == 1)
            // 21 -> 1
            // DRT->     y
            // FIX->     x
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  x
            // FIX->DRT  x
            {
                QVERIFY(n_legs == 1);
                QVERIFY(n_trans == 0);
                QVERIFY(Pass_path::check_all_flexible_lines(leg_1));
            }
            else if (o == 22 && d == 1)
            // 22 -> 1
            // DRT->     y
            // FIX->     x
            // DRT->DRT  x
            // FIX->FIX  x
            // DRT->FIX  x
            // FIX->DRT  y
            {
                QVERIFY(n_legs == 1 || n_legs == 2);
                if(n_trans == 0)
                    QVERIFY(Pass_path::check_all_flexible_lines(leg_1));
                else if (n_trans == 1)
                {
                    QVERIFY(!leg_2.empty());
                    QVERIFY(Pass_path::check_all_fixed_lines(leg_1));
                    QVERIFY(Pass_path::check_all_flexible_lines(leg_2));
                }
            }
        } //for paths in od

    } // for od in ods
}


void TestFixedWithFlexible_2stop_2link::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    //QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    //QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));
    qDebug() << "Final day: " << net->day;
    //QVERIFY(net->day == theParameters->max_days); // current setup does not converge within 20 days
}

void TestFixedWithFlexible_2stop_2link::testPassAssignment_day2day()
{
/*
 * @todo 
*/
}

void TestFixedWithFlexible_2stop_2link::testPassAssignment_final()
{
    /**
      Check that passengers can reach their destinations for all OD stop pairs with demand associated with them (in this unidirectional demand network)

      @todo
        Print out first passenger that arrived for each OD, what their request ended up being and how far they managed to get, use the test attribute of ODstops 'first_passenger_start'
            - Time the passenger arrived
            - Connection, transitmode and dropoff stop decision made after Passenger::start
            - State of the request that they generated if transitmode is equal to flexible (Null, Unmatched, Assigned, Matched)
                - assert that if transitmode equal to fixed, no request was generated
            - If the passenger reached their final destination or not
            - If not, where did they end up, what is their current location
            
      @todo repeatability
        - print out all decisions for first pass
        - trace sequence of each vehicle, lifetime of each vehicle from e.g. transit_trajectory.dat
            - print state updates...
        - 
    */
    
    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    //QVERIFY2(odstops_demand.size() == 1, "Failure, network should have 1 od stop pairs (with non-zero demand defined in transit_demand.dat) ");

    for(auto od : odstops_demand)
    {
        // verify non-zero demand for this OD
        QVERIFY2((od->get_arrivalrate() > 0 || od->has_empirical_arrivals()),"Failure, all ODstops in Network::odstops_demand should have positive arrival rate.");

        QVERIFY(!od->get_passengers_during_simulation().empty()); // at least one passenger was generated
        QVERIFY(od->first_passenger_start != nullptr); // at least one passenger was added to the Eventlist and its Passenger::start action called
        Passenger* first_pass = od->first_passenger_start;
        
        QString orig_s = QString::number(od->get_origin()->get_id());
        QString dest_s = QString::number(od->get_destination()->get_id());

        // check expected passenger behavior for this network
        if(first_pass != nullptr)
        {
            bool finished_trip = first_pass->get_end_time() > 0;
            
            qDebug() << "First passenger for OD " << "(" << orig_s << "," << dest_s << "):";
            qDebug() << "\t" << "passenger id        : " << (first_pass->get_id());
            qDebug() << "\t" << "finished trip       : " << finished_trip;
            qDebug() << "\t" << "start time          : " << first_pass->get_start_time();
            qDebug() << "\t" << "last stop visited   : " << first_pass->get_chosen_path_stops().back().first->get_id();
            qDebug() << "\t" << "num denied boardings: " << first_pass->get_nr_denied_boardings();
            
            
            size_t n_transfers = (first_pass->get_selected_path_stops().size() - 4) / 2; // given path definition (direct connection - 4 elements, 1 transfers - 6 elements, 2 transfers - 8 elements, etc.
            if(finished_trip)
            {
                qDebug() << "\t" << "num transfers       : " << n_transfers;
            }
            
            // collect the first set of decisions for the first passenger for each ODstop with demand
            list<Pass_connection_decision> connection_decisions = od->get_pass_connection_decisions(first_pass);
            list<Pass_transitmode_decision> mode_decisions = od->get_pass_transitmode_decisions(first_pass);
            
            if(connection_decisions.front().chosen_connection_stop == od->get_origin()->get_id()) //if chosen connection stop is the same as the original origin the first connection decision was to stay, otherwise walk
            {
                qDebug() << "\t" << "first connection decision : stay at stop " << od->get_origin()->get_id();
            }
            else
            {
                qDebug() << "\t" << "first connection decision : walk from stop " << od->get_origin()->get_id() << " to " << connection_decisions.front().chosen_connection_stop;
            }
            
            if(!mode_decisions.empty())
            {
                if(mode_decisions.front().chosen_transitmode == TransitModeType::Flexible)
                    list<Pass_dropoff_decision> dropoff_decisions = od->get_pass_dropoff_decisions(first_pass);          
                QVERIFY(mode_decisions.front().chosen_transitmode != TransitModeType::Null); // A choice of either fixed or flexible should have always been made
                
                if(mode_decisions.back().chosen_transitmode == TransitModeType::Fixed) //last chosen mode
                {
                    QVERIFY(first_pass->get_curr_request() == nullptr); // a request should never have been generated if transit mode choice is fixed
                    
    //                if(finished_trip)
    //                    QVERIFY(n_transfers == 0); // a total of 1 vehicle should have been used to reach final dest. 
                }
                else if(mode_decisions.back().chosen_transitmode == TransitModeType::Flexible)
                {
                    if(finished_trip) // passenger completed their trip
                    {
                        QVERIFY(first_pass->get_curr_request() == nullptr); // curr request reset to null after a trip is completed
                    }
                    else // the passengers first decision was flexible but they never reached their destination
                    {
                        Request* request = first_pass->get_curr_request();
                        QVERIFY(request != nullptr); // not requests should have been rejected or canceled
                        // Print out the request attributes (maybe create a print member method of Request for this):
    //                    int id;
    //                    int pass_id = -1;    //!< id of passenger that created request
    //                    int ostop_id = -1;   //!< id of origin stop
    //                    int dstop_id = -1;   //!< id of destination stop
    //                    int load = -1;       //!< number of passengers in request
    //                    double desired_departure_time = -1.0;  //!< desired/earliest departure time for passenger
    //                    double time = -1.0;                    //!< time request was generated
    //                    RequestState state = RequestState::Null; //!< current state of the request
    //                    Passenger* pass_owner = nullptr; //!< passenger who sent this request
    //                    Bus* assigned_veh = nullptr; //!< vehicle that has been assigned to this request, nullptr if none has been assigned
    //                    Bustrip* assigned_trip = nullptr; //!< bustrip that has been assigned to this request, nullptr by default, updated when assigned
                        if(request != nullptr)
                        {
                            QVERIFY(request->pass_id == request->pass_owner->get_id());
                            
                            qDebug() << "\t\t chose FLEX, did not reach final dest, curr_request: ";
                            qDebug() << "\t\t pass_id      : " << request->pass_id;
                            qDebug() << "\t\t ostop_id     : " << request->ostop_id;
                            qDebug() << "\t\t dstop_id     : " << request->dstop_id;
                            qDebug() << "\t\t load         : " << request->load;
                            qDebug() << "\t\t t_desired_dep: " << request->time_desired_departure;
                            qDebug() << "\t\t t_request_gen: " << request->time_request_generated;
                            qDebug() << "\t\t request_state: " << Request::state_to_QString(request->state);
                            //qDebug() << request->assigned_trip-> //empty trip is on its way
                        }
                    }
                }
            }
            // verify that at least one passenger per OD made it to their destination
            QString failmsg = "Failure, at least one passenger for ODstop (" + orig_s + "," + dest_s + ") with non-zero demand should have reached final destination.";
            QVERIFY2(first_pass->get_end_time() > 0, qPrintable(failmsg)); // replaced the od->get_nr_pass_completed() call with this since there is some less intuitive dependency between this and calc_pass_measures() after saving results
        }
    }
}

void TestFixedWithFlexible_2stop_2link::testSaveResults()
{
    // remove old output files:
    for (const QString& filename : output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename);
    }

    nt->saveresults();

    //test if output files match the expected output files
    for (const QString& o_filename : output_filenames)
    {
        if (find(skip_output_filenames.begin(), skip_output_filenames.end(), o_filename) != skip_output_filenames.end())
            continue;

        qDebug() << "Comparing " + o_filename + " with ExpectedOutputs/" + o_filename;

        QString ex_o_fullpath = expected_outputs_path + o_filename;
        QFile ex_outputfile(ex_o_fullpath);

        QString msg = "Failure, cannot open ExpectedOutputs/" + o_filename;
        QVERIFY2(ex_outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

        QFile outputfile(o_filename);
        msg = "Failure, cannot open " + o_filename;
        QVERIFY2(outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

        msg = "Failure, " + o_filename + " differs from ExpectedOutputs/" + o_filename;
        QVERIFY2(outputfile.readAll() == ex_outputfile.readAll(), qPrintable(msg));

        ex_outputfile.close();
        outputfile.close();
    }
}

void TestFixedWithFlexible_2stop_2link::testFinalDay2dayOutput()
{
    //test if day2day files match the expected output files
    for (const QString& o_filename : d2d_output_filenames)
    {
        qDebug() << "Comparing " + o_filename + " with ExpectedOutputs/" + o_filename;

        QString ex_o_fullpath = expected_outputs_path + o_filename;
        QFile ex_outputfile(ex_o_fullpath);

        QString msg = "Failure, cannot open ExpectedOutputs/" + o_filename;
        QVERIFY2(ex_outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

        QFile outputfile(o_filename);
        msg = "Failure, cannot open " + o_filename;
        QVERIFY2(outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

        msg = "Failure, " + o_filename + " differs from ExpectedOutputs/" + o_filename;
        QVERIFY2(outputfile.readAll() == ex_outputfile.readAll(), qPrintable(msg));

        ex_outputfile.close();
        outputfile.close();
    }
}


void TestFixedWithFlexible_2stop_2link::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestFixedWithFlexible_2stop_2link)

#include "test_fixedwithflexible_2stop_2link.moc"
