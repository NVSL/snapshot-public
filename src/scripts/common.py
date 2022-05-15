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
import math


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

class Fig(object):
    fontsize_gbl = None
    figsize_gbl = None
    ax = None
    
    def config(fontsize, figsize):
        Fig.fontsize_gbl = fontsize
        Fig.figsize_gbl = figsize


    def __init__(self, ax, fontsize=None, figsize=None):
        if fontsize != None:
            Fig.fontsize_gbl = fontsize
            
        if figsize != None:
            Fig.figsize_gbl = figsize
        
        self.ax = ax

    def get_kwargs(bar=True, nofigsize=False, **kwargs):
        result = {
            'fontsize': Fig.fontsize_gbl, 
            'figsize': Fig.figsize_gbl,
            'zorder': 3,
        }
        
        if nofigsize:
            del result['figsize']
        
        if bar:
            result['edgecolor'] = 'black'
            
        return {**kwargs, **result}

    def fmt_grid(self, axis='both', **kwargs):
        self.ax.grid(axis=axis, **kwargs)

        return self

    def fmt_legend(self, **kwargs):
        kwargs = dict(kwargs)
        if 'loc' not in kwargs:
            kwargs['loc'] = 'upper center'
        
        if kwargs['loc'] != 'upper center':
            raise Exception("Unimplemented")

        bbox_to_anchor = (0, 0)

        if kwargs['loc'] == 'upper center':
            bbox_to_anchor = (0.5, 1.45)

        handles, labels = self.ax.get_legend_handles_labels()
        
        # Get the handles to format them in case asked to apply a mask
        mask = kwargs['mask'] if 'mask' in kwargs else None
        if mask:
            if len(mask) != len(handles):
                raise Exception("Mask length should be same as the number of handles")
            
            new_handles = []
            new_labels = []
            for iter in range(len(handles)):
                if mask[iter]:
                    new_handles.append(handles[iter])
                    new_labels.append(labels[iter])
            
            handles = new_handles
            labels = new_labels
            
        if 'fontsize' not in kwargs:
            kwargs['fontsize'] = Fig.fontsize_gbl
            
        if 'fancybox' not in kwargs:
            kwargs['fancybox'] = False
            
        if 'framealpha' not in kwargs:
            kwargs['framealpha'] = 1
            
        if 'ncol' not in kwargs:
            kwargs['ncol'] = 3
        
        if 'loc' not in kwargs:
            kwargs['loc'] = 'upper center'
        
        if 'bbox_to_anchor' not in kwargs:
            kwargs['bbox_to_anchor'] = bbox_to_anchor
        
        if 'edgecolor' not in kwargs:
            kwargs['edgecolor'] = 'black'
            
        _ = self.ax.legend(
            handles = handles,
            **kwargs
        )

        return self

    def fmt_label(self, xlabel, ylabel):
        self.ax.set_xlabel(xlabel, fontsize=Fig.fontsize_gbl, fontweight='bold')
        self.ax.set_ylabel(ylabel, fontsize=Fig.fontsize_gbl, fontweight='bold')

        return self

    def add_bar_labels(self, mask, fontsize=None, precision=1, over_fig=True, facecolor='white', alpha=1, edgecolor='white', label_move_thresh=0.005, label_off_frac=0.1):
        rects = self.ax.patches
        mask_full = mask
        
        label_fontsize = Fig.fontsize_gbl-2
        
        if fontsize:
            label_fontsize = fontsize

        # Check if the input mask array can be used to generate the full mask
        if len(rects)%len(mask) != 0:
            raise Exception("Bars in the axis must be a multiple of mask length")

        # Generate the full mask from the input mask
        if len(mask) != len(rects):
            mask_full = []
            factor = int(len(rects)/len(mask))
            for i in range(len(mask)):
                mask_full += [mask[i]] * factor

        ylim = self.ax.get_ylim()
        label_off = (ylim[1] - ylim[0]) * label_off_frac
        label_pos = ylim[1] + label_off
        
        i = 0
        for rect in rects:
            i += 1
            if not mask_full[i-1]:
                continue

            height = rect.get_height()
                
            # If the label is with the bar, get the bar height
            if not over_fig:
                label_pos = ylim[0] + height + label_off*0.5

                # If the label overlaps with the border, move it over
                if abs(label_pos - ylim[1]) < (ylim[1]-ylim[0])*label_move_thresh:
                    label_pos = ylim[1] + label_off
                
            label = round(height, precision)
            tx = self.ax.text(
                rect.get_x() + rect.get_width() / 2, label_pos, f"{label}x", ha="center", va="bottom",
                fontsize=label_fontsize,
                bbox=dict(facecolor=facecolor, alpha=alpha, edgecolor=edgecolor)
            )

        return self
    
    def save(self, name):
        save_fig(name)
        
        return self
        
    def xrot(self, rot=0):
        xlbl = self.ax.get_xticklabels()
        _ = self.ax.set_xticklabels(xlbl, rotation=0)
        
        return self
    
def rename_index(df, map):
    return df.index.map(map)

def rename_cols(df, map):
    df.rename(columns=lambda x: map[x], inplace=True)
    
    return df

def capitalize_index(df):
    df.index = df.index.map(lambda x: x.capitalize())
    
    return df
    