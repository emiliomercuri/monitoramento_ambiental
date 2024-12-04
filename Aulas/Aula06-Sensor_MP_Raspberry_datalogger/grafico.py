import pandas as pd 
import numpy as np
import matplotlib.pyplot as plt

# Read and concatenate all JSON files into a single dataframe
df = pd.read_csv('Dados/serie_temporal_mp.txt')
# Resample the data to hourly frequency
df['time'] = pd.to_datetime(df['data-hora'])
df.index = df['time']

#print(df)

plt.figure(figsize=(15,5))

plt.tick_params(axis='both', labelsize=14) #increase font size for ticks

# Plot the values
plt.plot(df['pm2.5'], 'bo', markersize=4, label = 'MP$_2.5$ ($\mu$g/m$^3$)')
#plt.xticks(rotation = 'horizontal');
plt.legend()
# Graph labels
plt.xlabel('Data', fontsize=18); plt.ylabel(r'MP$_2.5$ ($\mu$g/m$^3$)', fontsize=18); plt.title('Material Particulado Fino', fontsize=18);
plt.legend(loc="best", frameon=False, fontsize=18)

plt.savefig('mp_25.png', dpi = 300)
