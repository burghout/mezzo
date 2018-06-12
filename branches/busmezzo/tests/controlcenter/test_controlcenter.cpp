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

//! @brief Controlcenter Tests BusMezzo
//! Unit testing of Controlcenter features


//const std::string network_path = "../networks/SFnetwork/";
//const std::string network_name = "masterfile.mezzo";

//const long int seed = 42;

class TestControlcenter : public QObject
{
    Q_OBJECT

public:
    TestControlcenter(){}


private Q_SLOTS:
    void testConstruction(); //!< test contstruction
    void testConnectPassenger(); //!< tests connecting a new passenger


private:



};

void TestControlcenter::testConstruction()
{   
    Controlcenter * ccPtr = new Controlcenter ();

    QVERIFY2(ccPtr, "Failing to create a Controlcenter");
    delete ccPtr;

}

void TestControlcenter::testConnectPassenger()
{
    Controlcenter * ccPtr = new Controlcenter ();
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



QTEST_APPLESS_MAIN(TestControlcenter)

#include "test_controlcenter.moc"



