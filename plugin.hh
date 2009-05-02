#ifndef PLUGIN_HH
#define PLUGIN_HH

#include <map>
#include <set>

#include "edge.hh"

class sequencer;

class plugin {
public:
	typedef std::map<plugin*, edge*> plugin_map;
	typedef std::set<sequencer*> sequencer_set;

public:
	plugin();
	virtual ~plugin();

public:
	virtual void activate() = 0;
	virtual void deactivate() = 0;

	virtual void connect(unsigned int port, float* buffer);
	virtual void disconnect(unsigned int port);

	virtual void run(unsigned int sample_count) = 0;

public:
	float** _ports;

	plugin_map _deps;
	plugin_map _rev_deps;

	sequencer_set _seqs;
};

plugin::plugin()
{
}

plugin::~plugin()
{
}

void
plugin::connect(unsigned int port, float* buffer)
{
	_ports[port] = buffer;
}

void
plugin::disconnect(unsigned int port)
{
	_ports[port] = silence_buffer;
}

void
plugin::run(unsigned int sample_count)
{
	for (plugin_map::iterator i = _deps.begin(), end = _deps.end();
		i != end; ++i)
	{
		plugin* p = i->first;
		p->run(sample_count);
	}
}

#endif
