#ifndef MIXER_PLUGIN_HH
#define MIXER_PLUGIN_HH

#include "plugin.hh"

class mixer_plugin:
	public plugin
{
public:
	mixer_plugin(unsigned int inputs);
	~mixer_plugin();

public:
	void run(unsigned int sample_count);

private:
	unsigned int _nr_inputs;
};

mixer_plugin::mixer_plugin(unsigned int inputs):
	_nr_inputs(inputs)
{
	_ports = new float*[1 + inputs];

	_ports[0] = new float[buffer_size];
	for (unsigned int i = 0; i < inputs; ++i)
		_ports[1 + i] = silence_buffer;
}

mixer_plugin::~mixer_plugin()
{
	delete[] _ports[0];
	for (unsigned int i = 0; i < _nr_inputs; ++i)
		delete[] _ports[1 + i];

	delete[] _ports;
}

void
mixer_plugin::run(unsigned int sample_count)
{
	float* out = _ports[0];

	for (unsigned int i = 0; i < sample_count; ++i) {
		float sum = 0;

		for (unsigned int j = 0; j < _nr_inputs; ++j)
			sum += _ports[1 + j][i];

		out[i] = sum;
	}
}

#endif
