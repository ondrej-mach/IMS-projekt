import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

plt.rcParams.update({
    "text.usetex": True,
    "font.family": "Helvetica"
})

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
    plt.savefig(filename)
    plt.clf()


def plot_battery(df, filename='battery_energy.pdf'):
    plt.plot(df['batteryEnergy'], label='batteryEnergy')
    plt.savefig(filename)
    plt.clf()


def plot_solar(df, filename, time_period):
    if time_period == 'day':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        unit = 'minutes'
        plt.gca().set_yticks(np.arange(0,5, 0.5))
    elif time_period == 'month' or time_period == 'week':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%d.%m.'))
        unit = 'days'
    elif time_period == 'year':
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
        unit = 'months'

    plt.gca().set_ylim([0, 5])
    plt.plot(df['date'], df['solarPower'], label='Solar power')
    plt.xlabel('Time ('+unit+')')
    plt.ylabel('Output power (kW)')
    plt.grid(True, axis='y')
    plt.savefig(filename)
    plt.clf()
    

def plot_solar_hist(df, filename='solar_power_hist.pdf'):
    monthAvg = df.groupby([df['date'].dt.month])['solarPower'].sum()
    plt.gca().set_xticks(np.arange(12))
    plt.gca().set_xticklabels(np.arange(1,13))
    plt.gca().set_yticks(np.arange(0, 30000, 5000))
    plt.gca().set_yticklabels(np.arange(0, 30, 5))

    plt.xlabel('Month')
    plt.ylabel('Energy generated (MWh)')
    plt.bar(np.arange(12), monthAvg)
    plt.grid(True, axis='y')
    plt.savefig(filename)
    plt.clf()

# histogram for each month sum in kW

def plot_load(df, filename='load_power.pdf'):
    rollingAvg = df['loadPower'].rolling(window=int(6*hour), center=True).mean()

    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%d.%m.'))
    plt.plot(df['date'], df['loadPower'], label='Load power', color='tab:orange')
    plt.plot(df['date'], rollingAvg, label='Rolling average', color='tab:blue')

    plt.xlabel('Time')
    plt.ylabel('Power usage (kW)')
    plt.legend(loc='upper center')
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.savefig(filename)
    plt.clf()


def plot_average_day_load(df, filename='average_day_load.pdf'):
    dayAvg = df.groupby([df['date'].dt.hour])['loadPower'].mean()

    plt.bar(np.arange(24), dayAvg)
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.xlabel('Hour of day')
    plt.ylabel('Mean power usage (kW)')
    plt.savefig(filename)
    plt.clf()


df['date'] = pd.date_range(start='1/1/2022', freq='2min', periods=df.shape[0])

plot_load(df.loc[week:week+5*day])
plot_average_day_load(df)

plot_battery(df.loc[:2*week])
plot_solar(df.loc[:year], "solar_year.pdf", "year")
plot_solar(df.loc[month*4:month*5], "solar_month.pdf", "month")
plot_solar(df.loc[week*19:week*20], "solar_week.pdf", "week")
plot_solar(df.loc[day*181:day*182], "solar_day_clear.pdf", "day")
plot_solar(df.loc[day*180:day*181], "solar_day_cloudy.pdf", "day")
plot_solar_hist(df.loc[:month*12])
plot_all_power(df.loc[:week])
