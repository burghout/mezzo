#include <QString>
#include <QtTest/QtTest>
#include "network.h"
#ifdef Q_OS_WIN
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif
//#include <unistd.h>
#include <QFileInfo>

const std::string network_path = "../networks/DRTAlgorithms/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = ":/networks/DRTAlgorithms/ExpectedOutputs/";
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

const long int seed = 42;

//! @brief Unit testing of CSGM (Choice-set generation model)
class TestCSGM : public QObject
{
    Q_OBJECT

public:
    TestCSGM(){}

private Q_SLOTS:
    void testCreateNetwork(); //!< test loading the base network
    void testInitNetwork(); //!< test generating passenger path sets & initializing the network
    void testPathSetFilters(); //!< tests filtering methods for only allowing for specific path combinations (e.g. no mixed mode legs)
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestCSGM::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    qDebug() << " Randseed is " << randseed;
    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestCSGM::testInitNetwork()
{
    qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);

    ::fwf_wip::autogen_drt_lines_with_intermediate_stops = false;  //set manually (default false)
    ::fwf_wip::csgm_no_merging_or_filtering_paths = false; //set manually (default false)
    
    nt->init();

    QVERIFY2(net->get_links().size() == 46, "Failure, network should have 46 links ");
    QVERIFY2(net->get_nodes().size() == 24, "Failure, network should have 24 nodes ");
    QVERIFY2(net->get_odpairs().size() == 36, "Failure, network should have 36 od pairs ");
    QVERIFY(net->get_stopsmap().size() == 5);
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");
    QVERIFY2 (net->get_busstop_from_name("E")->get_id() == 5, "Failure, bus stop E should be id 5 ");

    QVERIFY2 (theParameters->drt == true, "Failure, DRT not activated in parameters for PentaFeeder_drt");
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == true, "Failure, generate direct routes of controlcenter is not set to 1");

    QVERIFY2(net->get_buslines().size() == 20, "Failure, network should have 20 bus lines defined");
    QVERIFY2(net->get_busvehicles().size() == 0, "Failure, network should have 0 scheduled vehicles");
    QVERIFY2(net->get_drtvehicles_init().size() == 2, "Failure, network should have 1 unassigned vehicles");
    QVERIFY2(get<0>(net->get_drtvehicles_init()[0])->get_capacity() == 25, "Failure, vehicles should have capacity 25");

    QVERIFY(theParameters->demand_format==3);
    //Test reading of empirical passenger arrivals
    QVERIFY2(theParameters->empirical_demand == 1, "Failure, empirical demand not set to 1 in parameters");
    vector<pair<ODstops*, double> > empirical_passenger_arrivals = net->get_empirical_passenger_arrivals();
    
    QVERIFY(theParameters->choice_set_indicator == 0); // Traveler choice set for this network is generated
    
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

ODpair* get_od_from_stops(Network* net, Busstop* o_stop, Busstop* d_stop)
{
    ODpair* od = nullptr;
    Origin* o = net->findNearestOriginToStop(o_stop);
    Destination* d = net->findNearestDestinationToStop(d_stop);
    
    auto od_it = find_if(net->get_odpairs().begin(), net->get_odpairs().end(),[o,d](ODpair* od){return (od->get_origin() == o) && (od->get_destination() == d);});
    if(od_it != net->get_odpairs().end())
        od = (*od_it);
    
    return od;
}
void TestCSGM::testPathSetFilters()
{
    /**
        Tests: Pass_path::get_unmixed_paths(const vector<vector<Busline*>>& original_path)
               Pass_path::has_no_connected_flexible_legs() const
        
          - Start with a path with only mixed transit legs
          - DRT/FIX DRT/FIX DRT/FIX
          - Should result in 2^3 = 8 separate paths with unmixed legs
                DRT - DRT - DRT x
                DRT - DRT - FIX x
                DRT - FIX - FIX
                FIX - FIX - FIX
                FIX - FIX - DRT
                FIX - DRT - DRT x
                FIX - DRT - FIX
                DRT - FIX - DRT
          - No connected DRT legs are allowed, after filtering for this 5 paths should remain
    */    
    
    // Create mixed path
    Busstop* s1 = net->get_busstop_from_name("A");
    Busstop* s2 = net->get_busstop_from_name("B");
    Busstop* s3 = net->get_busstop_from_name("C");
    Busstop* s4 = net->get_busstop_from_name("D");
    Busstop* s5 = net->get_busstop_from_name("E");
    vector<Busstop*> stops1 = {s1,s2};
    ODpair* od1 = get_od_from_stops(net,s1,s2);
    vector<Busstop*> stops2 = {s2,s3};
    ODpair* od2 = get_od_from_stops(net,s2,s3);
    vector<Busstop*> stops3 = {s3,s4,s5};
    ODpair* od3 = get_od_from_stops(net,s3,s5);
    double t_now = 0.0; //dummy sim time
    
    //get route via stops, doesnt matter if the o_node or d_node isnt the closest to the stop
    Busroute* route1 = net->create_busroute_from_stops(1, net->findNearestOriginToStop(s1), net->findNearestDestinationToStop(s2), stops1, t_now); 
    QVERIFY(route1);
    Busroute* route2 = net->create_busroute_from_stops(2, net->findNearestOriginToStop(s2), net->findNearestDestinationToStop(s3), stops2, t_now); 
    QVERIFY(route2);
    Busroute* route3 = net->create_busroute_from_stops(3, net->findNearestOriginToStop(s3), net->findNearestDestinationToStop(s5), stops3, t_now); 
    QVERIFY(route3);
    
    //Create a fixed and a flex lines with a multileg trip between stop A -> B
    Busline* fix_line1 = new Busline(1,21,"FIX1",route1,stops1,nullptr,od1,0,0,0,0,false);
    fix_line1->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route1,stops1));
    Busline* drt_line1 = new Busline(10001,888,"DRT1",route1,stops1,nullptr,od1,0,0,0,0,true);
    drt_line1->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route1,stops1));
    Busline* fix_line2 = new Busline(2,22,"FIX2",route2,stops2,nullptr,od2,0,0,0,0,false);
    fix_line2->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route2,stops2));
    Busline* drt_line2 = new Busline(10002,888,"DRT2",route2,stops2,nullptr,od2,0,0,0,0,true);
    drt_line2->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route2,stops2));
    Busline* fix_line3 = new Busline(3,23,"FIX3",route3,stops3,nullptr,od3,0,0,0,0,false);
    fix_line3->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route3,stops3));
    Busline* drt_line3 = new Busline(10003,888,"DRT3",route3,stops3,nullptr,od3,0,0,0,0,true);
    drt_line3->set_delta_at_stops(net->calc_interstop_freeflow_ivt(route3,stops3));
    
    //Construct a path (of transit legs, doesnt matter if they are connected or not, with mixed legs
    vector<Busline*> drt_leg = {drt_line1};
    vector<Busline*> leg1 = {fix_line1,drt_line1};
    vector<Busline*> leg2 = {fix_line2,drt_line2};
    vector<Busline*> leg3 = {fix_line3,drt_line3};
    vector<vector<Busline*>> mixed_path = {leg1,leg2,leg3};
    vector<vector<Busline*>> empty_path;
    vector<vector<Busline*>> single_leg_path = {leg1};
    vector<vector<Busline*>> unmixed_path = {drt_leg};
 
    //Now test filtering methods
    vector<vector<vector<Busline*>>> unmixed_paths;
    unmixed_paths = Pass_path::get_unmixed_paths(empty_path);
    QVERIFY(unmixed_paths.size() == 1);
    unmixed_paths = Pass_path::get_unmixed_paths(single_leg_path);
    QVERIFY(unmixed_paths.size() == 2);
    unmixed_paths = Pass_path::get_unmixed_paths(unmixed_path);
    QVERIFY(unmixed_paths.size() == 1);
    unmixed_paths = Pass_path::get_unmixed_paths(mixed_path);
    QVERIFY(unmixed_paths.size() == 8);
    
    Pass_path::remove_paths_with_connected_flexible_legs(unmixed_paths);
    QVERIFY(unmixed_paths.size() == 5);
    
    delete fix_line1;
    delete drt_line1;
    delete fix_line2;
    delete drt_line2;
    delete fix_line3;
    delete drt_line3;
}


void TestCSGM::testDelete()
{
	delete nt;
	QVERIFY2(true, "Failed to delete TestCSGM");
}

QTEST_APPLESS_MAIN(TestCSGM)

#include "test_csgm.moc"



