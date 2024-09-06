import os
import subprocess
from tqdm import tqdm

host_exe = os.path.expanduser("~/docacxx/build/linux/arm64-v8a/release/comch_bench_host")
dpu_exe = os.path.expanduser("~/docacxx/build/linux/arm64-v8a/release/comch_bench_dpu")

output_dir = os.path.expanduser("~/docacxx/bench_result/comch")
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# for dpu
dpu_dev_pci = "03:00.1"
dpu_dev_rep_pci = "cb:00.1"

# for host
host_username = "olh"
host_dev_pci = "cb:00.1"
host_ip = "192.168.100.1"

# msg size
kb = 1024
mb = 1024 * 1024
msizes = []
t = 4 * kb
while t <= 1 * mb:
    msizes.append(t)
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

with tqdm(total=len(ns)*len(bsizes)*len(drs), desc="comch bench") as pbar:
    for msize in msizes:
        for n in ns:
            dpu_command = [
                dpu_exe,
                "-a", dpu_dev_pci,
                "-p", dpu_dev_rep_pci,
                "-o", get_output_file(n, msize),
                "-b", str(msize),
                "-s", str(n),
                "-r", str(n)
            ]
            host_command = ' '.join([
                host_exe,
                "-a", host_dev_pci,
                "-b", str(msize),
                "-s", str(n),
                "-r", str(n)
            ])
            
            ssh_command = [
                "ssh", "-o", "StrictHostKeyChecking=no", f"{host_username}@{host_ip}", f"{host_command}"
            ]
            
            p1 = subprocess.Popen(dpu_command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            p2 = subprocess.Popen(ssh_command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            p1.wait()
            p2.wait()
            pbar.update(1)
