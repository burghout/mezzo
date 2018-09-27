#include <QString>
#include <QtTest/QtTest>
#include "Graph.h"
#include "linktimes.h"
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
    void testInitGraphs();
    void testSimpleGraph();
    void testTimeDependentGraph();
    void testTimeDependentDetour();

private:
    //    NetworkThread* nt; //!< contains the network thread
    //    Network* net;

    Graph<double, GraphNoInfo<double> > * simpleGraph; // simple graph with fixed costs
    Graph<double, LinkTimeInfo > * graph; // graph with time-dependent costs in LinkTimeInfo
    LinkTimeInfo* linkinfo; // time-dependent link costs


};

void TestRoutes::testInitGraphs()
{   
    simpleGraph = new Graph<double, GraphNoInfo<double> > (10, 10, 9999999.0); // creates a simple graph with fixed link costs <double>
    simpleGraph->addLink(1,1,2,10.0); // link 1 from node 1 to node 2
    simpleGraph->addLink(2,2,1,10.0);
    simpleGraph->addLink(3,2,3,10.0);
    simpleGraph->addLink(4,3,2,10.0);
    simpleGraph->addLink(5,3,4,10.0);
    QVERIFY (simpleGraph->nNodes()==10);
    QVERIFY (simpleGraph->nLinks()==10);

    // Time dependent graph init
    // 1. set up the linkinfo object that contains the time dependent costs for each link and time period
    linkinfo=new LinkTimeInfo();

    for (int id=1; id<=5; ++id)
    {
        LinkTime* lt = new LinkTime(id,3,20.0); // 3 periods of 20.0 seconds each (totaltime = 60.0)
        lt->times [0] = 10.0;
        lt->times [1] = 20.0;
        lt->times [2] = 30.0; // increasing travel times for all links, from 10.0 to 30.0
        linkinfo->times.insert(pair <int,LinkTime*>(id,lt));
    }

    graph = new Graph <double, LinkTimeInfo> (10,10,9999999.0);
    graph->addLink(1,1,2,10.0); // link 1 from node 1 to node 2
    graph->addLink(2,2,1,10.0);
    graph->addLink(3,2,3,10.0);
    graph->addLink(4,3,2,10.0);
    graph->addLink(5,3,4,10.0);
    graph->set_downlink_indices();

    QVERIFY (graph->nNodes()==10);
    QVERIFY (graph->nLinks()==10);
}
void TestRoutes::testSimpleGraph()
{
    for (int i = 1; i<=4; ++i) // for all root links except link 5
    {
        simpleGraph->labelCorrecting(i); // find shortest paths using label correcting from root link
        simpleGraph->printNodePathTree();
        for (int j=1; j <= 4; ++j) // for all destinations
        {
            QVERIFY(simpleGraph->reachable(j)); // test if they are reachable
        }
    }

}

void TestRoutes::testTimeDependentGraph()
{
    graph->labelCorrecting(1,0.0,linkinfo);
    for (int j=1; j <= 4; ++j) // for all destinations
    {
        QVERIFY(graph->reachable(j)); // test if they are reachable
    }
    QVERIFY (graph->costToNode(4)==40.0);
    graph->printNodePathTree();
    graph->labelCorrecting(1,10.0,linkinfo);
    QVERIFY (graph->costToNode(4)==60.0);
    graph->printNodePathTree();
    graph->labelCorrecting(1,20.0,linkinfo);
    QVERIFY (graph->costToNode(4)==80.0);
    graph->printNodePathTree();
}

void TestRoutes::testTimeDependentDetour()
{
     graph->addLink(6,2,4,10.0); // add a bypass
     LinkTime* lt = new LinkTime(6,3,20.0); // create the LinkTimes
     lt->times [0] = 30.0;
     lt->times [1] = 30.0;
     lt->times [2] = 60.0; // same
     linkinfo->times.insert(pair <int,LinkTime*>(6,lt));

     graph->labelCorrecting(1,0.0,linkinfo);
     for (int j=1; j <= 4; ++j) // for all destinations
     {
         QVERIFY(graph->reachable(j)); // test if they are reachable
     }
     QVERIFY (graph->costToNode(4)==40.0); // via node 3
     graph->printNodePathTree();
     graph->labelCorrecting(1,10.0,linkinfo);
     QVERIFY (graph->costToNode(4)==40.0); // now via detour link 6
     graph->printNodePathTree();
     graph->labelCorrecting(1,20.0,linkinfo); // now via node 3, as detour more expensive
     QVERIFY (graph->costToNode(4)==80.0);
     graph->printNodePathTree();

}

QTEST_APPLESS_MAIN(TestRoutes)

#include "test_routes.moc"



