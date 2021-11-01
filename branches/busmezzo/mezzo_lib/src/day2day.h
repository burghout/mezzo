#ifndef DAY2DAY_
#define DAY2DAY_

#include <map>
#include "od_stops.h"

using namespace std;



struct ODSL //structure for comparing ODSL combinations
{
	int pid;
	int orig;
	int dest;
	int stop;
	int line;

	bool operator == (const ODSL& rhs) const
	{
		return (pid == rhs.pid && orig == rhs.orig && dest == rhs.dest && stop == rhs.stop && line == rhs.line);
	}

	bool operator < (const ODSL& rhs) const
	{
		if (pid != rhs.pid)
			return pid < rhs.pid;
		else if (orig != rhs.orig)
			return orig < rhs.orig;
		else if (dest != rhs.dest)
			return dest < rhs.dest;
		else if (stop != rhs.stop)
			return stop < rhs.stop;
		else
			return line < rhs.line;
	}

	void print() const
	{
	    cout << "PID-" << pid << "-ODSL: " << orig << dest << stop << line << endl;
	}

};

struct ODSLL
{
	int pid;
	int orig;
	int dest;
	int stop;
	int line;
	int leg;

	bool operator == (const ODSLL& rhs) const
	{
		return (pid == rhs.pid && orig == rhs.orig && dest == rhs.dest && stop == rhs.stop && line == rhs.line && leg == rhs.leg);
	}

	bool operator < (const ODSLL& rhs) const
	{
		if (pid != rhs.pid)
			return pid < rhs.pid;
		else if (orig != rhs.orig)
			return orig < rhs.orig;
		else if (dest != rhs.dest)
			return dest < rhs.dest;
		else if (stop != rhs.stop)
			return stop < rhs.stop;
		else if (line != rhs.line)
			return line < rhs.line;
		else
			return leg < rhs.leg;
	}

	void print() const
	{
	    cout << "PID-" << pid << "-ODSLL: " << orig << dest << stop << line << leg << endl;
	}

};


struct Travel_time //structure for saving and adding data
{
	int counter;
	int day;
	float tt[5]; // EXP, PK, RTI (or crowding for ivt), anticip, anticip_EXP
	float alpha[3]; // EXP, PK, RTI (or crowding for ivt)
	float convergence;
	float temp_kapa_ATT = 0.0; // temporary placeholder for outputting the most recent weight used for discounting more recent travel time experiences when updating anticipated travel time
    
    friend float operator/ (const Travel_time& lhs, const Travel_time& rhs);

	Travel_time& operator += (const Travel_time& rhs)
	{
		counter += rhs.counter;
		tt[0] += rhs.tt[0];
		tt[1] += rhs.tt[1];
		tt[2] += rhs.tt[2];
		tt[3] += rhs.tt[3];
		tt[4] += rhs.tt[4];
		alpha[0] += rhs.alpha[0];
		alpha[1] += rhs.alpha[1];
		alpha[2] += rhs.alpha[2];
		return *this;
	}

	Travel_time &operator /= (const int& rhs)
	{
		tt[0] /= rhs;
		tt[1] /= rhs;
		tt[2] /= rhs;
		tt[3] /= rhs;
		tt[4] /= rhs;
		alpha[0] /= rhs;
		alpha[1] /= rhs;
		alpha[2] /= rhs;
		return *this;
	}

	void print_wt() const // print the contents of tt, alpha and convergence assuming that this is for waiting times
	{
	    qDebug() << "Printing day2day WT for day" << day;
		qDebug() << "\t wt_exp        : " << tt[0];
		qDebug() << "\t wt_pk         : " << tt[1];
		qDebug() << "\t wt_rti        : " << tt[2];
		qDebug() << "\t wt_anticip    : " << tt[3];
		qDebug() << "\t wt_anticip_exp: " << tt[4] << Qt::endl;
		qDebug() << "\t alpha_exp     : " << alpha[0];
		qDebug() << "\t alpha_pk      : " << alpha[1];
		qDebug() << "\t alpha_rti     : " << alpha[2] << Qt::endl;
		qDebug() << "\t kapa_AWT   : " << temp_kapa_ATT;
		qDebug() << "\t convergence   : " << convergence;
		qDebug() << "\t counter       : " << counter;
	}

	void print_ivt() const // print the contents of tt, alpha and convergence assuming that this is for in-vehicle times
	{
	    qDebug() << "Printing day2day IVT for day" << day;
		qDebug() << "\t ivt_exp        : " << tt[0];
		qDebug() << "\t ivt_pk         : " << tt[1];
		qDebug() << "\t ivt_crowding   : " << tt[2];
		qDebug() << "\t ivt_anticip    : " << tt[3];
		qDebug() << "\t ivt_anticip_exp: " << tt[4] << Qt::endl;
		qDebug() << "\t alpha_exp      : " << alpha[0];
		qDebug() << "\t alpha_pk       : " << alpha[1];
		qDebug() << "\t alpha_crowding : " << alpha[2] << Qt::endl;
		qDebug() << "\t kapa_AIVT   : " << temp_kapa_ATT;
		qDebug() << "\t convergence    : " << convergence;
		qDebug() << "\t counter        : " << counter;
	}
} ;

//template <typename id_type> float insert (map<id_type, Travel_time>& ODSL_reg, map<id_type, Travel_time>& ODSL_data); //Method for inserting data for one day into record

float insert (map<ODSL, Travel_time>& ODSL_reg, map<ODSL, Travel_time>& ODSL_data);
float insert (map<ODSLL, Travel_time>& ODSLL_reg, map<ODSLL, Travel_time>& ODSLL_data);

class Day2day
{
private:
	map<ODSL, Travel_time> wt_day; //record of ODSL data for the current day
	map<ODSLL, Travel_time> ivt_day; //record of ODSL data for the current day

	float wt_alpha_base[3];
	float ivt_alpha_base[3];

	float kapa[4];

	float v = 0; //salience parameter
	float v_c = 3; //crowding salience parameter
	float v1 = 3; //trust parameter
	float r = 1; //recency parameter
	int nr_of_reps = 0;
	int day = 0;
	bool aggregate = false;
	bool individual_wt = false;
	bool individual_ivt = false;

    void calc_anticipated_wt(Travel_time& row, bool is_flexible_leg = false);
    void calc_anticipated_ivt(Travel_time& row, bool is_flexible_leg = false);

	int nr_of_passengers = 0;
	int nr_of_changes = 0;
	int nr_of_legs = 0;
	double total_waiting_time = 0;
	double total_wt_pk = 0;
	double total_wt_rti = 0;
	double total_wt_exp = 0;
	double total_wt_anticip = 0;
	int total_nr_missed = 0;
	double total_in_vehicle_time = 0;
	double total_ivt_pk = 0;
	double total_ivt_exp = 0;
	double total_ivt_anticip = 0;
	double total_crowding = 0;
	double total_acrowding = 0;
	int nr_on_line_2 = 0;

public:
	Day2day (int nr_of_reps_);
	void reset ();
    void set_salience(float v_) {v = v_;}
    void set_crowding_salience(float v_c_) {v_c = v_c_;}
    void set_trust(float v1_) {v1 = v1_;}
    void set_recency(float r_) {r = r_;}
	void update_day (int d);
	void write_output (string filename, string addition);
	map<ODSL, Travel_time>& process_wt_replication (vector<ODstops*>& odstops, map<ODSL, Travel_time> wt_rec, const vector<Busline*>& buslines);
	map<ODSLL, Travel_time>& process_ivt_replication (vector<ODstops*>& odstops, map<ODSLL, Travel_time> ivt_rec, const vector<Busline*>& buslines);

	/** @ingroup DRT fwf_wip
        @{
    */
    static void print_wt_alphas(const map<ODSL, Travel_time>& wt_records, const ODSL& odsl); // print the alphas for a given ODSL record
    static void print_ivt_alphas(const map<ODSLL, Travel_time>& ivt_records, const ODSLL& odsll); // print the alphas for a given ODSLL record
    static void print_all_wt_records(const map<ODSL, Travel_time>& wt_records); // print out the contents of a ODSL table
    static void print_all_ivt_records(const map<ODSLL, Travel_time>& ivt_records); // print out the contents of a ODSLL table
    static void write_wt_alphas_header(string filename);
    static void write_wt_alphas(string filename, const map<ODSL, Travel_time>& wt_records); //write out all ODSL alphas to a csv file
	static void write_ivt_alphas_header(string filename);
	static void write_ivt_alphas(string filename, const map<ODSLL, Travel_time>& ivt_records); //write out all ODSLL alphas to a csv file
	/**@}*/
};

#endif
