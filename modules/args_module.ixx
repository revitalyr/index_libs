export module args_module;
import "args.hxx";

export namespace args {
    using args::ArgumentParser;
    using args::Positional;
    using args::ValueReader;

    template<typename T, class R>
    class ValueFlag;
}
