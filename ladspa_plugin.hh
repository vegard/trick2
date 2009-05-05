#ifndef LADSPA_PLUGIN_HH
#define LADSPA_PLUGIN_HH

extern "C" {
#include <dlfcn.h>
#include <ladspa.h>
}

#include "plugin.hh"
#include "sequencer.hh"

class ladspa_plugin:
	public plugin
{
public:
	ladspa_plugin(const char* path, const char* label);
	~ladspa_plugin();

public:
	void activate();
	void deactivate();

	void connect(unsigned int port, float* buffer);
	void disconnect(unsigned int port);

	void run(unsigned int sample_count);

public:
	void* _dl;
	const LADSPA_Descriptor* _descriptor;
	LADSPA_Handle _handle;
};

ladspa_plugin::ladspa_plugin(const char* path, const char* label)
{
	_dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!_dl)
		exit(1);

	void* sym = dlsym(_dl, "ladspa_descriptor");
	if (!sym)
		exit(1);

	LADSPA_Descriptor_Function df = (LADSPA_Descriptor_Function) sym;

	/* Look for the right ladspa_plugin */
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

ladspa_plugin::~ladspa_plugin()
{
	/* Delete audio buffers */
	for (unsigned int i = 0; i < _descriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor port
			= _descriptor->PortDescriptors[i];

		if (port & LADSPA_PORT_CONTROL) {
			delete[] _ports[i];
			continue;
		}

		if (port & LADSPA_PORT_AUDIO) {
			if (port & LADSPA_PORT_OUTPUT)
				delete[] _ports[i];
			continue;
		}
	}

	delete[] _ports;

	_descriptor->cleanup(_handle);

	dlclose(_dl);
}

void
ladspa_plugin::activate()
{
	_descriptor->activate(_handle);
}

void
ladspa_plugin::deactivate()
{
	if (_descriptor->deactivate)
		_descriptor->deactivate(_handle);
}

void
ladspa_plugin::connect(unsigned int port_nr, LADSPA_Data* buffer)
{
	const LADSPA_PortDescriptor port
		= _descriptor->PortDescriptors[port_nr];

	assert(port & LADSPA_PORT_AUDIO);
	assert(port & LADSPA_PORT_INPUT);

	plugin::connect(port_nr, buffer);
	_descriptor->connect_port(_handle, port_nr, _ports[port_nr]);
}

void
ladspa_plugin::disconnect(unsigned int port_nr)
{
	const LADSPA_PortDescriptor port
		= _descriptor->PortDescriptors[port_nr];

	assert(port & LADSPA_PORT_AUDIO);
	assert(port & LADSPA_PORT_INPUT);

	plugin::disconnect(port_nr);
	_descriptor->connect_port(_handle, port_nr, _ports[port_nr]);
}

void
ladspa_plugin::run(unsigned int sample_count)
{
	unsigned int sample_offset = 0;
	while (sample_count) {
		unsigned int n = sample_count;

		for (sequencer_map::iterator i = _seqs.begin(),
			end = _seqs.end(); i != end; ++i)
		{
			sequencer* seq = i->first;
			unsigned int voice = i->second;

			unsigned int d = seq->duration_remaining(voice);
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

		for (sequencer_map::iterator i = _seqs.begin(),
			end = _seqs.end(); i != end; ++i)
		{
			sequencer* seq = i->first;
			unsigned int voice = i->second;

			seq->advance(voice, n);
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
