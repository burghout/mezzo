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
//! Contains tests for testing fixed with flexible choice model implementation


const std::string network_path = "../networks/FWF_testnetwork2/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/FWF_testnetwork2/ExpectedOutputs/";
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

const vector<QString> skip_output_filenames =
{
//    "o_od_stop_summary_without_paths.dat",
//    "o_od_stops_summary.dat"
}; //!< Files skipped in testSaveResults. @todo Get different results for certain files on identical runs, not sure if differences are significant enough to dig into at the moment

const long int seed = 42;

class TestFixedWithFlexible : public QObject
{
    Q_OBJECT

public:
    TestFixedWithFlexible(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testPassPathSetDefinitions(); //!< tests if the auto-generated passenger path sets conform to 'valid' path set definitions for each OD stop pair with positive demand rate
    //void testPassengerStates(); //!< test that passengers are being initialized properly,
    void testRunNetwork();
    void testPassAssignment(); //!< tests path-set-generation + resulting assignment
    void testSaveResults();
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestFixedWithFlexible::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestFixedWithFlexible::testInitNetwork()
{
    //qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);

    ::fwf_wip::autogen_drt_lines_with_intermediate_stops = false;  //set manually (default false)
    ::fwf_wip::csgm_no_merging_rules = false; //set manually (default false)
    ::fwf_wip::csgm_no_filtering_dominancy_rules = false; //set manually (default false)
    
    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 28, "Failure, network should have 28 links ");

    //count number of 'dummy links' to see that these are not being mislabeled
    //qDebug("Counting dummy links...")
    int count = 0;
    auto links = net->get_links();
    for(const auto& link : links)
    {
        if(link.second->is_dummylink())
            count++;
    }
    QVERIFY2(count == 20, "Failure network should have 20 dummy links ");

    QVERIFY2(net->get_nodes().size() == 16, "Failure, network should have 16 nodes ");
    QVERIFY2(net->get_destinations().size() == 4, "Failure, network should have 4 destination nodes ");
    QVERIFY2(net->get_origins().size() == 4, "Failure, network should have 4 origin nodes ");
    QVERIFY2(net->get_odpairs().size() == 16, "Failure, network should have 12 od pairs ");
    QVERIFY2(net->get_stopsmap().size() == 8, "Failure, network should have 8 stops defined ");

    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    QVERIFY2(odstops_demand.size() == 1, "Failure, network should have 1 od stop pairs (defined in transit_demand) ");

    //Check OD stop demand rate between stop 1 and 4
    ODstops* stop_1to4 = net->get_ODstop_from_odstops_demand(1,4);
    QVERIFY2(AproxEqual(stop_1to4->get_arrivalrate(),300.0),"Failure, ODstops stop 1 to stop 4 should have 300 arrival rate");
    QVERIFY2(stop_1to4 != nullptr,"Failure, OD stop 1 to 4 is undefined ");

    //Parameters
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 3, "Failure, real time info is not set to 3 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 1.0), "Failure, share RTI network is not 1 in parameters");
    QVERIFY2(theParameters->choice_set_indicator == 1, "Failure, choice set indicator is not set to 1 in parameters");
    QVERIFY2(net->count_transit_paths() == 14, "Failure, network should have 14 transit paths defined");

    //try setting real_time info param from here
//    theParameters->real_time_info = 0;
//    theParameters->share_RTI_network = 0.0;

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

void TestFixedWithFlexible::testPassPathSetDefinitions()
{
    /**
       Want to check the validity of OD stops 1->4 path sets for FWF base network. 

@note 
       1. Min number of transfers is 0 (drt 2 from s1 to s4)
       2. Max number of transfers is 2 (fix or drt 1 -> fix2 -> fix or drt 3)
       3. Bidirectional transit links are defined but we have unidirectional demand
       4. No walking links, so the connection decision will always be to stay at the same stop, i.e. all pairs of stops should be the same
            The alt_line structure is fixed
       5. 14 paths total in 1->4 direction

Assertions:
       1. Check that the stop sequence of a path also matches stops on the lines of each path (if the line is direct, and the only line then this means the start and end stops of that leg)
       2. There should really be only one line in each 'alt_lines' set i.e. line leg set for this network (no opportunistic boarding)
       3. Check that each the paths of each actually follows the correct structure (e.g. branch to corridor should have 6 alt_stops and must include transfer stop
       4. All pairs of stops should be the same (no walking links)
       5. Each time a line leg
       
    */


    vector<ODstops*> ods = net->get_odstops_demand();
    for(auto od : ods)
    {
        int ostop = od->get_origin()->get_id();
        int dstop = od->get_destination()->get_id();
        qDebug() << "\tChecking paths for OD stop (" << ostop << "," << dstop << ")";  
        vector<Pass_path*> pathset = od->get_path_set();
        if(od->get_arrivalrate() > 0)
            QVERIFY(!pathset.empty()); // there should always be a path for each OD with non-zero demand rate for this network

        for(auto path : pathset)
        {
            qDebug() << "\tChecking validity of path: " << path->get_id();
            vector<vector<Busline*> > alt_lines = path->get_alt_lines();
            vector<vector<Busstop*> > alt_stops = path->get_alt_transfer_stops();

            //!< General conditions that should hold for any path
            // m = number of alt line sets in a path alternative
            // 2m+2 = number of transit stop sets including OD
            // m+1 = number of connection (walking) links
            // m - 1 = number of transfers
            size_t m = alt_lines.size();
            int n_transfers = path->get_number_of_transfers();
            QVERIFY(alt_stops.size() == 2*m + 2);
            QVERIFY(path->get_walking_distances().size() == m + 1);
            QVERIFY(n_transfers == static_cast<int>(m) - 1);

            QVERIFY(alt_stops.size() > 2); // this network should not have any walking only legs (since no walking links are defined, and we should not have OD demand from an origin to itself)
            QVERIFY(alt_stops.size() % 2 == 0); // alt stops sets always come in pairs (alight at stop+walk/stay at stop for departure/destination)

            vector<pair<Busstop*,Busstop*> > arriving_departing_stop_pairs; // pairs of stops on the path. First in each pair is the stop of arrival/origin, second is stop of departure/destination

            for(auto alt_stops_it = alt_stops.begin(); alt_stops_it != alt_stops.end(); advance(alt_stops_it,2)) // walk over pairs of alt stops
            {
                //qDebug() << "\t\tChecking alt_transfer_stops...";
                auto alt_stops_it2 = alt_stops_it+1;
                QVERIFY((*alt_stops_it).size() == 1); // only one stop in each alt stop vec for this network
                QVERIFY((*alt_stops_it2).size() == 1);

                Busstop* arr_stop = (*alt_stops_it).front();
                Busstop* dep_stop = (*alt_stops_it2).front();

                // every pair of alt stops starting from the first and second should be the same (since we do not have walking links, and we only have one stop in each alt_stop set)
                QVERIFY(arr_stop->get_id() == dep_stop->get_id());

                arriving_departing_stop_pairs.push_back(make_pair(arr_stop,dep_stop)); // store each pair of arrival and departure locations to compare to alt_lines
            }
            QVERIFY(arriving_departing_stop_pairs.size() == m + 1); // there should be as many pairs as walking links

        } //for paths in od
    } //for ods with demand

}

void TestFixedWithFlexible::testRunNetwork()
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

void TestFixedWithFlexible::testPassAssignment()
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
            
            if(mode_decisions.front().chosen_transitmode == TransitModeType::Flexible)
                list<Pass_dropoff_decision> dropoff_decisions = od->get_pass_dropoff_decisions(first_pass);          
            
            QVERIFY(connection_decisions.front().chosen_connection_stop == od->get_origin()->get_id()); // pass always stays (no walking links)
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
            
            // verify that at least one passenger per OD made it to their destination
            QString failmsg = "Failure, at least one passenger for ODstop (" + orig_s + "," + dest_s + ") with non-zero demand should have reached final destination.";
            QVERIFY2(first_pass->get_end_time() > 0, qPrintable(failmsg)); // replaced the od->get_nr_pass_completed() call with this since there is some less intuitive dependency between this and calc_pass_measures() after saving results
        }
    }
}

void TestFixedWithFlexible::testSaveResults()
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


void TestFixedWithFlexible::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestFixedWithFlexible)

#include "test_fixedwithflexible.moc"



