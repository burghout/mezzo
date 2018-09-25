#include <QString>
#include <QtTest/QtTest>
#include "Graph.h"
#ifdef Q_OS_WIN
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif
//#include <unistd.h>
#include <QFileInfo>

//! Route Tests
//! Checking if the routing module works as expected


//const std::string network_path = "../networks/SFnetwork/";
//const std::string network_name = "masterfile.mezzo";

//const QString expected_outputs_path = "://networks/SFnetwork/ExpectedOutputs/";
//const QString path_set_generation_filename = "o_path_set_generation.dat";
//const vector<QString> output_filenames =
//{
//	"o_od_stop_summary_without_paths.dat",
//	"o_od_stops_summary.dat",
//	"o_passenger_trajectory.dat",
//	"o_passenger_welfare_summary.dat",
//	"o_segments_line_loads.dat",
//	"o_segments_trip_loads.dat",
//	"o_selected_paths.dat",
//	"o_transit_trajectory.dat",
//	"o_transitline_sum.dat",
//	"o_transitlog_out.dat",
//	"o_transitstop_sum.dat",
//	"o_trip_total_travel_time.dat",
//};

//const long int seed = 42;

class TestRoutes : public QObject
{
    Q_OBJECT

public:
    TestRoutes(){}


private Q_SLOTS:
    void testInitGraph();
    void testSimpleGraph();

private:
    //    NetworkThread* nt; //!< contains the network thread
    //    Network* net;

    Graph<double, GraphNoInfo<double> > * simpleGraph;


};

void TestRoutes::testInitGraph()
{   
    simpleGraph = new Graph<double, GraphNoInfo<double> > (10, 100, 9999999.0); // creates a simple graph with fixed link costs <double>
    simpleGraph->addLink(10,1,2,10.0); // link 1 from node 1 to node 2
    simpleGraph->addLink(20,2,1,10.0); // link 2 from node 2 to node 1
    simpleGraph->addLink(30,2,3,10.0); // link 2 from node 2 to node 1
    simpleGraph->addLink(40,3,2,10.0); // link 2 from node 2 to node 1
    simpleGraph->addLink(50,3,4,10.0); // link 2 from node 2 to node 1
    QVERIFY (simpleGraph->nNodes()==10);
    QVERIFY (simpleGraph->nLinks()==100);
}

void TestRoutes::testSimpleGraph()
{
    simpleGraph->labelCorrecting(40); // find shortest paths using label correcting from root link 0
    simpleGraph->printLinkPathTree();
     simpleGraph->printNodePathTree();
     simpleGraph->printPathToNode(4);
}


QTEST_APPLESS_MAIN(TestRoutes)

#include "test_routes.moc"



