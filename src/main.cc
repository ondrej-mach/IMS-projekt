#include <simlib.h>

#include <stdio.h>


// Simulation parameters
const int SIM_START_TIME = 0; // minutes
const int SIM_END_TIME = 365*24*60; // minutes
const int SIM_TIME = SIM_END_TIME - SIM_START_TIME;

const float meterInterval = 2; // minute


// EV
const float EV_CAPACITY = 50; // kWh
const float EV_INITIAL_CHARGE = 25; // kWh
const float EV_LOW_LIMIT = 20; // kWh
const float EV_CHARGE_EFF = 0.85;
const float EV_DISCHARGE_EFF = 0.68;
const float EV_MAX_CHARGE_POWER = 6; // kW before the losses
const float EV_NORMAL_CHARGE_POWER = 1.5; // kW, activates when battery
const float EV_MAX_DISCHARGE_POWER = 6; // kW after the losses

// Household
// Stuff, that is on all the time
// this includes Network modems, switches, 
// network connected printers, TVs and monitors in standby mode etc.
const float CONST_POWER = 0.1; // kW

// Solar 
const float SOLAR_INSTALLED_POWER = 5; // kWp

// shared variables
float solarPower; // Power currently generated in kW
float loadPower = 1; // Power currently drawn in kW


// retruns number between 0 and 23
int getTimeOfDay() {
	return (int(Time)/60) % 24;
}

// returns number between 0 and 6
int getDayOfWeek() {
	return (int(Time)/60/24) % 7;
}

// returns number between 0 and 11
int getMonth() {
	// Kind of inaccurate but good enough for weather simulation
	return ((int(Time)/60/24) % 365) / 30;
}

// Solar power system
class Solar : public Process {
	void Behavior() {
		while (1) {
			int h = getTimeOfDay();
			if (h>8 and h<18) {
				solarPower = SOLAR_INSTALLED_POWER;
			} else {
				solarPower = 0;
			}
			Wait(5);
		}
	}
};

// This simulates the power load of the household
class Load : public Process {
	void Behavior() {
		while (1) {
			float currentPower = CONST_POWER;
			
			int h = getTimeOfDay();
			if (h>7 and h<23) {
				currentPower += 0.3;
			} else {
				currentPower += 0;
			}
			loadPower = currentPower;
			Wait(5);
		}
	}
};


enum EVMode{ V2G, V1G, DUMB, NONE };

class EV : public Process {
	double batteryEnergy = EV_INITIAL_CHARGE;
	EVMode mode = V2G;
	bool available = true;
	
	
	void Behavior() {
		while (1) {
			// available = false;
			int h = getTimeOfDay();
			if (h>7 and h<23) {
				currentPower += 0.3;
			} else {
				currentPower += 0;
			}
			Wait(5);
		}
	}
	
public:
	// energy in kwh, time in minutes
	float getEnergy(float energy, float time) {
		if (!available) {
			return 0;
		}
		
		float needed = energy / EV_DISCHARGE_EFF;
		
		if (mode == V2G) {
			if (batteryEnergy - needed > EV_LOW_LIMIT) {
				batteryEnergy -= needed;
				return energy;
			}
		}

		return 0;
	}
	
	// energy in kwh, time in minutes
	float chargeEnergy(float energy, float time) {
		if (!available) {
			return 0;
		}
		
		if ((mode == V1G) || (mode == V2G)) {
			float chargedEnergy;
			float maxEnergy = time/60 * EV_MAX_CHARGE_POWER;
			
			if (energy > EV_MAX_CHARGE_POWER) {
				chargedEnergy = time/60 * EV_MAX_CHARGE_POWER;
			} else {
				time/60 * EV_NORMAL_CHARGE_POWER;
			}
			
			if (batteryEnergy + energy < EV_CAPACITY) {
				batteryEnergy += energy * EV_CHARGE_EFF;
				return energy;
			}
			
			return 0;
		}
			
		if (mode == DUMB) {
			float chargedEnergy = time/60 * EV_NORMAL_CHARGE_POWER;
			if (batteryEnergy + chargedEnergy * EV_CHARGE_EFF < EV_CAPACITY) {
				batteryEnergy += chargedEnergy * EV_CHARGE_EFF;
				return chargedEnergy;
			}
		}

		// mode NONE never charges nor discharges
		return 0;
	}
};


static float loadPowerMemory[SIM_TIME];
static float solarPowerMemory[SIM_TIME];
static float netPowerMemory[SIM_TIME];
static float chargePowerMemory[SIM_TIME];
static float recoverPowerMemory[SIM_TIME];
static int measuredSamples = 0;

class Meter : public Process {
	EV *ev;
	
public:
	Meter(EV *vehicle) {
		ev = vehicle;
    }
	
private:
	void Behavior() {
		while (measuredSamples < SIM_TIME) {
			loadPowerMemory[measuredSamples] = loadPower;
			solarPowerMemory[measuredSamples] = solarPower;
			
			
			float netEnergy = (solarPower - loadPower)*meterInterval/60;
			
			float preferredCharge = (netEnergy > 0) ? netEnergy : 0;
			float chargedEnergy = ev->chargeEnergy(preferredCharge, meterInterval);
			netEnergy -= chargedEnergy;
			chargePowerMemory[measuredSamples] = chargedEnergy*60/meterInterval;
			
			float preferredDischarge = (netEnergy < 0) ? -netEnergy : 0;
			float recoveredEnergy = ev->getEnergy(preferredDischarge, meterInterval);
			netEnergy += recoveredEnergy;
			recoverPowerMemory[measuredSamples] = recoveredEnergy*60/meterInterval;
			
			
			netPowerMemory[measuredSamples] = netEnergy*60/meterInterval;
			measuredSamples++;
			Wait(meterInterval);
		}
	}
};

void saveData() {
	FILE *f = fopen("out.csv", "w");
	if (f == NULL) {
		return;
	}
	fprintf(f, "loadPower,solarPower,chargePower,recoverPower,netPower\n");
	
	for (int i=0; i<10000; i++) {
		fprintf(f, "%lf,%lf,%lf,%lf,%lf\n", loadPowerMemory[i], solarPowerMemory[i], chargePowerMemory[i], recoverPowerMemory[i], netPowerMemory[i]);
	}
	
	fclose(f);
}

void printMetrics() {
	double netEnergyDrawn = 0;
	double netEnergySupplied = 0;
	double energyGenerated = 0;
	double energyConsumed = 0;
	double energyCharged = 0;
	double energyRecovered = 0;
	
	for (int i=0; i<measuredSamples; i++) {
		
		energyGenerated += solarPowerMemory[i] * meterInterval/60;
		energyConsumed += loadPowerMemory[i] * meterInterval/60;
		energyCharged += chargePowerMemory[i] * meterInterval/60;
		energyRecovered += recoverPowerMemory[i] * meterInterval/60;
		
		float ne = netPowerMemory[i] * meterInterval/60;
		
		if (ne < 0) {
			netEnergyDrawn -= ne;
		} else {
			netEnergySupplied += ne;
		}
	}
	
	printf("Energy drawn from network: %lf MWh\n", netEnergyDrawn/1000);
	printf("Energy supplied to network: %lf MWh\n", netEnergySupplied/1000);
	printf("Energy generated: %lf MWh\n", energyGenerated/1000);
	printf("Energy consumed: %lf MWh\n", energyConsumed/1000);
	printf("Energy transferred into EV: %lf MWh\n", energyCharged/1000);
	printf("Energy recovered from EV: %lf MWh\n", energyRecovered/1000);
}


int main() {
	Init(SIM_START_TIME, SIM_END_TIME); // Time in minutes
	
	(new Load)->Activate();
	(new Solar)->Activate();
	EV *ev = new EV;
	Meter *meter = new Meter(ev);
	meter->Activate();
		
	Run();
	
	saveData();
	printMetrics();
	
	return 0;
};
