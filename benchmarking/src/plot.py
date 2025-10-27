import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import argparse
import os
import numpy as np
import matplotlib.axes as axes 
import matplotlib.figure as figure 
from typing import List, Tuple, Optional

# Global constant is no longer strictly needed for y-limits but kept for future use
FIXED_Y_PADDING = 1.0 

class BenchmarkPlotter:
    """
    A class to read benchmark results and generate error and distribution plots.

    Generates two plots:
    1. Agreement Scatter Plot (Approximation vs. Reference)
    2. Function Output Correlation Heatmap (Covariance)
    """

    def __init__(self, csv_filepath: str, plots_dir: str = "plots"):
        """
        Initializes the BenchmarkPlotter with file paths and sets Matplotlib rcParams.
        """
        self.csv_filepath = csv_filepath
        self.plots_dir = plots_dir
        self.df: Optional[pd.DataFrame] = None
        self.x_values: Optional[pd.Series] = None
        self.function_columns: Optional[List[str]] = None
        self.reference_col: Optional[str] = None
        self.approximation_cols: Optional[List[str]] = None

        # --- Matplotlib configuration ---
        plt.rcParams['agg.path.chunksize'] = 20000

        # --- Plot Aesthetics ---
        # Marker is fixed to 'o' (dot) and size is slightly increased
        self.scatter_marker = 'o'

        # Custom hex color list chosen for maximum visual distance (e.g., Red, Blue, Green, Purple, Orange)
        self.marker_styles: List[Tuple[str, int, int]] = [
            ('#E41A1C', 100, 1),  # Red, 100 size, 1 alpha
            ('#377EB8', 55, 0.8),  # Blue, 55 size, 0.8 alpha
            ('#4DAF4A', 50, 0.7),  # Green, 50 size, 0.7 alpha
            ('#984EA3', 45, 0.6),  # Purple, 45 size, 0.6 alpha
            ('#FF7F00', 40, 0.5),  # Orange, 40 size, 0.5 alpha
            ("#737300", 35, 0.4),  # Olive, 35 size, 0.4 alpha
            ('#A65628', 30, 0.3),  # Brown, 30 size, 0.3 alpha
            ('#F781BF', 25, 0.2)   # Pink, 25 size, 0.2 alpha
        ]

    def _load_data(self) -> bool:
        """
        Loads data from the CSV file and identifies reference/approximation columns.
        """
        try:
            temp_df = pd.read_csv(self.csv_filepath)
        except FileNotFoundError:
            print(f"Error: CSV file not found at '{self.csv_filepath}'")
            return False
        except Exception as e:
            print(f"Error reading CSV file: {e}")
            return False

        if 'x' not in temp_df.columns:
            print("Error: CSV file must contain an 'x' column.")
            return False

        self.df = temp_df.sort_values(by='x').reset_index(drop=True)
        self.x_values = self.df['x']
        
        all_cols = [col for col in self.df.columns if col != 'x']
        if len(all_cols) < 1:
             print("Error: CSV must contain at least one function output column.")
             return False

        self.reference_col = all_cols[0]
        self.approximation_cols = all_cols[1:]
        self.function_columns = all_cols
        
        return True

    def _plot_agreement(self, ax: axes.Axes):
        """
        Creates a scatter plot of approximation outputs vs. reference outputs (Y=X plot).
        """
        assert self.df is not None and self.reference_col is not None
        
        # Determine shared limits for the square plot
        ref_values = self.df[self.reference_col]
        min_val = ref_values.min()
        max_val = ref_values.max()
        
        padding = (max_val - min_val) * 0.05 + 0.01 
        if padding < 0.1: padding = 0.1

        plot_min = min_val - padding
        plot_max = max_val + padding

        # 1. Plot the y=x (agreement) line
        ax.plot([plot_min, plot_max], [plot_min, plot_max], 
                'k--', alpha=0.8, label='Perfect Agreement (y=x)') # Draw the dotted line y=x
        
        # 2. Scatter approximation outputs against reference
        for i, col in enumerate(self.approximation_cols):
            style = self.marker_styles[i % len(self.marker_styles)]
            sns.scatterplot(
                x=self.df[self.reference_col], 
                y=self.df[col],
                ax=ax, 
                label=col.replace('_output', ''),
                color=style[0], # Use specified color
                marker=self.scatter_marker, # Use simple dot marker
                alpha=style[2], # Use specified alpha
                s=style[1]  # Use specified size
            )
        
        ax.set_title('Agreement Scatter Plot: Approximation Output vs. Reference Output')
        ax.set_xlabel(f'Reference Output ({self.reference_col.replace("_output", "")})')
        ax.set_ylabel('Approximation Output')
        ax.set_xlim(plot_min, plot_max)
        ax.set_ylim(plot_min, plot_max)
        ax.set_aspect('equal', adjustable='box')
        ax.legend(loc='upper left', fontsize='small')

    # Function to insert a newline after a certain word/character or fixed length (e.g., 8 chars)
    def wrap_label(self, label, max_length=10):
        # A simple wrapping: insert newline if label is too long
        if len(label) > max_length:
                # Find the best split point (e.g., after the first word)
                parts = label.split('_', 1)
                if len(parts) > 1:
                    return parts[0] + '-\n' + parts[1]
                return label[:max_length] + '\n' + label[max_length:]
        return label

    def _plot_covariance_heatmap(self, ax: axes.Axes):
        """
        Plots a correlation heatmap between all function outputs.
        """
        assert self.df is not None and self.function_columns is not None
        
        # Calculate the Correlation Matrix (Correlation is robust against scale differences)
        corr_matrix = self.df[self.function_columns].corr()

        # Rename columns/index for cleaner labels
        display_names = [col.replace('_output', '') for col in corr_matrix.columns]
        corr_matrix.columns = display_names
        corr_matrix.index = display_names

        wrapped_x_labels = [self.wrap_label(name) for name in display_names]
        wrapped_y_labels = [self.wrap_label(name) for name in display_names]

        # Use a high-contrast color map like 'rocket' or 'mako' for a clean, dark heatmap
        sns.heatmap(
            corr_matrix,
            annot=True,              
            fmt=".3f",               
            # cmap='viridis',          
            cbar_kws={'label': 'Correlation Coefficient'},
            ax=ax,
            linewidths=0.5,
            linecolor='white',
            vmin=-1.0, vmax=1.0 # Ensure full range for correlation
        )
        
        # 1. Reduce the size of the x and y tick labels
        # You may need to slightly increase labelsize from 5 if the heatmap is small.
        ax.tick_params(axis='both', labelsize=7) 
        ax.set_xticklabels(wrapped_x_labels, rotation=0, ha='center') 
        ax.set_yticklabels(wrapped_y_labels, rotation=90, ha='center')
        ax.set_title('Function Output Correlation Matrix (Covariance proxy)')
        
        # ax.set_xticklabels(ax.get_xticklabels(), rotation=0)
        # ax.set_yticklabels(ax.get_yticklabels(), rotation=90)
        ax.tick_params(axis='y', pad=12)

        ax.set_title('Function Output Correlation Matrix (Covariance proxy)')

    def _save_plot(self, fig: figure.Figure, plot_type: str):
        """
        Saves the generated plot to a PNG file and closes the figure.
        """
        os.makedirs(self.plots_dir, exist_ok=True)
        base_filename = os.path.basename(self.csv_filepath).replace('raw_', '').replace('.csv', '')
        
        # Generate specific filename based on plot_type
        if plot_type == 'agreement':
             output_filename = os.path.join(self.plots_dir, f'{base_filename}_agreement.png')
        elif plot_type == 'covariance':
             output_filename = os.path.join(self.plots_dir, f'{base_filename}_covariance.png')
        else:
             output_filename = os.path.join(self.plots_dir, f'{base_filename}_plot.png')

        fig.savefig(output_filename, dpi=300, bbox_inches='tight') 
        print(f"{plot_type.capitalize()} plot saved successfully to '{output_filename}'")
        plt.close(fig) 

    def plot(self) -> bool:
        """
        Orchestrates the entire multi-plot generation process into two separate files.
        """
        if not self._load_data():
            return False

        if not self.approximation_cols:
             print("Error: No approximation columns found to compare against the reference.")
             return False
        
        # 1. AGREEMENT PLOT
        fig_agree, ax_agree = plt.subplots(nrows=1, ncols=1, figsize=(10, 10))
        self._plot_agreement(ax_agree)
        fig_agree.tight_layout()
        self._save_plot(fig_agree, 'agreement')

        # 2. COVARIANCE HEATMAP
        fig_covar, ax_covar = plt.subplots(nrows=1, ncols=1, figsize=(10, 8))
        self._plot_covariance_heatmap(ax_covar)
        fig_covar.tight_layout()
        self._save_plot(fig_covar, 'covariance')

        return True

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Plot benchmark results from a CSV file.")
    parser.add_argument("csv_file", help="Path to the input CSV file.")
    
    args = parser.parse_args()
    
    plotter = BenchmarkPlotter(args.csv_file)
    plotter.plot()