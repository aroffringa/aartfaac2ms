#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <memory>

template<typename T>
class Optional
{
public:
	Optional() :
		_value(),
		_hasValue(false)
	{ }
	
	Optional(const T& value) :
		_value(value),
		_hasValue(true)
	{ }
	
	Optional(T&& value) :
		_value(std::move(value)),
		_hasValue(true)
	{ }
	
	Optional<T>& operator=(const T& value)
	{
		_value = value;
		_hasValue = true;
		return *this;
	}
	
	Optional<T>& operator=(T&& value)
	{
		_value = std::move(value);
		_hasValue = true;
		return *this;
	}
	
	T& Value() { return _value; }
	const T& Value() const { return _value; }
	
	T& operator*() { return _value; }
	const T& operator*() const { return _value; }
	T* operator->() { return &_value; }
	const T* operator->() const { return &_value; }
	
	T& ValueOr(T& alt) {
		if(_hasValue)
			return Value();
		else
			return alt;
	}
	
	const T& ValueOr(const T& alt) const {
		if(_hasValue)
			return Value();
		else
			return alt;
	}
	
	bool HasValue() const { return _hasValue; }
	
private:
	T _value;
	bool _hasValue;
};

#endif
