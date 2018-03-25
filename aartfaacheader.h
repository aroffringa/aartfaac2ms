#ifndef AARTFAAC_HEADER_H
#define AARTFAAC_HEADER_H

#include <cstdint>
#include <string>
#include <sstream>

struct AartfaacHeader {
	static const uint32_t
		CORR_HDR_MAGIC= 0x000000003B98F002; // Magic number in raw corr visibilities.
	
	uint32_t magic;
	uint16_t nrReceivers;
	uint8_t  nrPolarizations;
	uint8_t  correlationMode;
	double   startTime, endTime;
	uint32_t weights[78]; // Fixed-sized field, independent of #stations!
	uint32_t nrSamplesPerIntegration;
	uint16_t nrChannels;

	char     pad1[170];
	
	std::string ToString() {
		std::ostringstream str;
		str
		<< "magic = " << magic << ' ';
		switch(magic) {
			case CORR_HDR_MAGIC: str << "CORR_HDR_MAGIC" << '\n'; break;
			default: str << "????\n"; break;
		}
		str
		<< "nrReceivers = " << nrReceivers << '\n'
		<< "nrPolarizations = " << unsigned(nrPolarizations) << '\n'
		<< "correlationMode = " << unsigned(correlationMode) << '\n'
		<< "startTime = " << startTime << '\n'
		<< "endTime = " << endTime << " (total: " << (endTime-startTime) << " s)\n"
		<< "weights = [" << weights[0];
		for(size_t i=1; i!=78; ++i)
			str << ", " << weights[i];
		str
		<< "]\nnrSamplesPerIntegration = " << nrSamplesPerIntegration << '\n'
		<< "nrChannels = " << nrChannels << '\n';
		return str.str();
	}
};

static_assert(sizeof(AartfaacHeader) == 512, "Header should be of size 512 bytes");

#endif
