#include <QString>
#include <QtTest/QtTest>
#include "network.h"
// Integration Tests BusMezzo

class TestIntegration : public QObject
{
    Q_OBJECT

public:
    TestIntegration(){}


private Q_SLOTS:
    void testLoadNetwork();


};

void TestIntegration::testLoadNetwork()
{   
    const std::string network_name = "masterfile.mezzo";
    const long int seed = 42;
    NetworkThread* net1 = new NetworkThread(network_name,1,seed);
    net1->init(); // reads the input files

    net1->start(QThread::HighestPriority);
    net1->wait();
    net1->saveresults();

    QVERIFY2(true, "Failure ");
    delete net1;
}



QTEST_APPLESS_MAIN(TestIntegration)

#include "test_integration.moc"



