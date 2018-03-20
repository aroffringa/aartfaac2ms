#ifndef AARTFAAC2MS_H
#define AARTFAAC2MS_H

#include <aoflagger.h>

#include "aartfaacfile.h"
#include "aligned_ptr.h"
#include "averagingwriter.h"
#include "stopwatch.h"
#include "progressbar.h"
#include "writer.h"

#include "aocommon/uvector.h"

#include <complex>
#include <map>
#include <memory>
#include <vector>

struct UVW { double u, v, w; };

class Aartfaac2ms : private UVWCalculater
{
public:
	enum OutputFormat { MSOutputFormat, FitsOutputFormat };
		
	Aartfaac2ms();
	
	void Run(const char* inputFilename, const char* outputFilename, const char* antennaConfFilename);
	
	void SetMemPercentage(double memPercentage) { _memPercentage = memPercentage; }
	void SetTimeAveraging(size_t factor) { _timeAvgFactor = factor; }
	void SetInterval(size_t start, size_t end) { _intervalStart = start; _intervalEnd = end; }
	
private:
	void allocateBuffers();
	void processAndWriteTimestep(size_t timeIndex, size_t chunkStart);
	void initializeWriter(const char* outputFilename);
	void initializeWeights(float* outputWeights, double integrationTime);
	virtual void CalculateUVW(double date, size_t antenna1, size_t antenna2, double &u, double &v, double &w);
	
	void setAntennas();
	void setSPWs();
	void setSource();
	void setField();
	void setObservation();
	
	size_t NTimestepsSelected() const
	{
		size_t nTimesteps = _file->NTimesteps();
		if(_intervalEnd!=0 && nTimesteps>(_intervalEnd-_intervalStart))
			nTimesteps = _intervalEnd - _intervalStart;
		return nTimesteps;
	}
	
	std::unique_ptr<class AartfaacFile> _file;
	aoflagger::AOFlagger _flagger;
	std::unique_ptr<Writer> _writer;
	std::unique_ptr<aoflagger::Strategy> _strategy;
	
	// settings
	OutputFormat _outputFormat;
	bool _rfiDetection;
	size_t _timeAvgFactor, _freqAvgFactor;
	double _memPercentage;
	size_t _intervalStart, _intervalEnd;
	bool _useDysco;
	size_t _dyscoDataBitRate;
	size_t _dyscoWeightBitRate;
	std::string _dyscoDistribution;
	std::string _dyscoNormalization;
	double _dyscoDistTruncation;
	
	// data fields
	size_t _nParts;
	std::vector<aoflagger::ImageSet> _imageSetBuffers;
	std::vector<aoflagger::FlagMask> _flagBuffers;
	std::vector<Timestep> _timesteps;
	std::vector<UVW> _uvws;
	ao::uvector<double> _channelFrequenciesHz;
	
	// write buffers
	ao::uvector<bool> _outputFlags;
	aligned_ptr<std::complex<float>> _outputData;
	aligned_ptr<float> _outputWeights;
	
	Stopwatch _readWatch, _processWatch, _writeWatch;
};

#endif

