#include <simlib.h>

#include <stdio.h>
#include <math.h>
#include <cstring>

// Simulation parameters
const int SIM_START_TIME = 0; // minutes
const int SIM_END_TIME = 365*24*60; // minutes
const int meterInterval = 2; // minute
const int CAPTURED_SAMPLES = (SIM_END_TIME - SIM_START_TIME) / 2;

const int EXPORT_SAMPLES = CAPTURED_SAMPLES;

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
const float SOLAR_EFF_DETERIORATION = 0.005; // yearly drop in percent

// shared variables
float solarPower; // Power currently generated in kW

// retruns number between 0 and 23
int getTimeOfDay() {
	return (int(Time)/60) % 24;
}

// returns number between 0 and 6
int getDayOfWeek() {
	return (int(Time)/60/24) % 7;
}

const int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,};

// returns number between 1 and 31 
int getDayOfMonth()
{
	int temp = (int(Time) / 60 / 24);
	int m = 0;
	while (true)
	{
		if (temp <= daysInMonth[m])
		{
			break;
		}
		temp -= daysInMonth[m];
		m++;
	}
	return (temp != 0) ? temp : 1;
}

// returns number between 0 and 11
int getMonth()
{
	int temp = (int(Time) / 60 / 24);
	int m = 0;
	while (true)
	{
		if (temp <= daysInMonth[m])
		{
			break;
		}
		temp -= daysInMonth[m];
		m++;
	}
	return (m);
}

// data from https://www.johnnyspraguetours.com/when-sunrise-sunset-prague/
// date is in the middle of the month
const float monthlySunrise[12] = 
	{7.92, 7.22, 6.27, 6.15, 
	5.27, 4.87, 5.17, 5.87, 
	6.65, 7.42, 7.27, 7.92,};

const float monthlySunset[12] = 
	{16.48, 17.35, 18.12, 19.95, 
	20.72, 21.23, 21.1, 20.33, 
	19.25, 18.17, 16.28, 16,};

static bool dayIsCloudy[32];
static int lastValue;

// Solar power system
class Solar : public Process
{
	const float monthlyCoef[12] = {0.2,0.4,0.6,0.65,0.7,0.9,0.85,0.8,0.7,0.5,0.2,0.1,};

	// data from https://www.chmi.cz/historicka-data/pocasi/uzemni-teploty?l=en#
	// location: Brno, CZ
	const float yearlyTemp[24] = {-1.2, 0.0, 2.1, 5.0, 9.8, 18.1, 17.8, 15.4, 13.4, 7.2, 2.7, 0.6,};

	// data from https://weather-and-climate.com/average-monthly-hours-Sunshine,Brno,Czech-Republic
	const int monthlySunshineHours[12] = {55, 82, 139, 210, 225, 247, 245, 244, 177, 113, 61, 45,};

	// data from https://weather-and-climate.com/average-monthly-hours-Sunshine,Brno,Czech-Republic
	const int monthlyChanceToRain[12] = {20, 17, 23, 20, 27, 30, 30, 23, 20, 20, 23, 27,};

	float computeCloudyWeather(float solarPower, float modifier)
	{
		float r = Random();
		lastValue = r;
		return (modifier * solarPower * r + lastValue) / 2;
	}

	// Linear interpolation between months
	float effByMonth()
	{
		int m = getMonth();
		int d = getDayOfMonth();
		int daysM = daysInMonth[m];
		float dist = d - daysInMonth[m] / 2;
		int n;
		if (dist > 0)
		{
			n = (m + 1) % 12;
		}
		else
		{
			n = (m == 0) ? 11 : (m - 1);
			dist *= -1;
		}
		int daysN = daysInMonth[n];
		float q = ((daysM + daysN) / 2);
		float x = q - dist;
		return (dist * monthlyCoef[n] + x * monthlyCoef[m]) / q;
	}

	// https://www.cleanenergyreviews.info/blog/most-efficient-solar-panels
	float effDropTemp()
	{
		// Reduce efficiency when temperature
		int m = getMonth();
		return ((-3.0 * yearlyTemp[m]) / 10.0 + 107.5) / 100.0 * effByMonth();
	}

	float effDropDeterioration(float base_eff)
	{
		// Reduce efficiency by 0.5% per year
		return base_eff - ((SOLAR_EFF_DETERIORATION / 365) * (int(Time) / (24 * 60)));
	}

	float getEfficencyDrop()
	{
		float base_eff = effDropTemp();
		float eff = effDropDeterioration(base_eff);
		return eff;
	}

	float getSunnyDays(int month)
	{
		float dayLen = monthlySunset[month] - monthlySunrise[month];
		float sunnyDays = monthlySunshineHours[month] / dayLen;
		return sunnyDays;
	}

	// basic sinusoidal model of solar power generated
	float getSolarPower(int h, int m)
	{
		float sunrise = monthlySunrise[m];
		float sunset = monthlySunset[m];
		float midday = (sunset + sunrise) / 2;
		float eff = getEfficencyDrop();

		// Only when sun is in the air
		if (h > sunrise and h < sunset) {
			solarPower = eff * SOLAR_INSTALLED_POWER * cos((h - midday) * M_PI / (sunrise - sunset));
		}
		else {
			return 0;
		}
		return solarPower;
	}

	void Behavior() {

		float random_modifier = Random();

		while (1) {
			int h = getTimeOfDay();
			int d = getDayOfMonth();
			int m = getMonth();
			int daysMonth = daysInMonth[m];

			// Get base solar power
			solarPower = getSolarPower(h, m);

			int sunnyDays = int(getSunnyDays(m)) + 1;
			int cloudyDays = daysMonth - sunnyDays;
		
			if ((d == 1) and (h == 0)) {
				memset(dayIsCloudy, 0, sizeof(dayIsCloudy));
				// Designate some days as cloudy
				for (int i = 0; i < cloudyDays; i++)
				{
					while (1) {
						int chosen_day = rand() % daysMonth;
						if (dayIsCloudy[chosen_day]) {
							continue;
						} 
						else {
							dayIsCloudy[chosen_day] = 1;
							break;
						}
					}
				}
			}
			// Random scale of clousiness
			if (h == 0) {
				random_modifier = Random();
			}

			// If day chosen as cloudy throw in some randomness
			if (dayIsCloudy[d])
			{
				solarPower = computeCloudyWeather(solarPower, random_modifier);
			}
			Wait(5);
		}
	}
};

class Load : public Process {
public:
	virtual float readPower() = 0;
};


float bedTime = 22;

class LightBulb : public Load {
public:
	LightBulb(float power) {
		this->power = power;
    }
    
    float readPower() {
		return active ? power : 0;
	}
	
private:
	float power;
	bool active;
	constexpr static int duskHour[12] = {17,18,18,20,21,22,22,21,20,18,18,17};
	
	void Behavior() {
		while (1) {
			int h = getTimeOfDay();
			int timeToWait = (duskHour[getMonth()] - h) * 60;
			timeToWait = Normal(timeToWait, 15*15);
			timeToWait = (timeToWait > 0) ? timeToWait : 0;
			Wait(timeToWait);
			
			active = true;
			h = getTimeOfDay();
			timeToWait = (bedTime - h) * 60;
			timeToWait = Normal(timeToWait, 15*15);
			timeToWait = (timeToWait > 0) ? timeToWait : 0;
			Wait(timeToWait);
			active = false;
		}
	}
	
};


const float APPLIANCE_TICK_TIME = 1;

class Appliance : public Load {
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
	
	float intStart;
	float intStop;
	
	void Behavior() {
		float integrator = 0;
		float intStart = Uniform(0, avgTime);
		float intStop = Uniform(-avgTime, 0);
		
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
				if (integrator < intStop) {
					active = false;
					intStart = Uniform(0, avgTime);
				}
			} else {
				// turn on
				if (integrator > intStart) {
					active = true;
					intStop = Uniform(-avgTime, 0);
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


float foodProbability[24] = {
	0.01, 0.01, 0.01, 0.01, 0.01, 0.01,
	0.02, 0.05, 0.06, 0.04, 0.04, 0.02,
	0.05, 0.02, 0.02, 0.02, 0.06, 0.11,
	0.11, 0.07, 0.03, 0.02, 0.01, 0.01,
};


float computerProbability[24] = {
	0.01, 0.00, 0.00, 0.00, 0.00, 0.00,
	0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
	0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
	0.03, 0.03, 0.03, 0.02, 0.02, 0.02,
};


float cleanProbability[24] = {
	0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
	0.01, 0.02, 0.07, 0.10, 0.07, 0.06,
	0.05, 0.02, 0.02, 0.02, 0.02, 0.02,
	0.02, 0.01, 0.01, 0.00, 0.00, 0.00,
};

float washClothesProbability[24] = {
	0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
	0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
	0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
	0.02, 0.02, 0.01, 0.01, 0.00, 0.00,
};



// This simulates the power load of the household
class Household : public Load {
	float loadPower = 0;
	int numPeople = 4;
	float tvProbScaled[24];
	float microwaveScaled[24];
	
	void calculateProbabilities() {
		for (int i=0; i<24; i++) {
			tvProbScaled[i] = pow(tvProbability[i], numPeople);
		}
		
		for (int i=0; i<24; i++) {
			microwaveScaled[i] = foodProbability[i] * numPeople;
		}
	}
	
	void Behavior() {
		calculateProbabilities();
		
		Load *appliances[] = {
			new Appliance(tvProbScaled, tvTime, tvPower), // 1 TV for whole household
			new Appliance(computerProbability, 360, 0.200), // Gaming PC
			new Appliance(computerProbability, 360, 0.100), // normal PC
			new Appliance(computerProbability, 360, 0.040), // laptop
			new Appliance(computerProbability, 360, 0.020), // laptop
			new Appliance(microwaveScaled, 5, 0.800), // microwave
			new Appliance(foodProbability, 45, 2.130), // electric oven
			new Appliance(cleanProbability, 60, 2), // vacuum
			new Appliance(washClothesProbability, 120, 1), // clothes washer
			new LightBulb(0.008),
			new LightBulb(0.008),
			new LightBulb(0.008),
			new LightBulb(0.012),
			new LightBulb(0.012),
			new LightBulb(0.015),
		};
		
		for (auto a : appliances) {
			a->Activate();
		}
		
		while (1) {
			float currentPower = CONST_POWER;
			
			for (auto a : appliances) {
				currentPower += a->readPower();
			}
			loadPower = currentPower;
			Wait(1);
		}
	}
	
public:
	float readPower() {
		return loadPower;
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
static float loadPowerMemory[CAPTURED_SAMPLES];
static float solarPowerMemory[CAPTURED_SAMPLES];
static float netPowerMemory[CAPTURED_SAMPLES];
static float evExchangePowerMemory[CAPTURED_SAMPLES];

static float batteryEnergyMemory[CAPTURED_SAMPLES]; // in kWh

static int measuredSamples = 0;

class Meter : public Process {
	EV *ev;
	Load *household;
	
public:
	Meter(EV *vehicle, Load *household) {
		ev = vehicle;
		this->household = household;
    }
	
private:
	void Behavior() {
		while (measuredSamples < CAPTURED_SAMPLES) {
			loadPowerMemory[measuredSamples] = household->readPower();
			solarPowerMemory[measuredSamples] = solarPower;
			
			
			float netEnergy = (solarPower - household->readPower())*meterInterval/60;
			
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
	
	for (int i=0; i<EXPORT_SAMPLES; i++) {
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
	
	Household *household = new Household;
	(new Solar)->Activate();
	EV *ev = new EV;
	ev->Activate();
	household->Activate();
	Meter *meter = new Meter(ev, household);
	meter->Activate();
		
	Run();
	
	saveData();
	printMetrics();
	
	return 0;
};
