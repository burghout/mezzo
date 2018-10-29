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

//! DRT Tests BusMezzo
//! Contains tests of parts of the DRT framework


const std::string network_path = "../networks/SFnetwork/";
const std::string network_name = "masterfile.mezzo";

const QString expected_outputs_path = "://networks/SFnetwork/ExpectedOutputs/";
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
};

const long int seed = 42;

class TestDRT : public QObject
{
    Q_OBJECT

public:
    TestDRT(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testCreateBusroute(); //!< tests creation of busroutes from stop pairs
    void testCreateAllDRTLines(); //!< tests creation of buslines
    void testRunNetwork(); //!< test running the network
//   void testSaveResults(); //!< tests saving results
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread
    Network* net;
};

void TestDRT::testCreateNetwork()
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

void TestDRT::testInitNetwork()
{
	qDebug() << QFile::remove(path_set_generation_filename); //remove old passenger path sets

    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 15, "Failure, network should have 15 links ");
    QVERIFY2(net->get_nodes().size() == 13, "Failure, network should have 13 nodes ");
    QVERIFY2(net->get_odpairs().size() == 4, "Failure, network should have 4 nodes ");
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");

    QVERIFY2 (net->get_currenttime() == 0, "Failure, currenttime should be 0 at start of simulation");


	//Test if newly generated passenger path sets match expected output
	QString ex_path_set_fullpath = expected_outputs_path + path_set_generation_filename;
	QFile ex_path_set_file(ex_path_set_fullpath); //expected o_path_set_generation.dat
	QVERIFY2(ex_path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open ExpectedOutputs/o_path_set_generation.dat");
	
	QFile path_set_file(path_set_generation_filename); //generated o_path_set_generation.dat
	QVERIFY2(path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open o_path_set_generation.dat");

	QVERIFY2(path_set_file.readAll() == ex_path_set_file.readAll(), "Failure, o_path_set_generation.dat differs from ExpectedOutputs/o_path_set_generation.dat");

	ex_path_set_file.close();
	path_set_file.close();
}

void TestDRT::testCreateBusroute()
{
    // Test correct route
    ODpair* od_pair = net->get_odpairs().front(); // path from Origin 1 to Destination 4
    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopD = net->get_busstop_from_name("D"); // on link 34
    vector <Busstop*> stops;

    Busroute* emptyStopsRoute = net->create_busroute_from_stops(1, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(emptyStopsRoute == nullptr);

    stops.push_back(stopA);
    stops.push_back(stopD);
    Busroute* routeViaAandD = net->create_busroute_from_stops(2, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(routeViaAandD); // if nullptr something went wrong

    // verify the route links: 12 -> 23 -> 34
    vector <Link*> links = routeViaAandD->get_links();
    QVERIFY(links.size() == 3);
    QVERIFY(links.front()->get_id() == 12);
    QVERIFY(links.back()->get_id() == 34);

    //test incorrect route
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 67, unreachable
    stops.push_back(stopB);

    Busroute* routeViaAandDandB = net->create_busroute_from_stops(3, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(routeViaAandDandB == nullptr); // should return nullptr since link 67 is not reachable

    //Vtype* vtype = new Vtype(888, "octobus", 1.0, 20.0);

    // test to create direct busroutes from/to all stops
    auto stopsmap = net->get_stopsmap();
    vector<Busroute*> routesFound;
    routesFound.reserve(6);

    int counter = 100;
    for (auto startstop : stopsmap)
    {
        for (auto endstop : stopsmap)
        {
            if (startstop != endstop)
            {
                stops.clear();
                stops.push_back(startstop.second);
                stops.push_back(endstop.second);
                qDebug() << "checking route for busline: " << startstop.first << " to " << endstop.first;
                Busroute* newRoute = net->create_busroute_from_stops(counter, od_pair->get_origin(), od_pair->get_destination(), stops);
                if (newRoute != nullptr)
                {
                    qDebug() << " route found";

                    //test if route found is a connected sequence of links
                    vector<Link*> rlinks = newRoute->get_links();
                    for (Link* rlink : rlinks)
                    {
                        Link* nextlink = newRoute->nextlink(rlink);
                        if (nextlink)
                        {
                            QString msg = QString("Failure, link %1 is not an upstream link to link %2").arg(rlink->get_id()).arg(nextlink->get_id());
                            QVERIFY2(rlink->get_out_node_id() == nextlink->get_in_node_id(), qPrintable(msg)); //outnode of preceding link should be innode of succeeding link
                        }
                    }

                    routesFound.push_back(newRoute);
                }
                else
                    qDebug() << " no route found";
                counter++;
            }
        }
    }

    QVERIFY2(routesFound.size() == 6, "Failure, there should be 6 direct routes from/to all stops for the SF network: A->B, A->C, A->D, B->C, B->D and C->D");

}

void TestDRT::testCreateAllDRTLines()
{
    auto busroutes = net->get_busroutes();
    auto buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 4);
    qDebug() << buslines.size();
    QVERIFY (buslines.size() == 4);
    net->createAllDRTLines(); // should find 6 more routes and create 6 more lines

    busroutes = net->get_busroutes();
    buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 10);
    QVERIFY (buslines.size() == 16);

    // WILCO temp crap
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 12
    auto o = net->findNearestOriginToStop(stopB);

}

void TestDRT::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QVERIFY2 (net->get_currenttime() == 5400.1, "Failure current time should be 5400.1 after running the simulation");

    // Example: way to check typical value for e.g. number of last departures from stop A:
   // qDebug() << net->get_busstop_from_name("A")->get_last_departures().size();
    // and here you turn it into a test
    QVERIFY2 ( net->get_busstop_from_name("A")->get_last_departures().size() == 2, "Failure, get_last_departures().size() for stop A should be 2");
}

//void TestDRT::testSaveResults()
//{
//    // remove old files:
//	for (const QString& filename : output_filenames)
//	{
//		qDebug() << QFile::remove(filename);
//	}

//    // save results:
//    nt->saveresults();
//     // test here the properties that should be true after saving the results

//	//test if output files match the expected output files
//    for (const QString& o_filename : output_filenames)
//	{
//		QString ex_o_fullpath = expected_outputs_path + o_filename;
//		QFile ex_outputfile(ex_o_fullpath);

//		QString msg = "Failure, cannot open ExpectedOutputs/" + o_filename;
//		QVERIFY2(ex_outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

//		QFile outputfile(o_filename);
//		msg = "Failure, cannot open " + o_filename;
//		QVERIFY2(outputfile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(msg));

//		msg = "Failure, " + o_filename + " differs from ExpectedOutputs/" + o_filename;
//		QVERIFY2(outputfile.readAll() == ex_outputfile.readAll(), qPrintable(msg));

//		ex_outputfile.close();
//		outputfile.close();
//	}
//}

void TestDRT::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestDRT)

#include "test_drt.moc"



