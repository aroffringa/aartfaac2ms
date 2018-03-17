#ifndef AARTFAAC2MS_H
#define AARTFAAC2MS_H

#include <memory>

class Aartfaac2ms
{
public:
	
private:
	void allocateBuffers();
	std::unique_ptr<class AartfaacFile> _file;
};

#endif

