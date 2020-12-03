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
  Tests on the SF network with fixed services only
*/

const std::string network_path_1 = "../networks/SFnetwork/";
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

class TestSpiessFlorianFixed : public QObject
{
    Q_OBJECT

public:
    TestSpiessFlorianFixed(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test generating passenger path sets & loading a network
    void testRoutingGraph(); //!< test the routing graph and mapping
    //void testPathProbabilities(); //!< @todo add sanity checks of resulting probabilities of SF network, match this to original paper
    void testRunNetwork(); //!< test running the network
    void testPassAssignment(); //!< tests path-set-generation + resulting assignment
    void testSaveResults(); //!< tests saving results
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt = nullptr; //!< contains the network thread
    Network* net = nullptr;
};

void TestSpiessFlorianFixed::testCreateNetwork()
{   
    chdir(network_path_1.c_str());

    QFileInfo check_file(network_name.c_str());
    QVERIFY2 (check_file.exists(), "Failure, masterfile cannot be found");

    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");
}

void TestSpiessFlorianFixed::testInitNetwork()
{
    qDebug() << "Removing file " + path_set_generation_filename + ": " << QFile::remove(path_set_generation_filename); //remove old passenger path sets
    qDebug() << "Initializing network in " + QString::fromStdString(network_path_1);

    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialized
    QVERIFY2(net->get_links().size() == 15, "Failure, network should have 15 links ");
    QVERIFY2(net->get_nodes().size() == 13, "Failure, network should have 13 nodes ");
    QVERIFY2(net->get_odpairs().size() == 5, "Failure, network should have 6 od pairs ");
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");
    QVERIFY2 (AproxEqual(net->get_currenttime(),0.0), "Failure, currenttime should be 0 at start of simulation");
    QVERIFY2(theParameters->drt == false, "Failure, DRT is not set to false in parameters");
    QVERIFY2(theParameters->real_time_info == 2, "Failure, real time info is not set to 2 in parameters");
    QVERIFY2(theParameters->choice_set_indicator == 0, "Failure, choice set indicator is not set to 0 in parameters");

	//Test if newly generated passenger path sets match expected output
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

void TestSpiessFlorianFixed::testRoutingGraph()
{
    for (auto l:net->get_links())
    {
        QVERIFY(net->get_graphlink_to_link(net->get_link_to_graphlink(l.first)) == l.first);
    }

    for (auto n:net->get_nodes())
    {
        QVERIFY(net->get_graphnode_to_node(net->get_node_to_graphnode(n.first)) == n.first);
    }
    net->shortest_paths_all();

    auto destA = net->findNearestDestinationToStop(net->get_busstop_from_name("A"));
    auto oriA = net->findNearestOriginToStop(net->get_busstop_from_name("A"));

    auto destB = net->findNearestDestinationToStop(net->get_busstop_from_name("B"));
    auto oriB = net->findNearestOriginToStop(net->get_busstop_from_name("B"));

    auto destC = net->findNearestDestinationToStop(net->get_busstop_from_name("C"));
    auto oriC = net->findNearestOriginToStop(net->get_busstop_from_name("C"));

    auto destD = net->findNearestDestinationToStop(net->get_busstop_from_name("D"));
    auto oriD = net->findNearestOriginToStop(net->get_busstop_from_name("D"));

    QVERIFY (destA->get_id() == 4);
    QVERIFY (oriA->get_id() == 1);
    QVERIFY (destB->get_id() == 12);
    QVERIFY (oriB->get_id() == 5);
    QVERIFY (destC->get_id() == 12);
    QVERIFY (oriC->get_id() == 9);
    QVERIFY (destD->get_id() == 4);
    QVERIFY (oriD->get_id() == 1);

    auto linklist = net->shortest_path_to_node(12,4,0.0);
    QVERIFY (linklist [0]->get_id() == 12);
    QVERIFY (linklist [1]->get_id() == 23);
    QVERIFY (linklist [2]->get_id() == 34);

    linklist = net->shortest_path_to_node(56,10,0.0);
    QVERIFY (linklist [0]->get_id() == 56);
    QVERIFY (linklist [1]->get_id() == 67);
    QVERIFY (linklist [2]->get_id() == 710);
}

void TestSpiessFlorianFixed::testRunNetwork()
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

void TestSpiessFlorianFixed::testPassAssignment()
{
    /**
      @todo Check that all OD stop pairs with demand associated with them (in this unidirectional demand)
    */
}

void TestSpiessFlorianFixed::testSaveResults()
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

void TestSpiessFlorianFixed::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");
}


QTEST_APPLESS_MAIN(TestSpiessFlorianFixed)

#include "test_sf_fixed.moc"



