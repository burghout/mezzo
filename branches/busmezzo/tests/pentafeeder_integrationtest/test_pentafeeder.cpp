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


//const std::string network_path_1 = "../networks/SFnetwork/"; // Spiess Florian network
//const std::string network_path_2 = "../networks/DRTFeeder/"; // DRT feeder network
const std::string network_path_3 = "../networks/PentaFeeder_DRTOnly/"; //Pentagram feeder DRT lines only

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

class TestPentaFeeder : public QObject
{
    Q_OBJECT

public:
    TestPentaFeeder(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testCalcBusrouteInterStopIVT(); //!< tests the calculation of ivts between stops along a busroute
    void testCreateBusroute(); //!< tests creation of busroutes from stop pairs
    void testFindOrigins(); //!< tests the findNearestOrigin function
    void testFindDestinations(); //!< tests the findNearestDestination function
    void testCreateAllDRTLines(); //!< tests creation of buslines
    void testCreateControlcenterDRTLines(); //!< tests the creation of direct buslines for the service area of a Controlcenter
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread
    Network* net;
};

void TestPentaFeeder::testCreateNetwork()
{
    nt = nullptr;
    net = nullptr;
    chdir(network_path_3.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestPentaFeeder::testInitNetwork()
{
    //QFile::remove(path_set_generation_filename); //remove old passenger path sets

    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 46, "Failure, network should have 46 links ");
    QVERIFY2(net->get_nodes().size() == 24, "Failure, network should have 24 nodes ");
    QVERIFY2(net->get_odpairs().size() == 36, "Failure, network should have 36 od pairs ");
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");
    QVERIFY2 (net->get_busstop_from_name("E")->get_id() == 5, "Failure, bus stop E should be id 5 ");
    QVERIFY2 (net->get_busstop_from_name("F")->get_id() == 6, "Failure, bus stop F should be id 6 ");

    QVERIFY2 (theParameters->drt == true, "Failure, DRT not activated in parameters for PentaFeeder_OnlyDRT");
    QVERIFY2 (net->get_currenttime() == 0, "Failure, currenttime should be 0 at start of simulation");

    //TODO: add tests for path_set_generation file. Check to see if the most important paths are included when generating path set for this network. Currently passenger choice sets are read from input file
    QVERIFY(theParameters->choice_set_indicator == true);

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

void TestPentaFeeder::testCalcBusrouteInterStopIVT()
{
    vector<Busline*> lines = net->get_buslines();
    vector<pair<Busstop*, double> > interstopIVT;

    //test for existing lines
    for (auto line : lines)
    {
        Busroute* route = line->get_busroute();
        vector<Busstop*> stops = line->stops;
        qDebug() << "Calculating busroute " << route->get_id() << " IVT between stop " << stops.front()->get_id() << " and " << stops.back()->get_id() << " for line " << line->get_id();
        interstopIVT = net->calc_interstop_freeflow_ivt(route, stops);
        qDebug() << "IVTs for busroute " << route->get_id() << " of line " << line->get_id() << ": ";
        for(auto ivt : interstopIVT)
        {
            qDebug() << ivt.second;
        }
        QVERIFY2(interstopIVT.size() == stops.size() , "Failure, calculation of inter-stop IVT failed for existing busline");
    }

    //test multiple stops on same real link
    Busroute* route = net->get_buslines()[0]->get_busroute(); //route of line 1 (FixedTrunk_West)
    Busstop* upstream = new Busstop(30, "Upstream", 1011, 1000, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 1000 meters from upstream node on link 1011
    Busstop* middle = new Busstop(40, "Middle", 1011, 3000, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 3000 meters from upstream node on link 1011
    Busstop* downstream = new Busstop(50, "Downstream", 1011, 5000, 20, 0, 0, 1.0, 0, 0, nullptr); //add stop 5000 meters from upstream node on link 1011
    vector<Busstop*> stops = { net->get_busstop_from_name("E"), upstream, middle, downstream, net->get_busstop_from_name("F") };
    interstopIVT = net->calc_interstop_freeflow_ivt(route, stops);

    QVERIFY2(interstopIVT.size() == stops.size(), "Failure, calculation of inter-stop IVT failed for route with multiple stops on same real link");

    delete upstream;
    delete middle;
    delete downstream;
}

void TestPentaFeeder::testCreateBusroute()
{
    // Test correct route
    ODpair* od_pair = net->find_odpair(100,66); // 50 to 66
    QVERIFY (od_pair);
    qDebug() << "od pair " << od_pair->get_origin()->get_id() << " to " << od_pair->get_destination()->get_id();
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
    QVERIFY(links.front()->get_id() == 1001);
    QVERIFY(links.back()->get_id() == 666);

    //test if first stop is on rootlink from origin and next stop is on link directly after rootlink
    od_pair = net->find_odpair(100, 22);
    int rootlinkA = od_pair->get_origin()->get_links().front()->get_id();
    Busstop* originA = new Busstop(888, "originA", rootlinkA, 19, 19, 0, 0, 1.0, 0, 0, nullptr); //add stop to origin dummy link of stop A (link 1001)
    stops = { originA, stopA };
    Busroute* routeOriginAToA = net->create_busroute_from_stops(3, od_pair->get_origin(), od_pair->get_destination(), stops);
    QVERIFY(routeOriginAToA);

    // verify the route links:
    links = routeOriginAToA->get_links();
    for (auto rlink:links) // verify the route links are connected
    {
        Link* nextlink = routeOriginAToA->nextlink(rlink);
        if (nextlink)
        {
            QString msg = QString("Failure, link %1 is not an upstream link to link %2").arg(rlink->get_id()).arg(nextlink->get_id());
            QVERIFY2(rlink->get_out_node_id() == nextlink->get_in_node_id(), qPrintable(msg)); //outnode of preceding link should be innode of succeeding link
        }
    }
    QVERIFY(links.size() == 3);
    QVERIFY(links.front()->get_id() == 1001);
    QVERIFY(links.back()->get_id() == 222);

    delete originA;

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
                    //test if route actually visits location of both stops
                    QString msg = QString("Failure, generated route between stops %1 -> %2 does not include link of stop %1").arg(startstop.second->get_id()).arg(endstop.second->get_id());
                    QVERIFY2(newRoute->has_link(startstop.second->get_link_id()), qPrintable(msg));
                    msg = QString("Failure, generated route between stops %1 -> %2 does not include link of stop %2").arg(startstop.second->get_id()).arg(endstop.second->get_id());
                    QVERIFY2(newRoute->has_link(endstop.second->get_link_id()), qPrintable(msg));

                    routesFound.push_back(newRoute);
                }
                else
                    qDebug() << " no route found";
                counter++;
            }
        }
    }
    qDebug() << "found routes size : " << routesFound.size();
    QVERIFY2(routesFound.size() == 30, "Failure, there should be 30 direct routes from/to all stops for the DRT network: all direct links between 6 stops: 6*5=30 permutations");
}

void TestPentaFeeder::testFindOrigins()
{
    // verify that each stop has an origin
    auto stopsmap = net->get_stopsmap();
    for (auto s:stopsmap)
    {
        auto o = net->findNearestOriginToStop(s.second);
        QVERIFY (o != nullptr);
    }

    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 34
    Busstop* stopC = net->get_busstop_from_name("C"); // on link 56
    Busstop* stopD = net->get_busstop_from_name("D"); // on link 78
    Busstop* stopE = net->get_busstop_from_name("E"); // on link 910
    Busstop* stopF = net->get_busstop_from_name("F"); // on link 1112

    auto oA=net->findNearestOriginToStop(stopA);
    QVERIFY (oA->get_id() == 100);

    auto oB=net->findNearestOriginToStop(stopB);
    QVERIFY (oB->get_id() == 300);

    auto oC=net->findNearestOriginToStop(stopC);
    QVERIFY (oC->get_id() == 500);

    auto oD=net->findNearestOriginToStop(stopD);
    QVERIFY (oD->get_id() == 700);

    auto oE=net->findNearestOriginToStop(stopE);
    QVERIFY (oE->get_id() == 900);

    auto oF=net->findNearestOriginToStop(stopF);
    QVERIFY (oF->get_id() == 1100);
}

void TestPentaFeeder::testFindDestinations()
{
    auto stopsmap = net->get_stopsmap();
    for (auto s:stopsmap)
    {
        auto d = net->findNearestDestinationToStop(s.second);
        QVERIFY (d != nullptr);
    }
    Busstop* stopA = net->get_busstop_from_name("A"); // on link 12
    Busstop* stopB = net->get_busstop_from_name("B"); // on link 34
    Busstop* stopC = net->get_busstop_from_name("C"); // on link 56
    Busstop* stopD = net->get_busstop_from_name("D"); // on link 78

    auto oA=net->findNearestDestinationToStop(stopA);
    QVERIFY (oA->get_id() == 22);

    auto oB=net->findNearestDestinationToStop(stopB);
    QVERIFY (oB->get_id() == 44);

    auto oC=net->findNearestDestinationToStop(stopC);
    QVERIFY (oC->get_id() == 66);

    auto oD=net->findNearestDestinationToStop(stopD);
    QVERIFY (oD->get_id() == 88);

}

void TestPentaFeeder::testCreateAllDRTLines()
{
    auto busroutes = net->get_busroutes(); //should be 22 existing busroutes
    auto buslines = net->get_buslines(); //should be 22 existing buslines
    qDebug() << " testCreateAllDRTLInes, busroutes.size() " << busroutes.size()
             << " buslines.size() " << buslines.size();
    QVERIFY (busroutes.size() == 22);
    QVERIFY (buslines.size() == 22);
    net->createAllDRTLines(); // should find 30 more routes and create 30 more lines (all direct routes between 6 stops, duplicates are not excluded)
    busroutes = net->get_busroutes();
    buslines = net->get_buslines();
    qDebug() << " testCreateAllDRTLInes, busroutes.size() " << busroutes.size()
             << " buslines.size() " << buslines.size();
    QVERIFY (busroutes.size() == 52);
    QVERIFY (buslines.size() == 52);
}

void TestPentaFeeder::testCreateControlcenterDRTLines()
{
    //create a dummy controlcenter
    Controlcenter* cc = new Controlcenter();

    //add stops A B and C to service area of controlcenter
    cc->addStopToServiceArea(net->get_busstop_from_name("A")); // on link 12
    cc->addStopToServiceArea(net->get_busstop_from_name("B")); // on link 34
    cc->addStopToServiceArea(net->get_busstop_from_name("C")); // on link 56

    QVERIFY(cc->getServiceArea().size() == 3); //there should be 3 stops in the service area of the controlcenter
    QVERIFY(cc->getServiceRoutes().size() == 0); //there should be no service routes available yet

    auto busroutes = net->get_busroutes();
    auto buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 52); //2 from input files, 20 from input controlcenter with 5 stops, 30 from createALLDRTLines
    QVERIFY (buslines.size() == 52);

    net->createControlcenterDRTLines(cc);

    //routes should not be duplicated, buslines can be duplicated
    busroutes = net->get_busroutes();
    buslines = net->get_buslines();
    QVERIFY (busroutes.size() == 52); //all direct routes already generated in testCreateAllDRTLines
    QVERIFY (buslines.size() == 58); //6 additional lines for 3 stops in service area of controlcenter

    QVERIFY(cc->getServiceRoutes().size() == 6); //6 lines have been added to controlcenter

    delete cc;
}

void TestPentaFeeder::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestPentaFeeder)

#include "test_pentafeeder.moc"



