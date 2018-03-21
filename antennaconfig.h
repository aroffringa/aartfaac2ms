#ifndef ANTENNA_CONFIG_H
#define ANTENNA_CONFIG_H

#include <algorithm> 
#include <cctype>
#include <fstream>
#include <locale>
#include <vector>
#include <sstream>
#include <map>
#include <numeric>
#include <string>

struct Position { double x, y, z; };

class AntennaConfig
{
public:
	struct Array
	{
		std::string name, band;
		std::vector<double> data;
	};
		
	AntennaConfig(const char* filename) : _str(filename)
	{
		next();
		struct Array a;
		while(ReadArray(a.name, a.band, a.data))
		{
			std::string key;
			if(a.band.empty())
				key = a.name;
			else
				key = a.band+"_"+a.name;
			std::cout << "Read values for " << key << '\n';
			_values.insert(std::make_pair(key, a));
		}
	}
	
	const std::vector<double>& GetArray(const std::string& name) const
	{
		return _values.find(name)->second.data;
	}
	
	std::vector<Position> GetLBAPositions() const
	{
		const std::vector<double>& arr = GetArray("LBA");
		std::vector<Position> pos;
		for(size_t index=0; index<arr.size(); index+=6)
		{
			pos.emplace_back(Position{arr[index], arr[index+1], arr[index+2]});
		}
		return pos;
	}
	
private:
	
	bool next()
	{
		if(_line.empty() || _linePos >= _line.size())
		{
			do {
				std::getline(_str, _line);
				if(!_str) {
					_token.clear();
					_line.clear();
					return false;
				}
				trim(_line);
			} while(_line.empty() || _line[0]=='#');
			_linePos = 0;
		}
		size_t pos = _line.find_first_of(" \t", _linePos);
		if(pos == std::string::npos)
		{
			_token = _line.substr(_linePos);
			_line.clear();
			_linePos = 0;
			return true;
		}
		else {
			_token = _line.substr(_linePos, pos-_linePos);
			_linePos = pos+1;
			while(_linePos<_line.size() && (_line[_linePos]==' ' || _line[_linePos]=='\t'))
				++_linePos;
			trim(_token);
			if(_token.empty())
				return next();
			else
				return true;
		}
	}
	
	std::vector<int> readDimensions()
	{
		std::vector<int> dimensions(1, std::atoi(_token.c_str()));
		do {
			if(!next())
				throw std::runtime_error("Antenna config file has bad format: expected dimensions");
			if(_token == "x")
			{
				if(!next())
					throw std::runtime_error("Antenna config file has bad format: expected another dimension after x");
				int dimValue = std::atoi(_token.c_str());
				dimensions.push_back(dimValue);
			}
			else if(_token != "[")
				throw std::runtime_error("Antenna config file has bad format");
		} while(_token != "[");
		return dimensions;
	}
	
	std::vector<double> readData(const std::vector<int>& dimensions)
	{
		int count = std::accumulate(dimensions.begin(), dimensions.end(), 1,
			[](int a, int b) { return a*b; });
		std::vector<double> values(count);
		for(int i=0; i!=count; ++i) {
			if(!next())
				throw std::runtime_error("Missing numbers");
			values[i] = std::atof(_token.c_str());
		}
		next(); // move TO ']'
		return values;
	}
	
	bool ReadArray(std::string& name, std::string& band, std::vector<double>& values)
	{
		values.clear();
		if(!std::isalpha(_token[0]))
			return false;
		name = _token;
		next();
		
		if(std::isalpha(_token[0])) {
			band = _token;
			next();
		}
		else {
			band.clear();
		}
		
		std::vector<int> dimensions1 = readDimensions();
		std::vector<double> data1 = readData(dimensions1);
		if(next() && _token[0]>='0' && _token[0]<='9')
		{
			std::vector<int> dimensions2 = readDimensions();
			std::vector<double> data2 = readData(dimensions2);
			next(); // skip ']'
			
			values = std::move(data2);
			for(size_t i=0; i!=values.size(); ++i)
				values[i] += data1[i%data1.size()];
		}
		else {
			values = std::move(data1);
		}
		return true;
	}
	
private:
	// trim from start (in place)
	static void ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
				return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	static void rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
				return !std::isspace(ch);
		}).base(), s.end());
	}

	// trim from both ends (in place)
	static void trim(std::string &s) {
		ltrim(s);
		rtrim(s);
	}
	
	std::map<std::string, Array> _values;
	
	std::ifstream _str;
	std::string _line;
	size_t _linePos;
	std::string _token;
};

#endif