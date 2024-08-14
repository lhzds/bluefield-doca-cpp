import os
import subprocess

exe = os.path.expanduser("~/docacxx/build/linux/arm64-v8a/release/ec_bench")
output_file = os.path.expanduser("~/docacxx/bench_result/xxx")

kb = 1024
bsize = 4 * kb

while bsize <= 4 * kb:
    command = [
        exe,
        "-a", "03:00.1",
        "-o", output_file,
        "-b", str(bsize),
        "-d", "4",
        "-r", "2",
        "-n", "12"
    ]
    subprocess.run(command)
    bsize *= 2
    print("------------------")
