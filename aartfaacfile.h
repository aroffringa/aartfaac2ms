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
		_blockSize = sizeof(std::complex<float>) * VisPerTimestep();
		
		if(_header.correlationMode != 15)
			throw std::runtime_error("Correlation mode of header was not 15. This tool can only handle sets with 4 polarizations.");
		
		// Read middle timestep to get central time of obs
		SeekToTimestep(NTimesteps()/2);
		AartfaacHeader midHeader;
		_file.read(reinterpret_cast<char*>(&midHeader), sizeof(AartfaacHeader));
		_centralCasaTime = TimeToCasa(midHeader.startTime);
		
		std::string fn(filename);
		size_t sbIndex = fn.rfind("SB");
		if(sbIndex == std::string::npos || sbIndex+5 > fn.size())
			throw std::runtime_error("Filename should have subband index preceded by 'SB' in it");
		_sbIndex = std::stoi(fn.substr(sbIndex+2, 3));
		std::cout << "Calculating frequency for " << fn.substr(sbIndex, 5) << '\n';
		
		double frequencyOffset = 0.0;
		switch(mode.mode) {
			case AartfaacMode::HBA110_190:
				frequencyOffset = 100e6;
				break;
			default:
				throw std::runtime_error("Don't know how to handle this mode: not implemented yet");
		}
		_frequency = Bandwidth() *  _sbIndex + frequencyOffset;
		
		SeekToTimestep(0);
	}
	
	size_t VisPerTimestep() const
	{
		size_t nBaselines = _header.nrReceivers * (_header.nrReceivers+1) / 2;
		return nBaselines * _header.nrChannels * _header.nrPolarizations;
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
	uint8_t CorrelationMode() const { return _header.correlationMode; }
	
	double Bandwidth() const { return 195312.5; }
	double StartTime() const { return TimeToCasa(_header.startTime); }
	double CentralTime() const { return _centralCasaTime; }
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
	double _frequency;
	double _centralCasaTime;
};

#endif
