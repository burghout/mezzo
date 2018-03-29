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
    void testLoadNetwork(); //!< test loading a network
    void testInitNetwork(); //!< test loading a network
    void testRunNetwork(); //!< test running the network
    void testSaveResults(); //!< tests saving results
    void testDelete(); //!< tests correct deletion

private:
    NetworkThread* nt; //!< contains the network thread


};

void TestIntegration::testLoadNetwork()
{   
    chdir(network_path.c_str());
    nt = new NetworkThread(network_name,1,seed);

    // need to get access to the network object to add more tests here.
    QVERIFY2(nt != nullptr, "Failure, could not create network thread");

}

void TestIntegration::testInitNetwork()
{
      nt->init();
      QVERIFY2(true, "Failure ");
}

void TestIntegration::testRunNetwork()
{

    nt->start(QThread::HighestPriority);
    nt->wait();
    QVERIFY2(true, "Failure ");


}

void TestIntegration::testSaveResults()
{
    nt->saveresults();

    QVERIFY2(true, "Failure ");
}

void TestIntegration::testDelete()
{
    delete nt;
    QVERIFY2(true, "Failure ");

}


QTEST_APPLESS_MAIN(TestIntegration)

#include "test_integration.moc"



