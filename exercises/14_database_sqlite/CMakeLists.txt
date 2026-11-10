include(FetchContent)

# SQLite amalgamation (sqlite3.c + sqlite3.h), pinned by URL and SHA3-256.
FetchContent_Declare(sqlite3_src
  URL https://www.sqlite.org/2024/sqlite-amalgamation-3470200.zip
  URL_HASH SHA3_256=0e4df2263ef8169f9396b9535d9654d8d64ae8eb1a339ba06c7b12a72e6fb020
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3_src)

add_library(sqlite3_amalgamation STATIC "${sqlite3_src_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3_amalgamation PUBLIC "${sqlite3_src_SOURCE_DIR}")
target_compile_definitions(sqlite3_amalgamation PRIVATE
  SQLITE_DQS=0                      # double-quoted strings are identifiers, never literals
  SQLITE_DEFAULT_FOREIGN_KEYS=1
  SQLITE_OMIT_LOAD_EXTENSION)
if(MSVC)
  target_compile_options(sqlite3_amalgamation PRIVATE /W0)
else()
  target_compile_options(sqlite3_amalgamation PRIVATE -w)
endif()

cppref_exercise(ex14_database_sqlite LIBRARIES sqlite3_amalgamation)
