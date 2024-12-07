add_rules("mode.debug", "mode.release")
set_languages("c++20")

add_requires("fmt 9.1.0")
add_packages("fmt 9.1.0", {private = true})

add_requires("doca-common", {system = true})
add_requires("doca-comch", {system = true})
add_requires("doca-compress", {system = true})
add_requires("doca-dma-lib", {system = true})
add_requires("doca-erasure-coding", {system = true, alias="doca-ec"})
add_defines("DOCA_ALLOW_EXPERIMENTAL_API")

add_includedirs("include", {public = true})

target("docacxx")
    set_kind("static")
    add_files("src/*.cpp")
    add_packages("doca-common", "doca-comch", "doca-compress", "doca-ec", "doca-dma-lib", {public = true})

-- includes("bench/xmake.lua")
