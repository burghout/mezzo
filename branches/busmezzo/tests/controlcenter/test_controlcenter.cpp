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
    void testConstruction(); //!< test construction
    void testConnectDisconnectPassenger(); //!< tests connecting a new passenger
	void testRequestHandler(); //!< tests for receiving and removing a request

private:



};

void TestControlcenter::testConstruction()
{   
    Controlcenter * ccPtr = new Controlcenter ();

    QVERIFY2(ccPtr, "Failing to create a Controlcenter");
    delete ccPtr;

}

void TestControlcenter::testConnectDisconnectPassenger()
{
    Controlcenter * ccPtr = new Controlcenter ();
    Busstop* origin = new Busstop();
    Busstop* destination = new Busstop();
    ODstops* OD_stop= new ODstops(origin,destination);
    Passenger* pass = new Passenger(0,0.0,OD_stop,nullptr);

    ccPtr->connectPassenger(pass);
    QVERIFY2 (ccPtr->connectedPass_.size() == 1, "Failure, connectedPass_.size should now be 1 after passenger connect");

	ccPtr->disconnectPassenger(pass);
	QVERIFY2(ccPtr->connectedPass_.size() == 0, "Failure, connectedPass_.size should be 0 after passenger disconnect");

//    delete origin;
//    delete destination; // cannot call delete on these
    delete OD_stop;
    delete pass;
    delete ccPtr;
}

void TestControlcenter::testRequestHandler()
{
	Controlcenter* ccPtr = new Controlcenter();
	int oid = 1;
	int did = 2;
	Busstop* origin = new Busstop(oid, "origin", 1, 1, 1, 0, 0, 1.0, 0, 0, nullptr);
	Busstop* destination = new Busstop(did, "destination", 1, 1, 1, 0, 0, 1.0, 0, 0, nullptr);
	ODstops* OD_stop = new ODstops(origin, destination);
	Passenger* pass = new Passenger(0, 0.0, OD_stop, nullptr);

	//test create request via passenger method
	Request req1 = pass->createRequest(1, 1.0, 1.0); //request with pass_id of passenger
	QVERIFY2(req1.ostop_id == oid, "Failure, createRequest returning incorrect origin stop id");
	QVERIFY2(req1.dstop_id == did, "Failure, createRequest returning incorrect destination stop id");
	
	//test recieve Request
	RequestHandler* rhPtr = &(ccPtr->rh_); //pointer to control center's request handler

	emit pass->sendRequest(req1, 0.0); 
	QVERIFY2(rhPtr->requestSet_.size() == 0, "Failure, there should be 0 requests in requestSet after sendRequest from unconnected passenger");

	ccPtr->connectPassenger(pass);
	emit pass->sendRequest(req1, 0.0);
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should be 1 request in requestSet after sendRequest from connected passenger");
	emit pass->sendRequest(req1, 0.0);
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should still be 1 request in requestSet after duplicate sendRequest signal from connected passenger");

	Request req2 = Request(1, 1, 1, 1, 0.0, 0.0); //creates request with pass_id 1
	rhPtr->addRequest(req2);
	QVERIFY2(rhPtr->requestSet_.size() == 2, "Failure, there should be 2 requests in requestSet after direct call to addRequest");

	//test remove Request
	ccPtr->disconnectPassenger(pass);
	emit pass->boardedBus(pass->get_id()); //should not be heard by controlcenter after disconnect
	QVERIFY2(rhPtr->requestSet_.size() == 2, "Failure, there should be 2 requests in requestSet after unheard boarded bus signal from disconnected passenger");

	ccPtr->connectPassenger(pass);
	emit pass->boardedBus(pass->get_id());
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should be 1 requests in requestSet after connected passenger sends boarded bus signal");
	
	rhPtr->removeRequest(1); //remove request associated with pass_id 1
	QVERIFY2(rhPtr->requestSet_.size() == 0, "Failure, there should be 0 requests in requestSet after direct call to removeRequest");


	delete pass;
	delete OD_stop;
	delete ccPtr;
}

QTEST_APPLESS_MAIN(TestControlcenter)

#include "test_controlcenter.moc"



