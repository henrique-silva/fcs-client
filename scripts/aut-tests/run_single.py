#!/usr/bin/python3

def run_single(argv):
    import sys
    import os
    from bpm_experiment import BPMExperiment

    input_metadata_file_path = argv[0]
    data_file_path = argv[1]

    if len(argv) > 2:
        if argv[2] != '0':
            fpga_hostname = argv[2]
        else:
            fpga_hostname = 'localhost'
    else:
        fpga_hostname = 'localhost'

    if len(argv) > 3:
        if argv[3] != '0':
            rffe_hostname = argv[3]
        else:
            rffe_hostname = 'localhost'
    else:
        rffe_hostname = 'localhost'

    if len(argv) > 4:
        askconfirmation = argv[4]
    else:
        askconfirmation = True

    # FIXME: FPGA and RFFE IPs should ideally come from function input arguments
    exp = BPMExperiment(fpga_hostname, rffe_hostname)

    datapaths = ['adc', 'tbt', 'fofb']

    while True:
        exp.load_from_metadata(input_metadata_file_path)
        print('\n====================')
        print('EXPERIMENT SETTINGS:')
        print('====================')
        print(''.join(sorted(exp.get_metadata_lines())))

        if askconfirmation:
            input_text = input('Press ENTER to run the experiment. \nType \'l\' and press ENTER to load new experiment settings from \'' + os.path.abspath(input_metadata_file_path) + '\'.\nType \'q\' and press ENTER to quit.\n')
        else:
            input_text = ''

        if not input_text:
            if 'rffe_v1' in exp.metadata['rffe_board_version']:
                exp.rffe_hostname = '192.168.10.101'
            elif 'rffe_v2' in exp.metadata['rffe_board_version']:
                exp.rffe_hostname = '192.168.10.104'
            else:
                print('Unknown version of RFFE. Ending experiment...\n')
                break

            # Assure that no file or folder will be overwritten
            ntries = 1;
            while True:
                data_filenames = []
                for datapath in datapaths:
                    data_filenames.append(os.path.join(os.path.normpath(data_file_path), datapath, 'data_' + str(ntries) + '_' + datapath + '.txt'))

                ntries = ntries+1
                if all(not os.path.exists(data_filename) for data_filename in data_filenames):
                    break

            for i in range(0,len(data_filenames)):
                print('        Running ' + datapaths[i] + ' datapath...', end='')
                sys.stdout.flush()
                exp.run(data_filenames[i], datapaths[i])
                print(' done. Results in: ' + data_filenames[i])

            print('The experiment has run successfully!\n');

            break

        elif input_text == 'q':
            break

if __name__ == "__main__":
    import sys
    run_single(sys.argv[1:])
