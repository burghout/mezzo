#include <QString>
#include <QtTest/QtTest>
#include "network.h"
#include "MMath.h"
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


const std::string network_path = "../networks/FWF_testnetwork1/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/FWF_testnetwork1/ExpectedOutputs/";
const QString path_set_generation_filename = "o_path_set_generation.dat";
const vector<QString> output_filenames =
{
    "o_od_stop_summary_without_paths.dat",
    "o_od_stops_summary.dat",
    "o_passenger_trajectory.dat",
    "o_passenger_welfare_summary.dat",
    "o_segments_line_loads.dat",
    "o_segments_trip_loads.dat",
    "o_selected_paths.dat",
    "o_transit_trajectory.dat",
    "o_transitline_sum.dat",
    "o_transitlog_out.dat",
    "o_transitstop_sum.dat",
    "o_trip_total_travel_time.dat",
    "o_fwf_summary.dat"
};

const vector<QString> skip_output_filenames =
{
//    "o_od_stop_summary_without_paths.dat",
//    "o_od_stops_summary.dat"
}; //!< Files skipped in testSaveResults. @todo Get different results for certain files on identical runs, not sure if differences are significant enough to dig into at the moment

const long int seed = 42;

class TestFixedWithFlexible_walking : public QObject
{
    Q_OBJECT

public:
    TestFixedWithFlexible_walking(){}

private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testFleetState(); //!< test methods for filtering vehicles dependent on fleetState in Controlcenter
    void testFlexiblePathExpectedLoS(); //!< test the reading and methods of a specific Pass_path instance, tests calculation of path attributes (ivt, wt, etc. via Controlcenter for flexible legs)
    void testPathSetUtilities(); //!< test navigating generated path-sets, calculating utilities under different circumstances (e.g. access to RTI for different legs)
    void testPassArrivedToWaitingDecisions(); //!< test decisions that move a passenger from a state of having just arrived to a stop (either by being generated there or alighting there) to a state of waiting for fixed or flexible transit service
    void testRunNetwork();
    void testSaveResults();
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestFixedWithFlexible_walking::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestFixedWithFlexible_walking::testInitNetwork()
{
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
    QVERIFY2(net->get_odpairs().size() == 14, "Failure, network should have 14 od pairs ");
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


    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    QVERIFY2(odstops_demand.size() == 7, "Failure, network should have 7 od stop pairs (non-zero or defined in transit_demand) ");

    //Check OD stop demand rate between stop 1 and 4
    ODstops* stop_1to4 = net->get_ODstop_from_odstops_demand(1,4);
    QVERIFY2(AproxEqual(stop_1to4->get_arrivalrate(),300.0),"Failure, ODstops stop 1 to stop 4 should have 300 arrival rate");
    QVERIFY2(stop_1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined ");

    ODstops* stop_5to4 = net->get_ODstop_from_odstops_demand(5,4);
    QVERIFY2(AproxEqual(stop_5to4->get_arrivalrate(),300.0),"Failure, ODstops stop 5 to stop 4 should have 300 arrival rate");
    QVERIFY2(stop_5to4 != nullptr,"Failure, OD stop 5 to 4 is undefined ");

    //Parameters
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 3, "Failure, real time info is not set to 3 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 1.0), "Failure, share RTI network is not 1 in parameters");
    QVERIFY2(theParameters->choice_set_indicator == 1, "Failure, choice set indicator is not set to 1 in parameters");
    QVERIFY2(net->count_transit_paths() == 27, "Failure, network should have 14 transit paths defined");

    //Static fixed service design
    QVERIFY2(AproxEqual(net->get_buslines()[0]->get_planned_headway(),360.0), "Failure, buslines should have a 360 second (6 min) planned headway");

    //Control center
    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == false, "Failure, generate direct routes of controlcenter is not set to false");

}

void TestFixedWithFlexible_walking::testFleetState()
{
    //tests for
    //Controlcenter::getVehiclesDrivingTo(Busstop* stop);
    //Controlcenter::getOnCallVehiclesAt(Busstop* stop);

    //create a vehicles with certain state and add to controlcenter
    Controlcenter* CC = net->get_controlcenters().begin()->second;
    vector<Busline*> serviceRoutes = CC->getServiceRoutes();
    set<Busstop*, ptr_less<Busstop*>> serviceArea = CC->getServiceArea();
    map<BusState,set<Bus*> > fleetState = CC->getFleetState();
    NaiveTripGeneration* tgs = new NaiveTripGeneration(); // borrowed to use helper functions to build trips

    QVERIFY(serviceRoutes.size() == 6);
    QVERIFY(fleetState.size() == 0);
    size_t count = 0;
    for(auto state : fleetState)
    {
        count += state.second.size();
    }
    QVERIFY(count == 0); // 8 vehicles given as input to CC should all be Null
    QVERIFY(CC->connectedVeh_.size() == 8);

    //get stop 1 and 4 in service area
    QVERIFY(serviceArea.size() == 4);
    Busstop* stop1 = net->get_stopsmap()[1];
    Busstop* stop2 = net->get_stopsmap()[2];
    QVERIFY(CC->isInServiceArea(stop1));
    QVERIFY(CC->isInServiceArea(stop2));

    QVERIFY(CC->getOnCallVehiclesAtStop(stop1).size() == 0); //there are 8 vehicles from input files, however these have not been initialized yet are are thus not in an onCall state yet
    QVERIFY(CC->getVehiclesDrivingToStop(stop1).size() == 0);
    QVERIFY(CC->getAllVehicles().size() == 0); // all vehicles should skip those that are in 'Null' state

    //Busline* drt1 = (*find_if(serviceRoutes.begin(),serviceRoutes.end(),[](const Busline* line){return line->get_id() == 8001;}));
    auto drt1_opp_it = find_if(serviceRoutes.begin(),serviceRoutes.end(),[](const Busline* line){return line->get_id() == 8021;});
    QVERIFY(drt1_opp_it != serviceRoutes.end());
    Busline* drt1_opp = *drt1_opp_it;

    vector<Visit_stop*> schedule = tgs->create_schedule(0.0,drt1_opp->get_delta_at_stops());
    Bustrip* trip = tgs->create_unassigned_trip(drt1_opp,0.0,schedule);

    pair<Bus*,double> closest; // closest bus to a stop with expected time until arrival

    //no vehicles activated yet
    closest = CC->getClosestVehicleToStop(stop1,0.0);
    QVERIFY(closest.first == nullptr); // nullptr returned if no vehicle found
    QVERIFY(AproxEqual(closest.second,DBL_INF)); // default time until arrival if no vehicle found

    // bus oncall at stop 1
    Bus* bus1 = new Bus(1,4,4,nullptr,nullptr,0.0,true,nullptr);
    Bustype* bustype = CC->connectedVeh_.begin()->second->get_bus_type();
    bus1->set_bustype_attributes(bustype);
    bus1->set_bus_id(1); //vehicle id and bus id mumbojumbo
    CC->connectVehicle(bus1); //connect vehicle to a control center
    CC->addVehicleToAllServiceRoutes(bus1);
    bus1->set_last_stop_visited(stop1); // need to do this before setting state always
    stop1->add_unassigned_bus(bus1,0.0); //should change the buses stat to OnCall and update connected Controlcenter
    QVERIFY(CC->getOnCallVehiclesAtStop(stop1).size() == 1);

    closest = CC->getClosestVehicleToStop(stop1,0.0);
    QVERIFY(closest.first != nullptr);
    QVERIFY(closest.first == bus1); // bus1 should be oncall at stop 1
    QVERIFY(AproxEqual(closest.second,0.0)); // should have 0.0s expected time until arrival

    stop1->remove_unassigned_bus(bus1,0.0); //remove bus from queue of stop, updates state to Idle, also in fleetState
    bus1->set_state(BusState::Null,0.0); //change from Idle to Null should remove bus from fleetState
    CC->disconnectVehicle(bus1); //should remove bus from Controlcenter

    // bus driving from stop 2 to stop 1 on drt1_opp
    Bus* bus2 = new Bus(2,4,4,nullptr,nullptr,0.0,true,nullptr);
    bus2->set_bus_id(2);
    bus2->set_bustype_attributes(bustype);
    CC->connectVehicle(bus2); //connect vehicle to a control center
    CC->addVehicleToAllServiceRoutes(bus2);
    trip->set_busv(bus2);
    bus2->set_curr_trip(trip);
    bus2->set_on_trip(true);
    bus2->set_last_stop_visited(stop2);
    bus2->get_curr_trip()->advance_next_stop(0.0,nullptr);
    bus2->set_state(BusState::DrivingEmpty,0.0);

    QVERIFY(bus2->is_driving() && bus2->get_on_trip());
    QVERIFY(CC->getVehiclesDrivingToStop(stop1).size() == 1);
    closest = CC->getClosestVehicleToStop(stop1,0.0);
    QVERIFY(closest.first == bus2);
    QVERIFY(AproxEqual(closest.second,154.0));
    trip->set_last_stop_exit_time(0.0);
    closest = CC->getClosestVehicleToStop(stop1,100.0); //time is now 100, so bus2 has been driving for 100 seconds
    QVERIFY(closest.first == bus2);
    QVERIFY(AproxEqual(closest.second,54.0));

    CC->disconnectVehicle(bus2);
    CC->fleetState_.clear();

    delete bus1;
    delete bus2;
    delete trip;
    delete tgs;
}

void TestFixedWithFlexible_walking::testFlexiblePathExpectedLoS()
{
    ODstops* stop5to4 = net->get_ODstop_from_odstops_demand(5,4);
    ODstops* stop1to4 = net->get_ODstop_from_odstops_demand(1,4);
    QVERIFY2(stop5to4 != nullptr,"Failure, OD stop 5 to 4 is undefined ");
    QVERIFY2(stop1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined ");
    vector<Pass_path*> path_set_5to4 = stop5to4->get_path_set();
    vector<Pass_path*> path_set_1to4 = stop1to4->get_path_set();
    QVERIFY(!path_set_5to4.empty());
    QVERIFY(!path_set_1to4.empty());

    vector<Pass_path*> drt_first_paths = stop5to4->get_flex_first_paths(); //retrieving this directly from OD membership instead
    vector<Pass_path*> fix_first_paths = stop5to4->get_fix_first_paths();
    QVERIFY2(path_set_5to4.size() == 7,"Failure, there should be a total of 7 paths available from stop 5->4");
    QVERIFY2(drt_first_paths.size() == 3,"Failure, there should be 3 paths available stop 5->4 where the first leg is flexible");
    QVERIFY2(fix_first_paths.size() == 4, "Failure, there should be 4 paths available stop 5->4 where the first leg is fixed");

    // Test path 17 (should also be 17 in input file even though reader relabels paths starting from id = 0):
    // Path 17 route: 5->4 with S5 -> Walk(330m) -> S1 -> DRT_1 -> S2 -> FIX_2 -> S3 -> DRT_3 -> S4
    auto path17_it = find_if(drt_first_paths.begin(),drt_first_paths.end(),[](Pass_path* path)->bool{return path->get_id() == 17;});
    auto path20_it = find_if(fix_first_paths.begin(),fix_first_paths.end(),[](Pass_path* path)->bool{return path->get_id() == 20;});
    auto path14_it = find_if(drt_first_paths.begin(),drt_first_paths.end(),[](Pass_path* path)->bool{return path->get_id() == 14;});
    auto path4_it = find_if(path_set_1to4.begin(),path_set_1to4.end(),[](Pass_path* path)->bool{return path->get_id() == 4;});
    Pass_path* path17 = nullptr;
    Pass_path* path20 = nullptr;
    Pass_path* path14 = nullptr;
    Pass_path* path4 = nullptr;

    if(path17_it != drt_first_paths.end())
        path17 = *path17_it;
    else
        QVERIFY(path17_it != drt_first_paths.end());

    if(path20_it != fix_first_paths.end())
        path20 = *path20_it;
    else
        QVERIFY(path20_it != fix_first_paths.end());

    if(path14_it != drt_first_paths.end())
        path14 = *path14_it;
    else
        QVERIFY(path14_it != drt_first_paths.end());

    if(path4_it != path_set_1to4.end())
        path4 = *path4_it;
    else
        QVERIFY(path4_it != path_set_1to4.end());

    Pass_path* test17 = net->get_pass_path_from_id(17);
    Pass_path* test888 = net->get_pass_path_from_id(888);
    QVERIFY(test17 == path17);
    QVERIFY(test888 == nullptr);

    qDebug() << "Testing reading and utility calculations for path 17:";
    //Check path 17 5->4 attributes: 8 alt stops, 3 alt lines all of size 1, 2 transfers, 1 non-zero walking link
    QVERIFY(path17->get_alt_transfer_stops().size() == 8);
    QVERIFY(path17->get_alt_lines().size() == 3);
    vector<vector<Busline*> > path17_lines = path17->get_alt_lines();
    for(auto linevec : path17->get_alt_lines())
    {
        QVERIFY(linevec.size() == 1);
    }
    QVERIFY(path17->get_number_of_transfers() == 2);
    QVERIFY(AproxEqual(path17->get_walking_distances()[0], 330.0));

    //check sequence of lines in path
    QVERIFY(path17_lines[0][0]->get_id()==8001);
    QVERIFY(path17_lines[1][0]->get_id()==2);
    QVERIFY(path17_lines[2][0]->get_id()==8003);

    vector<vector<Busstop*> > path17_stops = path17->get_alt_transfer_stops();
    //check sequence of stops in path
    QVERIFY(path17_stops[0][0]->get_id()==5);
    QVERIFY(path17_stops[1][0]->get_id()==1);
    QVERIFY(path17_stops[2][0]->get_id()==2);
    QVERIFY(path17_stops[3][0]->get_id()==2);
    QVERIFY(path17_stops[4][0]->get_id()==3);
    QVERIFY(path17_stops[5][0]->get_id()==3);
    QVERIFY(path17_stops[6][0]->get_id()==4);
    QVERIFY(path17_stops[7][0]->get_id()==4);

    //!< TEST Walking distances of Path 17
    QVERIFY(AproxEqual(theParameters->average_walking_speed,66.66)); // meters per minute
    QVERIFY(AproxEqual(path17->calc_total_walking_distance(),330.0));
    qDebug() << "path 17 total walking distance      : " << path17->calc_total_walking_distance();
    double path17_walking_time = path17->calc_total_walking_distance() / theParameters->average_walking_speed;
    qDebug() << "path 17 total walking time (minutes): " << path17_walking_time;
    qDebug() << "path 17 total walking time (seconds): " << path17_walking_time*60;
    QVERIFY(AproxEqual(path17_walking_time,4.9505));

    //!< TEST number of transfers of Path 17
    QVERIFY(path17->get_number_of_transfers() == 2);

    //!< TEST IVTs of Path 17
    //First line leg is drt1, should return the 'RTI' based expected IVT
    QVERIFY(path17->check_all_flexible_lines(path17_lines[0]));
    Busline* drt1 = path17_lines[0][0];
    QVERIFY(drt1->is_flex_line());
    Controlcenter* cc = drt1->get_CC();

    //rti based (should be roughly 152 seconds based on shortest path call)
    double leg1_ivt_rti = cc->calc_expected_ivt(drt1,drt1->stops.front(),drt1->stops.back(),true,0.0);
    qDebug() << "drt1 rti ivt     : " << leg1_ivt_rti;
    QVERIFY(AproxEqual(leg1_ivt_rti, 152.0));

    //schedule based (should match whatever estimate is placed into input files)
    double leg1_ivt_explore = cc->calc_expected_ivt(drt1,drt1->stops.front(),drt1->stops.back(),false,0.0);
    qDebug() << "drt1 explore ivt  : " << leg1_ivt_explore;
    qDebug() << "drt1 scheduled ivt: " << drt1->get_delta_at_stops().back().second;
    double drt1_sched_ivt = drt1->get_delta_at_stops().back().second;
    QVERIFY(AproxEqual(leg1_ivt_explore,drt1_sched_ivt));
    QVERIFY(AproxEqual(leg1_ivt_explore, 150.0));

    //Second line leg is fixed2, should not go via Controlcenter
    Busline* fixed2 = path17_lines[1][0];
    //qDebug() << fixed2->calc_curr_line_ivt(fixed2->stops.front(),fixed2->stops.back(),1,0.0);
    //qDebug() << fixed2->calc_curr_line_ivt(fixed2->stops.front(),fixed2->stops.back(),2,0.0);
    qDebug() << "fixed2 scheduled ivt: " << fixed2->calc_curr_line_ivt(fixed2->stops.front(),fixed2->stops.back(),3,0.0);
    double leg2_rti_ivt = fixed2->calc_curr_line_ivt(fixed2->stops.front(),fixed2->stops.back(),3,0.0);
    double leg2_sched_ivt = fixed2->get_delta_at_stops().back().second;
    QVERIFY(AproxEqual(leg2_rti_ivt, leg2_sched_ivt));
    QVERIFY(AproxEqual(leg2_rti_ivt, 600.0));

    //Third line leg is drt3, should return the 'exploration' or 'scheduled' IVT defined statically in input files
    Busline* drt3 = path17_lines[2][0];
    double leg3_ivt_rti = cc->calc_expected_ivt(drt3,drt3->stops.front(),drt3->stops.back(),true,0.0);
    qDebug() << "drt3 rti ivt: " << leg3_ivt_rti;
    QVERIFY(AproxEqual(leg3_ivt_rti,153.0));

    double leg3_ivt_explore = cc->calc_expected_ivt(drt3,drt3->stops.front(),drt3->stops.back(),false,0.0);
    qDebug() << "drt1 explore ivt  : " << leg3_ivt_explore;
    double leg3_ivt_sched = drt3->get_delta_at_stops().back().second;
    qDebug() << "drt1 scheduled ivt: " << leg3_ivt_sched;
    QVERIFY(AproxEqual(leg3_ivt_explore,leg3_ivt_sched));
    QVERIFY(AproxEqual(leg3_ivt_explore,150.0));

    //Test total IVT of path from perspective of passenger with OD 5 to 4
    Passenger* pass1 =  new Passenger(1,0.0,stop5to4); //create passenger with id 1 at stop 5 with stop 4 as final destination
    pass1->init();

    double path17_total_ivt = path17->calc_total_in_vehicle_time(0.0,pass1);
    qDebug() << "path 17 total ivt         : " << path17_total_ivt;
    double expected_total_ivt = (leg1_ivt_rti + leg2_rti_ivt + leg3_ivt_explore) / 60.0; //convert to minutes
    qDebug() << "path 17 expected total ivt: " << expected_total_ivt;
    QVERIFY(AproxEqual(path17_total_ivt,expected_total_ivt));

    //! Note that avg_walking_speed, as well as utility coefficients seem to be drawn randomly...Check if you are in the right ballpark first and then
    //! verify repeatability of draws when running a more elaborate integration test on these methods?
    //!
    //! Network state to check:
    //!     DRT vehicle 1 is on-call at stop 4
    //!     Passenger 1 at stop 5 going to stop 4
    //!     Passenger 2 at stop 1 going to stop 4

    Controlcenter* CC = drt1->get_CC();
    QVERIFY(CC != nullptr);
    QVERIFY(CC->getAllVehicles().size() == 0);// no vehicles have been initialized
    QVERIFY(AproxEqual(CC->calc_exploration_wt(), 0.0)); //exploration parameters initialized to 0.0
    // Busstop* stop1 = net->get_stopsmap()[1];
    // Busstop* stop5 = net->get_stopsmap()[5];
    Busstop* stop4 = net->get_stopsmap()[4];

    // Creating DRT vehicle 1 is on-call at stop 4
    Bus* bus1 = new Bus(1,4,4,nullptr,nullptr,0.0,true,nullptr);
    Bustype* bustype = CC->connectedVeh_.begin()->second->get_bus_type();
    bus1->set_bustype_attributes(bustype);
    bus1->set_bus_id(1); //vehicle id and bus id mumbojumbo
    CC->connectVehicle(bus1); //connect vehicle to a control center
    CC->addVehicleToAllServiceRoutes(bus1);
    bus1->set_last_stop_visited(stop4); // need to do this before setting state always
    stop4->add_unassigned_bus(bus1,0.0); //should change the buses stat to OnCall and update connected Controlcenter
    QVERIFY(CC->getOnCallVehiclesAtStop(stop4).size() == 1);

    //Create Passenger 2 at stop 1 going to stop 4
    Passenger* pass2 =  new Passenger(2,0.0,stop1to4);
    pass2->init();

    //! Paths to check:
    //!     Check path 17 attributes for pass 1 (includes walking first, first leg flexible (line 8001), second fixed (line 2), third flexible (line 8003))
    //!     Check path 20 attributes for pass 1 (no walking, first leg fixed (line 5), second fixed (line 2), third flexible (line 8003))
    //!     Check path 14 attributes for pass 1 (walk, first and only leg flexible (line 8002))
    //!     Check path 4 direct path for pass 2 (no walking, first and only leg flexible (line 8002))
    //!
    //!     Expected total waiting time for path 17 : drt1(908s) + fixed2(180s) + drt3(158s) - walking time for first leg (297.03 s) if less than waiting time
    //!     Expected total waiting time for path 20 : fixed5(180s) + fixed2(180s) + drt3(158s) - no walking
    //!     Expected total waiting time for path 14 : drt2(908s) - walking time for first leg (297.03s)
    //!     Expected total waiting time for path 4 : drt3(908s)
    //!
    //!     Passengers can save on walking time (only for first leg) by sending a request for the expected arrival to the stop...So the wt = max(0,expected waiting time of first leg - walking time)

    //!< TEST Waiting times of path 17
    qDebug() << "Checking path " << path17->get_id() << " between stop 5 to 4 attributes for passenger " << pass1->get_id() << ", 1 drt vehicle at stop " << bus1->get_last_stop_visited()->get_id();
    // Total waiting time
    qDebug() << "Path 17 total IVT time    : " << path17_total_ivt*60 << "s";
    double path17_total_wt = path17->calc_total_waiting_time(0.0,false,false,theParameters->average_walking_speed,pass1);
    qDebug() << "Path 17 total waiting time: " << path17_total_wt*60 << "s";

    // Path 17 first leg
    Busline* path17_leg1 = path17->get_alt_lines()[0][0];
    double path17_leg1_wt = CC->calc_expected_wt(path17_leg1,path17_leg1->stops.front(),path17_leg1->stops.back(),true,path17_walking_time*60,0.0);
    QVERIFY(AproxEqual(path17_leg1_wt,904.0 - path17_walking_time*60));

    // second leg
    Busline* path17_leg2 = path17->get_alt_lines()[1][0];
    double path17_leg2_wt_pk = path17_leg2->get_planned_headway() / 2.0; //should be 180 seconds headway = 360 divided by 2;
    QVERIFY(AproxEqual(path17_leg2_wt_pk,180.0));
    double arr_time = 0.0 + path17_leg1_wt + path17_walking_time*60 + leg1_ivt_rti;
    // qDebug() << arr_time;
    double path17_leg2_wt_rti3 = path17_leg2->time_till_next_arrival_at_stop_after_time(path17_leg2->stops.front(),arr_time);
    QVERIFY(AproxEqual(path17_leg2_wt_rti3,1200.0 - arr_time)); //time until first scheduled trip in this case
    double path17_leg2_wt = (theParameters->default_alpha_RTI * path17_leg2_wt_rti3 + (1-theParameters->default_alpha_RTI) * path17_leg2_wt_pk);
    // qDebug() << path17_leg2_wt;
    QVERIFY(AproxEqual(path17_leg2_wt,154.8));

    // third leg, should return exploration parameter
    Busline* path17_leg3 = path17->get_alt_lines()[2][0];
    arr_time = 0.0 + path17_leg1_wt + path17_leg2_wt + path17_walking_time*60 + leg1_ivt_rti + leg2_rti_ivt;
    double path17_leg3_wt = CC->calc_expected_wt(path17_leg3,path17_leg3->stops.front(),path17_leg3->stops.back(),false,path17_walking_time*60,arr_time);
    //qDebug() << "Path 17 leg 3 waiting time: " << path17_leg3_wt;
    QVERIFY(AproxEqual(path17_leg3_wt,::drt_exploration_wt));
    QVERIFY(AproxEqual(path17_leg1_wt + path17_leg2_wt + path17_leg3_wt, path17_total_wt*60));

    //!< TEST Waiting times of path 14
    qDebug() << "Checking path " << path14->get_id() << " between stop 5 to 4 attributes for passenger " << pass1->get_id()<< ", 1 drt vehicle at stop " << bus1->get_last_stop_visited()->get_id();
    double path14_total_ivt = path14->calc_total_in_vehicle_time(0.0,pass1); // need to call this before calc_total_waiting_time
    qDebug() << "Path 14 total IVT         : " << path14_total_ivt*60 << "s";
    double path14_total_wt = path14->calc_total_waiting_time(0.0,false,false,theParameters->average_walking_speed,pass1);
    qDebug() << "Path 14 total waiting time: " << path14_total_wt*60 << "s";

    // Path 14 first leg (direct to stop 4 via walking to stop 1)
    Busline* path14_leg1 = path14->get_alt_lines()[0][0];
    double path14_walking_time = path14->calc_total_walking_distance() / theParameters->average_walking_speed;
    double path14_leg1_wt = CC->calc_expected_wt(path14_leg1,path14_leg1->stops.front(),path14_leg1->stops.back(),true,path14_walking_time*60,0.0);
    // qDebug() << path14_leg1_wt;
    QVERIFY(AproxEqual(path14_leg1_wt,904.0 - path14_walking_time*60));
    QVERIFY(AproxEqual(path14_leg1_wt,path14_total_wt*60));

    //!< TEST Waiting times of path 20
    qDebug() << "Checking path " << path20->get_id() << " between stop 5 to 4 attributes for passenger " << pass1->get_id()<< ", 1 drt vehicle at stop " << bus1->get_last_stop_visited()->get_id();
    double path20_total_ivt = path20->calc_total_in_vehicle_time(0.0,pass1); // need to call this before calc_total_waiting_time
    qDebug() << "Path 20 total IVT         : " << path20_total_ivt*60 << "s";
    double path20_total_wt = path20->calc_total_waiting_time(0.0,false,false,theParameters->average_walking_speed,pass1);
    qDebug() << "Path 20 total waiting time: " << path20_total_wt*60 << "s";

    // Path 20 first leg fixed
    Busline* path20_leg1 = path20->get_alt_lines()[0][0];
    double path20_leg1_ivt_rti = path20_leg1->calc_curr_line_ivt(path20_leg1->stops.front(),path20_leg1->stops.back(),3,0.0);
    double path20_walking_time = path20->calc_total_walking_distance() / theParameters->average_walking_speed;
    arr_time = 0.0 + path20_walking_time; //should be zero
    double path20_leg1_wt_pk = path20_leg1->get_planned_headway() / 2.0; //should be 180 seconds headway = 360 divided by 2;
    double path20_leg1_wt_rti = path20_leg1->time_till_next_arrival_at_stop_after_time(path20_leg1->stops.front(),arr_time);
    double path20_leg1_wt = (theParameters->default_alpha_RTI * path20_leg1_wt_rti + (1-theParameters->default_alpha_RTI) * path20_leg1_wt_pk);
    // qDebug() << path20_leg1_wt_rti;
    QVERIFY(AproxEqual(path20_leg1_wt,894 - path20_walking_time*60));

    // second leg fixed
    Busline* path20_leg2 = path20->get_alt_lines()[1][0];
    double path20_leg2_ivt_rti = path20_leg2->calc_curr_line_ivt(path20_leg2->stops.front(),path20_leg2->stops.back(),3,0.0);
    double path20_leg2_wt_pk = path20_leg2->get_planned_headway() / 2.0; //should be 180 seconds headway = 360 divided by 2;
    QVERIFY(AproxEqual(path20_leg2_wt_pk,180.0));
    arr_time = 0.0 + path20_leg1_wt + path20_walking_time*60 + path20_leg1_ivt_rti;
    // qDebug() << arr_time;
    double path20_leg2_wt_rti3 = path20_leg2->time_till_next_arrival_at_stop_after_time(path20_leg2->stops.front(),arr_time);
    QVERIFY(AproxEqual(path20_leg2_wt_rti3,1200.0 - arr_time)); //time until first scheduled trip in this case
    double path20_leg2_wt = (theParameters->default_alpha_RTI * path20_leg2_wt_rti3 + (1-theParameters->default_alpha_RTI) * path20_leg2_wt_pk);
    // qDebug() << path20_leg2_wt;
    QVERIFY(AproxEqual(path20_leg2_wt,163.2));

    // third leg, should return exploration parameter
    Busline* path20_leg3 = path20->get_alt_lines()[2][0];
    arr_time = 0.0 + path20_leg1_wt + path20_leg2_wt + path20_walking_time*60 + path20_leg1_ivt_rti + path20_leg2_ivt_rti;
    double path20_leg3_wt = CC->calc_expected_wt(path20_leg3,path20_leg3->stops.front(),path20_leg3->stops.back(),false,path20_walking_time*60,arr_time);
    QVERIFY(AproxEqual(path20_leg3_wt,::drt_exploration_wt));
    QVERIFY(AproxEqual(path20_leg1_wt + path20_leg2_wt + path20_leg3_wt, path20_total_wt*60));
    //qDebug() << "Checking path " << path4->get_id() << " between stop 1 to 4 attributes for passenger " << pass2->get_id() << ", 1 drt vehicle at stop " << bus1->get_last_stop_visited()->get_id();

    //!< TEST waiting times path 4
    qDebug() << "Checking path " << path4->get_id() << " between stop 1 to 4 attributes for passenger " << pass2->get_id()<< ", 1 drt vehicle at stop " << bus1->get_last_stop_visited()->get_id();
    double path4_total_ivt = path4->calc_total_in_vehicle_time(0.0,pass2); // need to call this before calc_total_waiting_time
    qDebug() << "Path 14 total IVT         : " << path4_total_ivt*60 << "s";
    double path4_total_wt = path4->calc_total_waiting_time(0.0,false,false,theParameters->average_walking_speed,pass2);
    qDebug() << "Path 14 total waiting time: " << path4_total_wt*60 << "s";

    // Path 14 first leg (direct to stop 4 via walking to stop 1)
    Busline* path4_leg1 = path4->get_alt_lines()[0][0];
    double path4_walking_time = path4->calc_total_walking_distance() / theParameters->average_walking_speed;
    double path4_leg1_wt = CC->calc_expected_wt(path4_leg1,path4_leg1->stops.front(),path4_leg1->stops.back(),true,path4_walking_time*60,0.0);
    // qDebug() << path14_leg1_wt;
    QVERIFY(AproxEqual(path4_leg1_wt,904.0 - path4_walking_time*60));
    QVERIFY(AproxEqual(path4_leg1_wt,path4_total_wt*60));

    //cleanup
    stop4->remove_unassigned_bus(bus1,0.0); //remove bus from queue of stop, updates state from OnCall to Idle, also in fleetState
    bus1->set_state(BusState::Null,0.0); //change from Idle to Null should remove bus from fleetState
    CC->disconnectVehicle(bus1); //should remove bus from Controlcenter

    delete bus1;
    delete pass1;
    delete pass2;
}

void TestFixedWithFlexible_walking::testPathSetUtilities()
{
    //!< Check pathset of a given ODstop pair (5to4 or 1to4), verify that utilities are being calculated as expected and the highest utility alternative is chosen
    /* Check the parameters that have been set that are used in calculating utilities */
//    QVERIFY(AproxEqual(theParameters->walking_time_coefficient,-2.0));
//    QVERIFY(AproxEqual(theParameters->waiting_time_coefficient,-2.0));
//    QVERIFY(AproxEqual(theParameters->in_vehicle_time_coefficient,-1.0));
//    QVERIFY(AproxEqual(theParameters->transfer_coefficient,-5.0));


    /* Parameters taken from SF network */
    QVERIFY(AproxEqual(theParameters->transfer_coefficient,-0.334));
    QVERIFY(AproxEqual(theParameters->in_vehicle_time_coefficient,-0.04));
    QVERIFY(AproxEqual(theParameters->waiting_time_coefficient,-0.07));
    QVERIFY(AproxEqual(theParameters->walking_time_coefficient,-0.07));

    /* Other parameters that effect pass decisions*/
    QVERIFY(AproxEqual(theParameters->max_waiting_time,1800.0));
    QVERIFY(AproxEqual(theParameters->default_alpha_RTI,0.7)); // In case of RTI,

    /* Grab path sets for 5to4 and 1to4 */
    ODstops* stop1to4 = net->get_ODstop_from_odstops_demand(1,4);
    QVERIFY2(stop1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined ");
    ODstops* stop5to4 = net->get_ODstop_from_odstops_demand(5,4);
    QVERIFY2(stop5to4 != nullptr,"Failure, OD stop 5 to 4 is undefined ");

    //create a passenger with ODstop 5 to 4 and 1 to 4 to navigate paths from passengers perspective
    Passenger* pass1 = new Passenger(1,0,stop5to4,nullptr); //Passenger(id,start_time,ODstop*,QObject* parent)
    pass1->init(); //note this both sets the network level RTI for the traveler as well as the anticipated WT and IVT of the traveler if day2day is active
    Passenger* pass2 = new Passenger(2,0,stop1to4,nullptr);
    pass2->init();

    Busstop* bs_o = pass1->get_OD_stop()->get_origin();
    Busstop* bs_d = pass1->get_OD_stop()->get_destination();
    vector<Pass_path*> pathset_5to4 = bs_o->get_stop_od_as_origin_per_stop(bs_d)->get_path_set(); //no idea why we are accessing the path-set in this roundabout way but there is hopefully a good reason for it
    vector<Pass_path*> pathset_1to4 = stop1to4->get_path_set();

    QVERIFY2(stop5to4->check_path_set(), "Failure, no paths defined for stop 5 to 4");
    QVERIFY2(stop5to4->get_path_set().size() == 7, "Failure, there should be 7 paths defined for stop 5 to 4");
    QVERIFY2(pathset_5to4.size() == 7, "Failure, traveler with ODstops 5 to 4 should have 7 paths available to them");
    qDebug() << "Number of paths available to traveler stops 5->4: " << stop5to4->get_path_set().size();

    QVERIFY2(stop1to4->check_path_set(), "Failure, no paths defined for stop 1 to 4");
    QVERIFY2(pathset_1to4.size() == 7, "Failure, there should be 7 paths defined for stop 1 to 4");
    qDebug() << "Number of paths available to traveler stops 1->4: " << stop1to4->get_path_set().size();

    //create a DRT vehicle at stop 4
    Busstop* stop4 = net->get_stopsmap()[4];
    Controlcenter* CC = stop4->get_CC();

    // Creating DRT vehicle 1 is on-call at stop 4
    Bus* bus1 = new Bus(1,4,4,nullptr,nullptr,0.0,true,nullptr);
    Bustype* bustype = CC->connectedVeh_.begin()->second->get_bus_type();
    bus1->set_bustype_attributes(bustype);
    bus1->set_bus_id(1); //vehicle id and bus id mumbojumbo
    CC->connectVehicle(bus1); //connect vehicle to a control center
    CC->addVehicleToAllServiceRoutes(bus1);
    bus1->set_last_stop_visited(stop4); // need to do this before setting state always
    stop4->add_unassigned_bus(bus1,0.0); //should change the buses stat to OnCall and update connected Controlcenter
    QVERIFY(CC->getOnCallVehiclesAtStop(stop4).size() == 1);

    /* TEST PASSENGER INIT */
    //Test initialization of passenger with RTI at a network level
    //QVERIFY2(pass1->get_pass_RTI_network_level() == false, "Failure, default initilization of passenger network-level RTI should be false");    
    //QVERIFY2(pass1->get_pass_RTI_network_level() == true,"Failure, passenger should have network-level RTI after init with real_time_info=3 and share_RTI_network=1 in parameters");
    //QVERIFY2(pass1->has_access_to_flexible() == true, "Failure, passenger with network-level RTI should have access to flexible services");
    //Test creation of passenger with no RTI (should be default)
    //QVERIFY(pass2->get_pass_RTI_network_level() == false);
    //QVERIFY2(pass2->has_access_to_flexible() == false, "Failure, passenger without network-level RTI should not have access to flexible services");

//!< TEST connection decision
//! @note not quite like connection decision since 'no double walking rule' is not included, also the walking cost to access 5to4 or 1to4 pathsets calculated differently
    pathset_5to4 = pass1->get_OD_stop()->get_path_set();
    double sum_pathsetutil = 0.0;

    for(const auto& path : pathset_5to4)
    {
        double twkt = path->calc_total_walking_distance() / theParameters->average_walking_speed;
        double tivt = path->calc_total_in_vehicle_time(0.0, pass1);
        double twt = path->calc_total_waiting_time(0.0, false, false, theParameters->average_walking_speed, pass1);
        double n_trans = path->get_number_of_transfers();

        double path_utility = twkt * theParameters->walking_time_coefficient
                       + tivt * theParameters->in_vehicle_time_coefficient
                       + twt  * theParameters->waiting_time_coefficient
                       + n_trans * theParameters->transfer_coefficient;

        qDebug() << "Path " << path->get_id()  << ": " << Qt::endl
                 << "\t total walk time: " << twkt << Qt::endl
                 << "\t total IVT      : " << tivt << Qt::endl
                 << "\t total WT       : " << twt << Qt::endl
                 << "\t transfers      : " << n_trans << Qt::endl
                 << "\t num flex legs  : " << path->count_flexible_legs() << Qt::endl
                 << "\t utility        : " << path_utility << Qt::endl;

        if(twt*60 > theParameters->max_waiting_time) //dynamic filtering rule
        {
            path_utility = ::large_negative_utility;
        }

        sum_pathsetutil += exp(path_utility);
    }
    double logsum_5to4 = log(sum_pathsetutil);
    qDebug() << "Logsum pathset 5->4: " << logsum_5to4 << Qt::endl;

    for(const auto& path : pathset_1to4)
    {
        double twkt = path->calc_total_walking_distance() / theParameters->average_walking_speed;
        double tivt = path->calc_total_in_vehicle_time(0.0, pass1);
        double twt = path->calc_total_waiting_time(0.0, false, false, theParameters->average_walking_speed, pass1);
        double n_trans = path->get_number_of_transfers();

        double path_utility = twkt * theParameters->walking_time_coefficient
                       + tivt * theParameters->in_vehicle_time_coefficient
                       + twt  * theParameters->waiting_time_coefficient
                       + n_trans * theParameters->transfer_coefficient;

        qDebug() << "Path " << path->get_id()  << ": " << Qt::endl
                 << "\t total walk time: " << twkt << Qt::endl
                 << "\t total IVT      : " << tivt << Qt::endl
                 << "\t total WT       : " << twt << Qt::endl
                 << "\t transfers      : " << n_trans << Qt::endl
                 << "\t utility        : " << path_utility << Qt::endl;

        if(twt*60 > theParameters->max_waiting_time) //dynamic filtering rule
        {
            path_utility = ::large_negative_utility;
        }

        sum_pathsetutil += exp(path_utility);
    }
    double logsum_1to4 = log(sum_pathsetutil);
    qDebug() << "Logsum pathset 1->4: " << logsum_1to4 << Qt::endl;
    double MNL_denom = exp(logsum_1to4) + exp(logsum_5to4);
    double prob_5to4 = exp(logsum_5to4) / MNL_denom;

    qDebug() << "Pass1 at stop " << pass1->get_original_origin()->get_id() << " chooses to stay at stop 5 with probability: " << Round(100*prob_5to4) << "%";
    qDebug() << "Pass1 at stop " << pass1->get_original_origin()->get_id() << " chooses to walk to stop 1 with probability: " << Round(100*(1-prob_5to4)) << "%";

    Busstop* connection_stop = nullptr;
    connection_stop = pass1->make_connection_decision(0.0); //make connection decision with starttime = 0.0
    qDebug() << "make_connection_decision (with random draws): " << Qt::endl
             << "\t Pass1 at stop " << pass1->get_original_origin()->get_id() << " chooses to connect at stop " << connection_stop->get_id();

//    pathset_1to4.front()->random->randomize(); //experiment with changing the random seed, see how big of a difference this makes... @note seems to make a pretty big difference!
//    connection_stop = pass1->make_connection_decision(0.0); //make connection decision with starttime = 0.0
//    qDebug() << "make_connection_decision (with random draws): " << Qt::endl
//             << "\t Pass1 at stop " << pass1->get_original_origin()->get_id() << " chooses to connect at stop " << connection_stop->get_id();

    //cleanup
    stop4->remove_unassigned_bus(bus1,0.0); //remove bus from queue of stop, updates state from OnCall to Idle, also in fleetState
    bus1->set_state(BusState::Null,0.0); //change from Idle to Null should remove bus from fleetState
    CC->disconnectVehicle(bus1); //should remove bus from Controlcenter

    delete bus1;

    delete pass1;
    delete pass2;
}

void TestFixedWithFlexible_walking::testPassArrivedToWaitingDecisions()
{
    //create a DRT vehicle at stop 4
    Busstop* stop4 = net->get_stopsmap()[4];
    Controlcenter* CC = stop4->get_CC();

    // Creating DRT vehicle 1 is on-call at stop 4
    Bus* bus1 = new Bus(1,4,4,nullptr,nullptr,0.0,true,nullptr);
    Bustype* bustype = CC->connectedVeh_.begin()->second->get_bus_type();
    bus1->set_bustype_attributes(bustype);
    bus1->set_bus_id(1); //vehicle id and bus id mumbojumbo
    CC->connectVehicle(bus1); //connect vehicle to a control center
    CC->addVehicleToAllServiceRoutes(bus1);
    bus1->set_last_stop_visited(stop4); // need to do this before setting state always
    stop4->add_unassigned_bus(bus1,0.0); //should change the buses stat to OnCall and update connected Controlcenter
    QVERIFY(CC->getOnCallVehiclesAtStop(stop4).size() == 1);

    //Basically mimic what is supposed to happen in Passenger::start and Busstop::pass_activity_at_stop
    ODstops* stop5to4 = net->get_ODstop_from_odstops_demand(5,4);
    Passenger* pass1 = new Passenger(1,0,stop5to4,nullptr); //create a passenger at stop 1 going to stop 4
    pass1->init();

//!< Try out other parameters, Value of IVT (for bus, from 2006 Australian National Guidelines, conversion rate 0.63EUR/AUS) :
//! @note Larger VoT coefficients (i.e. IVT = 1, WT = 2...etc) seem at least at first glance, to increase the variance of resulting MNL probabilities with random draws
//    qDebug() << "Changing VoT coefficients to 2006 Australian National Guideline values";
//    theParameters->in_vehicle_time_coefficient= -0.001540; //EUR/s
//    theParameters->transfer_coefficient= -0.0077; // 5xIVT
//    theParameters->waiting_time_coefficient= -0.003080; //2xIVT, remember that the denied boarding coefficient is 3.5x whatever this is internal to BM
//    theParameters->walking_time_coefficient= -0.003080; //2xIVT

    //!< Recall, the utility of a path set for a given OD pair is given by all the paths at this OD that
    //! do NOT include walking distances (no double walking rule and walking to access this path-set calculated separately)
    //! For a traveler at stop 5 going to stop 4 we should have 2 paths available from stop 5->4 without walking
    //!     Both paths at stop 5->4 without walking are fixed paths, so 100% chance to choose this in make_transitmode_decision
    //! 5 paths availabe from stop 1->4 without walking
    //!     3 paths are flex first, 2 are fixed first
    //!     Calculate expected utilities for these
    ODstops* stop1to4 = net->get_ODstop_from_odstops_demand(1,4);
    vector<Pass_path*> paths1to4 = stop1to4->get_path_set();

    //calculate average utilities and expected probabilities, mimics a passenger walking to stop 1 to choose between fixed and flexible
    double fixed_u = 0.0;
    double flex_u = 0.0;
    int pathcount=0;
    for(const auto& path : paths1to4)
    {
        //double twkt = path->calc_total_walking_distance() / theParameters->average_walking_speed;
        if(path->calc_total_walking_distance() > 0) //no double walking
            continue;

        ++pathcount;
        double tivt = path->calc_total_in_vehicle_time(0.0, pass1);
        double twt = path->calc_total_waiting_time(0.0, false, false, theParameters->average_walking_speed, pass1);
        double n_trans = path->get_number_of_transfers();

        double path_utility = tivt * theParameters->in_vehicle_time_coefficient
                            + twt  * theParameters->waiting_time_coefficient
                            + n_trans * theParameters->transfer_coefficient;

        qDebug() << "Path " << path->get_id()  << ": " << Qt::endl
                 << "\t total IVT      : " << tivt << Qt::endl
                 << "\t total WT       : " << twt << Qt::endl
                 << "\t transfers      : " << n_trans << Qt::endl
                 << "\t utility        : " << path_utility << Qt::endl;

        if(twt*60 > theParameters->max_waiting_time) //dynamic filtering rule
        {
            path_utility = ::large_negative_utility;
        }

        if(path->is_first_transit_leg_fixed())
            fixed_u += exp(path_utility);
        if(path->is_first_transit_leg_flexible())
            flex_u += exp(path_utility);
    }
    QVERIFY(pathcount==5);
    fixed_u = (fixed_u == 0.0) ? -DBL_INF : log(fixed_u);
    flex_u = (flex_u == 0.0) ? -DBL_INF : log(flex_u);

    double MNL_denom = exp(fixed_u) + exp(flex_u);
    double fixed_p = exp(fixed_u) / MNL_denom;
    double flex_p = exp(flex_u) / MNL_denom;
    QVERIFY(AproxEqual(fixed_p + flex_p, 1.0));

    qDebug() << "\t Fixed prob with average utilities: " << fixed_p*100 << "%";
    qDebug() << "\t Flex prob with average utilities: " << flex_p*100 << "%";

    //!< @todo
    //!     Add another test if the resulting probabilities are close to those without random draws
    //!     Make sure coefficients are in EUR/min instead...or doublecheck this

    //!< Make a sequence of connection->transitmode->dropoff decisions and reset
    double t_now = 0.0; //fake current simulation clocktime
    map<Busstop*,int> connection_counts;
    map<TransitModeType,int> mode_counts;
    map<Busstop*,int> dropoff_counts;
    int draws = 100;
    for(int i = 0; i < draws; i++)
    {
        cout << endl;
        Busstop* connection_stop = pass1->make_connection_decision(t_now);
        cout << "\tChosen pickup: " << connection_stop->get_id() << endl;
        ++connection_counts[connection_stop];

        TransitModeType chosen_mode = pass1->make_transitmode_decision(connection_stop,t_now);
        cout << "\tChosen mode: " << chosen_mode << endl;
        ++mode_counts[chosen_mode];
        cout << endl;

        if(connection_stop->get_id() == 5)
            QVERIFY(chosen_mode==TransitModeType::Fixed); // only fixed first legs available from stop 5->4 without walking

        pass1->set_chosen_mode(chosen_mode);
        Busstop* dropoff_stop = nullptr;
        if(chosen_mode==TransitModeType::Flexible)
        {
            dropoff_stop = pass1->make_dropoff_decision(connection_stop,t_now);
            cout << "\tChosen dropoff: " << dropoff_stop->get_id() << endl;
            ++dropoff_counts[dropoff_stop];
            cout << endl;
        }

        pass1->set_chosen_mode(TransitModeType::Null); //reset chosen mode
        pass1->temp_connection_path_utilities.clear();
    }
    Busstop* stop5 = net->get_stopsmap()[5];
    Busstop* stop1 = net->get_stopsmap()[1];
    Busstop* stop2 = net->get_stopsmap()[2];

    if(connection_counts.count(stop5)!=0)
        cout << "Pickup stop 5 (%): " << 100 * connection_counts[stop5] / static_cast<double>(draws) << endl;
    if(connection_counts.count(stop1)!=0)
        cout << "Pickup stop 1 (%): " << 100 * connection_counts[stop1] / static_cast<double>(draws) << endl;

    cout << endl;
    if(mode_counts.count(TransitModeType::Null)!=0)
        cout << "Null  (%): " << 100 * mode_counts[TransitModeType::Null] / static_cast<double>(draws) << endl;
    if(mode_counts.count(TransitModeType::Fixed)!=0)
        cout << "Fixed (%): " << 100 * mode_counts[TransitModeType::Fixed] / static_cast<double>(draws) << endl;
    if(mode_counts.count(TransitModeType::Flexible)!=0)
        cout << "Flex  (%): " << 100 * mode_counts[TransitModeType::Flexible] / static_cast<double>(draws) << endl;

    cout << endl;
    if(dropoff_counts.count(stop2)!=0)
        cout << "Dropoff stop 2 (%): " << 100 * dropoff_counts[stop2] / static_cast<double>(draws) << endl;
    if(dropoff_counts.count(stop4)!=0)
        cout << "Dropoff stop 4 (%): " << 100 * dropoff_counts[stop4] / static_cast<double>(draws) << endl;

    //cleanup
    stop4->remove_unassigned_bus(bus1,0.0); //remove bus from queue of stop, updates state from OnCall to Idle, also in fleetState
    bus1->set_state(BusState::Null,0.0); //change from Idle to Null should remove bus from fleetState
    CC->disconnectVehicle(bus1); //should remove bus from Controlcenter

    delete bus1;
    delete pass1;
}

void TestFixedWithFlexible_walking::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));
}

void TestFixedWithFlexible_walking::testSaveResults()
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


void TestFixedWithFlexible_walking::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}

QTEST_APPLESS_MAIN(TestFixedWithFlexible_walking)

#include "test_fixedwithflexible_walking.moc"
