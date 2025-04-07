set_version("1.0.0")
add_rules("mode.debug", "mode.release")
set_languages("c++20")

add_requires("fmt 9.1.0")
add_packages("fmt", {private = true})

add_requires("doca-common", {system = true})
add_requires("doca-comch", {system = true})
add_requires("doca-compress", {system = true})
add_requires("doca-dma", {system = true})
add_requires("doca-erasure-coding", {system = true, alias="doca-ec"})
add_defines("DOCA_ALLOW_EXPERIMENTAL_API")

add_includedirs("include", {public = true})

target("docacxx")
    set_kind("static")
    add_files("src/*.cpp")
    add_packages("doca-common", "doca-comch", "doca-compress", "doca-ec", "doca-dma", {public = true})
    add_rules("utils.install.cmake_importfiles")
    add_rules("utils.install.pkgconfig_importfiles")

-- includes("bench/xmake.lua")
