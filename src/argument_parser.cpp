#include "argument_parser.h"

#include <iostream>

struct OptionData {
    Options flag;
    const char *short_opt;
    const char *long_opt;
    const char *short_val;
    const char *long_val;
    const char *help;
};

static const OptionData option_table[] = {
    {Options::THREADS,        "t", "threads",     "N",    "THREADS",        "Number of worker threads"},
    {Options::QUEUE_CAPACITY, "q", "queue",       "N",    "QUEUE_CAPACITY", "Task queue capacity"},
    {Options::BLOCK_SIZE,     "s", "block-size",  "N",    "BLOCK_SIZE",     "Block size"},
    {Options::BLOCK_COUNT,    "c", "block-count", "N",    "BLOCK_COUNT",    "Block count"},
    {Options::BUCKET_COUNT,   "b", "buckets",     "N",    "BUCKET_COUNT",   "Hashmap bucket count"},
    {Options::NAME,           "n", "shm-name",    "NAME", "SHM_NAME",       "Shared memory name"},
    {Options::OPERATIONS,     "n", "operations",  "N",    "OPERATIONS",     "Number of operations to do per thread"},
};

void print_usage(const char *name, Options options) {
    std::cout << "usage: " << name << " [-h]";

    for (const auto &opt : option_table) {
        if (!has_option(options, opt.flag))
            continue;
        std::cout << " [-" << opt.short_opt;
        if (opt.long_val)
            std::cout << " " << opt.long_val;
        std::cout << "]";
    }
    std::cout << "\n";
}

void print_help(const char *name, Options options) {
    constexpr size_t help_label_size = 10;
    constexpr size_t min_space = 3;

    print_usage(name, options);

    size_t max_len = help_label_size;
    for (const auto& opt : option_table) {
        if (!has_option(options, opt.flag))
            continue;
        std::string label = std::string("-") + opt.short_opt + ", --" + opt.long_opt + " " + opt.short_val;
        max_len = std::max(max_len, label.size());
    }

    std::cout << "\n" << "options:\n";
    for (const auto& opt : option_table) {
        if (!has_option(options, opt.flag))
            continue;
        std::string label = std::string("-") + opt.short_opt + ", --" + opt.long_opt + " " + opt.short_val;
        std::cout << "  " << label;

        size_t pad = max_len - label.size();
        for (size_t i = 0; i < pad + min_space; ++i)
            std::cout << ' ';

        std::cout << opt.help << "\n";
    }

    std::cout << "  -h, --help";

    for (size_t i = 0; i < max_len - help_label_size + min_space; ++i)
        std::cout << ' ';

    std::cout << "Show this help\n";
}

const OptionData* find_option(const std::string& arg) {
    for (const auto& opt : option_table) {
        if (opt.short_opt != nullptr && arg == "-" + std::string(opt.short_opt))
            return &opt;
        if (opt.long_opt != nullptr && arg == "--" + std::string(opt.long_opt))
            return &opt;
    }
    return nullptr;
}

void parse_args(int argc, char **argv, Config &cfg, Options options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help(argv[0], options);
            std::exit(0);
        }

        const OptionData*opt = find_option(arg);

        if (!opt || !has_option(options, opt->flag)) {
            std::cerr << "unknown option: " << arg << "\n";
            print_usage(argv[0], options);
            std::exit(1);
        }

        if (i + 1 >= argc) {
            std::cerr << "missing value for " << arg << "\n";
            std::exit(1);
        }

        const char *value = argv[++i];

        switch (opt->flag) {
        case Options::THREADS:
            cfg.n_threads = std::stoul(value);
            break;
        case Options::QUEUE_CAPACITY:
            cfg.queue_cap = std::stoul(value);
            break;
        case Options::BLOCK_SIZE:
            cfg.block_size = std::stoul(value);
            break;
        case Options::BLOCK_COUNT:
            cfg.block_count = std::stoul(value);
            break;
        case Options::BUCKET_COUNT:
            cfg.bucket_count = std::stoul(value);
            break;
        case Options::NAME:
            cfg.shm_name = value;
            break;
        default:
            break;
        }
    }
}
