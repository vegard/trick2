#ifndef WAV_OUTPUT_PLUGIN_HH
#define WAV_OUTPUT_PLUGIN_HH

extern "C" {
#include <stdio.h>
#include <sndfile.h>
}

#include "plugin.hh"

class wav_output_plugin:
	public plugin
{
public:
	wav_output_plugin(const char* filename);
	~wav_output_plugin();

public:
	void activate();
	void deactivate();

	void connect(unsigned int port, float* buffer);
	void disconnect(unsigned int port);

	void run(unsigned int sample_count);

private:
	SNDFILE* _file;
	float* _output_buffer;
};

wav_output_plugin::wav_output_plugin(const char* filename)
{
	_ports = new float*[2];

	SF_INFO info;
	info.samplerate = sample_rate;
	info.channels = 2;
	info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	_file = sf_open(filename, SFM_WRITE, &info);
	if (!_file)
		exit(1);

	_output_buffer = new float[2 * buffer_size];
}

wav_output_plugin::~wav_output_plugin()
{
	delete[] _output_buffer;
	sf_close(_file);
}

void
wav_output_plugin::activate()
{
}

void
wav_output_plugin::deactivate()
{
}

void
wav_output_plugin::connect(unsigned int port, float* buffer)
{
	assert(port < 2);

	plugin::connect(port, buffer);
}

void
wav_output_plugin::disconnect(unsigned int port)
{
	assert(port < 2);

	plugin::disconnect(port);
}

void
wav_output_plugin::run(unsigned int n)
{
	for (unsigned int i = 0; i < n; ++i) {
		_output_buffer[2 * i + 0] = _ports[0][i];
		_output_buffer[2 * i + 1] = _ports[1][i];
	}

	unsigned int i = 0;
	while (n > 0) {
		sf_count_t c = sf_writef_float(_file, _output_buffer + i, n);
		if (c < 0) {
			exit(1);
		}

		assert(c <= n);

		n -= c;
		i += c;
	}
}

#endif
