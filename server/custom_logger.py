import logging
import coloredlogs
import os

class LoggerSetup:
    """
    A class to set up a logger with console (colored) and file logging.
    Logs are appended to a specified file within a 'logs/' directory.
    """
    def __init__(self, log_name: str, log_level: str = 'INFO', filename: str = 'app.log'):
        """
        Initializes the logger.

        Parameters
        ----------
        log_name : str, optional
            The name of the logger. Defaults to the name of the current module.
        log_level : str, optional
            The logging level (e.g., 'DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL').
            Defaults to 'INFO'.
        filename : str, optional
            The base name of the log file (e.g., 'app.log').
            The file will be saved in a 'logs/' directory: 'logs/app.log'.
            Logs will be appended to this file across runs.
        """
        self.log = logging.getLogger(log_name)
        numeric_level = getattr(logging, log_level.upper(), None)
        if not isinstance(numeric_level, int):
            raise ValueError(f'Invalid log level: {log_level}')

        self.log.setLevel(numeric_level)

        if not self.log.handlers:
            coloredlogs.install(level=numeric_level, logger=self.log)

            log_dir = 'logs'
            if not os.path.exists(log_dir):
                os.makedirs(log_dir)

            log_file_path = os.path.join(log_dir, filename)

            if os.path.exists(log_file_path):
                # If the log file already exists, append a newline to separate logs
                with open(log_file_path, 'a') as f:
                    f.write('\n')

            file_handler = logging.FileHandler(log_file_path, mode='a')
            file_handler.setLevel(numeric_level)

            file_formatter = logging.Formatter('%(asctime)s %(name)s %(levelname)s %(message)s')
            file_handler.setFormatter(file_formatter)

            self.log.addHandler(file_handler)

    def get_logger(self) -> logging.Logger:
        """
        Returns the configured logger instance.
        """
        return self.log