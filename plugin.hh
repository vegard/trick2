#ifndef PLUGIN_HH
#define PLUGIN_HH

#include <map>

#include "edge.hh"

class sequencer;

class plugin {
public:
	typedef std::map<plugin*, edge*> plugin_map;
	typedef std::map<sequencer*, unsigned int> sequencer_map;

public:
	plugin();
	virtual ~plugin();

public:
	virtual void activate();
	virtual void deactivate();

	virtual void connect(unsigned int port, float* buffer);
	virtual void disconnect(unsigned int port);

	virtual void run(unsigned int sample_count) = 0;

public:
	float** _ports;

	plugin_map _deps;
	plugin_map _rev_deps;

	sequencer_map _seqs;
};

plugin::plugin()
{
}

plugin::~plugin()
{
}

void
plugin::activate()
{
}

void
plugin::deactivate()
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

#endif
