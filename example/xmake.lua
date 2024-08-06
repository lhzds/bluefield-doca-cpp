add_requires("lz4")

target("compressor")
    set_kind("binary")
    add_files("compressor.cpp")
    add_deps("docacxx")
    add_packages("lz4")