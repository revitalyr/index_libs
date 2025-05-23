module;

#include "sqlite_orm/sqlite_orm.h"

export module sqlite_orm_module;

export namespace sqlite_orm {
    using sqlite_orm::length;
    using sqlite_orm::make_storage;
    using sqlite_orm::where;
    using sqlite_orm::make_index;
    using sqlite_orm::indexed_column;
    using sqlite_orm::make_unique_index;
    using sqlite_orm::make_column;
    using sqlite_orm::make_table;
    using sqlite_orm::c;
    using sqlite_orm::like;
    using sqlite_orm::columns;
    using sqlite_orm::select;

    using sqlite_orm::make_error_code;

    using sqlite_orm::internal::greater_than_t;
    using sqlite_orm::internal::arithmetic_t;
    using sqlite_orm::internal::is_operator_argument;
    using sqlite_orm::internal::unwrap_expression_t;
    using sqlite_orm::internal::column_t;
    using sqlite_orm::internal::storage_t;

    export namespace internal {
        using sqlite_orm::internal::expression_t;
    }
}
