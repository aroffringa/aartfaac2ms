#include "aartfaac2ms.h"

int main(int argc, char* argv[])
{
	int argi=1;
	Aartfaac2ms af2ms;
	while(argi<argc && argv[argi][0] == '-')
	{
		std::string param(&argv[argi][1]);
		if(param == "mem") {
			++argi;
			af2ms.SetMemPercentage(std::atoi(argv[argi]));
		}
		else if(param == "time-avg") {
			++argi;
			af2ms.SetTimeAveraging(std::atoi(argv[argi]));
		}
		else if(param == "interval") {
			af2ms.SetInterval(std::atoi(argv[argi+1]), std::atoi(argv[argi+2]));
			argi+=2;
		}
		++argi;
	}
	
	if(argi+3 > argc)
	{
		std::cerr << "Syntax: aartfaac2ms [options] <input.vis> <output.ms> <antennas.conf>\n"
		"  -mem <percentage>\n"
		"\tLimit memory usage to the given fraction of the total system memory. \n"
		"  -time-avg <factor>\n"
		"  -interval <start> <end>\n";
		return 1;
	}
	af2ms.Run(argv[argi], argv[argi+1], argv[argi+2]);
	return 0;
}

