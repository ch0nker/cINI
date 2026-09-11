add_rules("mode.release", "mode.debug")

add_includedirs("include")

target("cini")
    set_kind("shared")
    
    add_files("src/**.c")
    add_headerfiles("include/**.h")

    set_prefixname("")
    set_basename("cini")

target("cini_test")
    add_deps("cini")

    set_kind("binary")
    
    set_default(false)
    
    add_files("test/**.c")
