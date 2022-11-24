import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('out.csv', dtype='float')

meter_tick = 2
hour = 60/meter_tick
day = hour*24
week = day*7

def plot_all_power(df, filename='all_power.pdf'):
    plt.plot(df['loadPower'], label='loadPower')
    plt.plot(df['solarPower'], label='solarPower')
    plt.plot(df['exchangePower'], label='exchangePower')
    plt.plot(df['netPower'], label='netPower')
    plt.grid(True)
    plt.legend(loc='best')
    #plt.show()
    plt.savefig(filename)
    plt.clf()

def plot_battery(df, filename='battery_energy.pdf'):
    plt.plot(df['batteryEnergy'], label='batteryEnergy')
    #plt.show()
    plt.savefig(filename)
    plt.clf()

def plot_solar(df, filename='solar_power.pdf'):
    plt.plot(df['solarPower'], label='solarPower')
    #plt.show()
    plt.grid(True)
    plt.savefig(filename)
    plt.clf()

def plot_load(df, filename='load_power.pdf'):
    plt.plot(df['loadPower'], label='loadPower')
    
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.savefig(filename)
    plt.clf()


plot_load(df.loc[week:week+3*day])
plot_battery(df.loc[:2*week])
plot_solar(df.loc[:week])
plot_all_power(df.loc[:week])
