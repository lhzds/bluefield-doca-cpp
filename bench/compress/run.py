import os
import subprocess
from tqdm import tqdm

exe = os.path.expanduser("~/repos/docacxx/build/linux/x86_64/release/compress_bench")
input_file =os.path.expanduser("~/workload/linux-5.10.100.tar")
output_dir = os.path.expanduser("~/docacxx/bench_result/comp")
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# algs
algs = ["deflate", "lz4"]

# dev_pci
dev_pci = "cb:00.0"

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


with tqdm(total=len(ns)*len(bsizes)*len(algs), desc="compress bench") as pbar:
    for alg in algs:
        for bsize in bsizes:
            for n in ns:
                command = [
                    exe,
                    "-a", dev_pci,
                    "-i", input_file,
                    "-o", get_output_file(alg, n, bsize),
                    "-b", str(bsize),
                    "-r", alg,
                    "-n", str(n)
                ]
                subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                pbar.update(1)
