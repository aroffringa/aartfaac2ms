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
		
		if(_header.magic != AartfaacHeader::CORR_HDR_MAGIC)
		{
			throw std::runtime_error("This file does not start with the standard header prefix. It is not a supported Aartfaac correlation file or is damaged.");
		}
		
		_blockSize = sizeof(std::complex<float>) * VisPerTimestep();
		
		if(_header.correlationMode != 15)
		{
			std::ostringstream str;
			str << "This Aartfaac file specifes a correlation mode of '" << int(_header.correlationMode) << "'. This tool can only handle sets with 4 polarizations (mode 15).";
			throw std::runtime_error(str.str());
		}
		
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
		std::cout << "Calculating frequency for " << fn.substr(sbIndex, 5) << ".\n";
		
		// This is from:
		// http://astron.nl/radio-observatory/astronomers/users/
		//   technical-information/frequency-selection/station-clocks-and-rcu
		double frequencyOffset = 0.0;
		switch(mode.mode) {
			case AartfaacMode::LBAInner10_90: // 200 MHz clock, Nyquist zone 1
			case AartfaacMode::LBAInner30_90:
			case AartfaacMode::LBAOuter10_90:
			case AartfaacMode::LBAOuter30_90:
				_bandwidth = 195312.5; // 1/1024 x nu_{clock}
				frequencyOffset = 0.0;
				break;
			case AartfaacMode::HBA110_190: // 200 MHz clock, Nyquist zone 2
				_bandwidth = 195312.5;
				frequencyOffset = 100e6;
				break;
			case AartfaacMode::HBA170_230: // 160 MHz clock, Nyquist zone 3
				_bandwidth = 156250.0;
				frequencyOffset = 160e6;
				break;
			case AartfaacMode::HBA210_270: // 200 MHz clock, Nyquist zone 3
				_bandwidth = 195312.5;
				frequencyOffset = 200e6;
				break;
			default:
				throw std::runtime_error("Don't know how to handle this mode: not implemented yet");
		}
		_frequency = _bandwidth *  _sbIndex + frequencyOffset;
		
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
	
	double Bandwidth() const { return _bandwidth; }
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
	double _frequency, _bandwidth;
	double _centralCasaTime;
};

#endif
