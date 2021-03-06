macro (DMA_SuppressClangTidy_START)
    set(RESTORE_CLANG_TIDY OFF)
    set(CLANG_TIDY_CACHED_VAL "")

    if( DMA_CLANG_TIDY_BUILD )
        set(RESTORE_CLANG_TIDY ON)
        set(CLANG_TIDY_CACHED_VAL CMAKE_CXX_CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY "")
    endif()     
endmacro(DMA_SuppressClangTidy_START)

macro (DMA_SuppressClangTidy_END)
    if( RESTORE_CLANG_TIDY )
        set(CMAKE_CXX_CLANG_TIDY CLANG_TIDY_CACHED_VAL)
        set(CLANG_TIDY_CACHED_VAL "")
        set(RESTORE_CLANG_TIDY OFF)
    endif()
endmacro(DMA_SuppressClangTidy_END)