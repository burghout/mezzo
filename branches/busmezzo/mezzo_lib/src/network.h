/*
	Mezzo Mesoscopic Traffic Simulation
    Copyright (C) 2008  Wilco Burghout

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETWORK_HH
#define NETWORK_HH

//#undef _NO_GUI
//#define _DEBUG_NETWORK
//#define _DEBUG_SP // shortest path routines
#define _USE_VAR_TIMES   //!< variable link travel times



// standard template library inclusions

#include <utility>
#include <vector>
#include <fstream>
#include <string>


//inclusion of parameter constants
#include "parameters.h"


//inclusion of label-correcting shortest path algorithm
#include "Graph.h"


// Mezzo inclusions
#include "server.h"
#include "link.h"
#include "node.h"
#include "sdfunc.h"
#include "turning.h"
#include "route.h"
#include "od.h"
#include "icons.h"
#include "vtypes.h"
#include "linktimes.h"
#include "eventlist.h"
#include "trafficsignal.h"
#include "busline.h"
#include "passenger.h"
#include "od_stops.h"
#include "pass_route.h"
#include "day2day.h"
#include "controlcenter.h"


// inclusions for the GUI
#ifndef _NO_GUI
	#include <QPixmap>
	#include <qpixmap.h>
	#include <QMatrix>
#endif // _NO_GUI

//thread support
#include <qthread.h>

//include the PVM communicator
#ifdef _PVM
#include "pvm.h"
#endif // _PVM

// Or include the VISSIMCOM communicator
#ifdef _VISSIMCOM
#include "vissimcom.h"
#endif //_VISSIMCOM

using namespace std;
class TurnPenalty;
class Incident;

//static Random* r1;

//!< @brief passenger output data for a fixed with flexible run/iteration. Can create for total, for fixed only and for drt only.
struct FWF_passdata 
{ 
	//Passengers
	int pass_completed=0; // pass completed trips (that reached their final destination)
	double total_wlkt = 0; //total walking time
	double avg_total_wlkt = 0; //avg walking time
	double total_wt=0; // total waiting time
	double avg_total_wt=0; // avg. total waiting time
	double total_denied_wt=0; // total denied waiting time
	double avg_denied_wt=0; // avg. denied waiting time
	double min_wt=DBL_INF; // min. max waiting time
	double max_wt=0; // max. waiting time
	double median_wt=0; // median waiting time
	double total_ivt=0; // total ivt time
	double avg_total_ivt = 0;
	double total_crowded_ivt = 0;
	double avg_total_crowded_ivt = 0;
	size_t npass = 0; //number of passengers statistics have been calculated for

	void calc_pass_statistics(const vector<Passenger*>& passengers); //fill in aggregate passengers statistics based on whatever vector of passengers
};

//!< @brief vehicle output data for fixed with flexible implementation. 
struct FWF_vehdata
{
	// Vehicles
	double total_vkt = 0; // total empty + occupied VKT
	double total_empty_vkt = 0; // total empty VKT
	double total_occupied_vkt = 0; // total occupied VKT

	double total_empty_time = 0;
	double total_occupied_time = 0;

	double total_driving_time = 0;
	double total_idle_time = 0;
	double total_oncall_time = 0;

	friend FWF_vehdata operator+(const FWF_vehdata& lhs, const FWF_vehdata& rhs);

	FWF_vehdata& operator+=(const FWF_vehdata& rhs)
	{
		total_vkt += rhs.total_vkt;
		total_occupied_vkt += rhs.total_occupied_vkt; // total occupied VKT
		total_empty_vkt += rhs.total_empty_vkt; // total empty VKT

		total_empty_time = rhs.total_empty_time;
		total_occupied_time = rhs.total_occupied_time;

		total_driving_time = rhs.total_driving_time;
		total_idle_time = rhs.total_idle_time;
		total_oncall_time = rhs.total_oncall_time;

		return *this;
	}

	// @todo calc_VKT, maybe use total driving time (empty), total driving time (full) - dwelltime * avgSpeed?
	// alt could grab the distance traversed while driving in different states?
	// NOTE: Turns out this is kindof tricky. Recall that bus instances represent the same bus but there are several of these instances being copied and destroyed over a run....
	// Instead.....things tend to revolve around trips again?

};

//!< @brief controlcenter output data for fixed with flexible implementation. 
// @todo Note, probably only one cc will be used, but still this is implemented as if there could be several. Not tested for this case.
struct FWF_ccdata
{
	int total_requests_recieved=0;
	int total_requests_rejected=0;
	int total_requests_accepted=0;
	int total_requests_served=0;
};

class ODRate
{
public:
	odval odid;
	int rate=0;
};


class ODSlice
{
public:
	vector <ODRate> rates;	
};


class  ODMatrix
{
public:
	ODMatrix ();
	void add_slice(double time, ODSlice* slice);
	void reset(Eventlist* eventlist,vector <ODpair*> * odpairs); // rebooks all the MatrixActions
private:
	vector < pair <double,ODSlice*> > slices;	

};

class MatrixAction: public Action
{
public:
	MatrixAction(Eventlist* eventlist, double time, ODSlice* slice_, vector<ODpair*> *ods_);
    bool execute(Eventlist* eventlist, double /*time*/) override;
private:
	ODSlice* slice;
	vector <ODpair*> * ods;
};

/*! Network is the main simulation class, containing all the network objects 
 * such as Links, Nodes, ODpairs, etc. as well as all the main simulation, reading and writing functions
 * It is included in the GUI part (MainForm in canvas_qt4.h) that calls it's public functions to read in the input files, simulate
 * and write results.
 *
 * last modifications:
 * add map <int,Object*> containers for Nodes, Links, Routes, Origins, Destinations, Junctions for faster look-up.
 * modified find_alternatives_all (int lid, double penalty, Incident* incident) to use the routemap structure at links for finding alternative routes.
 * Wilco Burghout
 * last change: 2008-01-11
  
 */


class Network
{
public:
	Network();
	//Network(const Network& net) {}
	~Network();
	bool readmaster(string name); //!< reads the master file.
#ifndef _NO_GUI
	double executemaster(QPixmap * pm_, QMatrix * wm_); //!< starts the scenario, returns total running time
	double get_scale() {return scale;} //!< returns the scale of the drawing
#endif //_NO_GUI
	double executemaster(); //!< without GUI
	int reset(); //!< resets the simulation to 0, clears all the state variables. returns runtime
	void delete_passengers(); //Delete all passengers
	unsigned int count_generated_passengers(); // !< returns the total number of passengers generated for this replication
	vector<Passenger*> get_all_generated_passengers(); //!< returns a vector of all passengers generated for each ODstop pair
	unsigned int count_transit_paths(); //!< returns the total number of paths that are currently defined between stop pairs
	void end_of_simulation(); //!< finalise all the temp values into containers (linktimes)
	double step(double timestep); //!< executes one step of simulation, called by the gui, returns current value of time
	bool writeall(unsigned int repl=0); //writes the output, appends replication number to output files
	bool readnetwork(string name); //!< reads the network and creates the appropriate structures
	bool init(); //!< creates eventlist and initializes first actions for all turnings at time 0 and starts the Communicator
	bool init_shortest_path(); //!< builds the shortest path graph
	vector<Link*> get_path(int destid); //!<gives the links on the shortest path to destid (from current rootlink)
    bool shortest_pathtree_from_origin_link(int lid, double start_time); //!<
    vector<Link*> shortest_path_to_node(int rootlink, int dest_node, double start_time); //!< returns shortest path Links
    bool shortest_paths_all(); //!< calculates shortest paths and generates the routes
	bool find_alternatives_all (int lid, double penalty, Incident* incident); //!< finds the alternative paths 'without' link lid.
	//void delete_spurious_routes(); //!< deletes all routes that have no OD pair.
	void renum_routes (); //!< renumerates the routes, to keep a consecutive series after deletions & additions
    Busroute* create_busroute_from_stops(int id, Origin* origin_node, Destination* destination_node, const vector<Busstop*>& stops, double time = 0.0); //!< creates a busroute beginning at origin node and ending at destination node that visits stops in sequential order. Returns nullptr if busroute is not possible
    Busline* create_busline(int busline_id, int opposite_busline_id, string name, Busroute* broute, vector <Busstop*> stops, Vtype* vtype, ODpair* odptr, int holding_strategy, double max_headway_holding, double init_occup_per_stop, int nr_stops_init_occup, bool flex_line); //!< creates a busline and adds it to Network::buslines
    bool createAllDRTLines(); //!< creates all the DRT lines between each stop pair that has a viable route between them
    bool createControlcenterDRTLines(Controlcenter* cc); //!< creates all direct DRT lines between each stop pair in the service area of Controlcenter and assigns these lines to Controlcenter
    Origin* findNearestOriginToStop(Busstop* stop); //!< returns the nearest Origin Node to  stop
    Destination* findNearestDestinationToStop(Busstop* stop);  //!< returns the nearest Destination Node from stop
    vector<pair<Busstop*, double> > calc_interstop_freeflow_ivt(const Busroute* route, const vector<Busstop*>& stops) const; //!< returns the free-flow IVT between all stops on a busroute

    bool run(int period); //!< RUNS the network for 'period' seconds
	bool addroutes (int oid, int did, ODpair* odpair); //!< adds routes to an ODpair
	bool add_od_routes()	; //!< adds routes to all ODpairs

	bool readdemandfile(string name);  //!< reads the OD matrix and creates the ODpairs
	bool readlinktimes(string name); //!< reads historical link travel times
	bool set_freeflow_linktimes();
	bool copy_linktimes_out_in(); //!< copies output link travel times to input
	bool readpathfile(string name); //!< reads the routes
	bool readincidentfile(string name); //!< reads the file with the incident (for now only 1)
	bool writepathfile (string name); //!< appends the routes found to the route file
	bool writeoutput(string name); //!< writes detailed output, at this time theOD output!
	bool writesummary(string name); //!< writes the summary of the OD output

	bool writeFWFsummary(ostream& out, const FWF_passdata& total_passdata, const FWF_passdata& fix_passdata, const FWF_passdata& drt_passdata, const FWF_vehdata& total_vehdata, const FWF_vehdata& fix_vehdata, const FWF_vehdata& drt_vehdata, const FWF_ccdata& cc_data); //!< summary of output for debugging fixed with flexible implementation	

	bool writelinktimes(string name); //!<writes average link traversal times.
	bool writeheadways(string name); //!< writes the timestamps of vehicles entering a Virtual Link (i e Mitsim).
	//!<same format as historical times read by readlinktimes(string name)
	bool register_links();//!<registers the links at the origins and destinations
	void set_incident(int lid, int sid, bool blocked, double blocked_until); //!< sets the incident on link lid (less capacity, lower max speed)
	void unset_incident(int lid); //!< restores the incident link to its normal behaviour
	void broadcast_incident_start(int lid); //!< informs the vehicles on the links of the incident on link lid
	void broadcast_incident_stop(int lid); //!< informs the vehicles that the incident on link lid has been cleared
	bool readturnings(string name); //!< reads the turning movements
	void create_turnings();          //!< creates the turning movements
	bool writeturnings(string name);  //!< writes the turing movements
	bool writemoes(string ending=""); //!< writes all the moes: speeds, inflows, outflows, queuelengths and densities per link
	bool writeallmoes(string name); //!< writes all the moes in one file.
	bool writeassmatrices(string name); //!< writes the assignment matrices
	bool write_v_queues(string name); //!< writes the virtual queue lengths
	

	bool readassignmentlinksfile(string name); //!< reads the file with the links for which the assignment matrix is collected

	bool readvtypes (string name); //!< reads the vehicles types with their lentghs and percentages.
	bool readvirtuallinks(string name); //!< reads the virtual links that connect boundary out nodes to boundary in nodes.
	bool readserverrates(string name); //!< reads in new rates for specified servers. This way server capacity can be variable over time for instance for exits.
	bool readsignalcontrols(string name); //!< reads the signal control settings
	void seed (long int seed_) {randseed=seed_; vehtypes.set_seed(seed_);}          //!< sets the random seed
	void removeRoute(Route* theroute);
	void reset_link_icons(); //!< makes sure all the link-icons are shown normally when the run button is pressed. This corrects the colours in case of an incident file (it colours show affected links)
		
	void set_output_moe_thickness ( unsigned int val ) ; // sets the output moe for the link thickness  for analysis
	void set_output_moe_colour ( unsigned int val ) ; // sets the output moe for the link colours for analysis

#ifndef _NO_GUI
	void recenter_image();   //!< sets the image in the center and adapts zoom to fit window
	QMatrix netgraphview_init(); //!< scale the network graph to the view initialized by pixmaps
	
	void redraw(); //!< redraws the image

#endif //_NO_GUI

	// GET's
	double get_currenttime(){return time;}
	double get_runtime(){return runtime;}
	double get_time_alpha(){return time_alpha;}
	Parameters* get_parameters () {return theParameters;} 

    // WILCO: note: returning a reference to member variables of an object is in general a BAD IDEA
	vector <ODpair*>& get_odpairs () {return odpairs;} // keep as vector
    ODpair* find_odpair (int origin_id, int dest_id); //!< returns the ODpair if found, nullptr otherwise

	map <int, Origin*>& get_origins() {return originmap;}
	map <int, Destination*>& get_destinations() {return destinationmap;}
	map <int, Node*>& get_nodes() {return nodemap;}
	map <int,Link*>& get_links() {return linkmap;}
	map<int, Controlcenter*> get_controlcenters() { return ccmap; }

	map <int, Busstop*>& get_stopsmap() { return busstopsmap; }
    vector <Busroute*>&  get_busroutes() { return busroutes; } // bad idea, but consistent with the other get_*
    vector <Busline*>& get_buslines() {return buslines;}
	vector <ODstops*> get_odstops_demand() { return odstops_demand; }
	vector<Bus*> get_busvehicles() { return busvehicles; }
	
	multimap<odval, Route*>::iterator find_route (int id, odval val);
	bool exists_route (int id, odval val); // checks if route with this ID exists for OD pair val
	bool exists_same_route (Route* route); // checks if a similar route with the same links already exists
// STATISTICS
	//Linktimes
	double calc_diff_input_output_linktimes (); //!< calculates the sum of the differences in output-input link travel times
	double calc_sumsq_input_output_linktimes (); //!< calculates the sum square of the differences in output-input link travel times
	double calc_rms_input_output_linktimes();//!< calculates the root mean square of the differences in output-input link travel times
	double calc_rmsn_input_output_linktimes();//!< calculates the Normalized (by mean) root mean square differences in output-input link travel times
	double calc_mean_input_linktimes(); //!< calculates the mean of the input link travel times;
	// OD times
	double calc_rms_input_output_odtimes();
	double calc_mean_input_odtimes();
	double calc_rmsn_input_output_odtimes();
	// SET's
	void set_workingdir (const string dir) {workingdir = dir;}
	void set_time_alpha(double val) {time_alpha=val;}

	// Public transport
	
	bool write_busstop_output(string name1, string name2, string name3, string name4, string name5, string name6, string name7, string name8, string name9, string name10, string name11, string name12, string name13, string name14, string name15, string name16, string name17, string name18, string name19, string name20); //<! writes all the bus-related output 
	void write_passenger_welfare_summary(ostream& out, double total_gtc, int total_pass);
	bool write_path_set (string name1); //!< writes the path-set generated at the initialization process (aimed to be used as an input file for other runs with the same network)
	bool write_path_set_per_stop (string name1, Busstop* stop);
	bool write_path_set_per_od (string name1, Busstop* origin_stop, Busstop* destination_stop);
	void write_transitlogout_header(ostream& out);
	void write_transitstopsum_header(ostream& out);
	void write_transitlinesum_header(ostream& out);
	void write_transit_trajectory_header(ostream& out);
	void write_selected_path_header(ostream& out);
	void write_od_summary_header(ostream& out);
	void write_triptotaltraveltime_header(ostream& out);
	void write_transitlineloads_header(ostream& out);
	void write_transittriploads_header(ostream& out);
    void write_transitroutes(string name); //!< Wilco 2018 -- writes the complete transit routes  (busroutes) to file.

	bool readtransitroutes(string name); //!< reads the transit routes, similar to readroutes
	bool readtransitnetwork(string name); //!< reads the stops, distances between stops, lines, trips and travel disruptions
	bool readtransitdemand (string name); //!< reads passenger demand for transit services
	bool readtransitfleet (string name); // !< reads transit vehicle types, vehicle scheduling and dwell time functions
	bool read_transitday2day (string name); // !< reads info on transit pass. day-to-day memory
	bool read_transitday2day(map<ODSL, Travel_time>& ivt_map);
	bool read_IVTT_day2day (string name);
	bool read_IVTT_day2day(map<ODSLL, Travel_time>& ivt_map);
	bool read_OD_day2day (istream& in); //!< reads day-to-dat info for a particular OD
	bool read_OD_day2day (pair<const ODSL, Travel_time>& wt_row);
	bool read_pass_day2day (pair<const ODSL, Travel_time>& wt_row);
	bool read_OD_IVTT (istream& in);
	bool read_OD_IVTT (pair<const ODSLL, Travel_time>& wt_row);
	bool read_pass_IVTT (pair<const ODSLL, Travel_time>& wt_row);
	bool readbusroute(istream& in); //!< reads a transit route
	bool readbusstop (istream& in); //!< reads a busstop
	bool readbusline(istream& in); //!< reads a busline
    bool readwalkingtimedistribution(istream& in); //!< reads a walking time distribution between two nodes that are within walking distance that can be generated using a dedicated walking model.
	bool readbustrip_format1(istream& in); //!< reads trips based on detailed time-table
	bool readbustrip_format2(istream& in); //!< reads trips based on dispatching time-table (time-independent intervals between stops)
  	bool readbustrip_format3(istream& in); //!< reads trips based on headway (time-independent intervals between stops)
	bool read_passenger_rates_format1 (istream& in); // reads the passenger rates in the format of arrival rate and alighting fraction per line and stop combination
  bool read_passenger_rates_format1_TD_basic (istream& in, int nr_rates);
  bool read_passenger_rates_format1_TD_slices (istream& in);
  bool read_passenger_rates_format2 (istream& in); // reads the passenger rates in the format of arrival rate per line, origin stop and destination stop combination
  bool read_passenger_rates_format3 (istream& in); // reads the passenger rates in the format of arrival rate per OD in terms of stops (no path is pre-determined)
  bool readbusstops_distances_format1 (istream& in); // !< reads distances between stops through vectors at the stop level - relevant only for demand format 3
  bool readbusstops_distances_format2 (istream& in); // !< reads distances between stops through matrices between sets of stops - relevant only for demand format 3
  bool read_travel_time_disruptions (istream& in); // reads the expected travel time between stops due to disruptions - does not affect the actual travel time, just passengers expectations in case of information provision
  bool read_dwell_time_function (istream& in); // reads the dwell time function structure and coefficients
  bool read_bustype (istream& in); // reads a bus type
  bool read_busvehicle(istream& in); // reads a bus vehicle 
  /** @ingroup DRT 
      @{
  */
  bool readcontrolcenters(const string& name); //!< reads global DRT parameters, control centers and initial strategy configurations
  bool read_unassignedvehicle(istream& in); //!< reads a bus vehicles that are initialized without a trip assigned to them
  ODstops* get_ODstop_from_odstops_demand(int os_id,int ds_id); //!< returns the ODstop with origin stop id os_id and destination stop id ds_id from odstops_demand, nullptr if not found
  Pass_path* get_pass_path_from_id(int path_id) const; //!< returns passenger path from its id, nullptr if not found. Note that the path IDs do not necessarily match those read from input files
  /**}*/
  bool read_transit_path_sets(string name); // reads the path set for every OD pair (an output file generated by the choice-set generation progress)
  bool read_transit_path(istream& in); //reads a single path
  void generate_stop_ods(); // generate stop od pairs between all stop combinations
  void generate_consecutive_stops (); // stores for each stop all the stops that can be reached within a direct trip
  bool find_direct_paths (Busstop* bs_origin, Busstop* bs_destination); // finds direct paths and generate new direct paths
  void generate_indirect_paths (); // generates new indirect paths
  vector<vector<Busline*> > compose_line_sequence (Busstop* destination);  // compose the list of direct lines between each pair of intermediate stops
  vector<vector<Busstop*> > compose_stop_sequence ();  // compose the list of stops in path definiton structure
 void find_all_paths (); // goes over all OD pairs to generate their path choice set
 void find_all_paths_fast ();
 void find_all_paths_with_OD_for_generation ();
//  void find_recursive_connection (Busstop* origin, Busstop* destination); // search recursively for a path (forward - from origin to destination) WITHOUT walking links
  void find_recursive_connection_with_walking (Busstop* origin, Busstop* destination); // search recursively for a path (forward - from origin to destination) WITH walking links
  void find_recursive_connection_with_walking (Busstop* origin); // search recursively for paths (forward - from origin) WITH walking links
  void merge_paths_by_stops (Busstop* origin_stop, Busstop* destination_stop);  // merge paths with same lines for all legs (only different transfer stops)
  void merge_paths_by_common_lines (Busstop* origin_stop, Busstop* destination_stop);  // merge paths with lines that have identical route between consecutive stops
  bool compare_same_lines_paths (Pass_path* path1, Pass_path* path2); // checks if two paths are identical in terms of lines
  bool compare_same_stops_paths (Pass_path* path1, Pass_path* path2); // checks if two paths are identical in terms of stops
  bool compare_common_partial_routes (Busline* line1, Busline* line2, Busstop* start_section, Busstop* end_section); // checks if two lines have the same route between two given stops
  bool check_constraints_paths (Pass_path* path); // checks if the path meets all the constraints
  bool check_path_no_repeating_lines (vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops_); // checks if the path does not include going on and off the same bus line at the same stop
//  bool check_sequence_no_repeating_lines(vector<vector<Busline*> > lines, vector <Busstop*> stops_); 
  bool check_path_no_repeating_stops (Pass_path* path); // chceks if the path deos not include going through the same stop more than once
  bool check_sequence_no_repeating_stops (vector<Busstop*> stops); // chceks if the sequence does not include going through the same stop more than once
  void static_filtering_rules (Busstop* stop); // delete paths which do not fulfill the global filtering rules
  void dominancy_rules (Busstop* stop); // delete paths which are dominated by other alterantive paths
  bool totaly_dominancy_rule (ODstops* odstops, vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops); // check if there is already a path with shorter IVT than the potential one
//  bool downstream_dominancy_rule (Pass_path* check_path); // check whether there is already a path with a transfer stop closer to the destination (to avoid further downstream transfers on the same line)
  bool check_consecutive (Busstop* first, Busstop* second); // checks whether second is consecutive of first 
  bool check_path_constraints(Busstop* destination); // check constraints during search process, return true if constraints are fulfilled
  bool check_stops_opposing_directions (Busstop* origin, Busstop* destination); // checks whether the
  bool check_path_no_opposing_lines (vector<vector<Busline*> > lines);
  const vector<Busstop*> & get_cons_stops (Busstop* stop) {return consecutive_stops[stop];}
  const vector<Busline*> & get_direct_lines (ODstops* odstops) {return od_direct_lines[odstops];}
  double calc_total_in_vechile_time (vector<vector<Busline*> > lines, vector<vector<Busstop*> > stops); // according to scheduled time
  bool read_od_pairs_for_generation (string name);
  void add_busstop_to_name_map(string /*bus_stop_name*/,Busstop* /*bus_stop_ptr*/);
  Busstop* get_busstop_from_name(string /*bus_stop_name*/);

#ifndef _NO_GUI
	double get_width_x() {return width_x;} //!< returns image width in original coordinate system
	double get_height_y() {return height_y;} //!< ... height ...
	void set_background (string name) {if (drawing) drawing->set_background(name.c_str());}
#endif // _NO_GUI 

protected:
	//vector <Node*> nodes;
	map <int, Node*> nodemap; //!< 
	//vector <Origin*> origins;
	map <int, Origin*> originmap; //!< 
	//vector <Destination*> destinations;
	map <int, Destination*> destinationmap; //!< 
	//  vector <Junction*> junctions;
	map <int, Junction*> junctionmap; //!< 
	vector <BoundaryOut*> boundaryouts; // Remove Later...
	map <int, BoundaryOut*> boundaryoutmap; //!< 
	vector <BoundaryIn*> boundaryins; // Remove Later...
	map <int, BoundaryIn*> boundaryinmap; //!< 
	//  vector <Link*> links;
	map <int, Link*> linkmap; //!< 
	// vector <Sdfunc*> sdfuncs;
	map <int, Sdfunc*> sdfuncmap; //!< 
	//  vector <Turning*> turnings;
	map <int, Turning*> turningmap; //!< 
	// vector <Server*> servers;
	map <int, Server*> servermap; //!< 
	vector <ChangeRateAction*> changerateactions; //!<
//	vector <Route*> routes;	
	multimap <odval, Route*> routemap; //!< 
	vector <ODpair*> odpairs; //!< keep using OD pair vector for now, as map is too much of a hassle with two indices.
	// map <int, ODpair*> odpairmap; 
	vector <Incident*> incidents;
	vector <VirtualLink*> virtuallinks;
	map <int, VirtualLink*> virtuallinkmap; //!< 
	// Turning penalties
	vector <TurnPenalty*> turnpenalties;
	// Vehicle types
	Vtypes vehtypes;

	vector <double> incident_parameters; // yes this is very ugly, as is the web of functions added, but I'll take them out asap
	vector <Stage*> stages;
	vector <SignalPlan*> signalplans;
	vector <SignalControl*> signalcontrols;
	// Public Transport
	vector <Busline*> buslines; //!< the buslines that generate bus trips on busroutes, serving busstops
	vector <Bustrip*> bustrips;  //!< the trips list of all buses
	vector <Busstop*> busstops; //!< stops on the buslines
	map<int, Busstop*> busstopsmap; //!< map of stops on buslines with id as key
	vector <Busroute*> busroutes; //!< the routes that buses follow
	vector <Dwell_time_function*> dt_functions;
    vector <Bustype*> bustypes; // types of bus vehicles
    vector <Bus*> busvehicles; // a list of the bus vehicles
	vector <ODstops*> odstops;
	map <Busstop*,vector<ODstops*> > odstops_map;
    map <string,Busstop*> busstop_name_map; //!< mapping PT node names to their object
	vector <ODzone*> odzones; 
	vector <ODstops*> odstops_demand; // contains only ODs with a non-zero demand
	vector<pair<Busstop*,Busstop*> > od_pairs_for_generation;
	vector<Busstop*> collect_im_stops; // compose the list of stops for a path
	vector<double> collect_walking_distances; // compose the list of walking distances for a path
	map <ODstops*, vector<Busline*> > od_direct_lines; // contains all direct lines between a pair of stops
//	map<int,map<int, vector<Busline*> > > direct_lines; // contains all direct lines between a couple of stops
	map<Busstop*,vector<Busstop*> > consecutive_stops; // contains all the stops that can be reached within no transfer per stop

	Day2day* day2day=nullptr;
	map<ODSL, Travel_time> wt_rec; //the record of waiting time data
	map<ODSLL, Travel_time> ivt_rec; //the record of in-vehicle time data
	int day=0;

/** @ingroup DRT
    @{
*/
	//DRT implementation
#include <tuple>
    typedef std::tuple<Bus*, Busstop*, Controlcenter*, double> DrtVehicleInit; //!< un-scheduled vehicle, initial busstop, initial controlcenter, initial time
	map<int, Controlcenter*> ccmap; //!< map of all control centers with id as key
	vector <DrtVehicleInit> drtvehicles; //!<  vector of all initially unassigned vehicles that are not assigned a schedule and line on input, along with values used for their initialization (used in Network::init())
public:
	vector<DrtVehicleInit> get_drtvehicles_init() { return drtvehicles; }
/**@}*/
protected:
	//Shortest path graph
#ifndef _USE_VAR_TIMES
	Graph<double, GraphNoInfo<double> > * graph;
#else
	Graph<double, LinkTimeInfo>* graph=nullptr;
#endif
	// Random stream
	Random* random=nullptr;

	//GUI
#ifndef _NO_GUI
	Drawing* drawing=nullptr; //!< the place where all the Icons live
	QPixmap* pm=nullptr; //!< the place where the drawing is drawn
	QMatrix* wm=nullptr; //!< worldmatrix that contains all the transformations of the drawing (scaling, translation, rotation, &c)
	QMatrix initview_wm; //!< world matrix that transform the drawing to the inital view
	double scale=100.0; //!< contains the scale of the drawing
	double width_x=100.0; //!< width of boundaries of drawing in original coordinate system
	double height_y=100.0; //!< height of boundaries of drawing in org. coord. sys.
#endif // _NO_GUI
	// Eventlist
	Eventlist* eventlist;
	// start of read functions
	bool readincidents(istream& in);
	bool readincident(istream & in);
	bool readincidentparams (istream &in);
	bool readincidentparam (istream &in);
	bool readx1 (istream &in);
	bool readtimes(istream& in);
	bool readtime(istream& in);
	bool readnodes(istream& in);
	bool readnode(istream& in);
	bool readsdfuncs(istream& in);
	bool readsdfunc(istream& in);
	bool readlinks(istream& in);
	bool readlink(istream& in);
	bool readservers(istream& in);
	bool readserver(istream& in);
	bool readturning(istream& in);
	bool readgiveway(istream& in);
	bool readgiveways(istream& in);
	bool readroutes(istream& in);
	bool readroute(istream& in);
	bool readods(istream& in);
	bool readod(istream& in, double scale=1.0);
	bool readvtype (istream & in);
	bool readvirtuallink(istream & in);
	bool readrates(istream & in);
	ODRate readrate(istream& in, double scale);
	bool readserverrate(istream& in);
	bool readsignalcontrol(istream & in);
	bool readsignalplan(istream& in, SignalControl* sc);
	bool readstage(istream& in, SignalPlan* sp);
	bool readparameters(string name);
	// end of read functions
	vector <string> filenames; //!< filenames for input/output as read in the master file
	string workingdir;
	unsigned int replication=0;
	int runtime=0; //!< == stoptime
	int starttime=0;
	bool calc_paths=false; //!< if true new shortest paths are calculated and new paths added to the route file
	double time=0.0;
	int no_ass_links=0; //!< number of links observed in assignment matrix
	// Linktimes
	int nrperiods=0; //!< number of linktime periods
	double periodlength=0; //!< length of each period in seconds.
	LinkTimeInfo* linkinfo=nullptr;
	// PVM communicator
#ifdef _PVM   
	PVM * communicator;
	int friend_id;
#endif // _NO_PVM 
#ifdef _VISSIMCOM
	VISSIMCOM * communicator;
	string vissimfile;
#endif // _VISSIMCOM
	// ODMATRIX
	ODMatrix odmatrix;
}; 
//end of network definition

//! Incident Class contains the methods needed for the simulation of incidents on a link.
//! It derives from Action, and the execute() creates four events:
//! 1. Set the incident on the affected link at the pre-specified time. This changes the speed-density function
//! 2. Start the broadcast of information to the affected links and origins
//! 3. End the incident on the affected link
//! 4. End the broadcast of the information.

class Incident: public Action
{
public:
	Incident (int lid_, int sid_, double start_, double stop_,double info_start_,double info_stop_, Eventlist* eventlist, Network* network_, bool blocked_);
    virtual ~Incident() {}
	bool execute(Eventlist* eventlist, double time); //!< Creates the events needed for setting and ending the incident and information broadcast
	void broadcast_incident_start(int lid); //!< Broadcasts the incident to all the affected links and origins. At origins a flag will be set so all created vehicles will automatically switch, until notification that incident is over
	void broadcast_incident_stop(int lid); //!< Broadcasts the end of an incident to all Origins (Not needed for Links? Check...)

	void set_affected_links(map <int, Link*> & affected_links_) {affected_links=affected_links_;} //!< sets the links that are affected by the incident
	void set_affected_origins(map<int, Origin*> & affected_origins_) {affected_origins=affected_origins_;} //!< sets the origins that are affected by the incident
	void set_incident_parameters(vector <double> & incident_parameters_) {incident_parameters = incident_parameters_;} //!< sets the incident parameters that are used for the choices
#ifndef _NO_GUI
	void set_icon(IncidentIcon* icon_) {icon=icon_;}
	void set_visible(bool val) {icon->set_visible(val);}
#endif

private:
	map <int, Link*> affected_links; 
	map <int, Origin*> affected_origins;
	double start;
	double stop;
	double info_start;
	double info_stop;
	int lid; //!< Incident link
	int sid;
	Network* network;
	bool blocked;
	vector <double> incident_parameters; 
#ifndef _NO_GUI
	IncidentIcon* icon=nullptr;
#endif
};

class TurnPenalty
{
public:
	int from_link;
	int to_link;
	double cost;
};

class NetworkThread: public QThread
{
public:

	NetworkThread (string masterfile,int threadnr = 1,long int seed=0):masterfile_(masterfile),threadnr_(threadnr),seed_(seed) 
		{
        Q_UNUSED(threadnr_)
			theNetwork= new Network();
			 
			 if (seed != 0)
			 {
				theRandomizers[0]->seed(seed);
			 }
			if (seed_)
					theNetwork->seed(seed_);
		}
	void init () 
		{
			theNetwork->readmaster(masterfile_);
			runtime_=theNetwork->executemaster();
		}
	void run ()
	  {				
				theNetwork->step(runtime_);
	  }
	void saveresults (unsigned int replication = 0)
	  {
		  cout << "Saving results" << endl;
		  theNetwork->writeall(replication);
		  cout << "Saved and done!" << endl;
	  }
	void reset ()
	{
		cout << "Resetting" << endl;
		theNetwork->delete_passengers();
		theNetwork->reset();
		cout << "Reset done!" << endl;
	}
	 ~NetworkThread () 
	  {
			delete theNetwork;
	  }

    Network* getNetwork()
    {
        return theNetwork;
    }
private:

    string masterfile_;
    int threadnr_;
    long int seed_;

    Network* theNetwork;
    double runtime_=0.0;

};


#endif
