#include <simlib.h>

#include <stdio.h>
#include <math.h>

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
				solarPower = 0.3 * SOLAR_INSTALLED_POWER;
			} else {
				solarPower = 0;
			}
			Wait(5);
		}
	}
};


const float APPLIANCE_TICK_TIME = 1;

class Appliance : public Process {
public:
	// probability is array of 24 values (for each hour in day)
	// avgTime is mean time of using this appliance
	// power is in kW
	Appliance(float *probability, float avgTime, float power) {
		this->probability = probability;
		this->avgTime = avgTime;
		this->power = power;
    }
    
    float readPower() {
		return active ? power : 0;
	}
	
private:
	bool active = false;
	float power;
	float *probability;
	float avgTime;
	
	void Behavior() {
		float integrator = 0;
		
		while (1) {
			// read probability of being active at this time
			float currentProb = probability[getTimeOfDay()];
			// Add to integrator
			integrator += currentProb * APPLIANCE_TICK_TIME;
			if (active) {
				// if it is currently active, subtract from integrator
				integrator -= APPLIANCE_TICK_TIME;
			}
			
			// There could be added some random element
			if (active) {
				// turn off
				if (integrator < -avgTime/2) {
					active = false;
				}
			} else {
				// turn on
				if (integrator > avgTime/2) {
					active = true;
			    }
			}
			
			Wait(APPLIANCE_TICK_TIME);
		}
	}
};


float tvProbability[24] = {
	0.10, 0.05, 0.02, 0.01, 0.02, 0.02,
	0.03, 0.07, 0.08, 0.08, 0.07, 0.07,
	0.08, 0.09, 0.10, 0.12, 0.13, 0.15,
	0.19, 0.30, 0.37, 0.45, 0.42, 0.28,
};
float tvTime = 120;
float tvPower = 0.120;



// This simulates the power load of the household
class Load : public Process {
	void Behavior() {
		
		Appliance *appliances[] = {
			new Appliance(tvProbability, tvTime, tvPower),

		};
		
		for (Appliance *a : appliances) {
			a->Activate();
		}
		
		while (1) {
			float currentPower = CONST_POWER;
			
			for (Appliance *a : appliances) {
				currentPower += a->readPower();
			}
			loadPower = currentPower;
			Wait(1);
		}
	}
};


enum EVMode{ V2G, V1G, DUMB, NONE };
static double energyUsedDriving;

class EV : public Process {
	double batteryEnergy = EV_INITIAL_CHARGE;
	EVMode mode = V2G;
	bool available = true;
	
	
	void Behavior() {
		while (1) {
			int h = getTimeOfDay();
			int d = getDayOfWeek();
			
			// Go to work on workdays after 7 am
			if (h==7 && d<5) {
				available = false;
				Wait(8*60);
				// energy lost by travelling
				// 5 kwh equates to about 25 km
				// https://ev-database.org/cheatsheet/energy-consumption-electric-car
				driveDischarge(5);
				available = true;
			}
			
			Wait(5);
		}
	}
	
public:
	// energy in kwh, time in minutes
	// positive energy goes into car, negative goes from car
	float exchangeEnergy(float energy, float time) {
		// Car is not available
		if (!available || mode == NONE) {
			return 0;
		}
		
		// Charge
		if (mode == DUMB || batteryEnergy < EV_LOW_LIMIT) {
			float chargedEnergy = time/60 * EV_NORMAL_CHARGE_POWER;
			if (batteryEnergy + chargedEnergy * EV_CHARGE_EFF < EV_CAPACITY) {
				batteryEnergy += chargedEnergy * EV_CHARGE_EFF;
				return chargedEnergy;
			}
		}
		
		if (energy > 0) {
			// Smart charging for V1G or V2G
			// limit the charging speed by maximum charger power
			float maxEnergy = time/60 * EV_MAX_CHARGE_POWER;
			if (energy > maxEnergy) {
				energy = maxEnergy;
			}
			// if the battery has enough free capacity, charge it
			float charged = energy * EV_CHARGE_EFF; // charging losses
			if (batteryEnergy + charged < EV_CAPACITY) {
				batteryEnergy += charged;
				return energy;
			}
			// battery is full
			return 0;
			
		} else {
			// DISCHARGE, only V2G can do this
			if (mode == V2G) {
				// limit the speed by the maximum discarge power
				float maxEnergy = time/60 * -EV_MAX_DISCHARGE_POWER;
				if (energy < maxEnergy) {
					energy = maxEnergy;
				}
				
				// if the battery has enough energy in it, give it back to grid
				float needed = energy / EV_DISCHARGE_EFF; // discharging losses
				if (batteryEnergy + needed > EV_LOW_LIMIT) {
					batteryEnergy += needed;
					return energy;
				}
			}
		}

		return 0;
	}
	
	float readBattery() {
		if (!available || mode==NONE) {
			return NAN;
		}
		return batteryEnergy;
	}
	
	void driveDischarge(float energy) {
		float used = (batteryEnergy > energy) ? energy : batteryEnergy;
		
		energyUsedDriving += used;
		batteryEnergy -= used;
	}
};

// all values in kW
static float loadPowerMemory[SIM_TIME];
static float solarPowerMemory[SIM_TIME];
static float netPowerMemory[SIM_TIME];
static float evExchangePowerMemory[SIM_TIME];

static float batteryEnergyMemory[SIM_TIME]; // in kWh

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
			
			float exchangedEnergy = ev->exchangeEnergy(netEnergy, meterInterval);
			evExchangePowerMemory[measuredSamples] = exchangedEnergy*60/meterInterval;
			
			netEnergy -= exchangedEnergy;
			
			netPowerMemory[measuredSamples] = netEnergy*60/meterInterval;
			batteryEnergyMemory[measuredSamples] = ev->readBattery();
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
	fprintf(f, "loadPower,solarPower,exchangePower,batteryEnergy,netPower\n");
	
	for (int i=0; i<10000; i++) {
		fprintf(f, "%lf,%lf,%lf,%lf,%lf\n", loadPowerMemory[i], solarPowerMemory[i], evExchangePowerMemory[i], batteryEnergyMemory[i], netPowerMemory[i]);
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
		
		float ex = evExchangePowerMemory[i] * meterInterval/60;
		if (ex > 0) {
			energyCharged += ex;
		} else {
			energyRecovered -= ex;
		}
		
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
	printf("Energy used for driving the EV: %lf MWh\n", energyUsedDriving/1000);

}


int main() {
	Init(SIM_START_TIME, SIM_END_TIME); // Time in minutes
	
	(new Load)->Activate();
	(new Solar)->Activate();
	EV *ev = new EV;
	ev->Activate();
	Meter *meter = new Meter(ev);
	meter->Activate();
		
	Run();
	
	saveData();
	printMetrics();
	
	return 0;
};
