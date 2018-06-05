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

//! Integration Tests BusMezzo
//! The tests contain loading, initialising, running, saving results and deleting network.
//! NOTE: currently the std::ifstream is used everywhere, precluding the use of qrc resources,
//! unless we switch to QFile everywhere.


const std::string network_path = "../networks/SFnetwork/";
const std::string network_name = "masterfile.mezzo";

const long int seed = 42;

class TestIntegration : public QObject
{
    Q_OBJECT

public:
    TestIntegration(){}


private Q_SLOTS:
    void testCreateNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test loading a network
    void testRunNetwork(); //!< test running the network
    void testSaveResults(); //!< tests saving results
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread
    Network* net;


};

void TestIntegration::testCreateNetwork()
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

void TestIntegration::testInitNetwork()
{
    nt->init();
 // Test here various properties that should be true after reading the network
    // Test if the network is properly read and initialised
    QVERIFY2(net->get_links().size() == 15, "Failure, network should have 15 links ");
    QVERIFY2(net->get_nodes().size() == 13, "Failure, network should have 13 nodes ");
    QVERIFY2(net->get_odpairs().size() == 4, "Failure, network should have 4 nodes ");
    QVERIFY2 (net->get_busstop_from_name("A")->get_id() == 1, "Failure, bus stop A should be id 1 ");
    QVERIFY2 (net->get_busstop_from_name("B")->get_id() == 2, "Failure, bus stop B should be id 2 ");
    QVERIFY2 (net->get_busstop_from_name("C")->get_id() == 3, "Failure, bus stop C should be id 3 ");
    QVERIFY2 (net->get_busstop_from_name("D")->get_id() == 4, "Failure, bus stop D should be id 4 ");

    QVERIFY2 (net->get_currenttime() == 0, "Failure, currenttime should be 0 at start of simulation");
}

void TestIntegration::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QVERIFY2 (net->get_currenttime() == 5400.1, "Failure current time should be 5400.1 after running the simulation");

    // Example: way to check typical value for e.g. number of last departures from stop A:
    qDebug() << net->get_busstop_from_name("A")->get_last_departures().size();
    // and here you turn it into a test
    QVERIFY2 ( net->get_busstop_from_name("A")->get_last_departures().size() == 2, "Failure, get_last_departures().size() for stop A should be 2");


}

void TestIntegration::testSaveResults()
{
    nt->saveresults();
     // test here the properties that should be true after saving the results
    QVERIFY2(true, "Failure ");
}

void TestIntegration::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");

}


QTEST_APPLESS_MAIN(TestIntegration)

#include "test_integration.moc"



