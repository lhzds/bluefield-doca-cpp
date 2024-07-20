target("dpu_server")
    set_kind("binary")
    add_files("comch/dpu_server.cpp")
    add_deps("docacxx")
    
target("host_client")
    set_kind("binary")
    add_files("comch/host_client.cpp")
    add_deps("docacxx")
