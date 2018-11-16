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


const std::string network_path_1 = "../networks/SFnetwork/"; // Spiess Florian network
const std::string network_path_2 = "../networks/DRTFeeder/"; // DRT feeder network

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
    void testCalcBusrouteInterStopIVT(); //!< tests the calculation of ivts between stops along a busroute
    void testCreateBusroute(); //!< tests creation of busroutes from stop pairs
    void testFindOrigins(); //!< tests the findNearestOrigin function
    void testFindDestinations(); //!< tests the findNearestDestination function
    void testCreateAllDRTLines(); //!< tests creation of buslines
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread
    Network* net;
};

void TestDRT::testCreateNetwork()
{   
    nt = nullptr;
    net = nullptr;
    chdir(network_path_2.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestDRT::testInitNetwork()
{
    QFile::remove(path_set_generation_filename); //remove old passenger path sets

    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 27, "Failure, network should have 27 links ");
    QVERIFY2(net->get_nodes().size() == 16, "Failure, network should have 16 nodes ");
    QVERIFY2(net->get_odpairs().size() == 9, "Failure, network should have 9 od pairs ");
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");

    QVERIFY2 (net->get_currenttime() == 0, "Failure, currenttime should be 0 at start of simulation");


	//Test if newly generated passenger path sets match expected output
//	QString ex_path_set_fullpath = expected_outputs_path + path_set_generation_filename;
//	QFile ex_path_set_file(ex_path_set_fullpath); //expected o_path_set_generation.dat
//	QVERIFY2(ex_path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open ExpectedOutputs/o_path_set_generation.dat");
	
//	QFile path_set_file(path_set_generation_filename); //generated o_path_set_generation.dat
//	QVERIFY2(path_set_file.open(QIODevice::ReadOnly | QIODevice::Text), "Failure, cannot open o_path_set_generation.dat");

//	QVERIFY2(path_set_file.readAll() == ex_path_set_file.readAll(), "Failure, o_path_set_generation.dat differs from ExpectedOutputs/o_path_set_generation.dat");

//	ex_path_set_file.close();
//	path_set_file.close();
}

void TestDRT::testCalcBusrouteInterStopIVT()
{
	vector<Busline*> lines = net->get_buslines();
	vector<pair<Busstop*, double> > interstopIVT;
	
	//test for existing lines
	for (auto line : lines)
	{
		Busroute* route = line->get_busroute();
		vector<Busstop*> stops = line->stops;
		interstopIVT = net->calc_interstop_freeflow_ivt(route, stops);

		QVERIFY2(interstopIVT.size() == stops.size() , "Failure, calculation of inter-stop IVT failed for existing busline");
	}

	//test multiple stops on same dummy links
	Busstop* Aprime = new Busstop(10, "Aprime", 12, 19, 19, 0, 0, 1.0, 0, 0, nullptr); //add to the same link as stop A, downstream of A
	Busstop* Bprime = new Busstop(20, "Bprime", 67, 19, 19, 0, 0, 1.0, 0, 0, nullptr); //add to the same link as stop B, downstream of B

	vector<Busstop*> stops = { net->get_busstop_from_name("A"), Aprime, net->get_busstop_from_name("B"), Bprime };
	Busroute* route = net->get_buslines()[1]->get_busroute(); //route of line 2

	interstopIVT = net->calc_interstop_freeflow_ivt(route, stops);
	QVERIFY2(interstopIVT.size() == stops.size(), "Failure, calculation of inter-stop IVT failed for route with multiple stops on dummy links");

	stops.clear();
	delete Aprime;
	delete Bprime;

	//test multiple stops on same real link
	route = net->get_buslines()[0]->get_busroute(); //route of line 1
	Busstop* upstream = new Busstop(30, "Upstream", 23, 5000, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 5000 meters from upstream node on link 23 
	Busstop* middle = new Busstop(40, "Middle", 23, 7500, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 7500 meters from upstream node on link 23
	Busstop* downstream = new Busstop(50, "Downstream", 23, 10000, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 10000 meters from upstream node on link 23
	stops = { net->get_busstop_from_name("A"), upstream, middle, downstream, net->get_busstop_from_name("D") };
	interstopIVT = net->calc_interstop_freeflow_ivt(route, stops);

	QVERIFY2(interstopIVT.size() == stops.size(), "Failure, calculation of inter-stop IVT failed for route with multiple stops on same real link");

	delete upstream;
	delete middle;
	delete downstream;
}

void TestDRT::testCreateBusroute()
{
    // Test correct route
    ODpair* od_pair = net->find_odpair(10,66); // 50 to 88
    QVERIFY (od_pair);
    qDebug() << "od pair " << od_pair->get_origin()->get_id() << " to "
             << od_pair->get_destination()->get_id();
    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopC = net->get_busstop_from_name("C"); // on link 56
    vector <Busstop*> stops;

    Busroute* emptyStopsRoute = net->create_busroute_from_stops(1, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(emptyStopsRoute == nullptr);

    stops.push_back(stopA);
    stops.push_back(stopC);
    Busroute* routeViaAandC = net->create_busroute_from_stops(2, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(routeViaAandC); // if nullptr something went wrong

    // verify the route links:
    vector <Link*> links = routeViaAandC->get_links();

    for (auto rlink:links) // verify the route links are connected
    {
        Link* nextlink = routeViaAandC->nextlink(rlink);
        if (nextlink)
        {
            QString msg = QString("Failure, link %1 is not an upstream link to link %2").arg(rlink->get_id()).arg(nextlink->get_id());
            QVERIFY2(rlink->get_out_node_id() == nextlink->get_in_node_id(), qPrintable(msg)); //outnode of preceding link should be innode of succeeding link
        }
    }
    QVERIFY(links.size() == 5);
    QVERIFY(links.front()->get_id() == 101);
    QVERIFY(links.back()->get_id() == 666);

//    //test incorrect route
//    Busstop* stopB = net->get_busstop_from_name("B"); // on link 67, unreachable
//    stops.push_back(stopB);

//    Busroute* routeViaAandDandB = net->create_busroute_from_stops(3, od_pair->get_origin(), od_pair->get_destination(), stops);
//    QVERIFY(routeViaAandDandB == nullptr); // should return nullptr since link 67 is not reachable

    // test to create direct busroutes from/to all stops
    auto stopsmap = net->get_stopsmap();
    vector<Busroute*> routesFound;
    // routesFound.reserve(6);

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
    qDebug() << "found routes size : " << routesFound.size();
    QVERIFY2(routesFound.size() == 12, "Failure, there should be 12 direct routes from/to all stops for the DRT network: all direct links between 4 stops: 4*3=12 permutations");
}

void TestDRT::testFindOrigins()
{
    // verify that each stop has an origin
    auto stopsmap = net->get_stopsmap();
    for (auto s:stopsmap)
    {
        auto o = net->findNearestOriginToStop(s.second);
        QVERIFY (o != nullptr);
    }

    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 67
    Busstop* stopC = net->get_busstop_from_name("C"); // on link 1011
    Busstop* stopD = net->get_busstop_from_name("D"); // on link 34

    auto oA=net->findNearestOriginToStop(stopA);
    QVERIFY (oA->get_id() == 1);

    auto oB=net->findNearestOriginToStop(stopB);
    QVERIFY (oB->get_id() == 5);

    auto oC=net->findNearestOriginToStop(stopC);
    QVERIFY (oC->get_id() == 9);

    auto oD=net->findNearestOriginToStop(stopD);
    QVERIFY (oD->get_id() == 1);
}

void TestDRT::testFindDestinations()
{
    auto stopsmap = net->get_stopsmap();
    for (auto s:stopsmap)
    {
        auto d = net->findNearestDestinationToStop(s.second);
        QVERIFY (d != nullptr);
    }
    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 67
    Busstop* stopC = net->get_busstop_from_name("C"); // on link 1011
    Busstop* stopD = net->get_busstop_from_name("D"); // on link 34

    auto oA=net->findNearestDestinationToStop(stopA);
    QVERIFY (oA->get_id() == 4);

    auto oB=net->findNearestDestinationToStop(stopB);
    QVERIFY (oB->get_id() == 12);

    auto oC=net->findNearestDestinationToStop(stopC);
    QVERIFY (oC->get_id() == 12);

    auto oD=net->findNearestDestinationToStop(stopD);
    QVERIFY (oD->get_id() == 4);

}

void TestDRT::testCreateAllDRTLines()
{
    auto busroutes = net->get_busroutes();
    auto buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 4);
    QVERIFY (buslines.size() == 4);
    net->createAllDRTLines(); // should find 6 more routes and create 6 more lines
    busroutes = net->get_busroutes();
    buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 10);
    QVERIFY (buslines.size() == 10);
}

void TestDRT::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestDRT)

#include "test_drt.moc"



