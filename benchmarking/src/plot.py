import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.axes as axes  # Import Axes directly for type hinting
import matplotlib.figure as figure  # Import Figure directly for type hinting
from typing import List, Tuple

class BenchmarkPlotter:
    """
    A class to read benchmark results from a CSV file and plot them.

    This class handles loading the data, configuring plot aesthetics,
    plotting multiple function outputs against a common x-axis, and
    saving the plot to a PNG file. It is designed to be robust against
    large datasets by configuring Matplotlib's chunking behavior.

    Attributes
    ----------
    csv_filepath : str
        The path to the input CSV file containing benchmark data.
    plots_dir : str
        The directory where the generated plots will be saved.
    df : Optional[pd.DataFrame]
        The DataFrame holding the loaded CSV data. None if loading fails.
    x_values : Optional[pd.Series]
        The 'x' column data from the CSV. None if not loaded.
    function_columns : Optional[List[str]]
        A list of column names representing the function outputs to plot.
        None if not loaded.
    linestyles : List[str]
        A list of linestyles to cycle through for plots.
    colors : List[Tuple[float, float, float, float]]
        A list of RGBA colors to cycle through for plots.
    """

    def __init__(self, csv_filepath: str, plots_dir: str = "plots"):
        """
        Initializes the BenchmarkPlotter with file paths and sets Matplotlib rcParams.

        Parameters
        ----------
        csv_filepath : str
            The path to the input CSV file.
        plots_dir : str, optional
            The directory to save plots. Defaults to "plots".
        """
        self.csv_filepath = csv_filepath
        self.plots_dir = plots_dir
        self.df = None
        self.x_values = None
        self.function_columns = None

        # --- Matplotlib configuration for large datasets ---
        # Set the chunk size for the Agg backend. This breaks large paths into smaller segments.
        # A value of 20000-50000 is often good for millions of points.
        plt.rcParams['agg.path.chunksize'] = 20000
        # For very dense plots, you might also consider increasing path.simplify_threshold
        # plt.rcParams['path.simplify_threshold'] = 0.5 # Example, adjust as needed (default is 0.111)
        # --- END Matplotlib configuration ---

        self.linestyles: List[str] = ['-', '--', ':', '-.']
        self.colors: List[Tuple[float, float, float, float]] = plt.get_cmap('tab10').colors 

    def _load_data(self) -> bool:
        """
        Loads data from the CSV file and performs initial validation.

        Loads the CSV into a pandas DataFrame, sorts it by the 'x' column,
        and populates `self.df`, `self.x_values`, and `self.function_columns`.

        Returns
        -------
        bool
            True if data loading and validation are successful, False otherwise.
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
            print("Error: CSV file must contain an 'x' column for the input values.")
            return False

        # Sort the DataFrame by the 'x' column for correct line plotting
        self.df = temp_df.sort_values(by='x').reset_index(drop=True)
        self.x_values = self.df['x']
        self.function_columns = [col for col in self.df.columns if col != 'x']
        return True

    def _determine_y_limits(self) -> Tuple[float, float]:
        """
        Calculates dynamic y-axis limits based on the min/max of all function outputs.

        Assumes `self.df` and `self.function_columns` are already populated by `_load_data`.

        Returns
        -------
        tuple[float, float]
            A tuple containing (min_y_limit, max_y_limit) for the plot.
        """
        # These assertions are for static analysis (Pylance) and runtime safety
        assert self.df is not None, "DataFrame not loaded. Call _load_data() first."
        assert self.function_columns is not None, "Function columns not prepared. Call _load_data() first."

        min_y = float('inf')
        max_y = float('-inf')

        for col in self.function_columns:
            y_values = self.df[col]
            min_y = min(min_y, y_values.min())
            max_y = max(max_y, y_values.max())

        padding = (max_y - min_y) * 0.05 # 5% padding
        return min_y - padding, max_y + padding

    def _plot_functions(self, ax: axes.Axes):
        """
        Plots each function's output on the given Axes object.

        Assumes `self.x_values`, `self.function_columns`, and `self.df` are populated by `_load_data`.

        Parameters
        ----------
        ax : matplotlib.axes.Axes
            The Axes object on which to draw the plots.
        """
        # Assertions for static analysis and runtime safety
        assert self.x_values is not None, "X values not loaded. Call _load_data() first."
        assert self.function_columns is not None, "Function columns not prepared. Call _load_data() first."
        assert self.df is not None, "DataFrame not loaded. Call _load_data() first."


        for i, col in enumerate(self.function_columns):
            y_values = self.df[col]
            
            linestyle = self.linestyles[i % len(self.linestyles)]
            color = self.colors[i % len(self.colors)]
            
            ax.plot(self.x_values, y_values,
                    label=col.replace('_output', ''), # Clean up label for legend
                    linestyle=linestyle,
                    color=color,
                    alpha=0.7, # Slightly transparent to see overlaps
                   )

    def _finalize_plot_layout(self, fig: figure.Figure, ax: axes.Axes, y_limits: Tuple[float, float]):
        """
        Applies final layout configurations to the plot.

        Parameters
        ----------
        fig : matplotlib.figure.Figure
            The Figure object for tight layout.
        ax : matplotlib.axes.Axes
            The Axes object to configure.
        y_limits : tuple[float, float]
            The (min, max) limits for the y-axis.
        """
        ax.set_ylim(y_limits)
        ax.set_title('Benchmark Function Outputs Comparison')
        ax.set_xlabel('X Value')
        ax.set_ylabel('Function Output')
        ax.grid(True, linestyle='--', alpha=0.6)
        ax.legend(loc='best', fontsize='small')
        fig.tight_layout()

    def _save_plot(self, fig: figure.Figure):
        """
        Saves the generated plot to a PNG file and closes the figure.

        Parameters
        ----------
        fig : matplotlib.figure.Figure
            The Figure object to save.
        """
        os.makedirs(self.plots_dir, exist_ok=True)
        # Adjust output filename: remove 'raw_' prefix and change extension to .png
        output_filename = os.path.join(self.plots_dir, os.path.basename(self.csv_filepath).replace('raw_', '').replace('.csv', '.png'))
        
        fig.savefig(output_filename, dpi=300) # Save with high resolution
        print(f"Plot saved successfully to '{output_filename}'")
        plt.close(fig) # Close the plot to free memory

    def plot(self) -> bool:
        """
        Orchestrates the entire plotting process.

        This is the main public method to call to generate the plot.
        It handles data loading, plotting, layout, and saving.

        Returns
        -------
        bool
            True if the plot is successfully generated and saved, False otherwise.
        """
        if not self._load_data():
            return False

        # Data is guaranteed to be loaded if we reach here due to the return False above.
 
        fig: figure.Figure # Explicit type hint for fig
        ax: axes.Axes      # Explicit type hint for ax
        fig, ax = plt.subplots(figsize=(12, 8))
        
        y_limits = self._determine_y_limits()
        self._plot_functions(ax)
        self._finalize_plot_layout(fig, ax, y_limits)
        
        self._save_plot(fig)
        return True

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Plot benchmark results from a CSV file.")
    parser.add_argument("csv_file", help="Path to the input CSV file.")
    
    args = parser.parse_args()
    
    plotter = BenchmarkPlotter(args.csv_file)
    plotter.plot()
