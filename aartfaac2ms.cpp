#include "aartfaacfile.h"

#include <aoflagger.h>

#include "aocommon/uvector.h"

#include <complex>
#include <iostream>
#include <fstream>

#include <unistd.h>

using namespace aoflagger;

void allocateBuffers(const AartfaacFile& file)
{
	bool rfiDetection = false;
	double memPercentage = 90.0, memLimit = 0.0;
	long int pageCount = sysconf(_SC_PHYS_PAGES), pageSize = sysconf(_SC_PAGE_SIZE);
	int64_t memSize = (int64_t) pageCount * (int64_t) pageSize;
	double memSizeInGB = (double) memSize / (1024.0*1024.0*1024.0);
	if(memLimit == 0.0)
		std::cout << "Detected " << round(memSizeInGB*10.0)/10.0 << " GB of system memory.\n";
	else {
		std::cout << "Using " << round(memLimit*10.0)/10.0 << '/' << round(memSizeInGB*10.0)/10.0 << " GB of system memory.\n";
		memSize = int64_t(memLimit * (1024.0*1024.0*1024.0));
		memPercentage = 100.0;
	}
	size_t maxBufferSize = memSize*memPercentage/(100*(sizeof(float)*2+1));
	size_t nChannels = file.NChannels();
	size_t nAntennas = file.NAntennas();
	size_t maxScansPerPart = maxBufferSize / (nChannels*(nAntennas+1)*nAntennas*2);
	
	if(maxScansPerPart<1)
	{
		std::cout << "WARNING! The given amount of memory is not even enough for one scan and therefore below the minimum that Cotter will need; will use more memory. Expect swapping and very poor flagging accuracy.\nWARNING! This is a *VERY BAD* condition, so better make sure to resolve it!";
		maxScansPerPart = 1;
	} else if(maxScansPerPart<20 && rfiDetection)
	{
		std::cout << "WARNING! This computer does not have enough memory for accurate flagging; expect non-optimal flagging accuracy.\n"; 
	}
	size_t partCount = 1 + file.NTimesteps() / maxScansPerPart;
	if(partCount == 1)
		std::cout << "All " << file.NTimesteps() << " scans fit in memory; no partitioning necessary.\n";
	else
		std::cout << "Observation does not fit fully in memory, will partition data in " << partCount << " chunks of " << (file.NTimesteps()/partCount) << " scans.\n";
	
	const size_t requiredWidthCapacity = (file.NTimesteps()+partCount-1)/partCount;
	for(size_t antenna1=0;antenna1!=file.NAntennas();++antenna1)
	{
		for(size_t antenna2=antenna1; antenna2!=file.NAntennas(); ++antenna2)
		{
			ImageSet *imageSet = new ImageSet(_flagger->MakeImageSet(_curChunkEnd-_curChunkStart, nChannels, 8, 0.0f, requiredWidthCapacity));
			_imageSetBuffers.insert(std::pair<std::pair<size_t,size_t>, ImageSet*>(
				std::pair<size_t,size_t>(antenna1, antenna2), imageSet
			));
		}
	}
}

int main(int argc, char* argv[])
{
	AartfaacFile file(argv[1]);
	ao::uvector<std::complex<float>> vis(file.VisPerTimestep());
	size_t nTimesteps = file.NTimesteps();
	size_t index = 0;
	allocateBuffers(file);
	while(file.HasMore())
	{
		file.ReadTimestep(vis.data());
		std::cout << index << '/' << nTimesteps << " " << vis[0] << '\n';
		++index;
	}
}
