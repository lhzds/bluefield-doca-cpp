add_rules("mode.debug", "mode.release")
set_languages("c++20")

if is_mode("debug") then
    set_symbols("debug")
    add_defines("DEBUG")
    set_optimize("none")
end

if is_mode("release") then
    set_symbols("hidden")
    add_defines("NDEBUG")
    set_optimize("fastest")
end

add_requires("fmt")
add_requires("doca-common", {system = true})
add_requires("doca-comch", {system = true})
add_requires("doca-compress", {system = true})
add_defines("DOCA_ALLOW_EXPERIMENTAL_API")

add_includedirs("include", {public = true})

target("docacxx")
    set_kind("static")
    add_files("src/*.cpp")
    add_packages("doca-common", "doca-comch", "doca-compress", "fmt", {public = true})

includes("example/xmake.lua")
