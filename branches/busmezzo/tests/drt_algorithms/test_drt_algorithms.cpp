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
    "o_transit_routes.dat",
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
    //    "o_segments_trip_loads.dat",
    //    "o_selected_paths.dat",
    //    "o_transit_trajectory.dat",
    //    "o_transitline_sum.dat",
    //    "o_transitlog_out.dat",
    //    "o_transitstop_sum.dat",
    //    "o_trip_total_travel_time.dat",
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
    void testSortedBustrips();
    void testAssignment();//!< test asssignment of passengers
    void testRunNetwork();
    void testPostRunAssignment(); //!< test assignment of passengers after running the network
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
    qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    
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

    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 1, "Failure, network should have 1 controlcenter");
    QVERIFY2(ccmap.begin()->second->getGeneratedDirectRoutes() == true, "Failure, generate direct routes of controlcenter is not set to 1");

    QVERIFY2(net->get_buslines().size() == 20, "Failure, network should have 20 bus lines defined");
    QVERIFY2(net->get_busvehicles().size() == 0, "Failure, network should have 0 scheduled vehicles");
    QVERIFY2(net->get_drtvehicles_init().size() == 1, "Failure, network should have 1 unassigned vehicles");
    QVERIFY2(get<0>(net->get_drtvehicles_init()[0])->get_capacity() == 25, "Failure, vehicles should have capacity 25");

    QVERIFY(theParameters->demand_format==3);
    //Test reading of empirical passenger arrivals
    QVERIFY2(theParameters->empirical_demand == 1, "Failure, empirical demand not set to 1 in parameters");
    vector<pair<ODstops*, double> > empirical_passenger_arrivals = net->get_empirical_passenger_arrivals();
    
    QVERIFY2(empirical_passenger_arrivals.size() == 1, "Failure, there should be 1 empirical passenger arrivals");
    vector<ODstops*> odstops_demand = net->get_odstops_demand(); //get all odstops with demand defined for them
    QVERIFY(odstops_demand.size() == 1); // the one empirical arrival is the one OD with demand
    
//    double total_demand = 0.0;
//    for(const auto& od : odstops_demand)
//    {
//        total_demand += od->get_arrivalrate();
//    }
//    QVERIFY2(AproxEqual(total_demand,200.0), "Failure, total demand should be 200 pass/h");
    
    //!< Test if newly generated passenger path sets match expected output
    //QVERIFY(theParameters->choice_set_indicator == 1); // Traveler choice set for this network is manually defined
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




struct compareBustripByNrRequests
{
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_requests().size() != rhs->get_requests().size())
            return lhs->get_requests().size() > rhs->get_requests().size();
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
};

struct compareBustripByEarliestStarttime
{
    bool operator () (const Bustrip* lhs, const Bustrip* rhs) const
    {
        if (lhs->get_starttime() != rhs->get_starttime())
            return lhs->get_starttime() < rhs->get_starttime();
        else
            return lhs->get_id() < rhs->get_id(); // tiebreaker return trip with smallest id
    }
};

void TestDRTAlgorithms::testSortedBustrips()
{
    //compare by num scheduled requests
    auto busline = net->get_buslines().front();
    set<Bustrip*> original;
    Bustrip* t1 = new Bustrip(1,1.0,busline);
    Bustrip* t2 = new Bustrip(2,1.0,busline);
    auto rq1 = new Request();
    auto rq2 = new Request();
    t2->add_request(rq1);
    t2->add_request(rq2);
    original.insert(t1);
    original.insert(t2);
    set<Bustrip*,compareBustripByNrRequests> sorted (original.begin(), original.end());
    auto firstOrig = *original.begin();
    auto firstSorted = *sorted.begin();
    QVERIFY(firstOrig->get_id() == 1);
    QVERIFY(firstSorted->get_id() == 2);
    QVERIFY(t1->get_requests().size() < t2->get_requests().size()) ;
    
    //check equivalence relation, lowest id first as tiebreaker
    auto rq3 = new Request();
    auto rq4 = new Request();
    t1->add_request(rq3);
    t1->add_request(rq4);
    QVERIFY(t1->get_requests().size() == t2->get_requests().size());
    
    set<Bustrip*,compareBustripByNrRequests> sorted2 (original.begin(), original.end());
    auto firstSorted2 = *sorted2.begin();
    QVERIFY(firstSorted2->get_id() == 1);
    QVERIFY(original.size() == 2);
    QVERIFY(sorted2.size() == 2);
    
    //compare by starttime
    t1->set_starttime(200.0);
    t2->set_starttime(100.0);
    set<Bustrip*,compareBustripByEarliestStarttime> sortedTime (original.begin(), original.end());
    auto firstSortedTime = *sortedTime.begin();
    QVERIFY(firstSortedTime->get_id() == 2);
    
    //check equivalence relation, lowest id first as tiebreaker
    t1->set_starttime(t2->get_starttime()); 
    set<Bustrip*,compareBustripByEarliestStarttime> sortedTime2 (original.begin(), original.end());
    auto firstSortedTime2 = *sortedTime2.begin();
    QVERIFY(firstSortedTime2->get_id() == 1);
    QVERIFY(original.size() == 2);
    QVERIFY(sortedTime2.size() == 2);
    
    delete t1;
    delete t2;
    delete rq1;
    delete rq2;
    delete rq3;
    delete rq4;
}



void TestDRTAlgorithms::testAssignment()
{


    // Create a dummy scenario to get access to all steps
//    auto stopA = net->get_busstop_from_name("A");
//    auto stopC = net->get_busstop_from_name("C");
//    //auto stopE = net->get_busstop_from_name("E");

//    auto cc = stopA->get_CC();

//    ODstops* OD_stop = new ODstops(stopA, stopC);
//    Passenger* pass = new Passenger(99999, 0.0, OD_stop, nullptr);
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

void TestDRTAlgorithms::testPostRunAssignment()
{
    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    //QVERIFY2(odstops_demand.size() == 140, "Failure, network should have 140 od stop pairs (with non-zero demand defined in transit_demand.dat) ");

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
            qDebug() << "First passenger for OD " << "(" << orig_s << "," << dest_s << "):";
            qDebug() << "\t" << "finished trip    : " << (first_pass->get_end_time() > 0);
            qDebug() << "\t" << "start time       : " << first_pass->get_start_time();
            qDebug() << "\t" << "last stop visited: " << first_pass->get_chosen_path_stops().back().first->get_id();
            
            // collect the first set of decisions for the first passenger for each ODstop with demand
            list<Pass_connection_decision> connection_decisions = od->get_pass_connection_decisions(first_pass);
            list<Pass_transitmode_decision> mode_decisions = od->get_pass_transitmode_decisions(first_pass);
            list<Pass_dropoff_decision> dropoff_decisions = od->get_pass_dropoff_decisions(first_pass);
            QVERIFY(connection_decisions.size() == 1);
            QVERIFY(mode_decisions.size() == 1);
            QVERIFY(dropoff_decisions.size() == 1);
            
            QVERIFY(connection_decisions.front().chosen_connection_stop == od->get_origin()->get_id()); // pass always stays (no walking links)
            QVERIFY(mode_decisions.front().chosen_transitmode != TransitModeType::Null); // A choice of either fixed or flexible should have always been made
            QVERIFY(mode_decisions.front().chosen_transitmode == TransitModeType::Flexible); //! only flexible modes available
        
            if(first_pass->get_end_time() > 0) // passenger completed their trip
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
                    qDebug() << "\t\t request_state: " << Request::state_to_string(request->state);
                }
            }
            
            // verify that at least one passenger per OD made it to their destination
            QString failmsg = "Failure, at least one passenger for ODstop (" + orig_s + "," + dest_s + ") with non-zero demand should have reached final destination.";
            QVERIFY2(first_pass->get_end_time() > 0, qPrintable(failmsg)); // replaced the od->get_nr_pass_completed() call with this since there is some less intuitive dependency between this and calc_pass_measures() after saving results
        }
    }
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



