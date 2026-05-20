proc fsx_script_dir {} {
    return [file dirname [file normalize [info script]]]
}

proc fsx_root_dir {} {
    return [file dirname [fsx_script_dir]]
}

proc fsx_variants_ini {} {
    return [file join [fsx_root_dir] src variants.ini]
}

proc fsx_default_engine {} {
    return [file join [fsx_root_dir] src stockfish]
}
