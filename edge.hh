#ifndef EDGE_HH
#define EDGE_HH

extern "C" {
#include <assert.h>
}

class edge {
public:
	edge();
	~edge();

public:
	unsigned int inc();
	unsigned int dec();

public:
	unsigned int _count;
};

edge::edge():
	_count(0)
{
}

edge::~edge()
{
	assert(_count == 0);
}

unsigned int
edge::inc()
{
	++_count;

	assert(_count != 0);
	return _count;
}

unsigned int
edge::dec()
{
	assert(_count != 0);

	--_count;
	return _count;
}

#endif
