#!/usr/bin/python3

import sys
import time
from time import sleep, strftime

from run_single import run_single
from run_sweep import run_sweep

input_metadata_file_path = sys.argv[1]
data_file_path = sys.argv[2]

if len(sys.argv) > 3:
    nminutes = float(sys.argv[3])
else:
    nminutes = 1

if len(sys.argv) > 4:
    run_type = sys.argv[4]
else:
    run_type = 'run_single'

last_experiment_time = time.time()

while True:
    print('\n\n\n======================================================')
    print('New experiment burst. Initiated at ' + strftime('%Y-%m-%d %H:%M:%S'))
    print('======================================================')

    if run_type == 'run_single':
        run_single([input_metadata_file_path, data_file_path, 'localhost', '0', False])
    elif run_type == 'run_sweep':
        run_sweep([input_metadata_file_path, data_file_path, 'localhost', '0', False])
    else:
        break

    try:
        print('Waiting for next experiment burst... (press ctrl-c to stop)')

        while time.time() - last_experiment_time < nminutes*60:
            sleep(1)

        last_experiment_time = time.time()

    except KeyboardInterrupt:
        print('\nThe bursts experiment has ended.\n')
        break

    except:
        raise
