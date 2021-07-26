#include "day2day.h"
#include <map>
#include <iostream>
#include <fstream>
#include <string>

enum k {EXP, PK, RTI, anticip, anticip_EXP};
enum l {e0, e1, crowding, e3, e4};
enum m {wt, ivt};

float operator/ (const Travel_time& lhs, const Travel_time& rhs)
{
	float quotient;
	if (rhs.tt[anticip] > 0)
		quotient = lhs.tt[anticip] / rhs.tt[anticip];
	else
		quotient = 1.0f;
	return quotient;
};


/**
 * @notes
 *	Calculates the convergence quotient (which is either based on individual experiences, or the average over individual experiences)
*/
template <typename id_type>
map<id_type, Travel_time>& operator << (map<id_type, Travel_time>& ODSLreg, pair<const id_type, Travel_time>& row) //if existing ODSL is found, data is replaced else a new row is inserted
{
    typename map<id_type, Travel_time>::iterator odsl_sum = ODSLreg.find(row.first);
	if (odsl_sum != ODSLreg.end())
	{
		row.second.convergence = row.second / odsl_sum->second;
		++row.second.day;
		odsl_sum->second = row.second;
	}
	else
		row.second.day = 1;
		ODSLreg.insert(row);
	return ODSLreg;
};


//note by Flurin: replace template by adding the specializations as two normal functions (see below)
/*
 template <typename id_type>
 float insert (map<id_type, Travel_time>& ODSL_reg, map<id_type, Travel_time>& ODSL_data) //Method for inserting data for one day into record
{
	float crit = 0;
    for (typename map<id_type, Travel_time>::iterator row = ODSL_data.begin(); row != ODSL_data.end(); row++) //aggregate over days
	{
		row->second /= row->second.counter; //finish the averaging by dividing by the number of occurences which is counted when adding
				
		ODSL_reg << *row; //if existing ODSL is found, data is replaced else a new row is inserted

		crit += abs(row->second.convergence - 1); //for the break criterium
	}

	crit /= ODSL_data.size(); //to get the average

	return crit;
};
 */


/**
 * @brief ODSL data (I think) usually corresponds to data that has already been aggregated over a day. I.e. if we have shared OD info, then all passenger experiences for a given od are shared
 *	First we finish the average of each ODSL row by dividing by the number of experiences in the sum. 
 * @param ODSL(L)_reg  : the record to insert into
 * @param ODSL(L)_data : the record to be inserted
 * @return the convergence criteria. The average change in anticipated waiting time between days over all segments and all travelers. 
*/
float insert (map<ODSL, Travel_time>& ODSL_reg, map<ODSL, Travel_time>& ODSL_data) //Method for inserting data for one day into record
{
    float crit = 0;
    for (auto row = ODSL_data.begin(); row != ODSL_data.end(); ++row) //aggregate over days
    {
        row->second /= row->second.counter; //finish the averaging by dividing by the number of occurences which is counted when adding
        
        ODSL_reg << *row; //if existing ODSL is found, data is replaced else a new row is inserted, also calculates the quotient of the anticipated wt and ivt of the current day and the previous day
        
        crit += abs(row->second.convergence - 1); //for the break criterium
    }
    
    crit /= ODSL_data.size(); //to get the average
    
    return crit;
};

float insert (map<ODSLL, Travel_time>& ODSLL_reg, map<ODSLL, Travel_time>& ODSLL_data) //Method for inserting data for one day into record
{
    float crit = 0;
    for (auto row = ODSLL_data.begin(); row != ODSLL_data.end(); ++row) //aggregate over days
    {
        row->second /= row->second.counter; //finish the averaging by dividing by the number of occurences which is counted when adding
        
        ODSLL_reg << *row; //if existing ODSL is found, data is replaced else a new row is inserted, also calculates the quotient of the anticipated wt and ivt of the current day and the previous day
        
        crit += abs(row->second.convergence - 1); //for the break criterium
    }
    
    crit /= ODSLL_data.size(); //to get the average
    
    return crit;
};



/**
 * @notes
 *	Aggregates inserts pair into map, if already exists, then add together the results. Average is calculated between current and previous day. The 'recency' parameter is held within the alpha coeff calculations....
*/
template <typename id_type>
map<id_type, Travel_time>& operator += (map<id_type, Travel_time>& ODSLreg, const pair<const id_type, Travel_time>& row) //if existing ODSL is found, data is added, else a new row is inserted
{
    typename map<id_type, Travel_time>::iterator odsl_sum = ODSLreg.find(row.first);
	if (odsl_sum != ODSLreg.end())
		odsl_sum->second += row.second;
	else
		ODSLreg.insert(row);
	return ODSLreg;
};


/**
 * @notes
 *	Used in two different contexts (i think).
 *		(1) Used in 'process_wt_replication or process_ivt_replication' when adding the temporary ODSL(L)_rep of aggregated waiting time experiences, to the Day2day record
 *		(2) Used when adding the final aggregated ODSL(L)_day to ODSL(L)_rec, the full accumulated history so far
 */
template <typename id_type>
map<id_type, Travel_time>& operator += (map<id_type, Travel_time>& ODSL_reg, map<id_type, Travel_time>& ODSL_data)
{
    for (typename map<id_type, Travel_time>::iterator row = ODSL_data.begin(); row != ODSL_data.end(); ++row) //aggregate over replications
	{
		row->second /= row->second.counter; //finish the averaging by dividing by the number of occurences which is counted when adding
		row->second.counter = 1;

		ODSL_reg += *row; //if existing ODSL is found, data is added, else a new row is inserted
	}

	return ODSL_reg;
}

template <typename id_type>
void insert_alphas (const id_type& tt_odsl, Travel_time& tt, map<id_type, Travel_time>& tt_rec, const float alpha_base[], const int& day)
{
	//insert base values
	tt.tt[anticip_EXP] = -1.0f;
	tt.alpha[0] = alpha_base[0];
	tt.alpha[1] = alpha_base[1];
	tt.alpha[2] = alpha_base[2];
	tt.day = 1;

	tt.counter = 1;
	tt.convergence = 2.0f;

	if (day != 1)
	{
		//load ODSLdep and replace base values when found
        typename map<id_type, Travel_time>::iterator odsl = tt_rec.find(tt_odsl);
		if (odsl != tt_rec.end())
		{
			tt.tt[anticip_EXP] = odsl->second.tt[anticip_EXP];
			tt.alpha[RTI] = odsl->second.alpha[RTI];
			tt.alpha[EXP] = odsl->second.alpha[EXP];
			tt.alpha[PK] = odsl->second.alpha[PK];
			tt.day = odsl->second.day + 1;
		}
	}
};

typedef map<ODSL, Travel_time>::iterator wt_map_iterator;
typedef map<ODSLL, Travel_time>::iterator ivt_map_iterator;

Day2day::Day2day (int nr_of_reps_)
{
	wt_alpha_base[RTI] = 0.5f;
	wt_alpha_base[EXP] = 0.0f;
	wt_alpha_base[PK] = 0.5f;

	ivt_alpha_base[EXP] = 0.0f;
	ivt_alpha_base[PK] = 1.0f;
	ivt_alpha_base[crowding] = 1.0f;

	if(fwf_wip::day2day_drt_no_rti)
	{
	    wt_alpha_base[RTI] = 0.0f;
		wt_alpha_base[PK] = 1.0f;
	}

	day = 1;
	v = 2.0f;
	r = 1.0f;

	kapa[RTI] = 1.0f;
	kapa[EXP] = 1.0f;
	kapa[PK] = 1.0f;
	kapa[anticip] = 1.0f;

	nr_of_reps = nr_of_reps_;
	aggregate = false;

	if (theParameters->pass_day_to_day_indicator == 2)
		individual_wt = true;
	else
		individual_wt = false;

	if (theParameters->in_vehicle_d2d_indicator == 2)
		individual_ivt = true;
	else
		individual_ivt = false;
}

void Day2day::reset ()
{
	wt_day.clear();
	ivt_day.clear();
}

void Day2day::update_day (int d)
{
	kapa[RTI] = sqrt(1.0f/d);
	kapa[EXP] = sqrt(1.0f/d);
	kapa[PK] = sqrt(1.0f/d);
	kapa[anticip] = 1.0f/d;
	day = d;
	wt_day.clear();
	ivt_day.clear();
}

void Day2day::write_output (string filename, string addition)
{
	map<ODSL, Travel_time> temp_output;
	for (auto row = wt_day.begin(); row != wt_day.end(); ++row)
	{
		const ODSL temp_odsl = {0, row->first.orig, row->first.dest, row->first.stop, row->first.line};
		pair<const ODSL, Travel_time> temp_row (temp_odsl, row->second);
		temp_output += temp_row;
	}
	for (auto row = temp_output.begin(); row != temp_output.end(); ++row) //aggregate over replications
	{
		row->second /= row->second.counter;
	}
	
    double average_nr_of_changes = static_cast<double>(nr_of_changes)  / static_cast<double>(nr_of_passengers) - 1;
    double average_nr_missed = static_cast<double>(total_nr_missed)  / static_cast<double>(nr_of_passengers);
	double average_waiting_time = total_waiting_time / nr_of_passengers;
	double average_wt_pk = total_wt_pk / nr_of_passengers;
	double average_wt_rti = total_wt_rti / nr_of_passengers;
	double average_wt_exp = total_wt_exp / nr_of_passengers;
	double average_wt_anticip = total_wt_anticip / nr_of_passengers;

	double average_in_vehicle_time = total_in_vehicle_time / nr_of_passengers;
	double average_ivt_pk = total_ivt_pk / nr_of_passengers;
	double average_ivt_exp = total_ivt_exp / nr_of_passengers;
	double average_ivt_anticip = total_ivt_anticip / nr_of_passengers;

	double average_travel_time = average_waiting_time + average_in_vehicle_time;
	ofstream out1(filename.c_str(),ios_base::app);
	out1.precision(0);
	out1 << fixed;
	out1 << day << "\t" << nr_of_passengers << "\t";
	out1 << average_waiting_time << "\t" << average_wt_pk << "\t" << average_wt_rti << "\t" << average_wt_exp << "\t" << average_wt_anticip << "\t";
	if (temp_output.size() <= 2)
	{
		for (auto row = temp_output.begin(); row != temp_output.end(); ++row)
		{
			out1 << row->first.line << "\t" << row->second.counter << "\t" << row->second.tt[EXP] << "\t" << row->second.tt[anticip_EXP] << "\t" << row->second.tt[anticip] << "\t";
		}
	}
	out1 << average_in_vehicle_time << "\t" << average_ivt_pk << "\t" << average_ivt_exp << "\t" << average_ivt_anticip << "\t";
	out1 << average_travel_time << "\t"; // << nr_on_line_2 << "\t";
	out1.precision(2);
	out1 << average_nr_of_changes << "\t" << total_crowding / nr_of_legs << "\t" << total_acrowding / nr_of_legs << "\t" << average_nr_missed << "\t";
	out1 << addition << endl;
}

map<ODSL, Travel_time>& Day2day::process_wt_replication (vector<ODstops*>& odstops, map<ODSL, Travel_time> wt_rec)
map<ODSL, Travel_time>& Day2day::process_wt_replication (vector<ODstops*>& odstops, map<ODSL, Travel_time> wt_rec, const vector<Busline*>& buslines)
{
	map<ODSL, Travel_time> wt_rep; //record of ODSL data for the current replication
	total_waiting_time = 0;
	total_wt_pk = 0;
	total_wt_rti = 0;
	total_wt_exp = 0;
	total_wt_anticip = 0;
	total_nr_missed = 0;
	nr_of_passengers = 0;
	nr_of_changes = 0;
	nr_on_line_2 = 0;


	// For each od collect list of passenger experiences, For each passenger calculate and anticipated EXP, and if information is shared then we average the experiences when calculated anticipated wt
	for (auto od_iter = odstops.begin(); od_iter != odstops.end(); ++od_iter)
	{
		map <Passenger*,list<Pass_waiting_experience> > pass_list = (*od_iter)->get_waiting_output();
		for (auto pass_iter1 = pass_list.begin(); pass_iter1 != pass_list.end(); ++pass_iter1)
		{
			nr_of_passengers++;
			list<Pass_waiting_experience> waiting_experience_list = (*pass_iter1).second;
			for (auto exp_iter = waiting_experience_list.begin(); exp_iter != waiting_experience_list.end(); ++exp_iter)
			{
				Travel_time wt;

				int pid = exp_iter->pass_id;
				int orig = exp_iter->original_origin;
				int dest = exp_iter->destination_stop;
				int stop = exp_iter->stop_id;
				int line = exp_iter->line_id;
				wt.tt[PK] = exp_iter->expected_WT_PK;
				wt.tt[RTI] = exp_iter->projected_WT_RTI;
				wt.tt[EXP] = exp_iter->experienced_WT;

				if (aggregate)
				{
					pid = 0;
					orig = 0;
					dest = 0;
				}
				if (!individual_wt)
				{
					pid = 0;
				}

				const ODSL wt_odsl = {pid, orig, dest, stop, line};

				//insert base values and replace when previous experience found
				insert_alphas(wt_odsl, wt, wt_rec, wt_alpha_base, day);


				bool is_flexible_leg = false; // true only if the current on-board experience was for a flexible line leg
				if(theParameters->drt)
				{
                    auto line_it = find_if(buslines.begin(), buslines.end(), [line](const Busline* l) { return l->get_id() == line; });
					assert(line_it != buslines.end());
					is_flexible_leg = (*line_it)->is_flex_line();
				}

				//calculate anticipated waiting time and add to experience for this replication
				calc_anticipated_wt(wt, is_flexible_leg);

				pair<const ODSL, Travel_time> wt_row (wt_odsl, wt);
				wt_rep += wt_row; //if existing ODSL is found, data is added, else a new row is inserted, @note this increments the 'counter' of the row. 

				//add to output;
				nr_of_changes++;
				total_waiting_time += wt.tt[EXP];
				total_wt_pk += wt.tt[PK];
				total_wt_rti += wt.tt[RTI];
				total_wt_exp += wt.tt[anticip_EXP];
				total_wt_anticip += wt.tt[anticip];
				total_nr_missed += exp_iter->nr_missed;
				if (line == 2)
					nr_on_line_2++;
			}
		}
	}

	if (nr_of_reps > 1 || theParameters->pass_day_to_day_indicator == 1) // only if experiences are aggregated per OD....
	{
		wt_day += wt_rep; //add repetition to day data, finishes the average..., wt_day stored at attribute level for writing, merged with Network wt_rec for following day
	}
	else
	{
		wt_day = wt_rep;
	}

	return wt_day;
}

map<ODSLL, Travel_time>& Day2day::process_ivt_replication (vector<ODstops*>& odstops, map<ODSLL, Travel_time> ivt_rec, const vector<Busline*>& buslines)
{
	map<ODSLL, Travel_time> ivt_rep; //record of ODSL data for the current replication
	nr_of_legs = 0;
	total_in_vehicle_time = 0;
	total_ivt_pk = 0;
	total_ivt_exp = 0;
	total_ivt_anticip = 0;
	total_crowding = 0;
	total_acrowding = 0;

	for (auto od_iter = odstops.begin(); od_iter != odstops.end(); ++od_iter)
	{
		map <Passenger*,list<Pass_onboard_experience> > pass_list = (*od_iter)->get_onboard_output();
		for (auto pass_iter = pass_list.begin(); pass_iter != pass_list.end(); ++pass_iter)
		{
			list<Pass_onboard_experience> onboard_experience_list = (*pass_iter).second;
			for (auto exp_iter = onboard_experience_list.begin(); exp_iter != onboard_experience_list.end(); ++exp_iter)
			{
				nr_of_legs++;
				Travel_time ivt;

				int pid = exp_iter->pass_id;
				int orig = exp_iter->original_origin;
				int dest = exp_iter->destination_stop;
				int stop = exp_iter->stop_id;
				int line = exp_iter->line_id;
				int leg = exp_iter->leg_id;
				ivt.tt[PK] = exp_iter->expected_ivt;
				ivt.tt[EXP] = exp_iter->experienced_ivt.first;
				ivt.tt[crowding] = exp_iter->experienced_ivt.second;

				if (aggregate)
				{
					orig = 0;
					dest = 0;
				}
				if (!individual_ivt)
				{
					pid = 0;
				}
				const ODSLL ivt_odsl = {pid, orig, dest, stop, line, leg};

				//insert base values and replace when previous experience found
				insert_alphas(ivt_odsl, ivt, ivt_rec, ivt_alpha_base, day);

				bool is_flexible_leg = false; // true only if the current on-board experience was for a flexible line leg
				if(theParameters->drt)
				{
                    auto line_it = find_if(buslines.begin(), buslines.end(), [line](const Busline* l) { return l->get_id() == line; });
					assert(line_it != buslines.end());
					is_flexible_leg = (*line_it)->is_flex_line();
				}

				//calculate anticipated waiting time and add to experience for this replication
				calc_anticipated_ivt(ivt, is_flexible_leg);
				pair<const ODSLL, Travel_time> ivt_row (ivt_odsl, ivt);
				ivt_rep += ivt_row; //if existing ODSL is found, data is added, else a new row is inserted

				//add to output;
				total_in_vehicle_time += ivt.tt[EXP];
				total_ivt_pk += ivt.tt[PK];
				total_ivt_exp += ivt.tt[anticip_EXP];
				total_ivt_anticip += ivt.alpha[EXP] * ivt.tt[anticip_EXP] + ivt.alpha[PK] * ivt.tt[PK];
				total_crowding += ivt.tt[crowding];
				total_acrowding += ivt.alpha[crowding];
			}
		}
	}

	if (nr_of_reps > 1 || theParameters->in_vehicle_d2d_indicator == 1)
	{
		ivt_day += ivt_rep; //add repetition to day data
	}
	else
	{
		ivt_day = ivt_rep;
	}

	return ivt_day;
}

void Day2day::print_wt_alphas(const map<ODSL, Travel_time>& wt_records, const ODSL& odsl)
{
    auto tt_it = wt_records.find(odsl);
    odsl.print();
    if (tt_it != wt_records.end())
    {
        cout << "\twt_alpha_exp: " << tt_it->second.alpha[EXP] << endl;
        cout << "\twt_alpha_pk : " << tt_it->second.alpha[PK] << endl;
        cout << "\twt_alpha_rti: " << tt_it->second.alpha[RTI] << endl;
    }
    else
    {
        cout << "\tNo record for this ODSL exists." << endl;
    }
}

void Day2day::print_ivt_alphas(const map<ODSLL, Travel_time>& ivt_records, const ODSLL& odsll)
{
    auto tt_it = ivt_records.find(odsll);
    odsll.print();
    if (tt_it != ivt_records.end())
    {
        cout << "\tivt_alpha_exp     : " << tt_it->second.alpha[EXP] << endl;
        cout << "\tivt_alpha_pk      : " << tt_it->second.alpha[PK] << endl;
        cout << "\tivt_alpha_crowding: " << tt_it->second.alpha[crowding] << endl;
    }
    else
    {
        cout << "No record for this ODSLL exists." << endl;
    }
}

void Day2day::print_all_wt_records(const map<ODSL, Travel_time>& wt_records)
{
	for(const auto& wt_row : wt_records)
	{
	    wt_row.first.print();
		wt_row.second.print_wt();
	}
}

void Day2day::print_all_ivt_records(const map<ODSLL, Travel_time>& ivt_records)
{
	for(const auto& ivt_row : ivt_records)
	{
	    ivt_row.first.print();
		ivt_row.second.print_ivt();
	}
}

void Day2day::write_wt_alphas_header(string filename)
{
	ofstream ofs(filename.c_str(),ios_base::app);
    ofs << "day\t" << "origin\t" << "destination\t" << "stop\t" << "line\t" << "wt_alpha_exp\t" << "wt_alpha_pk\t" << "wt_alpha_rti\t" <<  endl;
}
void Day2day::write_wt_alphas(string filename, const map<ODSL, Travel_time>& wt_records)
{
    ofstream ofs(filename.c_str(),ios_base::app);
    
    for(const auto& wt_rec : wt_records)
    {
		ODSL odsl = wt_rec.first;
		Travel_time tt = wt_rec.second;
        ofs << tt.day << "\t" << odsl.orig << "\t" << odsl.dest << "\t" << odsl.stop << "\t" << odsl.line << "\t" << tt.alpha[EXP] << "\t" << tt.alpha[PK] << "\t" << tt.alpha[RTI] << endl;
    }
}
void Day2day::write_ivt_alphas_header(string filename)
{
	ofstream ofs(filename.c_str(),ios_base::app);
    ofs << "day\t" << "origin\t" << "destination\t" << "stop\t" << "line\t"  << "leg\t" << "ivt_alpha_exp\t" << "ivt_alpha_pk\t" << "ivt_alpha_crowding" << endl;
}
void Day2day::write_ivt_alphas(string filename, const map<ODSLL, Travel_time>& ivt_records)
{
	ofstream ofs(filename.c_str(),ios_base::app);
    
    for (const auto& ivt_rec : ivt_records)
    {
        ODSLL odsll = ivt_rec.first;
        Travel_time tt = ivt_rec.second;
        ofs << tt.day << "\t" << odsll.orig << "\t" << odsll.dest << "\t" << odsll.stop << "\t" << odsll.line << "\t" << odsll.leg << "\t" << tt.alpha[EXP] << "\t" << tt.alpha[PK] << "\t" << tt.alpha[crowding] << endl;
    }

}


void Day2day::calc_anticipated_wt (Travel_time& row, bool is_flexible_leg)
{
	if(is_flexible_leg) assert(theParameters->drt);

	float& wtPK = row.tt[PK];
	float& wtRTI = row.tt[RTI];
	float& wtEXP = row.tt[EXP]; // most recent experience
	float& awtEXP = row.tt[anticip_EXP]; // rolling average of experiences
	float& alphaRTI = row.alpha[RTI];
	float& alphaEXP = row.alpha[EXP];
	float& alphaPK = row.alpha[PK];
	float& awtG = row.tt[anticip]; // anticipated travel time of previous day

	//calc awt - this could be moved to insert_alphas
	if (wtEXP == 0) wtEXP = 1.0; //to avoid division by zero

	if (awtEXP >= 0) //If there is prior experience
	{
		awtG = alphaRTI * wtRTI + alphaEXP * awtEXP + alphaPK * wtPK;
		float kapaAWT = 1 / pow(1 + row.day / pow(abs(awtEXP / wtEXP - 1) + 1, v), r);
		awtEXP = (1 - kapaAWT) * awtEXP + kapaAWT * wtEXP;
	}
	else
	{
		if(fwf_wip::day2day_drt_no_rti)
			alphaRTI = 0.0;
		awtG = alphaRTI * wtRTI + alphaPK * wtPK;
		awtEXP = wtEXP;
	}

	//calc temporary trust parameters for the alphas, to decide for which alpha trust should increase
	float aRTI = 1 / pow(abs(wtRTI / wtEXP - 1) + 1, v1);
	float aEXP = 1 / pow(abs(awtEXP / wtEXP - 1) + 1, v1);
	float aPK = 1 / pow(abs(wtPK / wtEXP - 1) + 1, v1);

    if (fwf_wip::day2day_drt_no_rti) // if no RTI is provisioned globally
    {
        aRTI = 0.0;
		if(is_flexible_leg)
            aPK = 0.0; // for flexible legs the 'prior knowledge' (PK) information source corresponds to a first exploration parameter. When experience exists trust in the exploration parameter is set to zero
    }

	//normalize a's
	float fnorm = 1 / (aRTI + aEXP + aPK);
	aRTI = fnorm * aRTI;
	aEXP = fnorm * aEXP;
	aPK = 1 - aRTI - aEXP;

	//calc alphas
	alphaRTI = (1 - kapa[RTI]) * alphaRTI + kapa[RTI] * aRTI;
	alphaEXP = (1 - kapa[EXP]) * alphaEXP + kapa[EXP] * aEXP;
	alphaPK = (1 - kapa[PK]) * alphaPK + kapa[PK] * aPK;

	//if (day == 1)
	//	alphaEXP = max(0.0, 1.0 - alphaPK- alphaRTI);

	//normalize alphas
	fnorm = 1 / (alphaRTI + alphaEXP + alphaPK);
	alphaRTI = fnorm * alphaRTI;
	alphaEXP = fnorm * alphaEXP;
	alphaPK = 1 - alphaRTI - alphaEXP;

	if (fwf_wip::day2day_drt_no_rti) // if no RTI is provisioned globally
    {
        alphaRTI = 0.0;
		if(is_flexible_leg)
		{
			alphaPK = 0.0; // for flexible legs the 'prior knowledge' (PK) information source corresponds to a first exploration parameter. When experience exists trust in the exploration parameter is set to zero
		    alphaEXP = 1.0; // credibility coeffs dont matter for flexible legs if no RTI is available globally
		}
	}

}

void Day2day::calc_anticipated_ivt (Travel_time& row, bool is_flexible_leg)
{
	if(is_flexible_leg) assert(theParameters->drt);

	float& ivtPK = row.tt[PK];
	float& ivtEXP = row.tt[EXP];
	float& aivtEXP = row.tt[anticip_EXP];
	float& alphaEXP = row.alpha[EXP];
	float& alphaPK = row.alpha[PK];
	float& aivtG = row.tt[anticip];
	float& crowdingEXP = row.tt[crowding];
	float& acrowdingEXP = row.alpha[crowding];

	//calc aivt
	if (aivtEXP >= 0)  //If there is prior experience
	{
		float kapaAWT = 1 / (1 + row.day / pow(abs(aivtEXP / ivtEXP - 1) + 1, v));
		aivtEXP = (1 - kapaAWT) * aivtEXP + kapaAWT * ivtEXP;
	}
	else
	{
		aivtEXP = ivtEXP;
	}
	float kapaCrowd = 1 / (1 + row.day / pow(abs(acrowdingEXP / crowdingEXP - 1) + 1, v_c));
	acrowdingEXP = (1 - kapaCrowd) * acrowdingEXP + kapaCrowd * crowdingEXP;

	//calc temporary trust parameters for the alphas, to decide for which alpha trust should increase
	float aEXP = 1 / pow(abs(aivtEXP / ivtEXP - 1) + 1, v1);
	float aPK = 1 / pow(abs(ivtPK / ivtEXP - 1) + 1, v1);

	//normalize a's
	float fnorm = 1 / (aEXP + aPK);
	aEXP = fnorm * aEXP;
	aPK = 1 - aEXP;

	//calc alphas
	alphaEXP = (1 - kapa[EXP]) * alphaEXP + kapa[EXP] * aEXP;
	alphaPK = (1 - kapa[PK]) * alphaPK + kapa[PK] * aPK;

	//if (day == 1)
	//	alphaEXP = max(0.0, 1.0 - alphaPK);

	//normalize alphas
	fnorm = 1 / (alphaEXP + alphaPK);
	alphaEXP = fnorm * alphaEXP;
	alphaPK = 1 - alphaEXP;

	if(is_flexible_leg) // @todo fwf_wip for now 'pk' or 'ivt exploration' is only used as a placeholder for experience on drt legs
	{
		alphaEXP = 1.0;
	    alphaPK = 0.0;
	}

	aivtG = acrowdingEXP *(alphaEXP * aivtEXP + alphaPK * ivtPK);

}

