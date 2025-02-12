module;

export module sqlite_orm_module;

import "sqlite_orm/sqlite_orm.h";

namespace sqlite_orm {
    export using sqlite_orm::length;
    export using sqlite_orm::make_storage;
    export using sqlite_orm::where;
    export using sqlite_orm::make_index;
    export using sqlite_orm::indexed_column;
    export using sqlite_orm::make_unique_index;
    export using sqlite_orm::make_column;
    export using sqlite_orm::make_table;
    export using sqlite_orm::c;

    export using sqlite_orm::make_error_code;

    export using sqlite_orm::internal::greater_than_t;
    export using sqlite_orm::internal::arithmetic_t;
    export using sqlite_orm::internal::is_operator_argument;
    export using sqlite_orm::internal::unwrap_expression_t;
    export using sqlite_orm::internal::column_t;
}
