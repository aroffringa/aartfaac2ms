#ifndef AARTFAAC_MS_H
#define AARTFAAC_MS_H

#include <memory>
#include <string>

struct AartfaacMSEnums
{
	enum Keywords
	{
		/* Antenna */
		AARTFAAC_COORDINATE_AXES,
		/* Main table */
		AARTFAAC_AF2MS_VERSION, AARTFAAC_AF2MS_VERSION_DATE
	};
	enum Columns
	{
		/* Observation */
		AARTFAAC_ANTENNA_TYPE,
		AARTFAAC_RCU_MODE,
		AARTFAAC_FLAG_WINDOW_SIZE
	};
};

class AartfaacMS
{
	public:
		AartfaacMS(const std::string& filename);
		
		~AartfaacMS();
		
		void InitializeFields()
		{
			addObservationFields();
		}
		
		void UpdateObservationInfo(std::string antennaType, int rcuMode, size_t flagWindowSize);
		
		void WriteKeywords(const std::string& af2msVersion, const std::string& af2msVersionDate, const std::array<double, 9>& coordinateAxes);
		
	private:
		void addObservationFields();
		
		const std::string &columnName(enum AartfaacMSEnums::Columns column)
		{
			return _columnNames[(int) column];
		}
		
		const std::string &keywordName(enum AartfaacMSEnums::Keywords keyword)
		{
			return _keywordNames[(int) keyword];
		}
		
		static const std::string _columnNames[], _keywordNames[];
		
		const std::string _filename;
		std::unique_ptr<struct AartfaacMSData> _data;
};

#endif
