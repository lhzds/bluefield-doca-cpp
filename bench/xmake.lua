add_requires("nlohmann_json", {alias = "json", private = true})
add_requires("lz4", "isal", "argparse", {private = true})

add_includedirs("util")
add_deps("docacxx")
add_packages("json", "argparse")

target("compress_bench")
    set_kind("binary")
    add_files("compress/*.cpp")
    add_packages("lz4", "isal")

target("ec_bench")
    set_kind("binary")
    add_files("ec/*.cpp")

target("comch_bench_host")
    set_kind("binary")
    add_files("comch/comch_bench_host.cpp")

target("comch_bench_dpu")
    set_kind("binary")
    add_files("comch/comch_bench_dpu.cpp")

