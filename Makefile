all: a.out

KV331_3_RondoAllaTurca.mid:
	wget http://www.mutopiaproject.org/ftp/MozartWA/KV331/KV331_3_RondoAllaTurca/KV331_3_RondoAllaTurca.mid

a.out: $(wildcard *.cc) $(wildcard *.hh) KV331_3_RondoAllaTurca.mid
	g++ -Wall -g -pg main.cc -lasound -lsndfile
