#!/usr/bin/python3

import sys
input_metadata_filename = sys.argv[1]
data_filename = sys.argv[2]

fpga_hostname = 'localhost'
rffe_hostname = '10.0.18.200'

from bpm_experiment import BPMExperiment
exp = BPMExperiment(fpga_hostname, rffe_hostname)

import os
from os.path import basename
data_filename_ext = os.path.splitext(data_filename)

rffe_attenuators_sweep = range(31,-1,-1)

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
        
        if 'rffe_v1_' in exp.metadata['rffe_board_version']:
            rffe_gains = [14, 14] # FIXME: get right values with Baron
            rffe_power_thresholds = [-15, 0] # FIXME: get right values with Baron
        elif 'rffe_v2_' in exp.metadata['rffe_board_version']:
            rffe_gains = [20]  # FIXME: get right values with Baron
            rffe_power_thresholds = [0] # FIXME: get right values with Baron
        else:
            print('Unknown version of RFFE. Ending experiment...\n')
            break

        import itertools
        att_combinations = list(itertools.product(rffe_attenuators_sweep, repeat = natt))
        
        nexp = 1
        # Sweep RFFE channels switching (switching on or off)
        for switching in ['off', 'on']:
            exp.metadata['rffe_switching'] = switching
            exp.metadata['dsp_sausaging'] = switching
            
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
                    data_filename_adc = data_filename_ext[0] + '_' + str(ntries) + '_adc' + data_filename_ext[1]
                    data_filename_tbt = data_filename_ext[0] + '_' + str(ntries) + '_tbt' + data_filename_ext[1]
                    data_filename_fofb = data_filename_ext[0] + '_' + str(ntries) + '_fofb' + data_filename_ext[1]
                    ntries = ntries+1
                    if (not os.path.exists(data_filename_adc)) and (not os.path.exists(data_filename_tbt)) and (not os.path.exists(data_filename_fofb)):
                        break

                print(str.rjust('Run #' + str(nexp), 12) + ': RFFE switching ' + exp.metadata['rffe_switching'] + '; RFFE attenuators: ' + exp.metadata['rffe_attenuators'] + ' ', end='')
                nexp = nexp+1
                exp.run(data_filename_adc, 'adc')
                print('.', end='')
                exp.run(data_filename_tbt, 'tbt')
                print('.', end='')
                exp.run(data_filename_fofb, 'fofb')
                print('.', end='')
                
                print('')
                
        print('The experiment has run successfully!\n');
        input_text = input('Press ENTER to load a new experiment setting from \'' + os.path.abspath(input_metadata_filename) + '\'.\nType \'q\' and press ENTER to quit.\n')
        
    if input_text == 'q':
        break        
