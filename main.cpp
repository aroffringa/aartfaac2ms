#include "aartfaac2ms.h"

#include "version.h"

#include "units/radeccoord.h"

int main(int argc, char* argv[])
{
	std::cout << "Running Aartfaac preprocessing pipeline, version " << AF2MS_VERSION_STR <<
		" (" << AF2MS_VERSION_DATE << ").\n"
		"Flagging is performed using AOFlagger " << aoflagger::AOFlagger::GetVersionString() << " (" << aoflagger::AOFlagger::GetVersionDate() << ").\n";
		
	int argi=1;
	Aartfaac2ms af2ms;
	AartfaacMode mode(AartfaacMode::Unused);
	size_t nCPUs = 0;
	while(argi<argc && argv[argi][0] == '-')
	{
		std::string param(&argv[argi][1]);
		if(param == "mem") {
			++argi;
			af2ms.SetMemPercentage(std::atoi(argv[argi]));
		}
		else if(param == "mode") {
			++argi;
			mode = AartfaacMode::FromNumber(std::atoi(argv[argi]));
			if(mode == AartfaacMode::Unused)
				throw std::runtime_error("Invalid mode. Valid modes are 1-7.");
		}
		else if(param == "flag") {
			af2ms.SetRFIDetection(true);
		}
		else if(param == "flag") {
			af2ms.SetRFIDetection(false);
		}
		else if(param == "time-avg") {
			++argi;
			af2ms.SetTimeAveraging(std::atoi(argv[argi]));
		}
		else if(param == "freq-avg") {
			++argi;
			af2ms.SetFrequencyAveraging(std::atoi(argv[argi]));
		}
		else if(param == "interval") {
			af2ms.SetInterval(std::atoi(argv[argi+1]), std::atoi(argv[argi+2]));
			argi+=2;
		}
		else if(param == "centre") {
			++argi;
			long double centreRA = RaDecCoord::ParseRA(argv[argi]);
			++argi;
			long double centreDec = RaDecCoord::ParseDec(argv[argi]);
			af2ms.SetPhaseCentre(centreRA, centreDec);
		}
		else if(param == "use-dysco")
		{
			af2ms.SetUseDysco(true);
		}
		else if(param == "dysco-config")
		{
			af2ms.SetAdvancedDyscoOptions(atoi(argv[argi+1]), atoi(argv[argi+2]), argv[argi+3], atof(argv[argi+4]), argv[argi+5]);
			argi += 5;
		}
		++argi;
	}
	
	if(argi+3 > argc)
	{
		std::cerr << "\nSyntax: aartfaac2ms [options] <input.vis> <output.ms> <antennas.conf>\n\n"
		"Options:\n"
		"  -mem <percentage>\n"
		"\tLimit memory usage to the given fraction of the total system memory. \n"
		"  -mode <number>\n"
		"\tSet RCU mode (1-4: LBA, 5-7: HBA).\n"
		"  -time-avg <factor>\n"
		"\tAverage in time (after flagging).\n"
		"  -freq-avg <factor>\n"
		"\tAverage in frequency (after flagging).\n"
		"  -interval <start> <end>\n"
		"\tOnly convert the selected timesteps.\n"
		"  -flag / -no-flag\n"
		"\tTurn RFI detection on/off. Default is currently off, but this might change.\n"
		"  -statistics / -no-statistics\n"
		"\tTurn collecting of quality on/off. Default is on. The statistics can be viewed\n"
		"\twith aoqplot.\n"
		"  -centre <ra> <dec>\n"
		"\tSet alternative phase centre, e.g. -centre 00h00m00.0s 00d00m00.0s.\n"
		"  -use-dysco\n"
		"\tCompress the measurement set with Dysco, using default settings (unless\n"
		"\tspecified with -dysco-config).\n"
		"  -dysco-config <data bits> <weight bits> <distribution> <truncation> <normalization>\n"
		"\tOverride default dysco settings\n";
		return 1;
	}
	if(nCPUs == 0)
		af2ms.SetThreadCount(sysconf(_SC_NPROCESSORS_ONLN));
	else
		af2ms.SetThreadCount(nCPUs);
	if(mode == AartfaacMode::Unused)
		throw std::runtime_error("Mode not set. Valid modes are 1-7.");
	af2ms.Run(argv[argi], argv[argi+1], argv[argi+2], mode);
	return 0;
}
