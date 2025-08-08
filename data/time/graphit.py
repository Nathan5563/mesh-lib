#!/usr/bin/env python3
"""
Graphit.py - Generate performance graphs from timing data

This script reads timing data from the time subdirectories and generates
bar charts comparing the performance of different executables for each input file.
"""

import os
import glob
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import numpy as np # Added for np.arange

def read_timing_data():
    """Read all timing data from the time subdirectories."""
    data = {}
    
    # Get all subdirectories in data/time/raw/
    time_dir = Path("data/time/raw")
    if not time_dir.exists():
        print("Error: data/time/raw directory not found!")
        return data
    
    # Find all subdirectories (input files)
    subdirs = [d for d in time_dir.iterdir() if d.is_dir()]
    
    for subdir in subdirs:
        input_name = subdir.name
        data[input_name] = {}
        
        # Find all .txt files in this subdirectory
        txt_files = list(subdir.glob("*.txt"))
        
        for txt_file in txt_files:
            executable_name = txt_file.stem  # Remove .txt extension
            # Strip "-harness" suffix to show just the library name
            if executable_name.endswith('-harness'):
                executable_name = executable_name[:-8]  # Remove "-harness"
            
            try:
                with open(txt_file, 'r') as f:
                    content = f.read().strip()
                    if content:
                        # Handle multiple lines by averaging all entries
                        lines = [line.strip() for line in content.split('\n') if line.strip()]
                        real_times = []
                        user_times = []
                        sys_times = []
                        
                        for line in lines:
                            # Parse CSV data: real,user,sys
                            real, user, sys = map(float, line.split(','))
                            real_times.append(real)
                            user_times.append(user)
                            sys_times.append(sys)
                        
                        # Calculate averages
                        avg_real = sum(real_times) / len(real_times)
                        avg_user = sum(user_times) / len(user_times)
                        avg_sys = sum(sys_times) / len(sys_times)
                        
                        data[input_name][executable_name] = {
                            'real': avg_real,
                            'user': avg_user,
                            'sys': avg_sys
                        }
            except Exception as e:
                print(f"Warning: Could not read {txt_file}: {e}")
    
    return data

def create_performance_graphs(data):
    """Create performance graphs for each input file."""
    if not data:
        print("No data found to graph!")
        return
    
    # Set up plotting style
    plt.style.use('default')
    sns.set_theme(style="whitegrid")
    
    # Color palette
    colors = {
        'real': '#2E86AB',    # blue
        'user': '#A23B72',    # purple
        'sys': '#F18F01'      # orange
    }
    
    # Create output directory for graphs
    output_dir = Path("data/time/graphs")
    output_dir.mkdir(exist_ok=True)
    
    for input_name, executables in data.items():
        if not executables:
            continue
            
        print(f"Creating graphs for {input_name}...")
        
        # Prepare data for plotting
        exec_names = list(executables.keys())
        real_times = [executables[exec]['real'] for exec in exec_names]
        user_times = [executables[exec]['user'] for exec in exec_names]
        sys_times = [executables[exec]['sys'] for exec in exec_names]
        
        # Create figure with styling
        fig, ax = plt.subplots(figsize=(14, 8))
        
        # Set up the bar positions
        x = np.arange(len(exec_names))
        width = 0.25
        
        # Create grouped bars for all three metrics
        bars1 = ax.bar(x - width, real_times, width, label='Real Time', 
                       color=colors['real'], alpha=0.9, edgecolor='white', linewidth=1.5)
        bars2 = ax.bar(x, user_times, width, label='User Time', 
                       color=colors['user'], alpha=0.9, edgecolor='white', linewidth=1.5)
        bars3 = ax.bar(x + width, sys_times, width, label='System Time', 
                       color=colors['sys'], alpha=0.9, edgecolor='white', linewidth=1.5)
        
        # Styling
        ax.set_title(f'{input_name.title()}', 
                    fontsize=20, fontweight='bold', pad=20, color='#2C3E50')
        ax.set_xlabel('Executable', fontsize=14, fontweight='bold', color='#2C3E50')
        ax.set_ylabel('Time (seconds)', fontsize=14, fontweight='bold', color='#2C3E50')
        
        # Set x-axis labels with mesh-lib in bold
        ax.set_xticks(x)
        ax.set_xticklabels(exec_names, rotation=0, ha='center', fontsize=12)
        
        # Make mesh-lib label bold
        for i, label in enumerate(ax.get_xticklabels()):
            if label.get_text() == 'mesh-lib':
                label.set_fontweight('bold')
                label.set_color('#2C3E50')
            else:
                label.set_fontweight('normal')
                label.set_color('#6C757D')
        
        # Add clearer horizontal grid lines for easier comparisons
        ax.grid(True, alpha=0.5, axis='y', linestyle='-', linewidth=1, color='#DEE2E6')
        ax.set_axisbelow(True)  # Put grid behind bars
        
        # Add value labels on bars with formatting
        def add_value_labels(bars, color):
            for bar in bars:
                height = bar.get_height()
                if height > 0:
                    # Format time values appropriately
                    if height < 0.01:
                        label = f'{height:.4f}s'
                    elif height < 1:
                        label = f'{height:.3f}s'
                    else:
                        label = f'{height:.2f}s'
                    
                    ax.text(bar.get_x() + bar.get_width()/2., height + height*0.02,
                           label, ha='center', va='bottom', fontsize=10, fontweight='bold',
                           color=color, bbox=dict(boxstyle="round,pad=0.2", 
                                                facecolor='white', alpha=0.8, 
                                                edgecolor=color, linewidth=1))
        
        add_value_labels(bars1, colors['real'])
        add_value_labels(bars2, colors['user'])
        add_value_labels(bars3, colors['sys'])
        
        # Positioned to avoid covering the graph
        legend = ax.legend(loc='upper left', frameon=True, fancybox=True, 
                          shadow=True, fontsize=12, bbox_to_anchor=(0, 1))
        legend.get_frame().set_facecolor('white')
        legend.get_frame().set_alpha(0.9)
        
        # Set background color
        ax.set_facecolor('#F8F9FA')
        fig.patch.set_facecolor('white')
        
        # Add subtle border
        for spine in ax.spines.values():
            spine.set_color('#E9ECEF')
            spine.set_linewidth(1.5)
        
        # Adjust layout
        plt.tight_layout()
        
        # Save the graph
        graph_file = output_dir / f"{input_name}_performance.png"
        plt.savefig(graph_file, dpi=300, bbox_inches='tight', 
                   facecolor='white', edgecolor='none')
        print(f"  Saved: {graph_file}")
        
        plt.close()





def main():
    """Main function to generate all graphs."""
    print("Graphit.py - Generating performance graphs from timing data")
    print("=" * 60)
    
    # Read timing data
    data = read_timing_data()
    
    if not data:
        print("No timing data found! Make sure to run the timing script first.")
        return
    
    print(f"Found timing data for {len(data)} input files:")
    for input_name, executables in data.items():
        print(f"  {input_name}: {len(executables)} executables")
    
    print("\nGenerating graphs...")
    
    # Create graphs
    create_performance_graphs(data)
    
    print("\nGraph generation completed!")
    print("Check the 'data/time/graphs/' directory for output files.")

if __name__ == "__main__":
    main()
