import os
import subprocess
from tqdm import tqdm

exe = os.path.expanduser("/home/olh/repos/docacxx/build/linux/x86_64/release/ec_bench")
output_dir = os.path.expanduser("~/docacxx/bench_result/ec")
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# dev_pci
dev_pci = "cb:00.0"

# data blocks/redundant blocks
drs = [(4, 2), (6, 3), (8, 3)]

# block size
kb = 1024
mb = 1024 * 1024
bsizes = []
t = 4 * kb
while t <= 1 * mb:
    bsizes.append(t)
    t *= 2
    
# parallel tasks
ns = []
t = 1
while t <= 128:
    ns.append(t)
    t *= 2
    
# output file name
def get_output_file(*args):
    args = map(str, args)
    return os.path.join(output_dir, '_'.join(args) + '.json')


with tqdm(total=len(ns)*len(bsizes)*len(drs), desc="ec bench") as pbar:
    for d, r in drs:
        for bsize in bsizes:
            for n in ns:
                command = [
                    exe,
                    "-a", dev_pci,
                    "-o", get_output_file(n, bsize, d, r),
                    "-b", str(bsize),
                    "-d", str(d),
                    "-r", str(r),
                    "-n", str(n),
                    "-m", str(n)
                ]

                subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                pbar.update(1)
