#!/usr/bin/python3

import sys
input_metadata_filename = sys.argv[1]
data_filename = sys.argv[2]

print(input_metadata_filename)
print(data_filename)

fpga_hostname = 'localhost'
rffe_hostname = '10.0.18.200'

from bpm_experiment import BPMExperiment
exp = BPMExperiment(fpga_hostname, rffe_hostname, True)

import os
from os.path import basename
data_filename_ext = os.path.splitext(data_filename)

while True:
    exp.load_from_metadata(input_metadata_filename)
    print('\n====================')
    print('EXPERIMENT SETTINGS:')
    print('====================')
    print(''.join(sorted(exp.get_metadata_lines())))
    input_text = input('Press ENTER to run the experiment. \nType \'l\' and press ENTER to load new experiment settings from \'' + os.path.abspath(input_metadata_filename) + '\'.\nType \'q\' and press ENTER to quit.\n')
    
    if not input_text:
        # Assure that no file or folder will be overwritten
        ntries = 1;
        while True:
            if ntries > 1:
                addntriesstr = str(ntries)
            else:
                addntriesstr = ''
            data_filename_adc = data_filename_ext[0] + '_adc' + addntriesstr + data_filename_ext[1]
            data_filename_tbt = data_filename_ext[0] + '_tbt' + addntriesstr + data_filename_ext[1]
            data_filename_fofb = data_filename_ext[0] + '_fofb' + addntriesstr + data_filename_ext[1]
            ntries = ntries+1
            if (not os.path.exists(data_filename_adc)) and (not os.path.exists(data_filename_tbt)) and (not os.path.exists(data_filename_fofb)):
                break
                
        exp.run(data_filename_adc, 'adc')
        exp.run(data_filename_tbt, 'tbt')
        exp.run(data_filename_fofb, 'fofb')
        
        print('The experiment has run successfully!\n');
        
        break
    
    elif input_text == 'q':
        break
