#ifndef TIME_RANGE_H
#define TIME_RANGE_H

class TimeRange
{
public:
	TimeRange(double start, double end) : _start(start), _end(end)
	{ }
	
	bool Contains(double timePos) const
	{
		if(_start <= _end) // no wrap
		{
			return _start <= timePos && timePos < _end;
		}
		else {
			return _start <= timePos || timePos < _end;
		}
	}
	
private:
	double _start, _end;
};

#endif
