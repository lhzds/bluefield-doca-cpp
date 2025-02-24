add_requires("nlohmann_json", {alias = "json", private = true})
add_requires("argparse", {private = true})
add_packages("json", "argparse")

add_includedirs("util")
add_deps("docacxx")

-- add_requires("lz4", "isal")
-- target("bench_compress")
--     set_kind("binary")
--     add_files("compress/*.cpp")
--     add_packages("lz4", "isal")

target("bench_ec")
    set_kind("binary")
    add_files("ec/*.cpp")

target("bench_comch_host")
    set_kind("binary")
    add_files("comch/host.cpp")

target("bench_comch_dpu")
    set_kind("binary")
    add_files("comch/dpu.cpp")
    