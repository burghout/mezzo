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
const std::string network_path = "../networks/FWF_testnetwork1_d2d/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/FWF_testnetwork1_d2d/ExpectedOutputs/";
const QString path_set_generation_filename = "o_path_set_generation.dat";

const vector<QString> d2d_output_filenames = 
{
    "o_convergence.dat",
    "o_fwf_ivt_alphas.dat",
    "o_fwf_wt_alphas.dat",
    "o_fwf_day2day_modesplit.dat",
    "o_fwf_day2day_boardings.dat"
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

class TestFixedWithFlexible_day2day : public QObject
{
    Q_OBJECT

public:
    TestFixedWithFlexible_day2day() = default;
    ~TestFixedWithFlexible_day2day() = default;

private slots:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testInitParameters(); //!< tests that the parameters for the loaded network are as expected
    void testValidRouteInput(); //!< tests if the routes that are read in are actually possible given the configuration of the network...
    void testPassengerStart(); //!< tests for when a traveler first enters the simulation and makes a connection, transitmode and dropoff decision, sends a request if mode is flexible, changes 'chosen mode' state of traveler...etc
    void testRunNetwork();
    void testPassAssignment_day2day(); //!< tests of resulting pass assignment between days
    void testPassAssignment_final(); //!< tests of resulting pass assignment on final day
    void testSaveResults();
    void testFinalDay2dayOutput(); //!< tests of day2day convergence
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;

    map< Busline*,map<int,int>, ptr_less<Busline*> > total_pass_boarded_per_line_d2d; //!< contains the resulting pass flows without day2day activated. Key1 = busline, Key2 = day, value = passenger load
    vector< Passenger* > all_pass;
};


void TestFixedWithFlexible_day2day::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestFixedWithFlexible_day2day::testInitNetwork()
{
    // remove old output files:
    for (const QString& filename : d2d_output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename); //remove old day2day convergence results
    }

    qDebug() << "Initializing network in " + QString::fromStdString(network_path);
    nt->init();

    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 34, "Failure, network should have 34 links ");

    //count number of 'dummy links' to see that these are not being mislabeled
    int count = 0;
    auto links = net->get_links();
    for(const auto& link : links)
    {
        if(link.second->is_dummylink())
            count++;
    }
    QVERIFY2(count == 24, "Failure network should have 24 dummy links ");

    QVERIFY2(net->get_nodes().size() == 20, "Failure, network should have 20 nodes ");
    QVERIFY2(net->get_destinations().size() == 5, "Failure, network should have 5 destination nodes ");
    QVERIFY2(net->get_origins().size() == 5, "Failure, network should have 5 origin nodes ");
    QVERIFY2(net->get_odpairs().size() == 18, "Failure, network should have 18 od pairs ");
    QVERIFY2(net->get_stopsmap().size() == 10, "Failure, network should have 10 stops defined ");

    //check that there are walking links between stops 1 and 5 and no other stops
    for(auto stop1 : net->get_stopsmap())
    {
        for(auto stop2 : net->get_stopsmap())
        {
            QString stops_str = QString("stops %1 -> %2").arg(stop1.first).arg(stop2.first);
            //qDebug() << "Checking walking links between " + stops_str;
            QString nolink_failmsg = "Failure, no walking link defined for " + stops_str;
            if(stop1.first == stop2.first) // recall, key in stopsmap is id of stop
            {
                QVERIFY2(stop1.second->is_within_walking_distance_of(stop2.second),qPrintable(nolink_failmsg));// all stops should have a walking distance of 0 with themselves I think
                QVERIFY2(AproxEqual(stop1.second->get_walking_time(stop2.second,0.0),0.0), qPrintable("Failure, non-zero walking time between " + stops_str));
            }
            else if(stop1.first == 1 && stop2.first == 5) // walking distances are always symmetric, so if 1->5 is defined so is 5->1
            {
                QVERIFY2(stop1.second->is_within_walking_distance_of(stop2.second),qPrintable(nolink_failmsg));
                QVERIFY2(AproxEqual(stop1.second->get_walking_distance_stop(stop2.second),330.0), qPrintable("Failure, walking distance between " + stops_str + " should be 330 meters"));
            }
            else if(stop1.first == 5 && stop2.first == 1)
            {
                QVERIFY2(stop1.second->is_within_walking_distance_of(stop2.second),qPrintable(nolink_failmsg));
                QVERIFY2(AproxEqual(stop1.second->get_walking_distance_stop(stop2.second),330.0), qPrintable("Failure, walking distance between " + stops_str + " should be 330 meters"));
            }
            else  // all other stops should not be connected via walking links
            {
                QVERIFY2(!stop1.second->is_within_walking_distance_of(stop2.second),qPrintable("Failure, there should be no walking link for " + stops_str));
            }
        }
    }


    // Mezzo parameters
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    QVERIFY2(odstops_demand.size() == 1, "Failure, network should have 2 od stop pairs (non-zero or defined in transit_demand) ");

    //Check OD stop demand rate between stop 1 and 4
//    ODstops* stop_1to4 = net->get_ODstop_from_odstops_demand(1,4);
//    QVERIFY2(AproxEqual(stop_1to4->get_arrivalrate(),500.0),"Failure, ODstops stop 1 to stop 4 should have 500 arrival rate");
//    QVERIFY2(stop_1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined ");

//    ODstops* stop_5to4 = net->get_ODstop_from_odstops_demand(5,4);
//    QVERIFY2(AproxEqual(stop_5to4->get_arrivalrate(),500.0),"Failure, ODstops stop 5 to stop 4 should have 500 arrival rate");
//    QVERIFY2(stop_5to4 != nullptr,"Failure, OD stop 5 to 4 is undefined ");


    //Static fixed service design
    QVERIFY2(AproxEqual(net->get_buslines()[0]->get_planned_headway(),360.0), "Failure, buslines should have a 360 second (6 min) planned headway");

    //Control center
    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == false, "Failure, generate direct routes of controlcenter is not set to false");

    QVERIFY(ccmap.begin()->second->getConnectedVehicles().size() == 20);

    //Check number of pass that have been generated:
    //Test reading of empirical passenger arrivals
    QVERIFY2(theParameters->empirical_demand == 1, "Failure, empirical demand not set to 1 in parameters");
    vector<pair<ODstops*, double> > empirical_passenger_arrivals = net->get_empirical_passenger_arrivals();
    QVERIFY2(empirical_passenger_arrivals.size() == 1, "Failure, there should be 2 empirical passenger arrivals");
    
    all_pass = net->get_all_generated_passengers();
    qDebug() << all_pass.size() << " passengers generated.";
    QVERIFY(all_pass.size() == 1); //should always get 2 generated passengers

}

void TestFixedWithFlexible_day2day::testInitParameters()
{
    //BusMezzo parameters
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 0, "Failure, real time info is not set to 3 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 0.0), "Failure, share RTI network is not 1 in parameters");
    QVERIFY2(theParameters->choice_set_indicator == 1, "Failure, choice set indicator is not set to 1 in parameters");
    QVERIFY2(net->count_transit_paths() == 27, "Failure, network should have 14 transit paths defined");

    //day2day params
    QVERIFY2(theParameters->pass_day_to_day_indicator == 1, "Failure, waiting time day2day indicator is not activated");
    QVERIFY2(theParameters->in_vehicle_d2d_indicator == 1, "Failure, IVT day2day indicator is not activade");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI,0.0), "Faliure, initial credibility coefficient for day2day RTI is not set to 0.5");
    QVERIFY2(AproxEqual(theParameters->break_criterium,0.1),"Failure, break criterium for day2day is not set to 0.1");
    QVERIFY2(theParameters->max_days == 20, "Failure, max days for day2day is not set to 20.");
}

void TestFixedWithFlexible_day2day::testValidRouteInput()
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


void TestFixedWithFlexible_day2day::testPassengerStart()
{
    //!< Network state:
    //!     - simulation time is 0.0, no fixed trips have started yet,
    //!     - one DRT vehicle is oncall at stop 4
    //!     - one Passenger at stop 5 with destination stop 4
    //!         - needs to decide to stay (and use fixed) or walk to stop 1 and use fixed or flexible
    //!
    //! @todo
    //!     - Make several sequences of decisions for the same traveler
    //!     - See that communication with Controlcenter, as well as all the state updates are working as expected
    //!     - Perhaps even emulate the walking/boarding process....via Passenger::execute...
    //!     - Make sure that travelers are not connected to a controlcenter unless they are using flexible
    //!

    //create a DRT vehicle at stop 4
//    Busstop* stop4 = net->get_stopsmap()[4];
//    Controlcenter* CC = stop4->get_CC();

//    // Creating DRT vehicle 1 is on-call at stop 4
//    Bus* bus1 = new Bus(1,4,4,nullptr,nullptr,0.0,true,nullptr);
//    Bustype* bustype = CC->getConnectedVehicles().begin()->second->get_bus_type();
//    bus1->set_bustype_attributes(bustype);
//    bus1->set_bus_id(1); //vehicle id and bus id mumbojumbo
//    CC->connectVehicle(bus1); //connect vehicle to a control center
//    CC->addVehicleToAllServiceRoutes(bus1);
//    bus1->set_last_stop_visited(stop4); // need to do this before setting state always
//    stop4->add_unassigned_bus(bus1,0.0); //should change the buses stat to OnCall and update connected Controlcenter
//    QVERIFY(CC->getOnCallVehiclesAtStop(stop4).size() == 1);

//    //Basically mimic what is supposed to happen in Passenger::start
//    ODstops* stop5to4 = net->get_ODstop_from_odstops_demand(5,4);
//    Passenger* pass1 = new Passenger(1,0,stop5to4,nullptr); //create a passenger at stop 1 going to stop 4
//    pass1->init();

//    //!< Make a sequence of connection->transitmode->dropoff decisions and reset
//    double t_now = 0.0; //fake current simulation clocktime
//    map<Busstop*,int> connection_counts;
//    map<TransitModeType,int> mode_counts;
//    map<Busstop*,int> dropoff_counts;
//    const int draws = 10;
//    for(int i = 0; i < draws; i++)
//    {
//        cout << endl;
//        Busstop* connection_stop = pass1->make_connection_decision(t_now);
//        cout << "\tChosen pickup: " << connection_stop->get_id() << endl;
//        ++connection_counts[connection_stop];

//        TransitModeType chosen_mode = pass1->make_transitmode_decision(connection_stop,t_now);
//        cout << "\tChosen mode: " << chosen_mode << endl;
//        ++mode_counts[chosen_mode];
//        cout << endl;

//        if(connection_stop->get_id() == 5)
//            QVERIFY(chosen_mode==TransitModeType::Fixed); // only fixed first legs available from stop 5->4 without walking

//        pass1->set_chosen_mode(chosen_mode);
//        Busstop* dropoff_stop = nullptr;
//        if(chosen_mode==TransitModeType::Flexible)
//        {
//            dropoff_stop = pass1->make_dropoff_decision(connection_stop,t_now);
//            cout << "\tChosen dropoff: " << dropoff_stop->get_id() << endl;
//            ++dropoff_counts[dropoff_stop];
//            cout << endl;
//        }

//        //Passenger creates a request based on choices

//        pass1->set_chosen_mode(TransitModeType::Null); //reset chosen mode
//        pass1->temp_connection_path_utilities.clear();
//    }
//    Busstop* stop5 = net->get_stopsmap()[5];
//    Busstop* stop1 = net->get_stopsmap()[1];
//    Busstop* stop2 = net->get_stopsmap()[2];

//    if(connection_counts.count(stop5)!=0)
//        cout << "Pickup stop 5 (%): " << 100 * connection_counts[stop5] / static_cast<double>(draws) << endl;
//    if(connection_counts.count(stop1)!=0)
//        cout << "Pickup stop 1 (%): " << 100 * connection_counts[stop1] / static_cast<double>(draws) << endl;

//    cout << endl;
//    if(mode_counts.count(TransitModeType::Null)!=0)
//        cout << "Null  (%): " << 100 * mode_counts[TransitModeType::Null] / static_cast<double>(draws) << endl;
//    if(mode_counts.count(TransitModeType::Fixed)!=0)
//        cout << "Fixed (%): " << 100 * mode_counts[TransitModeType::Fixed] / static_cast<double>(draws) << endl;
//    if(mode_counts.count(TransitModeType::Flexible)!=0)
//        cout << "Flex  (%): " << 100 * mode_counts[TransitModeType::Flexible] / static_cast<double>(draws) << endl;

//    cout << endl;
//    if(dropoff_counts.count(stop2)!=0)
//        cout << "Dropoff stop 2 (%): " << 100 * dropoff_counts[stop2] / static_cast<double>(draws) << endl;
//    if(dropoff_counts.count(stop4)!=0)
//        cout << "Dropoff stop 4 (%): " << 100 * dropoff_counts[stop4] / static_cast<double>(draws) << endl;

//    //cleanup
//    stop4->remove_unassigned_bus(bus1,0.0); //remove bus from queue of stop, updates state from OnCall to Idle, also in fleetState
//    bus1->set_state(BusState::Null,0.0); //change from Idle to Null should remove bus from fleetState
//    CC->disconnectVehicle(bus1); //should remove bus from Controlcenter

//    delete bus1;
//    delete pass1;
}

void TestFixedWithFlexible_day2day::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));
    qDebug() << "Final day: " << net->day;
    //QVERIFY(net->day == theParameters->max_days); // current setup does not converge within 20 days
}

void TestFixedWithFlexible_day2day::testPassAssignment_day2day()
{
/**
  Start by printing outputs for each Busline, separate the fixed ones and the flexible ones
  Try and deduce how passenger flows were allocated
*/

/**
  @todo
    - Check that network has been run with default input params
    - See how passenger flows have evolved. Print usage for each line.. over days
        - Add attributes of network to fill in the data structures we need for additional outputs over days
        - Perhaps look at the 'convergence' file for inspiration
        -

  @todo
    - Check where unserved passengers go, print output for unserved passengers
    - Look at where DRT credibility coefficients are updated
    - Basically look at all o_selected_paths but over days instead...
    - Print out all the attributes of passenger/od-stops over days as well


    See segments_line_loads.dat output file....Basically want the total number of travelers for each segment of each line. Since we only have one segment per line
    now this equates to the resulting flow assignment.
*/
//    qDebug() << "Running network for 1 day (no day2day active)....";
//    theParameters->max_days = 1; //reset day2day max days
//    nt->start(QThread::HighestPriority);
//    nt->wait();

//    // test here the properties that should be true after running the simulation
//    QVERIFY2(net->get_currenttime() > 0.0, "Failure, network has not run yet, simulation time is not greater than 0.0");

    total_pass_boarded_per_line_d2d = net->total_pass_boarded_per_line_d2d;
    qDebug() << "Printing total passenger output per line to csv file: ";
    vector<Busline*> buslines = net->get_buslines();
    //write resulting assignment to csv file
    //make the header first
    try
    {
        csvfile csv("test.csv",";");
        //header - rows are the days, lines are the columns,
        csv << "day";
        for(const auto& line : buslines)
        {
            csv << line->get_name();
        }
        csv << endrow;

        //data -  values are the total pass boarded
        for(int day = 1; day < net->day; ++day)
        {
            csv << day;
            for(const auto& line : buslines)
            {
                csv << total_pass_boarded_per_line_d2d[line][day];
            }
            csv << endrow;
        }
    }
    catch(const exception &ex)
    {
        cout << "Exception was thrown: " << ex.what() << endl;
    }

    //cleanup
    //theParameters->max_days = 20; //reset day2day max days
    //net->wt_rec.clear(); //reset day2day of network (not included in network->reset since these are used for kindof nested resets)
    //net->ivt_rec.clear(); //reset day2day of network (not included in network->reset since these are used for kindof nested resets)
    //nt->reset(); // delete all passengers and reset network
}

void TestFixedWithFlexible_day2day::testPassAssignment_final()
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

void TestFixedWithFlexible_day2day::testSaveResults()
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

void TestFixedWithFlexible_day2day::testFinalDay2dayOutput()
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


void TestFixedWithFlexible_day2day::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}



QTEST_APPLESS_MAIN(TestFixedWithFlexible_day2day)

#include "test_fixedwithflexible_day2day.moc"
