import os
import subprocess

exe = os.path.expanduser("~/docacxx/build/linux/arm64-v8a/release/compress_bench")
input_file =os.path.expanduser("~/workload/linux-5.10.100.tar")
output_dir = os.path.expanduser("~/docacxx/bench_result")


def output_filename(bz):
    return os.path.join(output_dir, "{}".format(bz))


kb = 1024
bsize = 4 * kb
while bsize <= 256 * kb:
    command = [
        exe,
        "-a", "03:00.1",
        "-b", str(bsize),
        "-i", input_file,
        "-o", output_filename(str(bsize))
    ]
    subprocess.run(command)
    bsize *= 2
    print("------------------")