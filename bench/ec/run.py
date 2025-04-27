import os
import subprocess
from tqdm import tqdm

script_path = os.path.abspath(__file__)
proj_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_path)))

exe = os.path.expanduser(os.path.join(proj_dir, "build/linux/x86_64/release/bench_ec"))
output_dir = os.path.expanduser(os.path.join(proj_dir, "bench_result/ec"))
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

print(exe)
print(output_dir)

# dev_pci
dev_pci = "b1:00.0"

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
