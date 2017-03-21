#include "eventlist.h"



// dummy implementation of  virtual execute (...,...) otherwise the linking gives
// problems: no virtual table generated
bool Action::execute(Eventlist*, double)        // unused Eventlist* eventlist, double time
{
	return true;
}

// Eventlist implementation
Eventlist::~Eventlist()

{
    /********** Delete old way of cleaning up actions
	multimap <double, Action*> :: iterator iter = thelist.begin();
	for (iter; iter != thelist.end(); iter)
	{
		
			//delete (*iter).second; // All the objects clean up their own actions
            iter = thelist.erase(iter);
	}
*/
    // New way (from Mezzo Trunk)
    thelist.clear();
    lastupdate=thelist.end();
}

void Eventlist::reset()
{
    /********* Delete old way of cleaning up
	multimap <double, Action*> :: iterator iter = thelist.begin();
	for (iter; iter != thelist.end(); iter)
	{
		// DO NOT delete the Actions, since they are simply reset (in Network), NOT re-created
		iter = thelist.erase(iter);
	}
	lastupdate=thelist.end();
    */
    // new way from Mezzo Trunk
    thelist.clear();
    lastupdate=thelist.end();
}
	
