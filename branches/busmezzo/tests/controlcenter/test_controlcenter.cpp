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
    void testConstruction(); //!< test construction of a Controlcenter
    void testConnectDisconnectPassenger(); //!< tests connecting and disconnecting passengers to/from Controlcenter 
	void testRequestHandler(); //!< tests for receiving and removing a request via passenger signals and controlcenter slots
	//void testBustripGenerator(); //!< tests for generation of unplanned trips and supporting methods
	void testDeletion(); //!< test deletion of Controlcenter

private:
	Controlcenter* ccPtr;
};

void TestControlcenter::testConstruction()
{   
    ccPtr = new Controlcenter();

    QVERIFY2(ccPtr, "Failed to create a Controlcenter");
}

void TestControlcenter::testConnectDisconnectPassenger()
{
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
}

void TestControlcenter::testRequestHandler()
{
	int oid = 1;
	int did = 2;
	Busstop* origin = new Busstop(oid, "origin", 1, 1, 1, 0, 0, 1.0, 0, 0, nullptr);
	Busstop* destination = new Busstop(did, "destination", 1, 1, 1, 0, 0, 1.0, 0, 0, nullptr);
	ODstops* OD_stop = new ODstops(origin, destination);
	Passenger* pass = new Passenger(0, 0.0, OD_stop, nullptr);

	//test create request via passenger method
    Request* invreq = pass->createRequest(origin, destination, 1, 1.0, 1.0);
    QVERIFY2(invreq == nullptr, "Failure, createRequest returned true for impossible request");

	//test recieve Request
	origin->add_CC(ccPtr);
	destination->add_CC(ccPtr);
	ccPtr->addStopToServiceArea(origin);
	ccPtr->addStopToServiceArea(destination);

    Request* req1 = pass->createRequest(origin, destination, 1, 1.0, 1.0); //request with pass_id of passenger
    QVERIFY2(req1->ostop_id == oid, "Failure, createRequest returning incorrect origin stop id");
    QVERIFY2(req1->dstop_id == did, "Failure, createRequest returning incorrect destination stop id");

	RequestHandler* rhPtr = &(ccPtr->rh_); //pointer to control center's request handler

    emit pass->sendRequest(req1, 0.0);
	QVERIFY2(rhPtr->requestSet_.size() == 0, "Failure, there should be 0 requests in requestSet after sendRequest from unconnected passenger");

	ccPtr->connectPassenger(pass);
    emit pass->sendRequest(req1, 0.0);
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should be 1 request in requestSet after sendRequest from connected passenger");
    emit pass->sendRequest(req1, 0.0);
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should still be 1 request in requestSet after duplicate sendRequest signal from connected passenger");

    Passenger* pass1 = new Passenger(1, 0.0, OD_stop, nullptr);
    Request* req2 = new Request(pass1,1, oid, did, 1, 0.0, 0.0); //creates request with pass_id 1
    rhPtr->addRequest(req2, ccPtr->serviceArea_);
	QVERIFY2(rhPtr->requestSet_.size() == 2, "Failure, there should be 2 requests in requestSet after direct call to addRequest");

    Passenger* pass2 = new Passenger(2,0.0,OD_stop,nullptr);
    Request* invalidreq = new Request(pass2,2, 1, 1000, 1, 0.0, 0.0); //creates request with destination stop outside of controlcenter service area
	rhPtr->addRequest(invalidreq, ccPtr->serviceArea_);
	QVERIFY2(rhPtr->requestSet_.size() == 2, "Failure, request accepted even though destination is outside of Controlcenter service area");
    QVERIFY2(pass2->get_curr_request() == nullptr, "Failure, RequestHandler did not reset passengers curr request to nullptr when rejecting request");

	//test remove Request
	ccPtr->disconnectPassenger(pass);
	emit pass->boardedBus(pass->get_id()); //should not be heard by controlcenter after disconnect
	QVERIFY2(rhPtr->requestSet_.size() == 2, "Failure, there should be 2 requests in requestSet after unheard boarded bus signal from disconnected passenger");

	ccPtr->connectPassenger(pass);
	emit pass->boardedBus(pass->get_id());
	QVERIFY2(rhPtr->requestSet_.size() == 1, "Failure, there should be 1 requests in requestSet after connected passenger sends boarded bus signal");
    QVERIFY2(pass->get_curr_request() == nullptr, "Failure, RequestHandler did not reset passengers curr request to nullptr when removing request after boardedBus signal");
	
	rhPtr->removeRequest(1); //remove request associated with pass_id 1
	QVERIFY2(rhPtr->requestSet_.size() == 0, "Failure, there should be 0 requests in requestSet after direct call to removeRequest");

    // Requests should have been cleaned up by RequestHandler

	delete pass;
    delete pass1;
    delete pass2;

	delete OD_stop;
}

void TestControlcenter::testDeletion()
{
	delete ccPtr;
	QVERIFY2(true, "Failed to delete Controlcenter");
}

QTEST_APPLESS_MAIN(TestControlcenter)

#include "test_controlcenter.moc"



