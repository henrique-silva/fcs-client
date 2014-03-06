#!/usr/bin/python3

# Only for debugging
#fpga_hostname = 'localhost'
#rffe_hostname = '10.0.18.200'
#data_filename = 'data.txt'
#input_metadata_filename = 'template.metadata'
#datapath = 'adc'

import sys
fpga_hostname = sys.argv[1]
rffe_hostname = sys.argv[2]
data_filename = sys.argv[3]
input_metadata_filename = sys.argv[4]
datapath = sys.argv[5]

# Metadata file is placed in the same path and filename as the data file but with
# different extension (.metadata)
import os
from os.path import basename
output_metadata_filename = os.path.splitext(data_filename)[0] + '.metadata'

# Parse metadata file into a dictionary
from metadata_parser import MetadataParser
metadata = MetadataParser()
metadata.parse(input_metadata_filename)

# Extract data from metadata dictionary and perform basic calculations
dsp_switching_frequency_ratio = metadata.options['dsp_switching_frequency_ratio']
bpm_Kx = metadata.options['bpm_Kx']
bpm_Ky = metadata.options['bpm_Ky']
rffe_switching = metadata.options['rffe_switching']
dsp_sausaging = metadata.options['dsp_sausaging']
deswitching_phase_offset = str(int(metadata.options['dsp_deswitching_phase']) - int(metadata.options['dsp_switching_phase']))
rffe_att1 = metadata.options['rffe_att1']
rffe_att2 = metadata.options['rffe_att2']

if datapath == 'adc':
    data_rate_decim_factor = '1'
    acq_channel = '0'
    acq_npts = '100000'
elif datapath == 'tbt':
    data_rate_decim_factor = metadata.options['adc_sampling_harmonic'] # FIXME: ideally read from FPGA
    acq_channel = '1'
    acq_npts = '100000'
elif datapath == 'fofb':
    data_rate_decim_factor = '1000' # FIXME: ideally read from FPGA
    acq_channel = '3'
    acq_npts = '1000000'

import subprocess
# Run FPGA configuration commands
subprocess.Popen(['fcs_client', '--setdivclk', dsp_switching_frequency_ratio, '--setkx', bpm_Kx, '--setky', bpm_Ky, '--setphaseclk', deswitching_phase_offset, '--setsw' + rffe_switching, '--setwdw' + dsp_sausaging, '--setsamples', acq_npts, '--setchan', acq_channel, '--setfpgahostname', fpga_hostname])

# Run RFFE configuration commands
subprocess.Popen(['fcs_client', '--setfesw' + rffe_switching, '--setfeatt1', rffe_att1, '--setfeatt2', rffe_att2, '--setrffehostname', rffe_hostname])

# TODO: Check if everything was properly set

# Timestamp the start of acquisition
# FIXME: ideally timestamp should come together with data. For instance, White Rabbit allows ns-precise UTC timestamping coming from hardware
from time import time
t = time()

# Run acquisition
p = subprocess.Popen(['fcs_client', '--startacq', acq_channel, '--setfpgahostname', fpga_hostname])

# The script execution is blocked here until data acquisition was completed

# Get result of data acquisition and write it to data file
f = open(data_filename, 'w') #TODO: must ensure the path exists
p = subprocess.Popen(['fcs_client', '--getcurve', acq_channel, '--setfpgahostname', fpga_hostname], stdout=f)
f.close()

# Compute data file signature
f = open(data_filename, 'r')
text = f.read()
f.close()

import hashlib
signature_method = metadata.options['data_signature_method']
if signature_method == 'md5':
    md = hashlib.md5()
elif signature_method == 'sha-1':
    md = hashlib.sha1()
elif signature_method == 'sha-256':
    md = hashlib.sha256()
md.update(text.encode(f.encoding))
filesignature = md.hexdigest()

# Format date and hour as an standard UTC timestamp (ISO 8601)
from time import strftime, gmtime
from math import floor
ns = int(floor((t * 1e9) % 1e9))
timestamp_start = '%s.%09dZ' % (strftime('%Y-%m-%dT%H:%M:%S', gmtime(t)), ns)

# Build metadata file based on template metadata file and post-processed metadata
configfile_lines = []

metadata_file = open(input_metadata_filename, 'r')
for line in metadata_file:
    configfile_lines.append(line)
metadata_file.close()

configfile_lines.append('\n')
configfile_lines.append('# Filled automatically by the \'run_experiment_slac.py\' script\n')
configfile_lines.append('data_original_filename = ' + os.path.basename(data_filename) + '\n')
configfile_lines.append('data_signature = ' + filesignature + '\n')
configfile_lines.append('data_signature_method = ' + signature_method + '\n')
configfile_lines.append('dsp_data_rate_decim_factor = ' + data_rate_decim_factor + '\n')
configfile_lines.append('timestamp_start = ' + timestamp_start + '\n')
#configfile_lines.append('adc_board_temperature = ' + '0' + ' C\n') #TODO: implement ADC temperature read on FPGA
#configfile_lines.append('rffe_board_temperature = ' + '0' + ' C\n') #TODO: implement RFFE temperature read on FPGA

f = open(output_metadata_filename, 'w')
f.writelines(configfile_lines)
f.close()
