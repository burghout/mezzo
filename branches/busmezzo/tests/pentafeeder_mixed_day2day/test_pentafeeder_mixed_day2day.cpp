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
const std::string network_path = "../networks/PentaFeeder_mixed_d2d/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/PentaFeeder_mixed_d2d/ExpectedOutputs/";
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

class TestPentaFeeder_mixed_day2day : public QObject
{
    Q_OBJECT

public:
    TestPentaFeeder_mixed_day2day() = default;
    ~TestPentaFeeder_mixed_day2day() = default;

private slots:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testInitParameters(); //!< tests that the parameters for the loaded network are as expected
    void testValidRouteInput(); //!< tests if the routes that are read in are actually possible given the configuration of the network...
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


void TestPentaFeeder_mixed_day2day::testCreateNetwork()
{
    chdir(network_path.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestPentaFeeder_mixed_day2day::testInitNetwork()
{
    // remove old output files:
    for (const QString& filename : d2d_output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename); //remove old day2day convergence results
    }
    
    qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    
    qDebug() << "Initializing network in " + QString::fromStdString(network_path);
    
    ::fwf_wip::autogen_drt_lines_with_intermediate_stops = false;  //set manually (default false)
    ::fwf_wip::csgm_no_merging_rules = true; //set manually (default false)
    ::fwf_wip::csgm_no_filtering_dominancy_rules = true; //set manually (default false)        
    
    nt->init();

    // Test if the network is properly read and initialized
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
    
    // Mezzo parameters
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");

    // Demand parameters
    QVERIFY2(theParameters->empirical_demand == 0, "Failure, empirical demand not set to 0 in parameters");  
    vector<ODstops*> odstops_demand = net->get_odstops_demand();
    double total_demand = 0.0;
    for(const auto& od : odstops_demand)
    {
        total_demand += od->get_arrivalrate();
    }
    QVERIFY2(AproxEqual(total_demand,200.0), "Failure, total demand should be 200 pass/h");

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

void TestPentaFeeder_mixed_day2day::testInitParameters()
{   
    //BusMezzo parameters, drt without RTI
    QVERIFY2(theParameters->drt == true, "Failure, DRT is not set to true in parameters");
    QVERIFY2(theParameters->real_time_info == 0, "Failure, real time info is not set to 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 0.0), "Failure, share RTI network is not 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI, 0.0), "Failure, default alpha RTI is not 0 in parameters");
    
    //! CSGM @todo
    QVERIFY2(theParameters->choice_set_indicator == 0, "Failure, choice set indicator is not set to 1 in parameters");
    //QVERIFY2(net->count_transit_paths() == 27, "Failure, network should have 14 transit paths defined");

    //day2day params
    QVERIFY2(theParameters->pass_day_to_day_indicator == 1, "Failure, waiting time day2day indicator is not activated");
    QVERIFY2(theParameters->in_vehicle_d2d_indicator == 1, "Failure, IVT day2day indicator is not activade");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI,0.0), "Faliure, default credibility coefficient for day2day RTI is not set to 0.0");
    QVERIFY2(AproxEqual(theParameters->break_criterium,0.1),"Failure, break criterium for day2day is not set to 0.1");
    QVERIFY2(theParameters->max_days == 20, "Failure, max days for day2day is not set to 20.");
}

void TestPentaFeeder_mixed_day2day::testValidRouteInput()
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

void TestPentaFeeder_mixed_day2day::testCasePathSet()
{
    /**
        Checks for the combination of autogenerated direct DRT lines and passenger choice sets match the desired case study

        PentaFeeder with unidirectional demand towards feeder stop (5)
            - There should be no paths generated with transfers, there is no incentive for any passenger to alight once boarding a trip with final destination the feeder stop
            - There should be at least one fixed only path and one drt only path for each OD with demand
    */
    
    vector<ODstops*> ods = net->get_odstops_demand();
    for(auto od : ods)
    {
        int ostop = od->get_origin()->get_id();
        int dstop = od->get_destination()->get_id();
        qDebug() << "Checking path set transfers for OD: ("<<ostop<<","<<dstop<<")";
        QVERIFY(dstop == 5); // only unidirectional demand towards stop 5

        vector<Pass_path*> pathset = od->get_path_set();
        QVERIFY2(!pathset.empty(), "Failure, there should be at least one path per OD"); // there should always be a path for each OD with non-zero demand rate for this network
        QVERIFY2(pathset.size() == 2, "Failure, there should only be two paths per OD with demand");

        for(auto path : pathset)
        {
            vector<vector<Busline*> > alt_lines = path->get_alt_lines();
            // Fixed and Flexible lines are always considered distinct from one another
            for(auto alt_line_set : alt_lines) // alt_lines should just be size 1 but we're looping through anyways because why not I may want to change this later
            {
                QVERIFY(path->check_all_fixed_lines(alt_line_set) || path->check_all_flexible_lines(alt_line_set)); // either the each alt line set is completely fixed or completely flex
            }
            
            vector<vector<Busstop*> > alt_stops = path->get_alt_transfer_stops();

            size_t m = alt_lines.size();
            int n_transfers = path->get_number_of_transfers();

            // test num transfers and number of transit legs
            QVERIFY(m == 1);
            QVERIFY(n_transfers == 0);

            //!< check that transit links match case description
            Busline* transit_link = alt_lines.front().front();
            QVERIFY(transit_link->stops.back()->get_id() == dstop); // all stops end at dstop

        } //for paths in od

    } // for od in ods
}


void TestPentaFeeder_mixed_day2day::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    //QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 10800.1 after running the simulation";
    //QVERIFY2 (AproxEqual(net->get_currenttime(),10800.1), qPrintable(msg));
    qDebug() << "Final day: " << net->day;
    //QVERIFY(net->day == theParameters->max_days); // current setup does not converge within 20 days
}

void TestPentaFeeder_mixed_day2day::testPassAssignment_day2day()
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

void TestPentaFeeder_mixed_day2day::testPassAssignment_final()
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

void TestPentaFeeder_mixed_day2day::testSaveResults()
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

void TestPentaFeeder_mixed_day2day::testFinalDay2dayOutput()
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


void TestPentaFeeder_mixed_day2day::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestPentaFeeder_mixed_day2day)

#include "test_pentafeeder_mixed_day2day.moc"
