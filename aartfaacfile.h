#ifndef AARTFAAC_FILE_H
#define AARTFAAC_FILE_H

#include "aartfaacheader.h"
#include "aartfaacmode.h"

#include <cstdint>
#include <complex>
#include <fstream>
#include <stdexcept>
#include <iostream>

struct Timestep
{
	double startTime, endTime;
};

// An AARTFAAC file has a header followed by the data, which is written as:
// std::complex<float> visibilities[nr_baselines][nr_channels][nr_pols][nr_pols];
class AartfaacFile {
public:
	AartfaacFile(const char* filename, AartfaacMode mode) :
		_file(filename), _mode(mode), _blockPos(0)
	{
		_file.seekg(0, std::ios::end);
		_filesize = _file.tellg();
		
		// Read first header
		_file.seekg(0, std::ios::beg);
		_file.read(reinterpret_cast<char*>(&_header), sizeof(AartfaacHeader));
		
		_header.Check();
		
		_blockSize = sizeof(std::complex<float>) * _header.VisPerTimestep();
		
		std::string fn(filename);
		size_t sbIndex = fn.rfind("SB");
		if(sbIndex == std::string::npos || sbIndex+5 > fn.size())
			throw std::runtime_error("Filename should contain subband index preceded by 'SB' in it");
		_sbIndex = std::stoi(fn.substr(sbIndex+2, 3));
		std::cout << "Calculating frequency for " << fn.substr(sbIndex, 5) << ".\n";
		
		// This is from:
		// http://astron.nl/radio-observatory/astronomers/users/
		//   technical-information/frequency-selection/station-clocks-and-rcu
		_bandwidth = mode.Bandwidth();
		const double frequencyOffset = mode.FrequencyOffset();
		_frequency = _bandwidth *  _sbIndex + frequencyOffset;
		
		SeekToTimestep(0);
	}
	
	AartfaacFile(const char* filename) :
		_file(filename), _mode(AartfaacMode::Unused), _blockPos(0)
	{
		_file.seekg(0, std::ios::end);
		_filesize = _file.tellg();
		
		// Read first header
		_file.seekg(0, std::ios::beg);
		_file.read(reinterpret_cast<char*>(&_header), sizeof(AartfaacHeader));
		
		_header.Check();
		
		_blockSize = sizeof(std::complex<float>) * _header.VisPerTimestep();
		
		SeekToTimestep(0);
	}
	
	void SkipTimesteps(int count)
	{
		_file.seekg(count * (sizeof(AartfaacHeader) + _blockSize), std::ios::cur);
		_blockPos += count;
	}
	
	void SeekToTimestep(size_t timestep)
	{
		_file.seekg(timestep * (sizeof(AartfaacHeader) + _blockSize), std::ios::beg);
		_blockPos = timestep;
	}
	
	Timestep ReadTimestep(std::complex<float>* buffer)
	{
		AartfaacHeader h;
		_file.read(reinterpret_cast<char*>(&h), sizeof(AartfaacHeader));
		_file.read(reinterpret_cast<char*>(buffer), _blockSize);
		if(!_file)
			throw std::runtime_error("Error reading file");
		++_blockPos;
		
		return Timestep{TimeToCasa(h.startTime), TimeToCasa(h.endTime) };
	}
	
	Timestep ReadMetadata()
	{
		AartfaacHeader h;
		_file.read(reinterpret_cast<char*>(&h), sizeof(AartfaacHeader));
		if(!_file)
			throw std::runtime_error("Error reading file");
		SeekToTimestep(_blockPos);
		
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
	
	size_t VisPerTimestep() const
	{
		return _header.VisPerTimestep();
	}
	
	size_t NChannels() const { return _header.nrChannels; }
	size_t NAntennas() const { return _header.nrReceivers; }
	uint8_t CorrelationMode() const { return _header.correlationMode; }
	
	double Bandwidth() const { return _bandwidth; }
	double StartTime() const { return TimeToCasa(_header.startTime); }
	double Frequency() const { return _frequency; }
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
	AartfaacHeader _header;
	AartfaacMode _mode;
	size_t _blockSize, _filesize, _blockPos, _sbIndex;
	double _frequency, _bandwidth;
};

#endif
