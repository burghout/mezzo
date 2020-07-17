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
    //void testPassengerStates(); //!< test that passengers are being initialized properly,
    void testPathSetUtilities(); //!< test navigating generated path-sets, calculating utilities under different circumstances (e.g. access to RTI for different legs)
    void testRunNetwork();
    void testSaveResults();
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread
    Network* net;
};

void TestFixedWithFlexible_walking::testCreateNetwork()
{
    nt = nullptr;
    net = nullptr;
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
    //qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);

    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 34, "Failure, network should have 34 links ");
    QVERIFY2(net->get_nodes().size() == 20, "Failure, network should have 20 nodes ");
    QVERIFY2(net->get_destinations().size() == 5, "Failure, network should have 5 destination nodes ");
    QVERIFY2(net->get_origins().size() == 5, "Failure, network should have 5 origin nodes ");
    QVERIFY2(net->get_odpairs().size() == 14, "Failure, network should have 14 od pairs ");
    QVERIFY2(net->get_stopsmap().size() == 10, "Failure, network should have 10 stops defined ");

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

    //Control center
    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == false, "Failure, generate direct routes of controlcenter is not set to false");

    //Test if newly generated passenger path sets match expected output
//    qDebug() << "Comparing " + path_set_generation_filename + " with ExpectedOutputs/" + path_set_generation_filename;
//    QString ex_path_set_fullpath = expected_outputs_path + path_set_generation_filename;
//    QFile ex_path_set_file(ex_path_set_fullpath); //expected o_path_set_generation.dat
//    QVERIFY2(ex_path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open ExpectedOutputs/o_path_set_generation.dat");

//    QFile path_set_file(path_set_generation_filename); //generated o_path_set_generation.dat
//    QVERIFY2(path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open o_path_set_generation.dat");

//    QVERIFY2(path_set_file.readAll() == ex_path_set_file.readAll(), "Failure, o_path_set_generation.dat differs from ExpectedOutputs/o_path_set_generation.dat");

//    ex_path_set_file.close();
//    path_set_file.close();
}

void TestFixedWithFlexible_walking::testPathSetUtilities()
{
    /* Test for paths between stop 1 to 4 */
    qDebug() << "Checking path-set for ODstops 1 to 4";
    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    ODstops* stop1to4 = nullptr;
    auto stop1to4_it = find_if(odstops_demand.begin(),odstops_demand.end(),[](ODstops* odstop)
    {
        return (odstop->get_origin()->get_id() == 1 && odstop->get_destination()->get_id() == 4);
    });
    if(stop1to4_it != odstops_demand.end())
        stop1to4 = *stop1to4_it;
    QVERIFY2(stop1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined");

    //create a passenger to with ODstop 1 to 4 and navigate through paths from the travelers perspective
    Passenger* pass1 = new Passenger(1,0,stop1to4,nullptr); //Passenger(id,start_time,ODstop*,QObject* parent)
    Busstop* bs_o = pass1->get_OD_stop()->get_origin();
    Busstop* bs_d = pass1->get_OD_stop()->get_destination();
    vector<Pass_path*> path_set = bs_o->get_stop_od_as_origin_per_stop(bs_d)->get_path_set(); //no idea why we are accessing the path-set in this roundabout way but there is hopefully a good reason for it

    QVERIFY2(stop1to4->check_path_set(), "Failure, no paths defined for stop 1 to 4");
    qDebug() << "Number of paths available to traveler: " << stop1to4->get_path_set().size();
    QVERIFY2(stop1to4->get_path_set().size() == 7, "Failure, there should be 7 paths defined for stop 1 to 4");
    QVERIFY2(path_set.size() == 7, "Failure, traveler with ODstops 1 to 4 should have 7 paths available to them");

    /*Test for building path (sub)sets in connection decision choice tree */
    //Busstop* candidate_connection_stops = nullptr;
    // make_connection_decision //stop to walk to
    // make_mode_decision //given available paths from chosen stop,

    for(auto path : path_set) // check so that all flexible transit legs
    {
        path->first_transit_leg_flexible();
    }

    /* TEST PASSENGER INIT */
    //Test initialization of passenger with RTI at a network level
    QVERIFY2(pass1->get_pass_RTI_network_level() == false, "Failure, default initilization of passenger network-level RTI should be false");
    pass1->init(); //note this both sets the network level RTI for the traveler as well as the anticipated WT and IVT of the traveler if day2day is active
    QVERIFY2(pass1->get_pass_RTI_network_level() == true,"Failure, passenger should have network-level RTI after init with real_time_info=3 and share_RTI_network=1 in parameters");
    QVERIFY2(pass1->get_access_to_flexible() == true, "Failure, passenger with network-level RTI should have access to flexible services");

    //Test creation of passenger with no RTI (should be default)
    Passenger* pass2 = new Passenger(2,0,stop1to4,nullptr);
    QVERIFY(pass2->get_pass_RTI_network_level() == false);
    QVERIFY2(pass2->get_access_to_flexible() == false, "Failure, passenger without network-level RTI should not have access to flexible services");

    //Connection decision for passenger with RTI
    Busstop* connection_stop = nullptr;
    connection_stop = pass1->make_connection_decision(0.0); //make connection decision with starttime = 0.0

    //Connection decision for passenger without RTI

    delete pass1;
    delete pass2;
}

void TestFixedWithFlexible_walking::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));

    // Example: way to check typical value for e.g. number of last departures from stop A:
   // qDebug() << net->get_busstop_from_name("A")->get_last_departures().size();
    // and here you turn it into a test
    //QVERIFY2 ( net->get_busstop_from_name("A")->get_last_departures().size() == 2, "Failure, get_last_departures().size() for stop A should be 2");
}

void TestFixedWithFlexible_walking::testSaveResults()
{
    // remove old files:
    for (const QString& filename : output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename);
    }

    // save results:
    nt->saveresults();
     // test here the properties that should be true after saving the results

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
