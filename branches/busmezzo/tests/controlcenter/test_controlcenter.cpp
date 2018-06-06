#include <QString>
#include <QtTest/QtTest>
#include "controlcenter.h"
#ifdef Q_OS_WIN
    #include <direct.h>
    #define chdir _chdir
#else
    #include <unistd.h>
#endif
//#include <unistd.h>
#include <QFileInfo>

//! @brief ControlCenter Tests BusMezzo
//! Unit testing of ControlCenter features


//const std::string network_path = "../networks/SFnetwork/";
//const std::string network_name = "masterfile.mezzo";

//const long int seed = 42;

class TestControlCenter : public QObject
{
    Q_OBJECT

public:
    TestControlCenter(){}


private Q_SLOTS:
    void testConstruction(); //!< test contstruction
    void testConnectPassenger(); //!< tests connecting a new passenger


private:



};

void TestControlCenter::testConstruction()
{   
    ControlCenter * ccPtr = new ControlCenter ();

    QVERIFY2(ccPtr, "Failing to create a ControlCenter");
    delete ccPtr;

}

void TestControlCenter::testConnectPassenger()
{
    ControlCenter * ccPtr = new ControlCenter ();
    Busstop* origin = new Busstop();
    Busstop* destination = new Busstop();
    ODstops* OD_stop= new ODstops(origin,destination);
    auto pass = new Passenger(0,0.0,OD_stop,nullptr,nullptr);

    ccPtr->connectPassenger(pass);
    QVERIFY2 (ccPtr->connectedPass_.size() == 1, "Failure, connectedPass_.size should now be 1" );



//    delete origin;
//    delete destination; // cannot call delete on these
    delete OD_stop;
    delete pass;
    delete ccPtr;
}



QTEST_APPLESS_MAIN(TestControlCenter)

#include "test_controlcenter.moc"



