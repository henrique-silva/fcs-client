#!/usr/bin/python3

import sys
import os
import itertools

from bpm_experiment import BPMExperiment

input_metadata_filename = sys.argv[1]
data_file_path = sys.argv[2]

fpga_hostname = 'localhost'
rffe_hostname = '10.0.17.200'

exp = BPMExperiment(fpga_hostname, rffe_hostname)

rffe_switching_sweep = ['off', 'on']

datapaths = ['adc', 'tbt', 'fofb']

while True:
    exp.load_from_metadata(input_metadata_filename)
    print('\n====================')
    print('EXPERIMENT SETTINGS:')
    print('====================')
    print(''.join(sorted(exp.get_metadata_lines())))
    input_text = input('Press ENTER to run the experiment. \nType \'l\' and press ENTER to load new experiment settings from \'' + os.path.abspath(input_metadata_filename) + '\'.\nType \'q\' and press ENTER to quit.\n')

    if not input_text:
        # Find the number of attenuators on the RFFE
        att_items = exp.metadata['rffe_attenuators'].split(',')
        natt = len(att_items)

        if 'rffe_v1' in exp.metadata['rffe_board_version']:
            rffe_gains = [13, 17]
            rffe_power_thresholds = [0, 0]
            rffe_attenuators_sweep = range(0,31,7)
        elif 'rffe_v2' in exp.metadata['rffe_board_version']:
            rffe_gains = [17]
            rffe_power_thresholds = [0]
            rffe_attenuators_sweep = range(0,31,5)
        else:
            print('Unknown version of RFFE. Ending experiment...\n')
            break

        att_combinations = list(itertools.product(rffe_attenuators_sweep, repeat = natt))

        nexp = 1
        # Sweep RFFE channels switching (switching on or off)
        for rffe_switching in rffe_switching_sweep:
            exp.metadata['rffe_switching'] = rffe_switching
            exp.metadata['dsp_sausaging'] = rffe_switching

            # Sweep all RFFE attenuator values which respect power level thresholds
            for att_value_set in att_combinations:
                # Check if power level at each RFFE stage is lower than the allowed threshold
                power_level = float(exp.metadata['rffe_signal_carrier_maxpower'].split()[0])

                exceeded_threshold = False
                i = 0
                for att_value in att_value_set:
                    power_level = power_level + rffe_gains[i] - float(att_value)
                    if power_level > rffe_power_thresholds[i]:
                        exceeded_threshold = True
                        break
                    i = i+1

                if exceeded_threshold:
                    continue

                # Write attenuation values to metadata
                exp.metadata['rffe_attenuators'] = '';
                for att_value in att_value_set:
                    if exp.metadata['rffe_attenuators']:
                        exp.metadata['rffe_attenuators'] = exp.metadata['rffe_attenuators'] + ', '
                    exp.metadata['rffe_attenuators'] = exp.metadata['rffe_attenuators'] + str(att_value) + ' dB'

                # Assure that no file or folder will be overwritten
                ntries = 1;
                while True:
                    data_filenames = []
                    for datapath in datapaths:
                        data_filenames.append(os.path.join(os.path.normpath(data_file_path), 'switching_' + exp.metadata['rffe_switching'], datapath, 'data_' + str(ntries) + '_' + datapath + '.txt'))

                    ntries = ntries+1
                    if all(not os.path.exists(data_filename) for data_filename in data_filenames):
                        break

                print(str.rjust('Run #' + str(nexp), 12) + ': RFFE switching ' + exp.metadata['rffe_switching'] + '; RFFE attenuators: ' + exp.metadata['rffe_attenuators'] + ' ')
                nexp = nexp+1

                for i in range(0,len(data_filenames)):
                    print('        Running ' + datapaths[i] + ' datapath...', end='')
                    sys.stdout.flush()
                    exp.run(data_filenames[i], datapaths[i])
                    print(' done. Results in: ' + data_filenames[i])

                print('')

        print('The experiment has run successfully!\n');
        input_text = input('Press ENTER to load a new experiment setting from \'' + os.path.abspath(input_metadata_filename) + '\'.\nType \'q\' and press ENTER to quit.\n')

    if input_text == 'q':
        break
