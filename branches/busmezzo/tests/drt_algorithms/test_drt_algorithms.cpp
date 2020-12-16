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
#include <QFileInfo>



//!< Pentagon feeder network with only DRT available
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
//    "o_od_stops_summary.dat",
//    "o_passenger_trajectory.dat",
//    "o_passenger_welfare_summary.dat",
//    "o_segments_line_loads.dat",
    "o_segments_trip_loads.dat",
    "o_selected_paths.dat",
    "o_transit_trajectory.dat",
    "o_transitline_sum.dat",
    "o_transitlog_out.dat",
    "o_transitstop_sum.dat",
    "o_trip_total_travel_time.dat",
}; //!< Files skipped in testSaveResults. @todo Get different results for certain files on identical runs, not sure if differences are significant enough to dig into at the moment

const long int seed = 42;


//! @brief Tests the algorithms used for DRT using the Pentafeeder DRT network for initial setups
//! Unit testing of DRT algorithms
class TestDRTAlgorithms : public QObject
{
    Q_OBJECT

public:
    TestDRTAlgorithms(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testAssignment();//!< test asssignment of passengers
    void testRunNetwork();
    void testSaveResults();
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestDRTAlgorithms::testCreateNetwork()
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

void TestDRTAlgorithms::testInitNetwork()
{
    //QFile::remove(path_set_generation_filename); //remove old passenger path sets
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);

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

    QVERIFY(theParameters->choice_set_indicator == 1); // Traveler choice set for this network is manually defined

    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == true, "Failure, generate direct routes of controlcenter is not set to 1");

    QVERIFY2(net->get_buslines().size() == 20, "Failure, network should have 20 bus lines defined");
    QVERIFY2(net->get_busvehicles().size() == 0, "Failure, network should have 0 scheduled vehicles");
    QVERIFY2(net->get_drtvehicles_init().size() == 4, "Failure, network should have 4 unassigned vehicles");
    QVERIFY2(get<0>(net->get_drtvehicles_init()[0])->get_capacity() == 25, "Failure, vehicles should have capacity 25");

    QVERIFY(theParameters->demand_format==3);
    vector<ODstops*> odstops_demand = net->get_odstops_demand(); //get all odstops with demand defined for them
    double total_demand = 0.0;
    for(const auto& od : odstops_demand)
    {
        total_demand += od->get_arrivalrate();
    }
    QVERIFY2(AproxEqual(total_demand,200.0), "Failure, total demand should be 200 pass/h");
}

void TestDRTAlgorithms::testAssignment()
{


    // Create a dummy scenario to get access to all steps
    auto stopA = net->get_busstop_from_name("A");
    auto stopC = net->get_busstop_from_name("C");
    //auto stopE = net->get_busstop_from_name("E");

    auto cc = stopA->get_CC();

    ODstops* OD_stop = new ODstops(stopA, stopC);
    Passenger* pass = new Passenger(99999, 0.0, OD_stop, nullptr);
//    pass->set_chosen_mode(TransitModeType::Flexible); // passenger is a flexible transit user
//    cc->connectPassenger(pass);

//    Request* req = pass->createRequest(stopA, stopC, 1, 1.0, 1.0);
//    pass->set_curr_request(req);
//    QVERIFY (req->state == RequestState::Null);
//    QVERIFY (req->assigned_trip == nullptr);
//    emit pass->sendRequest(req, 1.0);
//    QVERIFY (req->state == RequestState::Assigned);
//    QVERIFY (req->assigned_trip != nullptr);
//    qDebug() << " request assigned to trip " << req->assigned_trip->get_id();

//    cc->removeRequest(pass->get_id());
    // Clean up
//    delete pass;

    //modify runtime
//    auto original_runtime = net->get_runtime();
//    int teststep = 1;
//    net->step(teststep);

//    auto  closestToA = cc->getClosestVehicleToStop(stopA,0.0);
//    qDebug() << "Closest vehicle to stop A " << closestToA.first->get_bus_id()
//             << " traveltime " << closestToA.second;
//    auto  closestToC = cc->getClosestVehicleToStop(stopC,0.0);
//    qDebug() << "Closest vehicle to stop C " << closestToC.first->get_bus_id()
//             << " traveltime " << closestToC.second;
//    auto  closestToE = cc->getClosestVehicleToStop(stopE,0.0);
//    qDebug() << "Closest vehicle to stop E " << closestToE.first->get_bus_id()
//             << " traveltime " << closestToE.second;



//    net->set_runtime(original_runtime);

}

void TestDRTAlgorithms::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));
}

void TestDRTAlgorithms::testSaveResults()
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


void TestDRTAlgorithms::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestDRTAlgorithms)

#include "test_drt_algorithms.moc"



