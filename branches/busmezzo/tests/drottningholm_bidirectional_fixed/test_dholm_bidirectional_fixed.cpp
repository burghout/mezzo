#include <QString>
#include <QtTest/QtTest>
#include "network.h"
#include "MMath.h"
#ifdef Q_OS_WIN
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif
//#include <unistd.h>
#include <QFileInfo>

/**
  Tests on the Drottningholm network with fixed services merging into a common corridor
*/


const std::string network_path_1 = "../networks/Drottningholm_bidirectional_fixed/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/Drottningholm_bidirectional_fixed/ExpectedOutputs/";
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
    "o_fwf_summary_odcategory.dat"
};

const long int seed = 42;


const vector<int> branch_ids_176 = { 217619,217618,217617,217616,217615,217614,217613,217612,217611,217610,217609,217608,217607,217606,217605,217603,217602,217604,217600,277024,
                                      77024, 17600, 17601, 17602, 17604, 17605, 17606, 17607, 17608, 17609, 17610, 17611, 17612, 17613, 17614, 17615, 17616, 17617, 17618, 17619
                                   };
const vector<int> branch_ids_177 = { 277036,277035,277034,277033,277032,277031,277030,277029,277028,277027,277026,277025,277024,
                                      77024, 77025, 77026, 77027, 77028, 77029, 77030, 77031, 77032, 77033, 77034, 77035, 77036
                                   };
const vector<int> corridor_ids = { 277024,277023,277022,277021,277020,277019,277018,277017,277016,277015,277014,277013,277012,277011,277010,277009,277008,277007,277006,277005,277004,277003,277002,277001,
                                    77001, 77002, 77003, 77004,	77005, 77006, 77007, 77008, 77009, 77010, 77011, 77012, 77013, 77014, 77015, 77016, 77017, 77018, 77019, 77020, 77021, 77022, 77023, 77024
                                 };
const vector<int> transfer_stop_ids = { 277024, 77024 };

bool is_on_branch176(int stop_id)
{
    return find(branch_ids_176.begin(),branch_ids_176.end(),stop_id) != branch_ids_176.end();
}
bool is_on_branch177(int stop_id)
{
    return find(branch_ids_177.begin(),branch_ids_177.end(),stop_id) != branch_ids_177.end();
}
bool is_on_branch(int stop_id) // is on either branch
{
    return is_on_branch176(stop_id) || is_on_branch177(stop_id);
}
bool is_on_corridor(int stop_id)
{
    return find(corridor_ids.begin(),corridor_ids.end(),stop_id) != corridor_ids.end();
}
bool is_transfer_stop(int stop_id)
{
    return find(transfer_stop_ids.begin(),transfer_stop_ids.end(),stop_id) != transfer_stop_ids.end();
}
bool is_branch_to_branch(int ostop_id, int dstop_id) // if OD pair is branch to branch
{
    if(is_on_branch(ostop_id)) // origin of trip starts on a branch
    {
        if(is_transfer_stop(dstop_id)) // destination is the transfer stop (which is on both branch and corridor)
            return true;
        else
            return !is_on_corridor(dstop_id); // destination is NOT on corridor
    }
    return false;
}
bool is_branch_to_corridor(int ostop_id, int dstop_id)
{
    if(!is_transfer_stop(ostop_id) && is_on_branch(ostop_id)) // origin is not the transfer stop (which is on corridor) and is on branch
    {
        if(!is_transfer_stop(dstop_id) && is_on_corridor(dstop_id)) // destination IS on corridor and is not the transfer stop
            return true;
    }
    return false;
}
bool is_corridor_to_branch(int ostop_id, int dstop_id)
{
    return is_on_corridor(ostop_id) && !is_on_corridor(dstop_id); // origin is on corridor & destination is not on corridor (i.e. is on a branch but is not a transfer stop)
}
bool is_corridor_to_corridor(int ostop_id, int dstop_id)
{
    return is_on_corridor(ostop_id) && is_on_corridor(dstop_id);
}

enum class ODCategory { Null = 0, b2b, b2c, c2c, c2b };

ODCategory get_od_category(int o, int d)
{
    ODCategory category = ODCategory::Null;

    if (is_branch_to_branch(o, d))
        category = ODCategory::b2b;
    if (is_branch_to_corridor(o, d))
        category = ODCategory::b2c;
    if (is_corridor_to_corridor(o, d))
        category = ODCategory::c2c;
    if(is_corridor_to_branch(o,d))
        category = ODCategory::c2b;

    return category;
}



class TestDrottningholmBidirectional_fixed : public QObject
{
    Q_OBJECT

public:
    TestDrottningholmBidirectional_fixed(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testInitParameters(); //!< tests that the parameters for the loaded network are as expected
    void testValidRouteInput(); //!< tests if the routes that are read in are actually possible given the configuration of the network...
    void testRunNetwork(); //!< test running the network
    void testPassAssignment(); //!< tests path-set-generation + resulting assignment
    void testSaveResults(); //!< tests saving results
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestDrottningholmBidirectional_fixed::testCreateNetwork()
{
    chdir(network_path_1.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestDrottningholmBidirectional_fixed::testInitNetwork()
{
    qDebug() << "Initializing network in " + QString::fromStdString(network_path_1);

    ::fwf_wip::autogen_drt_lines_with_intermediate_stops = false;  //set manually (default false)
    //::fwf_wip::csgm_no_merging_rules = true; //set manually (default false)
    //::fwf_wip::csgm_no_filtering_dominancy_rules = true; //set manually (default false)
    
    ::PARTC::drottningholm_case = true;
    
    nt->init();

    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");
    
    map<int,Controlcenter*> ccmap = net->get_controlcenters();
    QVERIFY2(ccmap.size() == 0, "Failure, network should have 0 controlcenter");
}
void TestDrottningholmBidirectional_fixed::testInitParameters()
{
    // Manually set global test flags
    ::fwf_wip::write_all_pass_experiences = false; //set manually (default false)
    ::fwf_wip::randomize_pass_arrivals = true; //set manually (default true)
    ::fwf_wip::day2day_no_convergence_criterium = false; //set manually (default false)
    ::fwf_wip::zero_pk_fixed = true; //set manually (default false)
    
    //BusMezzo parameters, drt without RTI
    QVERIFY2(theParameters->drt == false, "Failure, DRT is not set to false in parameters");
    QVERIFY2(theParameters->real_time_info == 0, "Failure, real time info is not set to 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->share_RTI_network, 0.0), "Failure, share RTI network is not 0 in parameters");
    QVERIFY2(AproxEqual(theParameters->default_alpha_RTI, 0.0), "Failure, default alpha RTI is not 0 in parameters");
    
    //! CSGM @todo
    QVERIFY2(theParameters->choice_set_indicator == 1, "Failure, choice set indicator is not set to 0 in parameters");
    QVERIFY2(net->count_transit_paths() == 3618, "Failure, network should have 3618 transit paths defined");

    //day2day params
    QVERIFY2(theParameters->pass_day_to_day_indicator == 0, "Failure, waiting time day2day indicator is not activated");
    QVERIFY2(theParameters->in_vehicle_d2d_indicator == 0, "Failure, IVT day2day indicator is not activade");
    
    //demand params
    QVERIFY(theParameters->empirical_demand == true);
    vector<pair<ODstops*, double> > empirical_passenger_arrivals = net->get_empirical_passenger_arrivals();
    QVERIFY2(empirical_passenger_arrivals.size() == 255, "Failure, there should be 255 empirical passenger arrivals");
    
    
    // Passenger cost parameters
    QVERIFY(AproxEqual(theParameters->transfer_coefficient,-0.334));
    QVERIFY(AproxEqual(theParameters->in_vehicle_time_coefficient, -0.04));
    QVERIFY(AproxEqual(theParameters->waiting_time_coefficient, -0.07));
    QVERIFY(AproxEqual(theParameters->walking_time_coefficient, -0.07));
    QVERIFY(AproxEqual(theParameters->average_walking_speed, 66.66));
}

void TestDrottningholmBidirectional_fixed::testValidRouteInput()
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

void TestDrottningholmBidirectional_fixed::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    //QString msg = "Failure current time " + QString::number(net->get_currenttime()) + " should be 21600.1 after running the simulation";
    //qDebug() << "End of simulation time: " << net->get_currenttime() << Qt::endl;
    //QVERIFY2(AproxEqual(net->get_currenttime(),21600.1), qPrintable(msg));
}

void TestDrottningholmBidirectional_fixed::testPassAssignment()
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
    //QVERIFY2(odstops_demand.size() == 255, "Failure, network should have 255 od stop pairs (with non-zero demand defined in transit_demand.dat) ");

    for(auto od : odstops_demand)
    {
        
        int ostop = od->get_origin()->get_id();
        int dstop = od->get_destination()->get_id();
        ODCategory od_category = get_od_category(ostop,dstop);
        QVERIFY(od_category != ODCategory::Null); // each od should have a category
        
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
            QVERIFY(n_transfers == 0); // no paths with transfers should be defined for this case
            
            // collect the first set of decisions for the first passenger for each ODstop with demand
            list<Pass_connection_decision> connection_decisions = od->get_pass_connection_decisions(first_pass);
            QVERIFY(connection_decisions.front().chosen_connection_stop == od->get_origin()->get_id()); // pass always stays (no walking links)
            
            QVERIFY(first_pass->get_chosen_mode() == TransitModeType::Null); // with drt parameter set to false no mode choice is ever made (chosen_mode_ stays at default value Null)
            //QVERIFY(first_pass->get_curr_request() == nullptr); // a request should never have been generated if transit mode choice is fixed
            //if(finished_trip)
            //    QVERIFY(n_transfers == 0); // a total of 1 vehicle should have been used to reach final dest. No transfers are necessary
            
            // verify that at least one passenger per OD made it to their destination
            QString failmsg = "Failure, at least one passenger for ODstop (" + orig_s + "," + dest_s + ") with non-zero demand should have reached final destination.";
            QVERIFY2(finished_trip, qPrintable(failmsg)); // replaced the od->get_nr_pass_completed() call with this since there is some less intuitive dependency between this and calc_pass_measures() after saving results
        }
    }
}

void TestDrottningholmBidirectional_fixed::testSaveResults()
{
    // remove old files:
    for (const QString& filename : output_filenames)
    {
        qDebug() << "Removing file " + filename + ": " << QFile::remove(filename);
    }

    // save results:
    nt->saveresults();

    //test if output files match the expected output files
    for (const QString& o_filename : output_filenames)
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

void TestDrottningholmBidirectional_fixed::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestDrottningholmBidirectional_fixed)

#include "test_dholm_bidirectional_fixed.moc"



