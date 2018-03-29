#include <QString>
#include <QtTest/QtTest>
#include "network.h"
#include <unistd.h>

//! Integration Tests BusMezzo
//! The tests contain loading, initialising, running, saving results and deleting network.


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
    nt = new NetworkThread(network_name,1,seed);
    net = nt->getNetwork();

    QVERIFY2(nt != nullptr, "Failure, could not create network thread");
    QVERIFY2(net != nullptr, "Failure, could not create network");


}

void TestIntegration::testInitNetwork()
{
    nt->init();
 // Test here various properties that should be true after reading the network
    QVERIFY2(net->get_links().size() == 15, "Failure, network should have 15 links ");
    QVERIFY2(net->get_nodes().size() == 13, "Failure, network should have 13 nodes ");
    QVERIFY2(net->get_odpairs().size() == 4, "Failure, network should have 4 nodes ");

}

void TestIntegration::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();

    // test here the properties that should be true after running the simulation
    QVERIFY2(true, "Failure ");


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



