#include "aartfaac2ms.h"

#include "aartfaacms.h"
#include "averagingwriter.h"
#include "fitswriter.h"
#include "mswriter.h"
#include "threadedwriter.h"
#include "version.h"

#include "units/radeccoord.h"

#include <casacore/measures/Measures/MBaseline.h>
#include <casacore/measures/Measures/MCBaseline.h>
#include <casacore/measures/Measures/MCDirection.h>
#include <casacore/measures/Measures/MeasConvert.h>
#include <casacore/measures/Measures/MEpoch.h>
#include <casacore/measures/Measures/MPosition.h>
#include <casacore/measures/Measures/Muvw.h>

#include <complex>
#include <iostream>
#include <fstream>

#include <unistd.h>

#include <xmmintrin.h>

#define USE_SSE

#define SPEED_OF_LIGHT 299792458.0        // speed of light in m/s

using namespace aoflagger;

Aartfaac2ms::Aartfaac2ms() :
	_flagger(),
	_statistics(),
	_mode(AartfaacMode::Unused),
	_outputFormat(MSOutputFormat),
	_rfiDetection(false),
	_collectStatistics(true),
	_collectHistograms(false),
	_timeAvgFactor(1), _freqAvgFactor(1),
	_memPercentage(50),
	_intervalStart(0), _intervalEnd(0),
	_manualPhaseCentre(false),
	_manualPhaseCentreRA(0.0),
	_manualPhaseCentreDec(0.0),
	_useDysco(false),
	_dyscoDataBitRate(8),
	_dyscoWeightBitRate(12),
	_dyscoDistribution("TruncatedGaussian"),
	_dyscoNormalization("AF"),
	_dyscoDistTruncation(2.5),
	_threadCount(1),
	_outputData(empty_aligned<std::complex<float>>()),
	_outputWeights(empty_aligned<float>())
{
}

void Aartfaac2ms::allocateBuffers()
{
	bool rfiDetection = false;
	double memLimit = 0.0;
	long int pageCount = sysconf(_SC_PHYS_PAGES), pageSize = sysconf(_SC_PAGE_SIZE);
	int64_t memSize = (int64_t) pageCount * (int64_t) pageSize;
	double memSizeInGB = (double) memSize / (1024.0*1024.0*1024.0);
	double memPercentage = _memPercentage;
	if(memLimit == 0.0)
		std::cout << "Detected " << round(memSizeInGB*10.0)/10.0 << " GB of system memory.\n";
	else {
		std::cout << "Using " << round(memLimit*10.0)/10.0 << '/' << round(memSizeInGB*10.0)/10.0 << " GB of system memory.\n";
		memSize = int64_t(memLimit * (1024.0*1024.0*1024.0));
		memPercentage = 100.0;
	}
	size_t nChannelSpace = ((((_reader->NChannels()-1)/4)+1)*4);
	size_t maxSamples = memSize*memPercentage/(100*(sizeof(float)*2+1));
	size_t nAntennas = _reader->NAntennas();
	size_t maxScansPerPart = maxSamples / (4*nChannelSpace*(nAntennas+1)*nAntennas/2);
	std::cout << "Timesteps that fit in memory: " << maxScansPerPart << '\n';
	if(maxScansPerPart<1)
	{
		std::cout << "WARNING! The given amount of memory is not even enough for one scan and therefore below the minimum that Aartfaac2ms will need; will use more memory. Expect swapping and very poor flagging accuracy.\nWARNING! This is a *VERY BAD* condition, so better make sure to resolve it!";
		maxScansPerPart = 1;
	} else if(maxScansPerPart<20 && rfiDetection)
	{
		std::cout << "WARNING! This computer does not have enough memory for accurate flagging; expect non-optimal flagging accuracy.\n"; 
	}
	size_t nTimesteps = NTimestepsSelected();
	_nParts = 1 + nTimesteps / maxScansPerPart;
	if(_nParts == 1)
		std::cout << "All " << nTimesteps << " scans fit in memory; no partitioning necessary.\n";
	else
		std::cout << "Observation does not fit fully in memory, will partition data in " << _nParts << " chunks of " << (nTimesteps/_nParts) << " scans.\n";
	
	const size_t requiredWidthCapacity = (nTimesteps+_nParts-1)/_nParts;
	for(size_t antenna1=0;antenna1!=_reader->NAntennas();++antenna1)
	{
		for(size_t antenna2=antenna1; antenna2!=_reader->NAntennas(); ++antenna2)
		{
			_imageSetBuffers.emplace_back(_flagger.MakeImageSet(requiredWidthCapacity, _reader->NChannels(), 8, 0.0f, requiredWidthCapacity));
		}
	}
}

void Aartfaac2ms::initializeWriter(const char* outputFilename)
{
	switch(_outputFormat)
	{
		case FitsOutputFormat:
			_writer.reset(new ThreadedWriter(std::unique_ptr<Writer>(new FitsWriter(outputFilename))));
			break;
		case MSOutputFormat: {
			std::unique_ptr<MSWriter> msWriter(new MSWriter(outputFilename));
			if(_useDysco)
				msWriter->EnableCompression(_dyscoDataBitRate, _dyscoWeightBitRate, _dyscoDistribution, _dyscoDistTruncation, _dyscoNormalization);
			_writer.reset(new ThreadedWriter(std::move(msWriter)));
		} break;
	}
	
	if(_freqAvgFactor != 1 || _timeAvgFactor != 1)
	{
		_writer.reset(new ThreadedWriter(std::unique_ptr<Writer>(new AveragingWriter(std::move(_writer), _timeAvgFactor, _freqAvgFactor))));
	}
	
	setAntennas();
	setSPWs();
	setSource();
	setField();
	_writer->WritePolarizationForLinearPols(false);
	setObservation();
}

void Aartfaac2ms::setAntennas()
{
	std::vector<Writer::AntennaInfo> antennas(_reader->NAntennas());
	for(size_t ant=0; ant!=antennas.size(); ++ant) {
		Writer::AntennaInfo &antennaInfo = antennas[ant];
		antennaInfo.name = std::string("A12_") + std::to_string(ant);
		antennaInfo.station = "AARTFAAC";
		antennaInfo.type = "GROUND-BASED";
		antennaInfo.mount = "ALT-AZ"; // TODO should be "FIXED", but Casa does not like
		antennaInfo.x = _antennaPositions[ant].getValue()(0);
		antennaInfo.y = _antennaPositions[ant].getValue()(1);
		antennaInfo.z = _antennaPositions[ant].getValue()(2);
		antennaInfo.diameter = 1; /** TODO can probably give more exact size! */
		antennaInfo.flag = false;
	}
	_writer->WriteAntennae(antennas, _reader->StartTime());
}

void Aartfaac2ms::setSPWs()
{
	std::vector<MSWriter::ChannelInfo> channels(_reader->NChannels());
	_channelFrequenciesHz.resize(_reader->NChannels());
	std::ostringstream str;
	str << "AARTF_BAND_" << (round(1e-6*_reader->Frequency()*10.0)/10.0);
	const double chWidth = _reader->Bandwidth() / _reader->NChannels();
	double startFrequency = _reader->Frequency() - _reader->Bandwidth()*0.5;
	for(size_t ch=0; ch!=channels.size(); ++ch)
	{
		MSWriter::ChannelInfo& channel = channels[ch];
		_channelFrequenciesHz[ch] = startFrequency + chWidth*(0.5+double(ch));
		channel.chanFreq = _channelFrequenciesHz[ch];
		channel.chanWidth = chWidth;
		channel.effectiveBW = chWidth;
		channel.resolution = chWidth;
	}
	_writer->WriteBandInfo(str.str(),
		channels,
		_reader->Frequency(),
		_reader->Bandwidth(),
		false
	);
}

void Aartfaac2ms::setSource()
{
	MSWriter::SourceInfo source;
	source.sourceId = 0;
	source.time = _reader->StartTime();
	source.interval = _reader->StartTime() + _reader->IntegrationTime()*_reader->NTimesteps();
	source.spectralWindowId = 0;
	source.numLines = 0;
	source.name = "AARTFAAC";
	source.calibrationGroup = 0;
	source.code = "";
	double ra = _phaseDirection.getAngle().getValue()[0];
	double dec = _phaseDirection.getAngle().getValue()[1];
	source.directionRA = ra; // (in radians)
	source.directionDec = dec;
	source.properMotion[0] = 0.0;
	source.properMotion[1] = 0.0;
	_writer->WriteSource(source);
}

void Aartfaac2ms::setField()
{
	MSWriter::FieldInfo field;
	field.name = "AARTFAAC";
	field.code = std::string();
	field.time = _reader->StartTime();
	field.numPoly = 0;
	double ra = _phaseDirection.getAngle().getValue()[0];
	double dec = _phaseDirection.getAngle().getValue()[1];
	field.delayDirRA = ra; // (in radians)
	field.delayDirDec = dec;
	field.phaseDirRA = field.delayDirRA;
	field.phaseDirDec = field.delayDirDec;
	field.referenceDirRA = field.delayDirRA;
	field.referenceDirDec = field.delayDirDec;
	field.sourceId = -1;
	field.flagRow = false;
	_writer->WriteField(field);
}

void Aartfaac2ms::setObservation()
{
	Writer::ObservationInfo observation;
	observation.telescopeName = "AARTFAAC";
	observation.startTime = _reader->StartTime();
	observation.endTime = _reader->StartTime() + _reader->IntegrationTime()*_reader->NTimesteps();
	observation.observer = "Unknown";
	observation.scheduleType = "AARTFAAC";
	observation.project = "Unknown";
	observation.releaseDate = 0;
	observation.flagRow = false;
	
	_writer->WriteObservation(observation);
}

void Aartfaac2ms::baselineProcessThreadFunc(ProgressBar* progressBar)
{
	QualityStatistics threadStatistics =
		_flagger.MakeQualityStatistics(_timestepsStart.data(), _timestepsStart.size(), &_channelFrequenciesHz[0], _channelFrequenciesHz.size(), 4, _collectHistograms);
	
	size_t baseline;
	while(_baselinesToProcess.read(baseline))
	{
		size_t currentTaskCount = baseline;
		std::unique_lock<std::mutex> lock(_mutex);
		progressBar->SetProgress(currentTaskCount, _imageSetBuffers.size());
		lock.unlock();
		
		processBaseline(baseline, threadStatistics);
	}
	
	// Mutex needs to be locked
	std::unique_lock<std::mutex> lock(_mutex);
	if(_statistics == nullptr)
		_statistics.reset(new QualityStatistics(threadStatistics));
	else
		(*_statistics) += threadStatistics;
}

void Aartfaac2ms::processBaseline(size_t baselineIndex, QualityStatistics& threadStatistics)
{
	ImageSet& imageSet = _imageSetBuffers[baselineIndex];
	FlagMask& flagMask = _flagBuffers[baselineIndex];
	const std::pair<size_t, size_t>& baseline = _baselines[baselineIndex];
	
	if(_rfiDetection && (baseline.first != baseline.second))
		flagMask = _flagger.Run(*_strategy, imageSet);
	else
		flagMask = _flagger.MakeFlagMask(_timestepsStart.size(), _channelFrequenciesHz.size(), false);

	_flagger.CollectStatistics(threadStatistics, imageSet, flagMask, _correlatorMask, baseline.first, baseline.second);
}

void Aartfaac2ms::Run(const char* inputFilename, const char* outputFilename, const char* antennaConfFilename, AartfaacMode mode)
{
	_mode = mode;
	_reader.reset(new AartfaacFile(inputFilename, mode));
	
	readAntennaPositions(antennaConfFilename);
	
	ao::uvector<std::complex<float>> vis(_reader->VisPerTimestep());
	size_t index = 0;
	
	allocateBuffers();
	
	initializeWriter(outputFilename);
	
	if(_rfiDetection)
	{
		_strategy.reset(new Strategy(_flagger.MakeStrategy(
			aoflagger::TelescopeId::AARTFAAC_TELESCOPE,
			aoflagger::StrategyFlags::NONE,
			_reader->Frequency(),
			_reader->IntegrationTime(),
			_reader->Bandwidth() )));
	}
	
	_reader->SeekToTimestep(_intervalStart);
	
	ao::uvector<size_t> baselineMap(_reader->NAntennas()*_reader->NAntennas());
	size_t bIndex = 0;
	for(size_t antenna1=0; antenna1!=_reader->NAntennas(); ++antenna1)
	{
		for(size_t antenna2=antenna1; antenna2!=_reader->NAntennas(); ++antenna2)
		{
			baselineMap[antenna2 + antenna1*_reader->NAntennas()] = bIndex;
			++bIndex;
		}
	}

	for(size_t chunkIndex = 0; chunkIndex != _nParts; ++chunkIndex)
	{
		std::cout << "=== Processing chunk " << (chunkIndex+1) << " of " << _nParts << " ===\n";
		
		size_t nTimesteps = NTimestepsSelected();
		size_t chunkStart = nTimesteps*chunkIndex/_nParts + _intervalStart;
		size_t chunkEnd = nTimesteps*(chunkIndex+1)/_nParts + _intervalStart;
		for(ImageSet& imageSet : _imageSetBuffers)
			imageSet.ResizeWithoutReallocation(chunkEnd-chunkStart);
		
		_correlatorMask = _flagger.MakeFlagMask(chunkEnd-chunkStart, _reader->NChannels(), false);
		
		_readWatch.Start();
		_timestepsStart.clear();
		_timestepsEnd.clear();
		ProgressBar progress("Reading");
		for(size_t timeIndex=chunkStart; timeIndex!=chunkEnd; ++timeIndex)
		{
			progress.SetProgress(timeIndex-chunkStart, chunkEnd-chunkStart);
			
			Timestep step = _reader->ReadTimestep(vis.data());
			_timestepsStart.emplace_back(step.startTime);
			_timestepsEnd.emplace_back(step.endTime);
			
			size_t bufferIndex = timeIndex-chunkStart;
			std::complex<float>* visPtr = vis.data();
			for(size_t antenna1=0; antenna1!=_reader->NAntennas(); ++antenna1)
			{
				for(size_t antenna2=0; antenna2<=antenna1; ++antenna2)
				{
					size_t bIndex = baselineMap[antenna1 + antenna2*_reader->NAntennas()];
					ImageSet& imageSet = _imageSetBuffers[bIndex];
					for(size_t ch=0; ch!=_reader->NChannels(); ++ch)
					{
						for(size_t p=0; p!=4; ++p)
						{
							float
								*realPtr = imageSet.ImageBuffer(p*2)+bufferIndex,
								*imagPtr = imageSet.ImageBuffer(p*2+1)+bufferIndex;
							realPtr[ch*imageSet.HorizontalStride()] = visPtr->real();
							imagPtr[ch*imageSet.HorizontalStride()] = visPtr->imag();
							++visPtr;
						}
					}
				}
			}
		}	
		++index;
		_readWatch.Pause();
		
		progress = ProgressBar("Processing baselines");
		_processWatch.Start();
		
		_flagBuffers.clear();
		_baselines.clear();
		for(size_t antenna1=0;antenna1!=_reader->NAntennas();++antenna1)
		{
			for(size_t antenna2=antenna1; antenna2!=_reader->NAntennas(); ++antenna2)
			{
				_flagBuffers.emplace_back();
				_baselines.emplace_back(antenna1, antenna2);
			}
		}
		
		_baselinesToProcess.resize(_threadCount);
		std::vector<std::thread> threadGroup;
		for(size_t i=0; i!=_threadCount; ++i)
			threadGroup.emplace_back(std::bind(&Aartfaac2ms::baselineProcessThreadFunc, this, &progress));
		for(size_t baselineIndex=0; baselineIndex!=_imageSetBuffers.size(); ++baselineIndex)
			_baselinesToProcess.write(baselineIndex);
		_baselinesToProcess.write_end();
		for(std::thread& t : threadGroup)
			t.join();
		
		_processWatch.Pause();
		progress = ProgressBar("Writing");
		_writeWatch.Start();
		_outputFlags.resize(_reader->NChannels()*4);
		_outputData = make_aligned<std::complex<float>>(_reader->NChannels()*4, 16);
		_outputWeights = make_aligned<float>(_reader->NChannels()*4, 16);
		for(size_t timeIndex=chunkStart; timeIndex!=chunkEnd; ++timeIndex)
		{
			progress.SetProgress(timeIndex-chunkStart, chunkEnd-chunkStart);
			processAndWriteTimestep(timeIndex, chunkStart);
		}
		_writeWatch.Pause();
		
		_flagBuffers.clear();
	}
	
	std::cout << "Read: " << _readWatch.ToString() << ", processing: " << _processWatch.ToString() << ", writing: " << _writeWatch.ToString() << '\n';
	
	_writer.reset();
	
	if(_collectStatistics) {
		std::cout << "Writing statistics to measurement set...\n";
		_flagger.WriteStatistics(*_statistics, outputFilename);
	}
	
	if(_outputFormat == MSOutputFormat)
	{
		std::cout << "Writing AARTFAAC fields to measurement set...\n";
		writeAartfaacFieldsToMS(outputFilename, NTimestepsSelected() /_nParts);
	}
}

casacore::Muvw calculateUVW(const casacore::MPosition &antennaPos, const casacore::MPosition &refPos,
	const casacore::MEpoch &time, const casacore::MDirection &direction)
{
	const casacore::Vector<double> posVec = antennaPos.getValue().getVector();
	const casacore::Vector<double> refVec = refPos.getValue().getVector();
	casacore::MVPosition relativePos(posVec[0]-refVec[0], posVec[1]-refVec[1], posVec[2]-refVec[2]);
	casacore::MeasFrame frame(time, refPos, direction);
	casacore::MBaseline baseline(casacore::MVBaseline(relativePos), casacore::MBaseline::Ref(casacore::MBaseline::ITRF, frame));
	casacore::MBaseline j2000Baseline = casacore::MBaseline::Convert(baseline, casacore::MBaseline::J2000)();
	casacore::MVuvw uvw(j2000Baseline.getValue(), direction.getValue());
	return casacore::Muvw(uvw, casacore::Muvw::J2000);
}

void Aartfaac2ms::processAndWriteTimestep(size_t timeIndex, size_t chunkStart)
{
	const size_t nAntennas = _reader->NAntennas();
	const size_t nChannels = _reader->NChannels();
	const size_t nBaselines = nAntennas*(nAntennas+1)/2;
	const double startTime = _timestepsStart[timeIndex - chunkStart];
	const double exposure = _timestepsEnd[timeIndex - chunkStart] - startTime;
	
	_uvws.resize(_reader->NAntennas());
	casacore::MEpoch timeEpoch = casacore::MEpoch(casacore::MVEpoch(startTime/86400.0), casacore::MEpoch::UTC);
	for(size_t antenna=0; antenna!=nAntennas; ++antenna)
	{
		casacore::MVuvw uvw = calculateUVW(_antennaPositions[antenna], _antennaPositions[0], timeEpoch, _phaseDirection).getValue();
		_uvws[antenna] = UVW { uvw.getVector()[0], uvw.getVector()[1], uvw.getVector()[2] };
	}
	
	_writer->AddRows(nBaselines);
	
	ao::uvector<double>
		cosAngles(nChannels),
		sinAngles(nChannels);
	
	initializeWeights(_outputWeights.get(), exposure);
	size_t baselineIndex = 0;
	for(size_t antenna1=0; antenna1!=nAntennas; ++antenna1)
	{
		for(size_t antenna2=antenna1; antenna2!=nAntennas; ++antenna2)
		{
			const ImageSet& imageSet = _imageSetBuffers[baselineIndex];
			const FlagMask& flagMask = _flagBuffers[baselineIndex];
			
			const size_t stride = imageSet.HorizontalStride();
			const size_t flagStride = flagMask.HorizontalStride();
			double
				u = _uvws[antenna1].u - _uvws[antenna2].u,
				v = _uvws[antenna1].v - _uvws[antenna2].v,
				w = _uvws[antenna1].w - _uvws[antenna2].w;
					
			// Pre-calculate rotation coefficients for geometric phase delay correction
			for(size_t ch=0; ch!=nChannels; ++ch)
			{
				double angle = -2.0*M_PI*w*_channelFrequenciesHz[ch] / SPEED_OF_LIGHT;
				sinAngles[ch] = sin(angle);
				cosAngles[ch] = cos(angle);
			}

			size_t bufferIndex = timeIndex - chunkStart;
#ifndef USE_SSE
			for(size_t p=0; p!=4; ++p)
			{
				const float
					*realPtr = imageSet.ImageBuffer(p*2)+bufferIndex,
					*imagPtr = imageSet.ImageBuffer(p*2+1)+bufferIndex;
				const bool *flagPtr = flagMask.Buffer()+bufferIndex;
				std::complex<float> *outDataPtr = &_outputData[p];
				bool *outputFlagPtr = &_outputFlags[p];
				for(size_t ch=0; ch!=nChannels; ++ch)
				{
					const float rtmp = *realPtr, itmp = *imagPtr;
					// Apply geometric phase delay (for w)
					*outDataPtr = std::complex<float>(
						cosAngles[ch] * rtmp - sinAngles[ch] * itmp,
						sinAngles[ch] * rtmp + cosAngles[ch] * itmp
					);
					*outputFlagPtr = *flagPtr;
					realPtr += stride;
					imagPtr += stride;
					flagPtr += flagStride;
					outDataPtr += 4;
					outputFlagPtr += 4;
				}
			}
	#else
			const float
				*realAPtr = imageSet.ImageBuffer(0)+bufferIndex,
				*imagAPtr = imageSet.ImageBuffer(1)+bufferIndex,
				*realBPtr = imageSet.ImageBuffer(2)+bufferIndex,
				*imagBPtr = imageSet.ImageBuffer(3)+bufferIndex,
				*realCPtr = imageSet.ImageBuffer(4)+bufferIndex,
				*imagCPtr = imageSet.ImageBuffer(5)+bufferIndex,
				*realDPtr = imageSet.ImageBuffer(6)+bufferIndex,
				*imagDPtr = imageSet.ImageBuffer(7)+bufferIndex;
			const bool *flagPtr = flagMask.Buffer()+bufferIndex;
			std::complex<float> *outDataPtr = &_outputData[0];
			bool *outputFlagPtr = &_outputFlags[0];
			for(size_t ch=0; ch!=nChannels; ++ch)
			{
				// Apply geometric phase delay (for w)
				// Note that order within set_ps is reversed; for the four complex numbers,
				// the first two compl are loaded corresponding to set_ps(imag2, real2, imag1, real1).
				__m128 ra = _mm_set_ps(*realBPtr, *realBPtr, *realAPtr, *realAPtr);
				__m128 rb = _mm_set_ps(*realDPtr, *realDPtr, *realCPtr, *realCPtr);
				__m128 rgeom = _mm_set_ps(sinAngles[ch], cosAngles[ch], sinAngles[ch], cosAngles[ch]);
				__m128 ia = _mm_set_ps(*imagBPtr, *imagBPtr, *imagAPtr, *imagAPtr);
				__m128 ib = _mm_set_ps(*imagDPtr, *imagDPtr, *imagCPtr, *imagCPtr);
				__m128 igeom = _mm_set_ps(cosAngles[ch], -sinAngles[ch], cosAngles[ch], -sinAngles[ch]);
				__m128 outa = _mm_add_ps(_mm_mul_ps(ra, rgeom), _mm_mul_ps(ia, igeom));
				__m128 outb = _mm_add_ps(_mm_mul_ps(rb, rgeom), _mm_mul_ps(ib, igeom));
				_mm_store_ps((float*) outDataPtr, outa);
				_mm_store_ps((float*) (outDataPtr+2), outb);
				
				*outputFlagPtr = *flagPtr; ++outputFlagPtr;
				*outputFlagPtr = *flagPtr; ++outputFlagPtr;
				*outputFlagPtr = *flagPtr; ++outputFlagPtr;
				*outputFlagPtr = *flagPtr; ++outputFlagPtr;
				realAPtr += stride; imagAPtr += stride;
				realBPtr += stride; imagBPtr += stride;
				realCPtr += stride; imagCPtr += stride;
				realDPtr += stride; imagDPtr += stride;
				flagPtr += flagStride;
				outDataPtr += 4;
			}
	#endif
				
			_writer->WriteRow(startTime, startTime, antenna1, antenna2, u, v, w, exposure, _outputData.get(), _outputFlags.data(), _outputWeights.get());
			++baselineIndex;
		}
	}
}

void Aartfaac2ms::readAntennaPositions(const char* antennaConfFilename)
{
	AntennaConfig antConf(antennaConfFilename);
	std::vector<Position> positions;
	switch(_mode.mode)
	{
		case AartfaacMode::LBAInner10_90:
		case AartfaacMode::LBAInner30_90:
		case AartfaacMode::LBAOuter10_90:
		case AartfaacMode::LBAOuter30_90:
			std::cout << "Using LBA antenna positions.\n";
			positions = antConf.GetLBAPositions();
			_antennaAxes = antConf.GetLBAAxes();
			break;
		case AartfaacMode::HBA110_190:
		case AartfaacMode::HBA170_230:
		case AartfaacMode::HBA210_270:
			std::cout << "Using HBA antenna positions.\n";
			positions = antConf.GetHBAPositions();
			_antennaAxes = antConf.GetHBA0Axes();
			break;
		default:
			throw std::runtime_error("Wrong RCU mode");
	}
	for(const Position& p : positions)
	{
		_antennaPositions.emplace_back(
			casacore::MVPosition(p.x, p.y, p.z),
			casacore::MPosition::ITRF);
	}
	
	size_t lastTimestep = _intervalEnd;
	if(lastTimestep == 0)
		lastTimestep = _reader->NTimesteps();
	_reader->SeekToTimestep((_intervalStart + lastTimestep) / 2);
	double centralTime = _reader->ReadMetadata().startTime;
	casacore::MEpoch time = casacore::MEpoch(casacore::MVEpoch(centralTime/86400.0), casacore::MEpoch::UTC);
	casacore::MeasFrame frame(_antennaPositions[0], time);

	const casacore::MDirection::Ref azelRef(casacore::MDirection::AZEL, frame);
	const casacore::MDirection::Ref j2000Ref(casacore::MDirection::J2000, frame);
	casacore::MDirection zenithAzEl(casacore::MVDirection(0.0, 0.0, 1.0), azelRef);
	_phaseDirection = casacore::MDirection::Convert(zenithAzEl, j2000Ref)();
	double ra = _phaseDirection.getAngle().getValue()[0];
	double dec = _phaseDirection.getAngle().getValue()[1];
	std::cout << "Central time: " << time << ", zenith direction: " << RaDecCoord::RaDecToString(ra, dec) << '\n';
	if(_manualPhaseCentre) {
		_phaseDirection.set(casacore::MVDirection(_manualPhaseCentreRA, _manualPhaseCentreDec), j2000Ref);
		std::cout << "Using manual phase centre: "
			<< RaDecCoord::RaDecToString(_manualPhaseCentreRA, _manualPhaseCentreDec) << '\n';
	}
	else {
		std::cout << "Zenith direction at central time is used as phase direction.\n";
	}
}

void Aartfaac2ms::initializeWeights(float* outputWeights, double integrationTime)
{
	// Weights are normalized so that a 'reasonable' res of 200 kHz, 1s has
	// weight of "1" per sample.
	// Note that this only holds for numbers in the WEIGHTS_SPECTRUM column; WEIGHTS will hold the sum.
	double weightFactor = integrationTime * (_reader->Bandwidth()/_reader->NChannels());
	for(size_t ch=0; ch!=_reader->NChannels(); ++ch)
	{
		for(size_t p=0; p!=4; ++p)
			outputWeights[ch*4 + p] = weightFactor;
	}
}

void Aartfaac2ms::writeAartfaacFieldsToMS(const std::string& outputFilename, size_t flagWindowSize)
{
	AartfaacMS afMs(outputFilename);
	afMs.InitializeFields();
	const char* modeStr;
	switch(_mode.mode)
	{
		case AartfaacMode::LBAInner10_90:
		case AartfaacMode::LBAInner30_90:
		case AartfaacMode::LBAOuter10_90:
		case AartfaacMode::LBAOuter30_90:
			modeStr = "LBA";
			break;
		case AartfaacMode::HBA110_190:
		case AartfaacMode::HBA170_230:
		case AartfaacMode::HBA210_270:
			modeStr = "HBA";
			break;
		default:
			modeStr = "?";
			break;
	}
	afMs.UpdateObservationInfo(modeStr, _mode.mode, flagWindowSize);
	afMs.WriteKeywords(AF2MS_VERSION_STR, AF2MS_VERSION_DATE, _antennaAxes);
}
