decl str include_dir %sh{echo `dirname $kak_source`/lib}

hook global WinSetOption filetype=(c|cpp) %{
    clang-enable-autocomplete
    clang-enable-diagnostics
    set window clang_options %sh{echo "-I"$kak_opt_include_dir}
    alias buffer p clang-parse
}
