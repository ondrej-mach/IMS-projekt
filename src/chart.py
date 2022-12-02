import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

df = pd.read_csv('out.csv', dtype='float')

meter_tick = 2
hour = 60/meter_tick
day = hour*24
week = day*7
month = 30.4*day
year = 12*month


def plot_all_power(df, filename='all_power.pdf'):
    plt.plot(df['loadPower'], label='loadPower')
    plt.plot(df['solarPower'], label='solarPower')
    plt.plot(df['exchangePower'], label='exchangePower')
    plt.plot(df['netPower'], label='netPower')
    plt.grid(True)
    plt.legend(loc='best')
    # plt.show()
    plt.savefig(filename)
    plt.clf()


def plot_battery(df, filename='battery_energy.pdf'):
    plt.plot(df['batteryEnergy'], label='batteryEnergy')
    # plt.show()
    plt.savefig(filename)
    plt.clf()


def plot_solar(df, filename, time_period):
    
    if time_period == 'day':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        unit = 'minutes'
        plt.gca().set_yticks(np.arange(0,5, 0.5))
    elif time_period == 'month' or time_period == 'week':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%d.%m'))
        unit = 'days'
    elif time_period == 'year':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
        unit = 'months'

    plt.gca().set_ylim([0, 5])
    plt.plot(df['date'], df['solarPower'], label='solarPower')
    plt.xlabel('Time('+unit+')')
    plt.ylabel('Output(kW)')
    plt.title('Solar panel output over ' + time_period)
    plt.grid(True, axis='y')
    plt.savefig(filename)
    plt.clf()
    

def plot_solar_hist(df, filename='solar_power_hist.pdf'):
    monthAvg = df.groupby([df['date'].dt.month])['solarPower'].sum()
    plt.gca().set_xticks(np.arange(12))
    plt.gca().set_xticklabels(np.arange(1,13))
    plt.gca().set_yticks(np.arange(0, 30000, 5000))
    plt.gca().set_yticklabels(np.arange(0, 30, 5))
    plt.title('Monthly production throughout year')
    plt.xlabel('Time(month)')
    plt.ylabel('Output power(MW)')
    plt.bar(np.arange(12), monthAvg)
    # plt.show()
    plt.grid(True, axis='y')
    plt.savefig(filename)
    plt.clf()

# histogram for each month sum in kW

def plot_load(df, filename='load_power.pdf'):
    plt.plot(df['loadPower'], label='Load power')
    rollingAvg = df['loadPower'].rolling(window=int(6*hour), center=True).mean()
    plt.plot(rollingAvg, label='Rolling average')

    plt.legend(loc='best')
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.savefig(filename)
    plt.clf()


def plot_average_day_load(df, filename='average_day_load.pdf'):
    dayAvg = df.groupby([df['date'].dt.hour])['loadPower'].mean()
    plt.bar(np.arange(24) ,dayAvg)
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.savefig(filename)
    plt.clf()

df['date'] = pd.date_range(start='1/1/2022', freq='2min', periods=df.shape[0])

plot_load(df.loc[week:week+5*day])
plot_average_day_load(df)

plot_battery(df.loc[:2*week])
plot_solar(df.loc[:year], "plot_solar_year.pdf", "year")
plot_solar(df.loc[month*4:month*5], "plot_solar_month.pdf", "month")
plot_solar(df.loc[week*19:week*20], "plot_solar_week.pdf", "week")
plot_solar(df.loc[day*181:day*182], "plot_solar_day.pdf", "day")
plot_solar_hist(df.loc[:month*12])
plot_all_power(df.loc[:week])
