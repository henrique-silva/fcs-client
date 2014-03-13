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

dsp_sausaging_sweep = ['off', 'on']
dsp_deswitching_phase_sweep = range(20,60,1)

datapaths = ['adc', 'tbt', 'fofb']

while True:
    exp.load_from_metadata(input_metadata_filename)
    exp.metadata['rffe_switching'] = 'on'

    print('\n====================')
    print('EXPERIMENT SETTINGS:')
    print('====================')
    print(''.join(sorted(exp.get_metadata_lines())))
    input_text = input('Press ENTER to run the experiment. \nType \'l\' and press ENTER to load new experiment settings from \'' + os.path.abspath(input_metadata_filename) + '\'.\nType \'q\' and press ENTER to quit.\n')

    if not input_text:
        # Find the number of attenuators on the RFFE
        att_items = exp.metadata['rffe_attenuators'].split(',')
        natt = len(att_items)

        nexp = 1
        # Sweep sausaging (sausaging on or off)
        for dsp_sausaging in dsp_sausaging_sweep:
            exp.metadata['dsp_sausaging'] = dsp_sausaging

            # Sweep deswitching phase
            for deswitching_value_set in dps_deswitching_phase_sweep:
                # Write deswitching values to metadata
                exp.metadata['dsp_deswitching_phase'] = str(deswitching_value_set);

                # Assure that no file or folder will be overwritten
                ntries = 1;
                while True:
                    data_filenames = []
                    for datapath in datapaths:
                        data_filenames.append(os.path.join(os.path.normpath(data_file_path), 'sausaging_' + exp.metadata['dsp_sausaging'], datapath, 'data_' + str(ntries) + '_' + datapath + '.txt'))

                    ntries = ntries+1
                    if all(not os.path.exists(data_filename) for data_filename in data_filenames):
                        break

                print(str.rjust('Run #' + str(nexp), 12) + ': Sausaging ' + exp.metadata['dsp_sausaging'] + '; Deswitching phase: ' + exp.metadata['dsp_deswitching_phase'] + ' ')
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
