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
	"AARTFAAC_COORDINATE_AXES"
	/* Main table */
	"AARTFAAC_AF2MS_VERSION", "AARTFAAC_AF2MS_VERSION_DATE"
};

const std::string AartfaacMS::_columnNames[] = {
	/* Observation */
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
	for(size_t i=0; i!=9; ++i)
		ptr[i] = coordinateAxes[i];
	_data->_measurementSet.rwKeywordSet().define(
		keywordName(AartfaacMSEnums::AARTFAAC_COORDINATE_AXES), coordinateAxesVec);
}

void AartfaacMS::addObservationFields()
{
	try {
		MSObservation obsTable = _data->_measurementSet.observation();
		
		ScalarColumnDesc<int> flagWindowSizeCD =
			ScalarColumnDesc<int>(columnName(AartfaacMSEnums::AARTFAAC_FLAG_WINDOW_SIZE));
		
		obsTable.addColumn(flagWindowSizeCD);
	} catch(std::exception& e) { }
}

