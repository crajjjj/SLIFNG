set_xmakever("3.0.0") -- CommonLibSSE-NG v7 (alandtse) requires xmake 3.x

-- Globals
PROJECT_NAME = "SLIFNG"

-- Project
set_project(PROJECT_NAME)
set_version("0.4.4")
set_languages("cxx23")
set_license("gplv3")
set_warnings("allextra")

-- Dependencies & Includes
-- CommonLibSSE-NG (alandtse fork), pinned as a submodule at v8.0.1.
-- One DLL covers SE / AE / VR. After cloning:
--   git submodule update --init --recursive
-- (the --recursive matters: CommonLib pulls openvr itself, and without it the
--  build dies on a missing openvr.h)
includes("lib/commonlibsse-ng")

-- policies
set_policy("package.requires_lock", true)

-- rules
add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    add_defines("DEBUG")
    set_optimize("none")
    set_runtimes("MTd")
elseif is_mode("release") then
    add_defines("NDEBUG")
    set_optimize("fastest")
    set_symbols("debug")
    set_runtimes("MT")
end

add_defines("_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")

-- Target
target(PROJECT_NAME)
    set_kind("shared")

    -- CommonLibSSE-NG
    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = PROJECT_NAME,
        author = "crajjjj",
        description = "SLIF NG native engine (SLIF-compatible inflation framework)."
    })

    -- Keep the v7 rule's auto-install out of the live MO2 instance (see BF NG).
    on_config(function (target)
        target:set("installdir", path.join(target:autogendir(), "install"))
    end)

    -- Source files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    -- Exports
    add_ldflags("/DEF:exports.def", { force = true })

    -- flags
    add_cxxflags(
        "cl::/diagnostics:caret",
        "cl::/wd4200",
        "cl::/wd4201",
        "cl::/Zc:preprocessor",
        "cl::/utf-8"
    )

    if is_mode("debug") then
        add_cxxflags("cl::/bigobj")
    end

    -- Post Build: stage into dist/ like BF NG
    after_build(function (target)
        local plugin_folder = path.join(os.projectdir(), "dist", "Core", "skse", "plugins")
        if not os.isdir(plugin_folder) then
            os.mkdir(plugin_folder)
        end
        os.cp(target:targetfile(), plugin_folder)
        if is_mode("debug") then
            local pdb = target:symbolfile()
            if pdb then
                os.cp(pdb, plugin_folder)
            end
        end
    end)
target_end()
