import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import re
import warnings


from IPython.display import Markdown, display
from os import path
from typing import Dict, List, Set

import matplotlib.ticker as mtick
import matplotlib as mpl
from scipy.stats.mstats import gmean
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator, ScalarFormatter, LogLocator)
import matplotlib.patches as patches

import seaborn as sns



def stats_to_dict(stats_path: str) -> Dict:
    f = open(stats_path)
    result = {}
    for line in f:
        if not line.strip() == '' and not line.strip()[0] == '-':
            clean = re.sub(r'\s+', ' ', line).strip()
            split = clean.strip().split()
            result[split[0]] = split[1]
    return result

def set_ax_font(ax, size):
    for item in ([ax.title, ax.xaxis.label, ax.yaxis.label] +
             ax.get_xticklabels() + ax.get_yticklabels() +  ax.legend().get_texts()):
        item.set_fontsize(size)


def init_notebook():
    from IPython.core.display import display, HTML
    display(HTML("<style>.container { width:80% !important; }</style>"))

    pd.set_option('display.max_rows', 500)
    pd.set_option('display.max_columns', 500)
    pd.set_option('display.width', 150)

    font = {
        'family' : 'sans serif',
        'weight' : 'normal',
        'size'   : 22
    }
    mpl.rc('font', **font)

def printmd(string):
    display(Markdown(string))
    
c_disable_fig_save = False
c_save_loc = '/tmp/'
c_save_prefix="undefined"

def config_common(disable_fig_save=False, save_loc='/tmp', save_prefix='undefined'):
    global c_disable_fig_save
    global c_save_loc
    global c_save_prefix
    
    c_disable_fig_save = disable_fig_save
    c_save_loc = save_loc
    c_save_prefix = save_prefix

def save_fig(name, usepdfbound=True):
    if c_disable_fig_save:
        return
    
    save_dir = c_save_loc + '/' + c_save_prefix
    dest = save_dir + '/' + name + '.png'

    if os.path.exists(save_dir):
        if not os.path.isdir(c_save_loc):
            printmd('`' + c_save_loc + '` : <span style=\'color:red\'>is not a directory, FATAL</span>')
            return 
    else:
        os.mkdir(save_dir)
        
    plt.savefig(dest, bbox_inches='tight', dpi=400, transparent=False)    
    printmd('Plot saved as `' + dest + '`')
    
    plt.savefig(dest[:-4] + '.pdf', bbox_inches='tight', dpi=400, transparent=False, pad_inches=0)
    printmd('Plot saved as `' + dest + '`')
    
    if usepdfbound:
        cmd = 'pdfcrop %s.pdf %s.pdf' % (dest[:-4], dest[:-4])
        printmd('Using pdfcrop on `' + dest + '` with command `' + cmd + '`') 
        os.system(cmd)
    
def save_df(df, name):
    if c_disable_fig_save:
        return
    
    save_dir = c_save_loc + '/' + c_save_prefix
    dest = save_dir + '/' + name + '.html'

    if os.path.exists(save_dir):
        if not os.path.isdir(c_save_loc):
            printmd('`' + c_save_loc + '` : <span style=\'color:red\'>is not a directory, FATAL</span>')
            return
    else:
        os.mkdir(save_dir)
        
    df.to_html(dest)    
    printmd('DataFrame saved as `' + dest + '`')

def clk_to_ns(clk: str) -> int:
    toks: [str] = clk.split()
    result: int = 0
    for tok in toks:
        if tok[-2:] == "ms":
            result += int(tok[:-2])*(10**6)
        elif tok[-2:] == "us":
            result += int(tok[:-2])*(10**3)
        elif tok[-2:] == "ns":
            result += int(tok[:-2])
        elif tok[-1:] == "s":
            result += int(tok[:-1])*(10**9)
        
    return result
