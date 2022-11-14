all: sim


sim:
	g++ -o sim -I/usr/local/include src/main.cc -L/usr/local/lib -lsimlib -lm
	
run: sim
	LD_LIBRARY_PATH=/usr/local/lib/ ./sim
