#ifndef SIMPLE_SEQUENCER_HH
#define SIMPLE_SEQUENCER_HH

struct note {
	unsigned int tone;

	/* In 16th-notes */
	unsigned long duration;

	double frequency;
	double gate;

	/* In samples */
	unsigned long timestamp;
};

class simple_sequencer:
	public sequencer
{
public:
	simple_sequencer(unsigned int tempo,
		struct note notes[], unsigned int nr_notes,
		LADSPA_Data* output_frequency);
	~simple_sequencer();

public:
	unsigned int duration_remaining();
	void advance(unsigned int duration);

public:
	struct note* _notes;
	unsigned int _nr_notes;
	unsigned int _note_i;
	unsigned int _duration;

	LADSPA_Data* _output_frequency;
};

simple_sequencer::simple_sequencer(unsigned int tempo,
	struct note notes[], unsigned int nr_notes,
	LADSPA_Data* output_frequency):
	_notes(notes),
	_nr_notes(nr_notes),
	_note_i(0),
	_duration(0),
	_output_frequency(output_frequency)
{
	/* In 16th-notes */
	unsigned long timestamp = 0;

	for (unsigned int i = 0; i < nr_notes; ++i) {
		struct note *n = &notes[i];

		/* p = 69 + 12 * log2(f / 440) */
		/* p - 69 = 12 * log2(f / 440) */
		/* (p - 69) / 12 = log2(f / 440) */
		/* 2^((p - 69) / 12) = f / 440 */
		/* 440 * 2^((p - 69) / 12) = f */

		if (n->tone) {
			n->frequency = 440. * pow(2, (n->tone - 69.) / 12.);
			n->gate = 1;
		} else {
			n->frequency = 0;
			n->gate = 0;
		}

		n->timestamp = timestamp * (60. * sample_rate) / (8. * tempo);

		timestamp += n->duration;
	}

	_duration = _notes[_note_i + 1].timestamp
		- _notes[_note_i].timestamp;
	*_output_frequency = _notes[_note_i].frequency;
}

simple_sequencer::~simple_sequencer()
{
}

unsigned int
simple_sequencer::duration_remaining()
{
	return _duration;
}

void
simple_sequencer::advance(unsigned int duration)
{
	assert(duration <= _duration);
	assert(duration > 0);

	_duration -= duration;
	if (_duration == 0) {
		if (++_note_i == _nr_notes - 1) {
			static unsigned int loops = 3;

			if (--loops == 0)
				exit(0);

			printf("looping...\n");
			_note_i = 0;
		}

		_duration = _notes[_note_i + 1].timestamp
			- _notes[_note_i].timestamp;

		*_output_frequency = _notes[_note_i].frequency;
	}
}

#endif
