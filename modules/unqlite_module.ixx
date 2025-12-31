module;

#include <unqlite.h>

export module unqlite_module;

export {
    using ::unqlite;
    using ::unqlite_int64;
    using ::unqlite_open;
    using ::unqlite_close;
    using ::unqlite_kv_store;
    using ::unqlite_kv_append;
    using ::unqlite_kv_cursor;
    using ::unqlite_kv_cursor_init;
    using ::unqlite_kv_cursor_release;
    using ::unqlite_kv_cursor_first_entry;
    using ::unqlite_kv_cursor_valid_entry;
    using ::unqlite_kv_cursor_next_entry;
    using ::unqlite_kv_cursor_key;
    using ::unqlite_kv_cursor_data;
}

// Export constants as constexpr since macros aren't exported
export namespace unqlite_flags {
    constexpr int OK = UNQLITE_OK;
    constexpr int OPEN_CREATE = UNQLITE_OPEN_CREATE;
    constexpr int OPEN_READONLY = UNQLITE_OPEN_READONLY;
    constexpr int OPEN_READWRITE = UNQLITE_OPEN_READWRITE;
    constexpr int OPEN_OMIT_JOURNALING = UNQLITE_OPEN_OMIT_JOURNALING;
}