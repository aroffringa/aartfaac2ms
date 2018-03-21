#ifndef AARTFAAC_FILE_H
#define AARTFAAC_FILE_H

#include <cstdint>
#include <complex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

struct Header {
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

struct Timestep
{
	double startTime, endTime;
};

static_assert(sizeof(Header) == 512, "Header should be of size 512 bytes");

// An AARTFAAC file has a header followed by the data, which is written as:
// std::complex<float> visibilities[nr_baselines][nr_channels][nr_pols][nr_pols];
class AartfaacFile {
public:
	AartfaacFile(const char* filename) :
		_file(filename), _blockPos(0)
	{
		_file.seekg(0, std::ios::end);
		_filesize = _file.tellg();
		
		// Read first header
		_file.seekg(0, std::ios::beg);
		_file.read(reinterpret_cast<char*>(&_header), sizeof(Header));
		std::cout << _header.ToString();
		_blockSize = sizeof(std::complex<float>) * VisPerTimestep();
		std::cout << "buffersize=" << _blockSize << " (%512=" << (_blockSize%512) << ")\n";
		std::cout << "filesize=" << _filesize << '\n';
		std::cout << "nblocks=" << _filesize / (_blockSize+512) << "(%=" << _filesize%(_blockSize+512) << ")\n";
		
		// Read middle timestep to get central time of obs
		SeekToTimestep(NTimesteps()/2);
		Header midHeader;
		_file.read(reinterpret_cast<char*>(&midHeader), sizeof(Header));
		_centralCasaTime = TimeToCasa(midHeader.startTime);
		
		std::string fn(filename);
		size_t sbIndex = fn.find_last_of("SB");
		if(sbIndex == std::string::npos || sbIndex+5 > fn.size())
			throw std::runtime_error("Filename should have subband index preceded by 'SB' in it");
		_sbIndex = std::stoi(fn.substr(sbIndex+2, 3));
		
		SeekToTimestep(0);
	}
	
	size_t VisPerTimestep() const
	{
		size_t nBaselines = _header.nrReceivers * (_header.nrReceivers+1) / 2;
		return nBaselines * _header.nrChannels * _header.nrPolarizations;
	}
	
	void SkipTimesteps(int count)
	{
		_file.seekg(count * (sizeof(Header) + _blockSize), std::ios::cur);
		_blockPos += count;
	}
	
	void SeekToTimestep(size_t timestep)
	{
		_file.seekg(timestep * (sizeof(Header) + _blockSize), std::ios::beg);
		_blockPos = timestep;
	}
	
	Timestep ReadTimestep(std::complex<float>* buffer)
	{
		Header h;
		_file.read(reinterpret_cast<char*>(&h), sizeof(Header));
		//std::cout << h.ToString() << '\n';
		_file.read(reinterpret_cast<char*>(buffer), _blockSize);
		if(!_file)
			throw std::runtime_error("Error reading file");
		++_blockPos;
		
		return Timestep{TimeToCasa(h.startTime), TimeToCasa(h.endTime) };
	}
	
	bool HasMore() const
	{
		return _blockPos < (_filesize / _blockSize);
	}
	
	size_t NTimesteps() const
	{
		return _filesize / _blockSize;
	}
	
	size_t NChannels() const { return _header.nrChannels; }
	size_t NAntennas() const { return _header.nrReceivers; }
	
	double Bandwidth() const { return 195312.5; }
	double StartTime() const { return TimeToCasa(_header.startTime); }
	double CentralTime() const { return _centralCasaTime; }
	double Frequency() const { return Bandwidth() *  _sbIndex; }
	double IntegrationTime() const { return _header.endTime-_header.startTime; }
	
	/**
	 * CASA times are in MJD, but in s.
	 */
	static double TimeToCasa(double timestamp)
	{
		return timestamp + ((2440587.5 - 2400000.5) * 86400.0);
	}
private:
	std::ifstream _file;
	Header _header;
	size_t _blockSize, _filesize, _blockPos, _sbIndex;
	double _centralCasaTime;
};

#endif
