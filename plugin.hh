#ifndef PLUGIN_HH
#define PLUGIN_HH

#include <map>
#include <set>

extern "C" {
#include <dlfcn.h>
#include <ladspa.h>
}

#include "edge.hh"
#include "sequencer.hh"

class plugin {
public:
	typedef std::map<plugin*, edge*> plugin_map;
	typedef std::set<sequencer*> sequencer_set;

public:
	plugin(const char* path, const char* label);
	~plugin();

public:
	void activate();
	void deactivate();

	void connect(unsigned int port, LADSPA_Data* buffer);
	void disconnect(unsigned int port);

	void run(unsigned int sample_count);

public:
	void* _dl;
	const LADSPA_Descriptor* _descriptor;
	LADSPA_Handle _handle;

	LADSPA_Data** _ports;

	plugin_map _deps;
	plugin_map _rev_deps;

	sequencer_set _seqs;
};

plugin::plugin(const char* path, const char* label)
{
	_dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!_dl)
		exit(1);

	void* sym = dlsym(_dl, "ladspa_descriptor");
	if (!sym)
		exit(1);

	LADSPA_Descriptor_Function df = (LADSPA_Descriptor_Function) sym;

	/* Look for the right plugin */
	for (unsigned int i = 0; ; ++i) {
		const LADSPA_Descriptor *d = df(i);
		if (!d)
			break;

		if (!strcmp(d->Label, label)) {
			_descriptor = d;
			break;
		}
	}

	if (!_descriptor)
		exit(1);

	_handle = _descriptor->instantiate(_descriptor, sample_rate);
	if (!_handle)
		exit(1);

	_ports = new LADSPA_Data*[_descriptor->PortCount];

	for (unsigned int i = 0; i < _descriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor port
			= _descriptor->PortDescriptors[i];
		const LADSPA_PortRangeHint* hint
			= &_descriptor->PortRangeHints[i];

		if (port & LADSPA_PORT_CONTROL) {
			_ports[i] = new LADSPA_Data[1];
			_ports[i][0] = 0.5 * hint->LowerBound
					+ 0.5 * hint->UpperBound;
		}

		if (port & LADSPA_PORT_AUDIO) {
			if (port & LADSPA_PORT_INPUT)
				_ports[i] = silence_buffer;
			if (port & LADSPA_PORT_OUTPUT)
				_ports[i] = new LADSPA_Data[buffer_size];
		}

		_descriptor->connect_port(_handle, i, _ports[i]);
	}
}

plugin::~plugin()
{
	_descriptor->cleanup(_handle);

	for (unsigned int i = 0; i < _descriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor port
			= _descriptor->PortDescriptors[i];

		if (!(port & LADSPA_PORT_AUDIO))
			continue;
		if (!(port & LADSPA_PORT_OUTPUT))
			continue;

		delete[] _ports[i];
	}

	dlclose(_dl);
}

void
plugin::activate()
{
	_descriptor->activate(_handle);
}

void
plugin::deactivate()
{
	if (_descriptor->deactivate)
		_descriptor->deactivate(_handle);
}

void
plugin::connect(unsigned int port_nr, LADSPA_Data* buffer)
{
	const LADSPA_PortDescriptor port
		= _descriptor->PortDescriptors[port_nr];

	assert(port & LADSPA_PORT_AUDIO);
	assert(port & LADSPA_PORT_INPUT);

	_ports[port_nr] = buffer;
	_descriptor->connect_port(_handle, port_nr, buffer);
}

void
plugin::disconnect(unsigned int port_nr)
{
	const LADSPA_PortDescriptor port
		= _descriptor->PortDescriptors[port_nr];

	assert(port & LADSPA_PORT_AUDIO);
	assert(port & LADSPA_PORT_INPUT);

	_ports[port_nr] = silence_buffer;
	_descriptor->connect_port(_handle, port_nr, silence_buffer);
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

	unsigned int sample_offset = 0;
	while (sample_count) {
		unsigned int n = sample_count;

		for (sequencer_set::iterator i = _seqs.begin(),
			end = _seqs.end(); i != end; ++i)
		{
			sequencer* seq = *i;
			unsigned int d = seq->duration_remaining();
			if (d < n)
				n = d;
		}

		/* Update buffers */
		for (unsigned int i = 0; i < _descriptor->PortCount; ++i) {
			const LADSPA_PortDescriptor port
				= _descriptor->PortDescriptors[i];

			if (!(port & LADSPA_PORT_AUDIO))
				continue;

			_descriptor->connect_port(_handle, i, _ports[i] + sample_offset);
		}

		_descriptor->run(_handle, n);

		for (sequencer_set::iterator i = _seqs.begin(),
			end = _seqs.end(); i != end; ++i)
		{
			sequencer* seq = *i;
			seq->advance(n);
		}

		sample_count -= n;
		sample_offset += n;
	}

	/* Reset buffers */
	for (unsigned int i = 0; i < _descriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor port
			= _descriptor->PortDescriptors[i];

		if (!(port & LADSPA_PORT_AUDIO))
			continue;

		_descriptor->connect_port(_handle, i, _ports[i]);
	}
}

#endif
