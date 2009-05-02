a.out: $(wildcard *.cc) $(wildcard *.hh)
	g++ -Wall -g -pg main.cc -lasound
