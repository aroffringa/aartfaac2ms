#include "aartfaacms.h"

#include <casacore/ms/MeasurementSets/MeasurementSet.h>

#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>

#include <casacore/measures/TableMeasures/TableMeasDesc.h>

#include <casacore/measures/Measures/MEpoch.h>

#include <stdexcept>
#include <sstream>

using namespace casacore;

struct AartfaacMSData
{
	AartfaacMSData(const std::string& filename) :
		_measurementSet(filename, Table::Update)
	{ }
	
	MeasurementSet _measurementSet;
};

const std::string AartfaacMS::_keywordNames[] = {
	/* Antenna */
	"AARTFAAC_COORDINATE_AXES",
	/* Main table */
	"AARTFAAC_AF2MS_VERSION", "AARTFAAC_AF2MS_VERSION_DATE"
};

const std::string AartfaacMS::_columnNames[] = {
	/* Observation */
	"AARTFAAC_ANTENNA_TYPE",
	"AARTFAAC_RCU_MODE",
	"AARTFAAC_FLAG_WINDOW_SIZE"
};

AartfaacMS::AartfaacMS(const string& filename) :
	_filename(filename),
	_data(new AartfaacMSData(filename))
{ }

AartfaacMS::~AartfaacMS()
{ }

void AartfaacMS::WriteKeywords(const std::string& af2msVersion, const std::string& af2msVersionDate, const std::array<double, 9>& coordinateAxes)
{
	_data->_measurementSet.rwKeywordSet().define(
		keywordName(AartfaacMSEnums::AARTFAAC_AF2MS_VERSION), af2msVersion);
	_data->_measurementSet.rwKeywordSet().define(
		keywordName(AartfaacMSEnums::AARTFAAC_AF2MS_VERSION_DATE), af2msVersionDate);
	
	casacore::Matrix<double> coordinateAxesVec(3, 3);
	double* ptr = coordinateAxesVec.data();
	for(size_t y=0; y!=3; ++y)
	{
		for(size_t x=0; x!=3; ++x)
		{
			// Perform transpose
			ptr[y+x*3] = coordinateAxes[x+y*3];
		}
	}
	_data->_measurementSet.antenna().rwKeywordSet().define(
		keywordName(AartfaacMSEnums::AARTFAAC_COORDINATE_AXES), coordinateAxesVec);
}

void AartfaacMS::addObservationFields()
{
	try {
		MSObservation obsTable = _data->_measurementSet.observation();
		
		ScalarColumnDesc<casacore::String> antennaTypeCD =
			ScalarColumnDesc<casacore::String>(columnName(AartfaacMSEnums::AARTFAAC_ANTENNA_TYPE));
		ScalarColumnDesc<int> rcuModeCD =
			ScalarColumnDesc<int>(columnName(AartfaacMSEnums::AARTFAAC_RCU_MODE));
		ScalarColumnDesc<int> flagWindowSizeCD =
			ScalarColumnDesc<int>(columnName(AartfaacMSEnums::AARTFAAC_FLAG_WINDOW_SIZE));
		
		obsTable.addColumn(flagWindowSizeCD);
		obsTable.addColumn(antennaTypeCD);
		obsTable.addColumn(rcuModeCD);
	} catch(std::exception& e) { }
}

void AartfaacMS::UpdateObservationInfo(std::string antennaType, int rcuMode, size_t flagWindowSize)
{
	MSObservation obsTable = _data->_measurementSet.observation();
	
	if(obsTable.nrow() != 1) {
		std::stringstream s;
		s << "The observation table of a AARTFAAC MS should have exactly one row, but in " << _filename << " it has " << obsTable.nrow() << " rows.";
		throw std::runtime_error(s.str());
	}
	
	ScalarColumn<casacore::String> antennaTypeCol =
		ScalarColumn<casacore::String>(obsTable, columnName(AartfaacMSEnums::AARTFAAC_ANTENNA_TYPE));
	ScalarColumn<int> rcuModeCol =
		ScalarColumn<int>(obsTable, columnName(AartfaacMSEnums::AARTFAAC_RCU_MODE));
	ScalarColumn<int> flagWindowSizeCol =
		ScalarColumn<int>(obsTable, columnName(AartfaacMSEnums::AARTFAAC_FLAG_WINDOW_SIZE));
		
	antennaTypeCol.put(0, antennaType);
	rcuModeCol.put(0, rcuMode);
	flagWindowSizeCol.put(0, flagWindowSize);
}


