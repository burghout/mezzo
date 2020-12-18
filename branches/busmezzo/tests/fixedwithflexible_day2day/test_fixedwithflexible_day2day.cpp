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
const QString d2d_convergence_filename = "o_convergence.dat";

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
    void testPassengerStart(); //!< tests for when a traveler first enters the simulation and makes a connection, transitmode and dropoff decision, sends a request if mode is flexible, changes 'chosen mode' state of traveler...etc
    void testRunNetwork();
    void testPassAssignment(); //!< tests of resulting mode split from only one day
    void testSaveResults();
    void testConvergence(); //!< tests of day2day convergence
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
    qDebug() << "Removing file " + d2d_convergence_filename + ": " << QFile::remove(d2d_convergence_filename); //remove old day2day convergence results

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


    // Mezzo parameters
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    QVERIFY2(odstops_demand.size() == 7, "Failure, network should have 7 od stop pairs (non-zero or defined in transit_demand) ");

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
    all_pass = net->get_all_generated_passengers();
    qDebug() << all_pass.size() << " passengers generated.";
    QVERIFY(all_pass.size() == 6); //should always get 6 generated passengers

}

void TestFixedWithFlexible_day2day::testInitParameters()
{
    //BusMezzo parameters
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 3, "Failure, real time info is not set to 3 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 1.0), "Failure, share RTI network is not 1 in parameters");
    QVERIFY2(theParameters->choice_set_indicator == 1, "Failure, choice set indicator is not set to 1 in parameters");
    QVERIFY2(net->count_transit_paths() == 27, "Failure, network should have 14 transit paths defined");

    //day2day params
    QVERIFY2(theParameters->pass_day_to_day_indicator == 1, "Failure, waiting time day2day indicator is not activated");
    QVERIFY2(theParameters->in_vehicle_d2d_indicator == 1, "Failure, IVT day2day indicator is not activade");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI,0.5), "Faliure, initial credibility coefficient for day2day RTI is not set to 0.5");
    QVERIFY2(AproxEqual(theParameters->break_criterium,0.1),"Failure, break criterium for day2day is not set to 0.1");
    QVERIFY2(theParameters->max_days == 20, "Failure, max days for day2day is not set to 20.");
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

void TestFixedWithFlexible_day2day::testPassAssignment()
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

void TestFixedWithFlexible_day2day::testConvergence()
{

        QString o_filename = d2d_convergence_filename;
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


void TestFixedWithFlexible_day2day::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}



QTEST_APPLESS_MAIN(TestFixedWithFlexible_day2day)

#include "test_fixedwithflexible_day2day.moc"
