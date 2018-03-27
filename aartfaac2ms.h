#ifndef AARTFAAC2MS_H
#define AARTFAAC2MS_H

#include <aoflagger.h>

#include "aartfaacfile.h"
#include "aligned_ptr.h"
#include "antennaconfig.h"
#include "averagingwriter.h"
#include "stopwatch.h"
#include "progressbar.h"
#include "writer.h"

#include "aocommon/lane.h"
#include "aocommon/uvector.h"

#include <casacore/measures/Measures/MDirection.h>
#include <casacore/measures/Measures/MPosition.h>

#include <complex>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

struct UVW { double u, v, w; };

class Aartfaac2ms
{
public:
	enum OutputFormat { MSOutputFormat, FitsOutputFormat };
		
	Aartfaac2ms();
	
	void Run(const char* inputFilename, const char* outputFilename, const char* antennaConfFilename, AartfaacMode mode);
	
	void SetMemPercentage(double memPercentage) { _memPercentage = memPercentage; }
	void SetThreadCount(size_t nThreads) { _threadCount = nThreads; }
	void SetTimeAveraging(size_t factor) { _timeAvgFactor = factor; }
	void SetFrequencyAveraging(size_t factor) { _freqAvgFactor = factor; }
	void SetInterval(size_t start, size_t end) { _intervalStart = start; _intervalEnd = end; }
	void SetPhaseCentre(double ra, double dec) {
		_manualPhaseCentre = true;
		_manualPhaseCentreRA = ra;
		_manualPhaseCentreDec = dec;
	}
	void SetRFIDetection(bool detectRFI) { _rfiDetection = detectRFI; }
	void SetUseDysco(bool useDysco) { _useDysco = useDysco; }
	void SetAdvancedDyscoOptions(size_t dataBitRate, size_t weightBitRate, const std::string& distribution, double distTruncation, const std::string& normalization)
	{
		_dyscoDataBitRate = dataBitRate;
		_dyscoWeightBitRate = weightBitRate;
		_dyscoDistribution = distribution;
		_dyscoDistTruncation = distTruncation;
		_dyscoNormalization = normalization;
	}
	
private:
	void allocateBuffers();
	void processAndWriteTimestep(size_t timeIndex, size_t chunkStart);
	void initializeWriter(const char* outputFilename);
	void initializeWeights(float* outputWeights, double integrationTime);
	void readAntennaPositions(const char* antennaConfFilename);
	void baselineProcessThreadFunc(ProgressBar* progressBar);
	void processBaseline(size_t baseline, aoflagger::QualityStatistics& threadStatistics);
	void writeAartfaacFieldsToMS(const std::string& outputFilename, size_t flagWindowSize);
	
	void setAntennas();
	void setSPWs();
	void setSource();
	void setField();
	void setObservation();
	
	size_t NTimestepsSelected() const
	{
		size_t nTimesteps = _reader->NTimesteps();
		if(_intervalEnd!=0 && nTimesteps>(_intervalEnd-_intervalStart))
			nTimesteps = _intervalEnd - _intervalStart;
		return nTimesteps;
	}
	
	std::unique_ptr<class AartfaacFile> _reader;
	aoflagger::AOFlagger _flagger;
	std::unique_ptr<aoflagger::QualityStatistics> _statistics;
	std::unique_ptr<Writer> _writer;
	std::unique_ptr<aoflagger::Strategy> _strategy;
	std::mutex _mutex;
	ao::lane<size_t> _baselinesToProcess;
	
	// settings
	AartfaacMode _mode;
	OutputFormat _outputFormat;
	bool _rfiDetection, _collectStatistics, _collectHistograms;
	size_t _timeAvgFactor, _freqAvgFactor;
	double _memPercentage;
	size_t _intervalStart, _intervalEnd;
	bool _manualPhaseCentre;
	double _manualPhaseCentreRA, _manualPhaseCentreDec;
	bool _useDysco;
	size_t _dyscoDataBitRate;
	size_t _dyscoWeightBitRate;
	std::string _dyscoDistribution;
	std::string _dyscoNormalization;
	double _dyscoDistTruncation;
	size_t _threadCount;
	
	// data fields
	size_t _nParts;
	std::vector<aoflagger::ImageSet> _imageSetBuffers;
	std::vector<aoflagger::FlagMask> _flagBuffers;
	aoflagger::FlagMask _correlatorMask;
	std::vector<double> _timestepsStart, _timestepsEnd;
	std::vector<std::pair<size_t, size_t>> _baselines;
	std::vector<UVW> _uvws;
	std::vector<casacore::MPosition> _antennaPositions;
	std::array<double, 9> _antennaAxes;
	casacore::MDirection _phaseDirection;
	ao::uvector<double> _channelFrequenciesHz;
	
	// write buffers
	ao::uvector<bool> _outputFlags;
	aligned_ptr<std::complex<float>> _outputData;
	aligned_ptr<float> _outputWeights;
	
	Stopwatch _readWatch, _processWatch, _writeWatch;
};

#endif

