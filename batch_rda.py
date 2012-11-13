#!/usr/bin/python
"""Runs the reuse-distance analysis pin tool in batch, for Parsec programs"

usage: ./batch_rda.py
<benchmark list, separated by comma; or group name>
<number of threads>
<interval size in million>
"""
import os
import re
import shutil
import subprocess
import sys


if len(sys.argv) != 4:
    sys.stderr.write("Incorrect number of arguments. Program desc:"
                     + __doc__)
    sys.exit()

benchmarks = sys.argv[1].split(',')
numthreads = sys.argv[2]
interval_size_in_mil = sys.argv[3]
interval_size = int(interval_size_in_mil) * 1000000
home_dir = os.path.expanduser("~")
#inputs = ["simsmall", "simmedium", "simlarge"]
inputs = ["simlarge"]
#find the latest pin version installed in the system
pin_directories = [dir for dir in os.listdir(home_dir)
                   if (os.path.isdir(os.path.join(home_dir, dir)) and
                   dir.startswith('pin'))]
print pin_directories
installed_pin_versions = [re.findall(r'pin-[\d]\.[\d]+-[\d]+', dir)[0]
                          for dir in pin_directories]
print installed_pin_versions
pin_version = sorted(installed_pin_versions,
                     key=lambda x: int(re.findall(r'[\d]\.[\d]+', x)[0][2:]),
                     reverse=True)[0]
print pin_version
#set environmental variables required to run the pintool
os.environ['PARSEC_CPU_BASE'] = "0"
os.environ['PARSEC_CPU_NUM'] = numthreads
os.environ['PIN_VERSION'] = pin_version

print ("interval size", interval_size)
os.environ['PIN_INTERVAL_SIZE'] = str(interval_size)
apps = ["blackscholes", "bodytrack", "facesim", "ferret", "raytrace",
        "swaptions", "fluidanimate", "vips", "x264"]
kernels = ["canneal", "dedup", "streamcluster"]
# we can run by group as well
if benchmarks[0] == "apps":
    benchmarks = apps
elif benchmarks[0] == "kernels":
    benchmarks = kernels
else:
    pass

for bm in benchmarks:
    if bm in apps:
        group = "apps"
    elif bm in kernels:
        group = "kernels"
    else:
        sys.stderr.write("Invalid benchmark name %s\n" % bm)
        continue
    for input_size in inputs:
        id = input_size[3:] + "_" + numthreads
        dirname = "~/phooks_" + id
        output_file = ("rda_" + bm + "_" + id + "_" + interval_size_in_mil +
                       "mil.out")
        #make directory if required
        try:
            os.mkdir(dirname)
        except OSError:  # directory exists
            pass
        #set env variable of output file
        os.environ['PIN_OUTPUT'] = output_file
        run_string = ('~/parsec-2.1/bin/parsecmgmt' +
                      ' -a ' + 'run' +
                      ' -p ' + bm +
                      ' -i ' + input_size +
                      ' -n ' + numthreads +
                      ' -d ' + dirname +
                      ' -c ' + 'gcc-hooks' +
                      ' -s ' + 'runpin_rda.sh')
        print run_string
        subprocess.call(run_string, shell=True)
        #copy the output file if it is created
        inter_outfile = (dirname + "/" + group + "/" + bm + "/run/inter_" +
                         output_file)
        if os.path.exists(inter_outfile):
            shutil.copy(inter_outfile, "~/rda_out/")

sys.stderr.write("my work is done here\n")
