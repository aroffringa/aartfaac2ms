#include <iostream>
#include <vector>

#include "aartfaacfile.h"
#include "aartfaacheader.h"
#include "optional.h"
#include "timerange.h"

#include "units/radeccoord.h"

#include <casacore/measures/Measures/MCEpoch.h>
#include <casacore/measures/Measures/MeasConvert.h>
#include <casacore/measures/Measures/MEpoch.h>
#include <casacore/measures/Measures/MPosition.h>

int main(int argc, char* argv[])
{
	int argi = 1;
	Optional<size_t> intervalStart, intervalEnd;
	Optional<double> lstStart, lstEnd;
	bool showLst = false;
	while(argi < argc && argv[argi][0] == '-')
	{
		std::string p(&argv[argi][1]);
		if(p == "trim-start")
		{
			++argi;
			intervalStart = atoi(argv[argi]);
		}
		else if(p == "trim-end")
		{
			++argi;
			intervalEnd = atoi(argv[argi]);
		}
		else if(p == "lst-start")
		{
			++argi;
			lstStart = atof(argv[argi]);
		}
		else if(p == "lst-end")
		{
			++argi;
			lstEnd = atof(argv[argi]);
		}
		else if(p == "show-lst")
		{
			showLst = true;
		}
		else {
			std::cerr << "Invalid parameter -" << p << '\n';
			return 1;
		}
		++argi;
	}
	if((argi+2 > argc && !showLst) || argi+1 > argc )
	{
		std::cerr <<
			"Syntax: afedit [options] <input filename> [<output filename>]\n"
			"options:\n"
			"  -trim-start <start index>\n"
			"  -trim-end <end index>\n"
			"  -lst-start <start lst>\n"
			"  -lst-end <end lst>\n"
			"  -show-lst <lst>\n";
	}
	const char *inputFilename(argv[argi]);
	const char *outputFilename;
	if(showLst)
		outputFilename = nullptr;
	else
		outputFilename = argv[argi+1];
		
	if(lstStart.HasValue() || lstEnd.HasValue() || showLst)
	{
		AartfaacFile file(inputFilename);
		casacore::MPosition aartfaacPos(casacore::MVPosition(3826577.022720000, 461022.995082000, 5064892.814), casacore::MPosition::ITRF);
		casacore::MeasFrame frame(aartfaacPos);
		TimeRange range(lstStart.ValueOr(0.0), lstEnd.ValueOr(24.0));
		size_t lstStartIndex = file.NTimesteps(), lstEndIndex = 0;
		double firstLst = 0.0, lastLst = 0.0;
		
		size_t
			tStart = intervalStart.ValueOr(0),
			tEnd = intervalEnd.ValueOr(file.NTimesteps());
		
		// TODO this could be done with binary search to make it faster
		for(size_t timestep=tStart; timestep!=tEnd; ++timestep)
		{
			file.SeekToTimestep(timestep);
			Timestep t = file.ReadMetadata();
			double obsTime = AartfaacFile::TimeToCasa((t.startTime + t.endTime) * 0.5);
			casacore::MEpoch timeEpoch = casacore::MEpoch(casacore::MVEpoch(obsTime/86400.0), casacore::MEpoch::UTC);
			casacore::MEpoch lst = casacore::MEpoch::Convert(timeEpoch, casacore::MEpoch::Ref(casacore::MEpoch::LAST, frame))();
			double hour = lst.getValue().getDayFraction() * 24.0;
			if(range.Contains(hour))
			{
				if(lstStartIndex == file.NTimesteps())
					lstStartIndex = timestep;
				lstEndIndex = timestep;
			}
			if(timestep == tStart)
				firstLst = hour;
			if(timestep+1 == tEnd)
				lastLst = hour;
		}
		std::cout << "LST range of observation: " << RaDecCoord::RAToString(firstLst*(M_PI/12.0)) << " - " << RaDecCoord::RAToString(lastLst*(M_PI/12.0)) << " (in hours: " << firstLst << " - " << lastLst << ").\n";
		if(showLst)
			return 0;
		if(lstStartIndex == file.NTimesteps())
		{
			std::cerr << "File has no timesteps in given LST interval.\n";
			return 1;
		}
		++lstEndIndex;
		std::cout << "Selected timesteps from LST interval: " << lstStartIndex << " - " << lstEndIndex << '\n';
		intervalStart = lstStartIndex;
		intervalEnd = lstEndIndex;
	}
	
	std::ifstream inFile(inputFilename);
	inFile.seekg(0, std::ios::end);
	size_t filesize = inFile.tellg();
	if(!inFile || filesize == 0)
	{
		std::cerr << "Error reading file " << inputFilename << ".\n";
		return 1;
	}
	
	// Read first header
	AartfaacHeader header;
	inFile.seekg(0, std::ios::beg);
	inFile.read(reinterpret_cast<char*>(&header), sizeof(AartfaacHeader));
	header.Check();
	size_t blockSize = sizeof(std::complex<float>) * header.VisPerTimestep();
	size_t timesteps = filesize / blockSize;
	
	if(!intervalStart.HasValue())
		intervalStart = 0;
	if(!intervalEnd.HasValue())
		intervalEnd = timesteps;
	if(*intervalStart >= *intervalEnd || *intervalEnd > timesteps)
	{
		std::cerr << "Invalid trimming interval.\n";
		return 1;
	}
	
	// Copy the interval
	std::ofstream outFile(outputFilename);
	std::vector<char> block(sizeof(AartfaacHeader) + blockSize);
	inFile.seekg(block.size() * (*intervalStart), std::ios::beg);
	for(size_t timestep = *intervalStart; timestep != *intervalEnd; ++timestep)
	{
		inFile.read(block.data(), block.size());
		if(!inFile)
		{
			std::cerr << "Error reading input file.\n";
			return 1;
		}
		outFile.write(block.data(), block.size());
		if(!outFile)
		{
			std::cerr << "Error writing output file.\n";
			return 1;
		}
	}
}
