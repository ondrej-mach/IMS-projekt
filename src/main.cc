#include <simlib.h>

#include <stdio.h>

const int p_krav = 100;

Store dojicky("Dojicky", 3);
Facility rampa("Nakladaci rampa");

int konvic = 0; // sdilena promenna - znamena pocet hotovych konvic s mlikem

Stat nalozeni("Jak dlouho ceka auto na nalozeni");

// cas je v minutach

// proces kravy
class Krava : public Process {
	void Behavior() {
	
	  // zivotni cyklus
	  while (1) {
		Wait(Exponential(15*60)); // 15 hod
		//Wait(Normal(15*60, 60));
		
		Enter(dojicky, 1); // bere dojicku
		
		// doba dojeni
		if (Random()<=0.1) Wait(Exponential(15));
		else Wait(Exponential(8));
		
		
		konvic++; // dalsi hotova konvice
		
		Leave(dojicky, 1); // uvolneni dojicky
	  }
	}
} ;


// proces auta nakladajiciho a odvazejiciho konvice
class Auto : public Process {

	void Behavior() {
	
	  while (1) {
		Seize(rampa); // postavi se na rampu
		double time=Time;
		
		// bere 20 konvic
		for (int a=0; a<20; a++) {
			_WaitUntil(konvic>0); // ceka na hotovou konvici
			konvic--;
			//if (konvic<0) Print("EEEEEEEEroro\n");
			Wait(Uniform(1,2)); // nalozi ji
		}
		
		Release(rampa); 
		
		nalozeni(Time-time); // doba nakladani
		
		Wait(60);
	  }
	}
} ;

int main()
{
	Init(0,200*60); // 200 hodin casovy ramec
	
	// vygenerovat 100 krav do systemu (zustavaji tam)
	for (int a=0; a<p_krav; a++)
		(new Krava)->Activate();
		
	// dve auta do systemu
	(new Auto)->Activate();
	(new Auto)->Activate();
		
	Run();
	
	rampa.Output();
	dojicky.Output();
	nalozeni.Output();
};
