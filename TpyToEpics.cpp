#include "stdafx.h"
#include "ParseUtilConst.h"
#include "ParseTpyConst.h"
#include "ParseTpy.h"
#include "TpyToEpicsConst.h"
#include "TpyToEpics.h"

using namespace std;
using namespace ParseTpy;
using namespace ParseUtil;

#pragma warning (disable: 4996)

/** @file TpyToEpics.cpp
	Source for methods that generate EPICs .db file from a .tpy file
 ************************************************************************/

namespace EpicsTpy {

/* Parse command line arguments
 ************************************************************************/
int epics_conversion::getopt (int argc, const char* const argv[], bool argp[])
{
	int num = 0;
	for (int i = 1; i < argc; ++i) {
		if (argp && argp[i]) continue;
		if (!argv[i]) {
			if (argp) argp[i] = true;
			continue;
		}
		std::stringcase arg (argv[i]);
		int oldnum = num;
		// Does not apply any special conversion rules
		if (arg == "-rn" || arg == "/rn" ) {
			set_conversion_rule (no_conversion);
			++num;
		}
		// Replaces dots with underscores in channel names
		else if (arg == "-rd" || arg == "/rd" ) {
			set_conversion_rule (no_dot);
			++num;
		}
		// LIGO standard conversion rule (default)
		else if (arg == "-rl" || arg == "/rl" ) {
			set_conversion_rule (ligo_std);
			++num;
		}
		// LIGO standard conversion rule for vacuum channels
		else if (arg == "-rv" || arg == "/rv" ) {
			set_conversion_rule (ligo_vac);
			++num;
		}
		// Preserve case in EPICS channel names (default)
		else if (arg == "-cp" || arg == "/cp" ) {
			set_case_rule (preserve_case);
			++num;
		}
		// Force upper case in EPICS channel names
		else if (arg == "-cu" || arg == "/cu" ) {
			set_case_rule (upper_case);
			++num;
		}
		// Force lower case in EPICS channel names
		else if (arg == "-cl" || arg == "/cl" ) {
			set_case_rule (lower_case);
			++num;
		}
		// Eliminates leading dot in channel name
		else if (arg == "-nd" || arg == "/nd") {
			set_dot_rule (true);
			++num;
		}
		// Leaves leading dot in channel name (default)
		else if (arg == "-yd" || arg == "/yd") {
			set_dot_rule (false);
			++num;
		}
		// Replaces array brackets with underscore
		else if (arg == "-ni" || arg == "/ni") {
			set_array_rule (true);
			++num;
		}
		// Leave array indices as is (default)
		else if (arg == "-yi" || arg == "/yi") {
			set_array_rule (false);
			++num;
		}
		// no set flag to indicated a processed option
		if (argp && (num > oldnum)) {
			argp[i] = true;
		}
	}
	return num;
}

/* Obtain EPICS name from OPC name
 ************************************************************************/
string epics_conversion::to_epics (const stringcase& name) const
{
	stringcase n (name);
	stringcase::size_type pos;

	// eliminate leading dot
	if (no_leading_dot || (conv_rule == ligo_std) || (conv_rule == ligo_vac)) {
		std::stringcase::size_type pos;
		if (!n.empty() && (n[0] == '.')) {
			n.erase (0, 1);
		}
		// TwinCAT 3 doesn't use an empty name for globals
		else if ((pos = n.find ('.')) != std::stringcase::npos) {
			n.erase (0, pos + 1);
		}
	}

	// apply conversion rules
	switch (conv_rule) {
		// ligo standard
	case ligo_std:
		// replace first dot with colon
		pos = n.find ('.');
		if (pos != stringcase::npos) {
			n[pos] = ':';
		}
		// replace second dot with dash
		pos = n.find ('.');
		if (pos != stringcase::npos) {
			n[pos] = '-';
		}
		// replace remaining dots with underscore
		while ((pos = n.find ('.')) != stringcase::npos) {
			n[pos] = '_';
		}
		break;
		// ligo standard
	case ligo_vac:
		// replace first dot with dash
		pos = n.find ('.');
		if (pos != stringcase::npos) {
			n[pos] = '-';
		}
		// replace second dot with colon
		pos = n.find ('.');
		if (pos != stringcase::npos) {
			n[pos] = ':';
		}
		// replace remaining dots with underscore
		while ((pos = n.find ('.')) != stringcase::npos) {
			n[pos] = '_';
		}
		break;
		// replace all dots with underscores
	case no_dot:
		while ((pos = n.find ('.')) != stringcase::npos) {
			n[pos] = '_';
		}
		break;
		// do nothing
	case no_conversion:
	default:
		break;
	}
	// force case if necessary
	if (case_epics_names != preserve_case) {
		for (pos = 0; pos < n.size(); ++pos) {
			n[pos] = (case_epics_names == upper_case) ? toupper (n[pos]) : tolower (n[pos]);
		}
	}
	// replace array brackets with underscore if necessary
	if (no_array_index) {
		while ((pos = n.find ('[')) != stringcase::npos) {
			n[pos] = '_';
		}
		while ((pos = n.find (']')) != stringcase::npos) {
			n.erase (pos, 1);
		}
	}
	return string (n.c_str());
}


/* Destructor
split_io_support::~split_io_support
************************************************************************/
split_io_support::~split_io_support()
{
	close();
}

/* Copy constructor
split_io_support::~split_io_support
************************************************************************/
split_io_support::split_io_support (const split_io_support& iosup)
	: outf(0), outf_io (0), outf_in (0)
{
	*this = iosup;
}

/* Assignment operator
split_io_support::~split_io_support
************************************************************************/
split_io_support& split_io_support::operator= (const split_io_support& iosup)
{
	close();
	error = iosup.error;
	outfilename = iosup.outfilename;
	split_io = iosup.split_io;
	split_n = iosup.split_n;
	rec_num = iosup.rec_num;
	rec_num_in = iosup.rec_num_in;
	rec_num_io = iosup.rec_num_io;
	file_num_in = iosup.file_num_in;
	file_num_io = iosup.file_num_io;
	file_num_in_s = iosup.file_num_in_s;
	file_num_io_s = iosup.file_num_io_s;
	file_in_s = iosup.file_in_s;
	file_io_s = iosup.file_io_s;
	outf = iosup.outf;
	outf_in = iosup.outf_in;
	outf_io = iosup.outf_io;
	iosup.outf = nullptr;
	iosup.outf_in = nullptr;
	iosup.outf_io = nullptr;
	return *this;
}

/* Parse command line arguments
 ************************************************************************/
int split_io_support::getopt (int argc, const char* const argv[], bool argp[])
{
	int num = 0;
	for (int i = 1; i < argc; ++i) {
		if (argp && argp[i]) continue;
		if (!argv[i]) {
			if (argp) argp[i] = true;
			continue;
		}
		std::stringcase arg (argv[i]);
		int oldnum = num;
		// Splits database into input only and input/ouput recrods 
		if (arg == "-sio" || arg == "/sio" ||
			arg == "-ysio" || arg == "/ysio") {
			set_split (true);
			++num;
		}
		// Does not split database by record type (default)
		else if (arg == "-nsio" || arg == "/nsio" ) {
			set_split (false);
			++num;
		}
		// Splits database into files with no more than num records
		else if (arg == "-sn" || arg == "/sn" ) {
			if  (i + 1 < argc && argv[i+1][0] != '/' && argv[i+1][0] != '-') {
				int n = atoi (argv[i+1]);
				set_max (n);
				if (argp) argp[i] = true;
				i += 1;
				num += 2;
			}
			else {
				++num;
			}
		}
		// no set flag to indicated a processed option
		if (argp && (num > oldnum)) {
			argp[i] = true;
		}
	}
	return num;
}

/* Set the file name
split_io_support::set_filename
************************************************************************/
void split_io_support::set_filename (const stringcase& fn)
{
	close();
	outfilename = fn;
	// check for non-empty (no stdout) filename
	if (!outfilename.empty()) {
		if (split_io || (split_n > 0)) {
			int nn = -123;
			if ((nn = outfilename.rfind (".db")) == outfilename.size() - 3) {
				outfilename.erase (outfilename.size() - 3);
			}
			if (split_n > 0) {
				char buf[20];
				sprintf_s (buf, sizeof (buf), ".%03i", file_num_in);
				file_num_in_s = buf;
				sprintf_s (buf, sizeof (buf), ".%03i", file_num_io);
				file_num_io_s = buf;
			}
			if (split_io) {
				file_in_s = ".in";
				file_io_s = ".io";
			}
			stringcase fname (outfilename + file_io_s + file_num_io_s + ".db");
			outf_io = fopen (fname.c_str(), "w");
			if (!outf_io) {
				fprintf (stderr, "Failed to open output %s.\n", fname.c_str());
				error = true;
				return;
			}
			outf = outf_io;
			if (split_io) {
				fname = outfilename + file_in_s + file_num_in_s + ".db";
				outf_in = fopen (fname.c_str(), "w");
				if (!outf_io) {
					fprintf (stderr, "Failed to open output %s.\n", fname.c_str());
					error = true;
					return;
				}
			}
		}
		else {
			outf_io = fopen (outfilename.c_str(), "w");
			if (!outf_io) {
				fprintf (stderr, "Failed to open output %s.\n", outfilename.c_str());
				error = true;
				return;
			}
			outf = outf_io;
		}
	}

	// empty string is stdout
	else {
		if (split_io || (split_n > 0)) {
			fprintf (stderr, "Cannot split output to console\n");
			error = true;
			return;
		}
		outf = stdout;
	}
}

/* Flush file content
split_io_support::flush
************************************************************************/
void split_io_support::flush()
{
	if (outf_io) fflush (outf_io);
	if (outf_in) fflush (outf_in);
}

/* Closes files
split_io_support::close
************************************************************************/
void split_io_support::close()
{
	if (outf_io) fclose (outf_io);
	outf_io = nullptr;
	if (outf_in) fclose (outf_in);
	outf_in = nullptr;
}

/* Increment record  number
split_io_support::increment
************************************************************************/
bool split_io_support::increment (bool readonly)
{
	// check if we need to open a new split file
	if (!error && (split_n > 0)) {
		if (split_io) {
			if (readonly) {
				if ((rec_num_in > 0) && (rec_num_in % split_n == 0) && outf_in) {
					++file_num_in;
					char buf[20];
					sprintf_s (buf, sizeof(buf), ".%03i", file_num_in);
					file_num_in_s = buf;
					fclose (outf_in);
					stringcase fname = outfilename + file_in_s + file_num_in_s + ".db";
					outf_in = fopen (fname.c_str(), "w");
					if (!outf_in) {
						fprintf (stderr, "Failed to open output %s.\n", fname.c_str());
						error = true;
					}
				}
			}
			else {
				if ((rec_num_io > 0) && (rec_num_io % split_n == 0) && outf_io) {
					++file_num_io;
					char buf[20];
					sprintf_s (buf, sizeof(buf), ".%03i", file_num_io);
					file_num_io_s = buf;
					fclose (outf_io);
					stringcase fname (outfilename + file_io_s + file_num_io_s + ".db");
					outf_io = fopen (fname.c_str(), "w");
					if (!outf_io) {
						fprintf (stderr, "Failed to open output %s.\n", fname.c_str());
						error = true;
					}
				}
			}
		}
		// no splitting
		else {
			if ((rec_num > 0) && (rec_num % split_n == 0) && outf_io) {
				++file_num_io;
				char buf[20];
				sprintf (buf, ".%03i", file_num_io);
				file_num_io_s = buf;
				fclose (outf_io);
				stringcase fname (outfilename + file_io_s + file_num_io_s + ".db");
				outf_io = fopen (fname.c_str(), "w");
				if (!outf_io) {
					fprintf (stderr, "Failed to open output %s.\n", fname.c_str());
					error = true;
				}
			}
		}
	}
	// set up output file
	if (split_io && readonly) outf = outf_in;
	else if (!outfilename.empty()) outf = outf_io;
	if (!outf) {
		outf = stdout;
		error = true;
	}
	// increase record number
	if (!error) {
		if (readonly) ++rec_num_in;
		else ++rec_num_io;
		++rec_num;
	}

	return error;
}

/* Option processing
   epics_list_processing::epics_list_processing
************************************************************************/
epics_list_processing::epics_list_processing (
		const std::stringcase& fname,
		int argc, const char* const argv[], bool argp[])
	: epics_conversion (argc, argv, argp), 
	  split_io_support (fname, argc, argv, argp) 
{
	mygetopt (argc, argv, argp); 
}

/* Option processing
   epics_list_processing::getopt
************************************************************************/
int epics_list_processing::getopt (int argc, const char* const argv[], 
								   bool argp[]) 
{
	return epics_conversion::getopt (argc, argv, argp) +
		   mygetopt (argc, argv, argp); 
}

/* Option processing
   epics_list_processing::mygetopt
************************************************************************/
int epics_list_processing::mygetopt (int argc, const char* const argv[], 
								     bool argp[]) 
{
	int num = 0;
	for (int i = 1; i < argc; ++i) {
		if (argp && argp[i]) continue;
		if (!argv[i]) {
			if (argp) argp[i] = true;
			continue;
		}
		std::stringcase arg (argv[i]);
		int oldnum = num;
		// generate a standard listing
		if (arg == "-l" || arg == "/l") {
			set_listing (listing_standard);
			set_verbose (false);
			++num;
		}
		// generate a standard long listing
		else if (arg == "-ll" || arg == "/ll") {
			set_listing (listing_standard);
			set_verbose (true);
			++num;
		}
		// generate a burt save/restore listing
		else if (arg == "-lb" || arg == "/lb") {
			set_listing (listing_autoburt);
			set_verbose (false);
			++num;
		}
		// now set flag to indicated a processed option
		if (argp && (num > oldnum)) {
			argp[i] = true;
		}
	}
	return num;
}

/* Process a channel
   epics_list_processing::operator()
************************************************************************/
bool epics_list_processing::operator() (const process_arg& arg)
{
	// quit if not atomic and not a standard listing
	if (!arg.is_atomic() && (listing != listing_standard)) {
		return false;
	}

	increment (arg.get_opc().is_readonly());
	// write record information to output file
	// produce a listing
	string epicsname = to_epics (arg.get_alias());
	if (listing == listing_autoburt) {
		stringcase ro = arg.get_opc().is_readonly() ? "RO " : "";
		fprintf (get_file(), "%s%s", ro.c_str(), epicsname.c_str());
	}
	// standard listing
	else {
		fprintf (get_file(), "%s", epicsname.c_str());
	}

	// long listing?
	if (verbose && (listing != listing_autoburt)) {
		fprintf (get_file(), " (%s", arg.get_process_string().c_str());
		fprintf (get_file(), ", opc %c", arg.get_opc().is_published() ? '1' : '0');
		for (property_map::const_iterator i = arg.get_opc().get_properties().begin();
			i != arg.get_opc().get_properties().end(); ++i) {
				stringcase s = i->second;
				trim_space (s);
				fprintf (get_file(), ", prop[%i]=\"%s\"", i->first, s.c_str());
		}
		fprintf (get_file(), ")");
	}
	fprintf (get_file(), "\n");
	return true;
}


/* Option processing
   epics_db_processing::epics_db_processing
************************************************************************/
epics_db_processing::epics_db_processing (
		const std::stringcase& fname,
		int argc, const char* const argv[], bool argp[])
	: epics_conversion (argc, argv, argp), 
	  split_io_support (fname, argc, argv, argp) 
{
	mygetopt (argc, argv, argp); 
}

/* Option processing
   epics_db_processing::getopt
************************************************************************/
int epics_db_processing::getopt (int argc, const char* const argv[], 
								 bool argp[]) 
{
	return epics_conversion::getopt (argc, argv, argp) +
		   mygetopt (argc, argv, argp); 
}

/* Option processing
   epics_db_processing::mygetopt
************************************************************************/
int epics_db_processing::mygetopt (int argc, const char* const argv[], 
								   bool argp[]) 
{
	int num = 0;
	for (int i = 1; i < argc; ++i) {
		if (argp && argp[i]) continue;
		if (!argv[i]) {
			if (argp) argp[i] = true;
			continue;
		}
		std::stringcase arg (argv[i]);
		int oldnum = num;
		// Splits database into input only and input/ouput recrods 
		if (arg == "-devopc" || arg == "/devopc") {
			set_device_support (device_support_opc_name);
			++num;
		}
		// Does not split database by record type (default)
		else if (arg == "-devtc" || arg == "/devtc" ) {
			set_device_support (device_support_tc_name);
			++num;
		}
		// now set flag to indicated a processed option
		if (argp && (num > oldnum)) {
			argp[i] = true;
		}
	}
	return num;
}

/* Process a channel
   epics_db_processing::operator()
************************************************************************/
bool epics_db_processing::operator() (const process_arg& arg)
{
	// quit if not atomic
	if (!arg.is_atomic()) {
		return false;
	}

	// readonly?
	bool readonly = arg.get_opc().is_readonly();
	increment (readonly);

	// default process type conversion
	stringcase tname;
	switch (arg.get_process_type()) {
	case pt_int:
		tname = readonly ? "longin" : "longout";
		break;
	case pt_real:
		tname = readonly ? "ai" : "ao";
		break;
	case pt_bool:
		tname = readonly ? "bi" : "bo";
		break;
	case pt_string:
		tname = readonly ? "stringin" : "stringout";
		break;
	case pt_enum:
		tname = readonly ? "mbbi" : "mbbo";
		break;
	default:
		fprintf (stderr, "Unknown type %s for %s\n", 
			arg.get_type_name().c_str(), arg.get_name().c_str());
		return false;
	}
	// check OPC_PROP_RECTYPE
	stringcase s;
	if (arg.get_opc().get_property (OPC_PROP_RECTYPE, s)) {
		tname = s;
	}

	// check record header
	string epicsname = to_epics (arg.get_alias());
	if (epicsname.size() > MAX_EPICS_CHANNEL) {
		fprintf (stderr, "Warning: channel name %s too long by %i\n", 
			epicsname.c_str(), epicsname.size() - MAX_EPICS_CHANNEL);
		return false;
	}
	// now print header
	fprintf(get_file(), "record(%s,\"%s\") {\n", tname.c_str(), epicsname.c_str());

	// check OPC_PROP_DESC
	if (arg.get_opc().get_property (OPC_PROP_DESC, s)) {
		if (s.size() > MAX_EPICS_DESC) {
			fprintf (stderr, "Warning: DESC for %s too long by %i\n", 
				arg.get_name().c_str(), s.size() - MAX_EPICS_DESC);
		}
		process_field_string (EPICS_DB_DESC, s);
	}
	// add SCAN
	process_field_string (EPICS_DB_SCAN, readonly ? "I/O Intr" : "Passive");
	// check for DTYP
	stringcase dtyp = device_support == device_support_tc_name ? "tcat" : "opc";
	if (arg.get_opc().get_property (OPC_PROP_DTYP, s)) {
		if (s.find ("raw") == stringcase::npos) {
			dtyp = device_support == device_support_tc_name ? "tcat raw" : "opcRaw";
		}
	}
	process_field_string (EPICS_DB_DTYP, dtyp);
	// INPUT/OUTPUT field
	stringcase inplink;
	stringcase servername;
	switch (device_support) {
	case device_support_opc_name:
	default:
		// check for server
		servername = "opc";
		arg.get_opc().get_property (OPC_PROP_SERVER, servername);
		// input/output link
		inplink = stringcase ("@") + servername + arg.get_name(); 
		break;
	case device_support_tc_name:
		// input/output link
		inplink = stringcase ("@") + arg.get_full();
		break;
	}
	process_field_string (readonly ? EPICS_DB_INP : EPICS_DB_OUT, inplink);

	// check for TSE
	int tse = -2;
	arg.get_opc().get_property (OPC_PROP_TSE, tse);
	process_field_numeric (EPICS_DB_TSE, tse);
	// check for PINI
	//int pini = readonly ? 1 : 0;
	int pini = 0;
	arg.get_opc().get_property (OPC_PROP_PINI, tse);
	process_field_numeric (EPICS_DB_PINI, pini);

	// go through properties
	for (property_map::const_iterator f = arg.get_opc().get_properties().begin();
		f != arg.get_opc().get_properties().end(); ++f) 
	{
		switch (f->first) {
		case OPC_PROP_UNIT :
			process_field_string (EPICS_DB_EGU, f->second);
			break;
		case OPC_PROP_DESC :
			// processed above
			break;
		case OPC_PROP_HIEU :
			process_field_numeric (EPICS_DB_HOPR, f->second);
			break;
		case OPC_PROP_LOEU :
			process_field_numeric (EPICS_DB_LOPR, f->second);
			break;
		case OPC_PROP_HIRANGE :
			process_field_numeric (EPICS_DB_DRVH, f->second);
			break;
		case OPC_PROP_LORANGE :
			process_field_numeric (EPICS_DB_DRVL, f->second);
			break;
		case OPC_PROP_CLOSE :
			process_field_string (EPICS_DB_ONAM, f->second);
			break;
		case OPC_PROP_OPEN :
			process_field_string (EPICS_DB_ZNAM, f->second);
			break;
		case OPC_PROP_PREC :
			process_field_numeric (EPICS_DB_PREC, f->second);
			break;
		case OPC_PROP_ZRST :
		case OPC_PROP_ZRST + 1 :
		case OPC_PROP_ZRST + 2 :
		case OPC_PROP_ZRST + 3 :
		case OPC_PROP_ZRST + 4 :
		case OPC_PROP_ZRST + 5 :
		case OPC_PROP_ZRST + 6 :
		case OPC_PROP_ZRST + 7 :
		case OPC_PROP_ZRST + 8 :
		case OPC_PROP_ZRST + 9 :
		case OPC_PROP_ZRST + 10 :
		case OPC_PROP_ZRST + 11 :
		case OPC_PROP_ZRST + 12 :
		case OPC_PROP_ZRST + 13 :
		case OPC_PROP_ZRST + 14 :
		case OPC_PROP_FFST :
			process_field_numeric (EPICS_DB_ZRVL[f->first-OPC_PROP_ZRST], f->first-OPC_PROP_ZRST);
			process_field_string (EPICS_DB_ZRST[f->first-OPC_PROP_ZRST], f->second);
			break;
		case OPC_PROP_RECTYPE :
		case OPC_PROP_INOUT :
		case OPC_PROP_TSE :
		case OPC_PROP_PINI :
		case OPC_PROP_DTYP :
		case OPC_PROP_SERVER :
		case OPC_PROP_PLCNAME :
		case OPC_PROP_ALIAS :
			// processed above
			break;
			// alarm
		case OPC_PROP_ALMOSV:
			if ((tname == "bi") || (tname == "bo")) {
				process_field_alarm (EPICS_DB_OSV, f->second);
			}
			break;
		case OPC_PROP_ALMZSV:
			if ((tname == "bi") || (tname == "bo")) {
				process_field_alarm (EPICS_DB_ZSV, f->second);
			}
			break;
		case OPC_PROP_ALMCOSV:
			if ((tname == "bi") || (tname == "bo") || (tname == "mbbi") || (tname == "mbbo")) {
				process_field_alarm (EPICS_DB_COSV, f->second);
			}
			break;
		case OPC_PROP_ALMUNSV:
			if ((tname == "mbbi") || (tname == "mbbo")) {
				process_field_alarm (EPICS_DB_UNSV, f->second);
			}
			break;
		case OPC_PROP_ALMZRSV:
		case OPC_PROP_ALMZRSV + 1:
		case OPC_PROP_ALMZRSV + 2:
		case OPC_PROP_ALMZRSV + 3:
		case OPC_PROP_ALMZRSV + 4:
		case OPC_PROP_ALMZRSV + 5:
		case OPC_PROP_ALMZRSV + 6:
		case OPC_PROP_ALMZRSV + 7:
		case OPC_PROP_ALMZRSV + 8:
		case OPC_PROP_ALMZRSV + 9:
		case OPC_PROP_ALMZRSV + 10:
		case OPC_PROP_ALMZRSV + 11:
		case OPC_PROP_ALMZRSV + 12:
		case OPC_PROP_ALMZRSV + 13:
		case OPC_PROP_ALMZRSV + 14:
		case OPC_PROP_ALMFFSV:
			if ((tname == "mbbi") || (tname == "mbbo")) {
				process_field_alarm (EPICS_DB_ZRSV[f->first-OPC_PROP_ALMZRSV], f->second);
			}
			break;
		case OPC_PROP_ALMHH:
			if ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_numeric (EPICS_DB_HIHI, f->second);
				process_field_alarm (EPICS_DB_HHSV, EPICS_DB_MAJOR);
			}
			break;
		case OPC_PROP_ALMH:
			if ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_numeric (EPICS_DB_HIGH, f->second);
				process_field_alarm (EPICS_DB_HSV, EPICS_DB_MINOR);
			}
			break;
		case OPC_PROP_ALML:
			if ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_numeric (EPICS_DB_LOW, f->second);
				process_field_alarm (EPICS_DB_LSV, EPICS_DB_MINOR);
			}
			break;
		case OPC_PROP_ALMLL:
			if ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_numeric (EPICS_DB_LOLO, f->second);
				process_field_alarm (EPICS_DB_LLSV, EPICS_DB_MAJOR);
			}
			break;
		case OPC_PROP_ALMHHSV:
			if  ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_alarm (EPICS_DB_HHSV, f->second);
			}
			break;
		case OPC_PROP_ALMHSV:
			if  ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_alarm (EPICS_DB_HSV, f->second);
			}
			break;
		case OPC_PROP_ALMLSV:
			if  ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_alarm (EPICS_DB_LSV, f->second);
			}
			break;
		case OPC_PROP_ALMLLSV:
			if  ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_alarm (EPICS_DB_LLSV, f->second);
			}
			break;
		case OPC_PROP_ALMDB:
			if ((tname == "ai") || (tname == "ao") || (tname == "longin") || (tname == "longout")) {
				process_field_numeric (EPICS_DB_HYST, f->second);
			}
			break;
			// unknown
		default:
			if (f->first >= 1000) {
				fprintf (stderr, "Unknown property %i for %s\n", f->first, arg.get_name().c_str());
			}
			break;
		}
	}
	// end with closing bracket
	fprintf(get_file(), "}\n");
	return true;
}

/* Process a field
epics_db_processing::process_field_string
************************************************************************/
bool epics_db_processing::process_field_string (stringcase name, 
												stringcase val)
{
	fprintf (get_file(), "\tfield(%s,\"%s\")\n", name.c_str(), val.c_str());
	return true;
}

/* Process a field
epics_db_processing::process_field_numeric
************************************************************************/
bool epics_db_processing::process_field_numeric (stringcase name, int val)
{
	char buf[40];
	sprintf_s (buf, sizeof(buf), "%i", val);
	return process_field_string (name, buf);
}

/* Process a field
epics_db_processing::process_field_numeric
************************************************************************/
bool epics_db_processing::process_field_numeric (stringcase name, 
												 stringcase val)
{
	int v = strtol (val.c_str(), NULL, 10);
	return process_field_numeric (name, v);
}

/* Process a field
epics_db_processing::process_field_alarm
************************************************************************/
bool epics_db_processing::process_field_alarm (stringcase name, 
											   stringcase severity)
{
	if ((severity == EPICS_DB_NOALARM) || (severity == EPICS_DB_MINOR) || 
		(severity == EPICS_DB_MAJOR)) {
		fprintf (get_file(), "\tfield(%s,\"%s\")\n", name.c_str(), severity.c_str());
		return true;
	}
	else {
		fprintf (stderr, "Unknown alarm severity %s for %s\n", severity.c_str(), name.c_str());
		return false;
	}
}


}
