#include "aartfaac2ms.h"

#include "averagingwriter.h"
#include "fitswriter.h"
#include "mswriter.h"
#include "threadedwriter.h"

#include <complex>
#include <iostream>
#include <fstream>

#include <unistd.h>

#define USE_SSE

using namespace aoflagger;

Aartfaac2ms::Aartfaac2ms() :
	_outputFormat(MSOutputFormat),
	_timeAvgFactor(1), _freqAvgFactor(1),
	_memPercentage(90),
	_useDysco(false),
	_dyscoDataBitRate(8),
	_dyscoWeightBitRate(12),
	_dyscoDistribution("TruncatedGaussian"),
	_dyscoNormalization("AF"),
	_dyscoDistTruncation(2.5),
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
	size_t nChannelSpace = ((((_file->NChannels()-1)/4)+1)*4);
	size_t maxSamples = memSize*memPercentage/(100*(sizeof(float)*2+1));
	size_t nAntennas = _file->NAntennas();
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
	_nParts = 1 + _file->NTimesteps() / maxScansPerPart;
	if(_nParts == 1)
		std::cout << "All " << _file->NTimesteps() << " scans fit in memory; no partitioning necessary.\n";
	else
		std::cout << "Observation does not fit fully in memory, will partition data in " << _nParts << " chunks of " << (_file->NTimesteps()/_nParts) << " scans.\n";
	
	const size_t requiredWidthCapacity = (_file->NTimesteps()+_nParts-1)/_nParts;
	for(size_t antenna1=0;antenna1!=_file->NAntennas();++antenna1)
	{
		for(size_t antenna2=antenna1; antenna2!=_file->NAntennas(); ++antenna2)
		{
			_imageSetBuffers.emplace_back(_flagger.MakeImageSet(requiredWidthCapacity, _file->NChannels(), 8, 0.0f, requiredWidthCapacity));
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
		_writer.reset(new ThreadedWriter(std::unique_ptr<Writer>(new AveragingWriter(std::move(_writer), _timeAvgFactor, _freqAvgFactor, *this))));
	}
	
	setAntennas();
	setSPWs();
}

void Aartfaac2ms::setAntennas()
{
	std::vector<Writer::AntennaInfo> antennas(_file->NAntennas());
	for(size_t ant=0; ant!=antennas.size(); ++ant) {
		Writer::AntennaInfo &antennaInfo = antennas[ant];
		antennaInfo.name = std::string("aartfaac") + std::to_string(ant);
		antennaInfo.station = "AARTFAAC";
		antennaInfo.type = "GROUND-BASED";
		antennaInfo.mount = "ALT-AZ"; // TODO should be "FIXED", but Casa does not like
		antennaInfo.x = 0; // TODO
		antennaInfo.y = 0;
		antennaInfo.z = 0;
		antennaInfo.diameter = 1; /** TODO can probably give more exact size! */
		antennaInfo.flag = false;
	}
	_writer->WriteAntennae(antennas, _file->StartTime());
}

void Aartfaac2ms::setSPWs()
{
	std::vector<MSWriter::ChannelInfo> channels(_file->NChannels());
	_channelFrequenciesHz.resize(_file->NChannels());
	std::ostringstream str;
	double centreFrequencyMHz = 1e-6*(_file->Frequency() + _file->Bandwidth()*0.5);
	str << "AARTF_BAND_" << (round(centreFrequencyMHz*10.0)/10.0);
	const double chWidth = _file->Bandwidth() / _file->NChannels();
	for(size_t ch=0; ch!=channels.size(); ++ch)
	{
		MSWriter::ChannelInfo& channel = channels[ch];
		_channelFrequenciesHz[ch] = _file->Frequency() + _file->Bandwidth()*ch/_file->Bandwidth();
		channel.chanFreq = _channelFrequenciesHz[ch];
		channel.chanWidth = chWidth;
		channel.effectiveBW = chWidth;
		channel.resolution = chWidth;
	}
	_writer->WriteBandInfo(str.str(),
		channels,
		centreFrequencyMHz*1000000.0,
		_file->Bandwidth(),
		false
	);
}

void Aartfaac2ms::Run(const char* inputFilename, const char* outputFilename)
{
	_file.reset(new AartfaacFile(inputFilename));
	ao::uvector<std::complex<float>> vis(_file->VisPerTimestep());
	size_t index = 0;
	
	allocateBuffers();
	
	initializeWriter(outputFilename);
	
	for(size_t chunkIndex = 0; chunkIndex != _nParts; ++chunkIndex)
	{
		std::cout << "=== Processing chunk " << (chunkIndex+1) << " of " << _nParts << " ===\n";
		
		size_t chunkStart = _file->NTimesteps()*chunkIndex/_nParts;
		size_t chunkEnd = _file->NTimesteps()*(chunkIndex+1)/_nParts;
		for(ImageSet& imageSet : _imageSetBuffers)
			imageSet.ResizeWithoutReallocation(chunkEnd-chunkStart);
		
		_readWatch.Start();
		_timesteps.clear();
		ProgressBar progress("Reading");
		for(size_t timeIndex=chunkStart; timeIndex!=chunkEnd; ++timeIndex)
		{
			_timesteps.emplace_back(_file->ReadTimestep(vis.data()));
			
			progress.SetProgress(timeIndex-chunkStart, chunkEnd-chunkStart);
			
			// TODO copy vis into the per-baseline buffers
		}	
		_readWatch.Pause();
		++index;
		
		// TODO Flag!
		for(size_t antenna1=0;antenna1!=_file->NAntennas();++antenna1)
		{
			for(size_t antenna2=antenna1; antenna2!=_file->NAntennas(); ++antenna2)
			{
				_flagBuffers.emplace_back(_flagger.MakeFlagMask(chunkEnd-chunkStart, _file->NChannels(), true));
			}
		}
		
		progress = ProgressBar("Writing");
		_outputFlags.resize(_file->NChannels()*4);
		_outputData = make_aligned<std::complex<float>>(_file->NChannels()*4, 16);
		_outputWeights = make_aligned<float>(_file->NChannels()*4, 16);
		for(size_t timeIndex=chunkStart; timeIndex!=chunkEnd; ++timeIndex)
		{
			progress.SetProgress(timeIndex-chunkStart, chunkEnd-chunkStart);
			processAndWriteTimestep(timeIndex, chunkStart);
		}
	}
}

void Aartfaac2ms::processAndWriteTimestep(size_t timeIndex, size_t chunkStart)
{
	const size_t nAntennas = _file->NAntennas();
	const size_t nChannels = _file->NChannels();
	const size_t nBaselines = nAntennas*(nAntennas+1)/2;
	const Timestep &timestep = _timesteps[timeIndex - chunkStart];
	const double dateMJD = timestep.startTime;
	
	//Geometry::UVWTimestepInfo uvwInfo;
	//Geometry::PrepareTimestepUVW(uvwInfo, dateMJD, _mwaConfig.ArrayLongitudeRad(), _mwaConfig.ArrayLattitudeRad(), _mwaConfig.Header().raHrs, _mwaConfig.Header().decDegs);
	
	//TODO calculate actual UVW values
	_uvws.resize(_file->NAntennas(), UVW{0.0, 0.0, 0.0});
	
	/*double antU[antennaCount], antV[antennaCount], antW[antennaCount];
	for(size_t antenna=0; antenna!=antennaCount; ++antenna)
	{
		const double
			x = _mwaConfig.Antenna(antenna).position[0],
			y = _mwaConfig.Antenna(antenna).position[1],
			z = _mwaConfig.Antenna(antenna).position[2];
		Geometry::CalcUVW(uvwInfo, x, y, z, antU[antenna],antV[antenna], antW[antenna]);
	}*/
	
	_writer->AddRows(nBaselines);
	
	initializeWeights(_outputWeights.get(), timestep.endTime-timestep.startTime);
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
					
			size_t bufferIndex = timeIndex;
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
					*outDataPtr = std::complex<float>(*realPtr, *imagPtr);
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
				*outDataPtr = std::complex<float>(*realAPtr, *imagAPtr);
				*(outDataPtr+1) = std::complex<float>(*realBPtr, *imagBPtr);
				*(outDataPtr+2) = std::complex<float>(*realCPtr, *imagCPtr);
				*(outDataPtr+3) = std::complex<float>(*realDPtr, *imagDPtr);
				
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
				
			_writer->WriteRow(dateMJD*86400.0, dateMJD*86400.0, antenna1, antenna2, u, v, w, timestep.endTime-timestep.startTime, _outputData.get(), _outputFlags.data(), _outputWeights.get());
			++baselineIndex;
		}
	}
}

void Aartfaac2ms::initializeWeights(float* outputWeights, double integrationTime)
{
	// Weights are normalized so that a 'reasonable' res of 200 kHz, 1s has
	// weight of "1" per sample.
	// Note that this only holds for numbers in the WEIGHTS_SPECTRUM column; WEIGHTS will hold the sum.
	double weightFactor = integrationTime * (_file->Bandwidth()/_file->NChannels());
	for(size_t ch=0; ch!=_file->NChannels(); ++ch)
	{
		for(size_t p=0; p!=4; ++p)
			outputWeights[ch*4 + p] = weightFactor;
	}
}

void Aartfaac2ms::CalculateUVW(double date, size_t antenna1, size_t antenna2, double& u, double& v, double& w)
{
	// TODO
}
