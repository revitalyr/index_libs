export module types;

import std;

export using String     = std::string;
export using Strings    = std::vector<String>;
export using StringSet  = std::unordered_set<String>;
export using StringList = std::forward_list<String>;
export using StrView    = std::string_view;
