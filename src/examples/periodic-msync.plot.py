#!/usr/bin/env python3

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys
import os

dir_name = os.path.dirname(os.path.realpath(__file__))

figsize=(6, 2.5)
fname = f"{dir_name}/periodic-msync.csv"

df = pd.read_csv(fname, index_col=0)
print(df)
# df = df.set_axis(['Execution time'], axis=1)
df = (df/1e9)

print(df)

ax = df.plot.bar(rot=0, figsize=figsize)

ax.set_title("Region size: 100 MiB")
ax.set_ylabel("Execution time (s)")
ax.set_xlabel("Updates between syncs")
plt.savefig(f"{dir_name}/periodic-msync.png", bbox_inches='tight')
plt.show()
