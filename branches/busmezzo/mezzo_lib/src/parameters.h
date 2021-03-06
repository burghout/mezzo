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

#ifndef PARAMETERS_
#define PARAMETERS_
/*! Here go all the constants that are used as parameters, later they will be read from a file.
	There should be a global Parameters class, and a global var Parameters theParameters; and all use of parameters should then be 
	theParameters.randseed for example
	Also, this file should specify which modules are used and which ones not, instead of the current DEFINES
*/
#include <string>
#include <vector>
#include "Random.h"
#include <QtGlobal>

#ifndef _NO_GUI
#include <qcolor.h>
#endif


// DEFINES

#define _DETERMINISTIC_OD_SERVERS
#define _DETERMINISTIC_ROUTE_CHOICE
#define _DETERMINISTIC_VTYPES
//#define _MULTINOMIAL_LOGIT
#define _UNSAFE // faster reading, but less checking of input data
#define _BUSES

// GLOBAL VARIABLES
extern long int randseed; // random seed
extern int vid;     // global vehicle id nr.
extern int pid;     // global passenger id nr.
extern double time_alpha;

// OLD Network.hh parameters

const double an_step=0.1;  // 10 times per second
const double speedup=50.0;
const double disturbance=0.10; // the disturbance in the shortest path link travel times.


// OLD Server.hh parameters
const double min_hdway=0.1; // minimum headway
const double cf_hdway=1.5;   // average carfolling headway
const double sd_cf_hdway=0.5; // sd of carfollowing headways (N(mu,sd))
const double max_cf_hdway=2.0; // minimum hdway for vehicles not in platoons
// Parameters for OD traffic generation
// const double odsig=0.5; // the standard sigma for bound traffic (N(1.5,0.5))


// sdfunc.hh parameters
const int min_speed=-1; //OLD PARAM to override the SD functions' min speeds, if Positive

// OLD q.hh parameters Determine Route-Changing behaviour. Needs to be changed with the route-switching module.
const double perc_receive_broadcast=0.7; // percentage of drivers that receive the INCIDENT broadcast.
const double dont_know_delay=0.114; // percentage of drivers that don't know delay


//OD parameters
//const double small_od_rate = 3.0; // determines when a OD can have more than 1 route

// PARAMETER CLASS
/*!  Here go all the global parameters used in the simulation and the GUI
*/
class Parameters
{	
public:
	Parameters () ;
	bool read_parameters (istream & in);
	void write_parameters(ostream & out);

// THE (PUBLIC) PARAMETERS
	// Drawing parameters
		
	// parameters for output analysis
	unsigned int viewmode; //<! Sets the view mode for the graphics: 0 = simulation run (standard), 1= show output data (such as flows etc)
	double max_thickness_value; // regulates scaling of values to thickness
	double max_colour_value;
	double min_thickness_value;
	double min_colour_value;
	bool inverse_colour_scale;
	int thickness_width;
	int show_period;
	double running_time; //!< total running time
	bool show_link_names;//!< if true show link names in output
	bool show_link_ids; //!< if true link ids are shown in output
	
	bool draw_link_ids; //!< If true link ID's are shown
	int link_thickness; //!< Thickness with which links are drawn
	int node_thickness; //!< Thickness with which nodes are drawn	
	int node_radius; //!< Radius with which nodes are drawn
	int queue_thickness; //!< Thickness with which the Queue part of a Link is drawn
	int selected_thickness; //!< Thickness with which selected objects are drawn
	int text_size; //!< Size of text in the network image
	bool show_background; //!< If true background image is displayed behind network (if any image was loaded)
	int background_x; //!< start_x for background image
	int background_y; //!< start_y 
	double background_scale; //!< scale for background image

#ifndef _NO_GUI
	QColor linkcolor; //!< Colour of links
	QColor nodecolor; //!< Colour of nodes
	QColor queuecolor; //!< Colour of queue part of links
	QColor backgroundcolor; //!< Background colour
	QColor selectedcolor; //!< Colour for selected objects
#endif
	double gui_update_step; //!< TODO: implement the gui update step!
	double zerotime; //!< Start time of simulation
	
// #moe_parameters
   double moe_speed_update; //!< update interval for link speed data
   double moe_inflow_update; //!<  update interval for link inflow data
   double moe_outflow_update; //!<  update interval for link outflow data
   double moe_queue_update; //!<  update interval for link queuelength data
   double moe_density_update; //!<  update interval for link density data
   double linktime_alpha; //!< smoothing factor for link travel times

// #assignment_matrix_parameters
   bool use_ass_matrix; //!< if true an assignment matrix is generated for all links in ass_links.dat
   double ass_link_period; //!< interval length for link periods in Ass. matrix
   double ass_od_period; //!< interval length for OD periods in ass. matrix

//#turning_parameters
   int default_lookback_size; //!< default queue look-back
   double turn_penalty_cost;  //!< added penalty in shortest path alg. if a turn is forbidden
   bool use_giveway; //!< if true, giveway logic is used
   double max_wait; //!< default max waiting time for give_way
// #server_parameters
   bool od_servers_deterministic; //!< if true the time headways in OD servers are determiunistic, otherwise neg_exp
   double odserver_sigma; //!< Obsolete since OD servers now have Mu=Sigma  (neg exp). Was for use with previous combined normal-neg_exp servers
   double sd_server_scale; // !< for the case study - scale for SD delay at the stochastic delay server
   int server_type; // !< can indicate that all servers are from a paricular type (3 - lognormal)

// #vehicle_parameters
   int standard_veh_length; //!< used to calculate a-priori capacity of links

// #route_parameters
   double update_interval_routes; //!< interval for re-calculating route costs based on time-dependent travel times.
   double mnl_theta; //!< parameter for Multinomial LOGIT route choice
   double kirchoff_alpha; //!< parameter for Kirchoff route choice
   bool delete_bad_routes; //!< deletes bad (long) routes as well as cleans up nr of routes for small od pairs.
   double max_rel_route_cost; //!< max relative cost of a route, compared to the minimum cost route, to be allowed in the choice set.
   double small_od_rate; //!< minimum od_rate for an od-pair to have more than 1 route

   // statevar:
   bool shortest_paths_initialised; //!< true if shortest paths have been initialised
// #mime_parameters: ONLY for use in HYBRID situations
   double mime_comm_step; //!< Communication step for MICRO-MESO information exchange
   int mime_min_queue_length; //!< min queue length for stopping / starting shockwaves
   double mime_queue_dis_speed; //!< queue dissipation speed if not calculated from flow & density data

// transit demand parameters:
   int demand_format;
   double demand_scale; // !< currently for demand format 1 only - multiplies the hourly arrival rate
   int choice_set_indicator; //!< 0 indicates process path-set generation; 1 indicates read path-set input file
   int pass_day_to_day_indicator; // 0 if irrelevant; 1 indicates read pass memory input and incorporate it on OD level; 2 indicates passenger specific memory
   int in_vehicle_d2d_indicator; // 0 if irrelevant; 1 indicates read pass memory input and incorporate it on OD level; 2 indicates passenger specific memory
   double break_criterium;
   double transfer_coefficient;
   double in_vehicle_time_coefficient;
   double waiting_time_coefficient;
   double walking_time_coefficient;
   double average_walking_speed;
   int max_nr_extra_transfers;
   int absolute_max_transfers;
   double max_in_vehicle_time_ratio;
   double max_walking_distance;
   double max_waiting_time;
   double dominancy_perception_threshold;
   int choice_model; // !< RUM used at passengers' route choice decisions: 1 - MNL; 2 - PSL with legs defining overlapping.
   int real_time_info; //!< real-time information at the network: 0 - none; 1 - for all lines stoping at each stop; 2 - for all lines stopping at all connected stop; 3 - for the entire network; 4 - based on the stop-specific values
   double share_RTI_network; // indicates the share of the population of travellers that has access to RTI at the network-level (smart phone penetration rate), takes 0 to 1 values
   double start_pass_generation; 
   double stop_pass_generation;
   bool od_pairs_for_generation;
    double gate_generation_time_diff; // passengers are generated slightly before service leaves to account for boarding time

// transit control parameters
   double riding_time_weight;
   double dwell_time_weight;
   double waiting_time_weight;
   double holding_time_weight;
   double compliance_rate; //!< Share [0,1] of bus trips that comply with the control strategy (assigned per trip)
   bool transfer_sync; //!< true if transfer synchronization is being used. Will effect the way the line definition in transit_network.dat is read.

// day2day assignment
   double default_alpha_RTI; //!< Initial value of the real-time information credibility coefficient set for all travellers

// TODO: Implement the use of the following paramaters
   double vissim_step; //!< time step for the VISSIM micro model
   double sim_speed_factor; //!< REALTIME factor to keep the hybrid model running at a fixed speed
};


extern Parameters* theParameters ;


// Some fixes to deal with the way Microsoft VS deals with min and max. all occurrences of min and max should
// be replaced by the macros _MIN and _MAX, which will be converted to the appropriate functions
// given the compiler.
#ifndef _MIN
#define _MIN min
#endif // _MIN

#ifndef _MAX
#define _MAX max
#endif // _MAX

#endif

