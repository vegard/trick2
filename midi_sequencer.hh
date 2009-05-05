#ifndef MIDI_SEQUENCER_HH
#define MIDI_SEQUENCER_HH

#include <set>
#include <vector>

extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

class midi_event {
public:
	midi_event(unsigned int track, unsigned long timestamp,
		uint8_t command, uint8_t channel,
		uint8_t note, uint8_t velocity);
	~midi_event();

public:
	unsigned int _track;
	unsigned long _timestamp;
	uint8_t _command;
	uint8_t _channel;
	uint8_t _note;
	uint8_t _velocity;
};

midi_event::midi_event(unsigned int track, unsigned long timestamp,
	uint8_t command, uint8_t channel,
	uint8_t note, uint8_t velocity):
	_track(track),
	_timestamp(timestamp),
	_command(command),
	_channel(channel),
	_note(note),
	_velocity(velocity)
{
}

midi_event::~midi_event()
{
}

class midi_voice
{
public:
	typedef std::vector<midi_event*> event_vector;

public:
	midi_voice();
	~midi_voice();

public:
	void connect_gate(float* input_port);
	void connect_frequency(float* input_port);

	unsigned int duration_remaining();
	void advance(unsigned int duration);

public:
	event_vector _events;
	unsigned int _event_i;
	unsigned int _duration;

	float* _gate;
	float* _frequency;
};

midi_voice::midi_voice():
	_event_i(0),
	_duration(0)
{
}

midi_voice::~midi_voice()
{
	for (event_vector::iterator i = _events.begin(), end = _events.end();
		i != end; ++i)
	{
		midi_event* e = *i;
		delete e;
	}
}

void
midi_voice::connect_gate(float* input_port)
{
	_gate = input_port;
}

void
midi_voice::connect_frequency(float* input_port)
{
	_frequency = input_port;
}

unsigned int
midi_voice::duration_remaining()
{
	return _duration;
}

#define TIMESTAMP_SCALE 100

void
midi_voice::advance(unsigned int duration)
{
	if (_event_i == _events.size())
		return;

	assert(duration <= _duration);
	//assert(duration > 0);

	_duration -= duration;
	while (_duration == 0) {
		midi_event* e = _events[_event_i];
		switch (e->_command) {
		case 0x80:
			*_gate = 0;
			break;
		case 0x90:
			if (e->_velocity == 0) {
				*_gate = 0;
			} else {
				*_gate = 1;
				*_frequency = 440.
					* pow(2, (e->_note - 69.) / 12.);
			}
			break;
		}

		if (_event_i == _events.size() - 1) {
			_duration = buffer_size;
			break;
		}

		_duration = TIMESTAMP_SCALE * (_events[_event_i + 1]->_timestamp
			- _events[_event_i]->_timestamp);


		++_event_i;
	}
}

class midi_sequencer:
	public sequencer
{
public:
	typedef std::vector<midi_voice*> voice_vector;
	typedef std::map<unsigned int, float*> port_map;

public:
	explicit midi_sequencer(const char* filename);
	~midi_sequencer();

public:
	void connect_gate(unsigned int output_port, float* input_port);
	void connect_frequency(unsigned int output_port, float* input_port);

	unsigned int duration_remaining(unsigned int voice);
	void advance(unsigned int voice, unsigned int duration);

public:
	voice_vector _voices;
};

static uint8_t
read_u8(uint8_t*& m)
{
	return *(m++);
}

static uint8_t
peek_u8(const uint8_t* m)
{
	return *m;
}

static uint16_t
read_u16(uint8_t*& m)
{
	uint16_t r = (m[0] << 8) | m[1];
	m += 2;
	return r;
}

static uint32_t
read_u32(uint8_t*& m)
{
	uint32_t r = (m[0] << 24) | (m[1] << 16) | (m[2] << 8) | m[3];
	m += 4;
	return r;
}

/* LEI = Length-Encoded Integral */
static uint32_t
read_lei(uint8_t*& m)
{
	uint32_t r = 0;

	while (*m & 0x80)
		r = (r << 7) | (*(m++) & ~0x80);

	return (r << 7) | *(m++);
}

static uint32_t
peek_lei(const uint8_t* m)
{
	uint32_t r = 0;

	while (*m & 0x80)
		r = (r << 7) | (*(m++) & ~0x80);

	return (r << 7) | *(m++);
}

/* XXX: This is _not_ safe against corrupt MIDI files. */
midi_sequencer::midi_sequencer(const char* filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
		exit(1);

	struct stat st;
	if (fstat(fd, &st) == -1)
		exit(1);

	void* mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mem == MAP_FAILED)
		exit(1);

	if (close(fd) == -1)
		assert(0);

	uint8_t* bytes = (uint8_t*) mem;

	struct {
		uint32_t length;
		uint16_t format;
		uint16_t tracks;
		uint16_t deltas;
	} midi;

	uint32_t mthd = read_u32(bytes);
	if (mthd != 0x4d546864)
		exit(1);

	midi.length = read_u32(bytes);
	if (midi.length != 6)
		exit(1);

	midi.format = read_u16(bytes);
	if (midi.format != 0 && midi.format != 1 && midi.format != 2) {
		printf("midi format wrong: %d\n", midi.format);
		exit(1);
	}

	midi.tracks = read_u16(bytes);
	midi.deltas = read_u16(bytes);

	printf("%s: format=%d, tracks=%d, deltas=%d\n",
		filename, midi.format, midi.tracks, midi.deltas);

	if (midi.tracks < 1)
		exit(1);

	struct track {
		uint32_t length;
		uint8_t* mem;
		uint8_t* bytes;

		bool done;

		uint8_t prev_command;
		unsigned long timestamp;
	};

	track* tracks = new track[midi.tracks];
	for (unsigned int i = 0; i < midi.tracks; ++i) {
		track* t = &tracks[i];

		uint32_t mtrk = read_u32(bytes);
		if (mtrk != 0x4d54726b) {
			printf("track header for track %d wrong: %08x\n",
				i, mtrk);
			exit(1);
		}

		t->length = read_u32(bytes);
		t->mem = bytes;
		t->bytes = bytes;
		bytes += t->length;

		t->done = false;

		t->prev_command = 0;
		t->timestamp = 0;
	}

	std::set<unsigned int> voices;
	unsigned int voices_playing[16][128];

	bool voices_is_playing[16][128];
	for (unsigned int i = 0; i < 16; ++i)
		for (unsigned int j = 0; j < 128; ++j)
			voices_is_playing[i][j] = false;

	while (true) {
		for (unsigned int i = 0; i < midi.tracks; ++i) {
			track* t = &tracks[i];

			if (t->done)
				continue;

			if (t->bytes == t->mem + t->length)
				t->done = true;
		}

		unsigned int smallest_track = midi.tracks;
		uint32_t smallest_delay;
		for (unsigned int i = 0; i < midi.tracks; ++i) {
			track* t = &tracks[i];

			if (t->done)
				continue;

			smallest_track = i;
			smallest_delay = t->timestamp + peek_lei(t->bytes);
			break;
		}

		/* All tracks finished, let's get out of here! */
		if (smallest_track == midi.tracks)
			break;

		for (unsigned int i = smallest_track + 1; i < midi.tracks; ++i) {
			track* t = &tracks[i];

			if (t->done)
				continue;

			uint32_t delay = t->timestamp + peek_lei(t->bytes);
			if (delay < smallest_delay) {
				smallest_delay = delay;
				smallest_track = i;
			}
		}

		track* t = &tracks[smallest_track];
		uint32_t delay = read_lei(t->bytes);

		/* XXX: Compute the timestamp correctly. */
		t->timestamp += delay;

		uint8_t command = peek_u8(t->bytes);
		if (!(command & 0x80))
			command = t->prev_command;
		else
			command = read_u8(t->bytes);

		if (command == 0xff) {
			read_u8(t->bytes);

			uint32_t skip = read_lei(t->bytes);
			t->bytes += skip;
		} else if (command == 0x2f) {
			t->done = true;
		} else {
			switch (command & 0xf0) {
			case 0x80: {
				uint8_t channel = command & 0x0f;
				uint8_t note = read_u8(t->bytes);
				uint8_t velocity = read_u8(t->bytes);
#if 0
printf("NOTEOFF %d %d %d\n", smallest_track, channel, note);
#endif

				if (!voices_is_playing[channel][note]) {
					printf("warning: note wasn't already playing\n");
				} else {
					unsigned int voice_nr = voices_playing[channel][note];
					midi_voice* v = _voices[voice_nr];

					voices.insert(voice_nr);

					voices_is_playing[channel][note] = false;

					v->_events.push_back(new midi_event(smallest_track,
						t->timestamp, command & 0xf0, channel,
						note, velocity));
				}
				break;
			}
			case 0x90: {
				uint8_t channel = command & 0x0f;
				uint8_t note = read_u8(t->bytes);
				uint8_t velocity = read_u8(t->bytes);

				if (velocity == 0) {
#if 0
printf("NOTEOFF %d %d %d\n", smallest_track, channel, note);
#endif

					if (!voices_is_playing[channel][note]) {
						printf("warning: note wasn't already playing\n");
					} else {
						unsigned int voice_nr = voices_playing[channel][note];
						midi_voice* v = _voices[voice_nr];

						voices_is_playing[channel][note] = false;

						voices.insert(voice_nr);
						v->_events.push_back(new midi_event(smallest_track,
							t->timestamp, command & 0xf0, channel,
							note, velocity));
					}
					break;
				}
#if 0
printf("NOTEON %d %d %d\n", smallest_track, channel, note);
#endif

				/* Find free voice */
				unsigned int voice_nr;
				midi_voice* v;

				if (voices_is_playing[channel][note]) {
					voice_nr = voices_playing[channel][note];
					v = _voices[voice_nr];
					printf("warning: note was already playing\n");
				} else if (voices.size() == 0) {
					voice_nr = _voices.size();
					v = new midi_voice();
					_voices.push_back(v);
				} else {
					std::set<unsigned int>::iterator i
						= voices.begin();
					voice_nr = *i;
					v = _voices[voice_nr];
					voices.erase(i);
				}

				voices_is_playing[channel][note] = true;
				voices_playing[channel][note] = voice_nr;

				v->_events.push_back(new midi_event(smallest_track,
					t->timestamp, command & 0xf0, channel,
					note, velocity));
				break;
			}
			case 0xa0:
			case 0xb0:
			case 0xe0:
				read_u8(t->bytes);
				read_u8(t->bytes);
				break;
			case 0xc0:
			case 0xd0:
				read_u8(t->bytes);
				break;
			case 0x20:
				break;
#if 1
			default:
				printf("unhandled midi command: %02x\n",
					command);
#endif
			}
		}

		t->prev_command = command;
	}

	printf("%d voices:\n", _voices.size());
	for (unsigned int i = 0; i < _voices.size(); ++i)
		printf(" * %d notes: %d\n", i, _voices[i]->_events.size());

	for (voice_vector::iterator i = _voices.begin(), end = _voices.end();
		i != end; ++i)
	{
		midi_voice* v = *i;

		v->_duration = TIMESTAMP_SCALE * v->_events[0]->_timestamp;
	}

	delete[] tracks;

	if (munmap(mem, st.st_size) < 0)
		assert(1);
}

midi_sequencer::~midi_sequencer()
{
	for (voice_vector::iterator i = _voices.begin(), end = _voices.end();
		i != end; ++i)
	{
		midi_voice* v = *i;
		delete v;
	}
}

void
midi_sequencer::connect_gate(unsigned int output_port, float* input_port)
{
	assert(output_port < _voices.size());

	_voices[output_port]->connect_gate(input_port);
}

void
midi_sequencer::connect_frequency(unsigned int output_port, float* input_port)
{
	assert(output_port < _voices.size());

	_voices[output_port]->connect_frequency(input_port);
}

unsigned int
midi_sequencer::duration_remaining(unsigned int voice)
{
	assert(voice < _voices.size());

	return _voices[voice]->duration_remaining();
}

void
midi_sequencer::advance(unsigned int voice, unsigned int duration)
{
	assert(voice < _voices.size());

	_voices[voice]->advance(duration);
}

#endif
