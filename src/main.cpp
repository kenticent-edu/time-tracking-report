#include "ThreadSafeQueue.h"
#include "time_measurement.h"

#include <boost/date_time/gregorian/parsers.hpp>
#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>

#include <array>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <unordered_map>

struct NameNMonth {
    std::string name;
    int month;
    int year;

    bool operator==(const NameNMonth &rhs) const
    {
        return name == rhs.name && month == rhs.month && year == rhs.year;
    }
};

namespace std {
    template<>
    struct hash<NameNMonth> {
        size_t operator()(const NameNMonth &name_n_month) const noexcept
        {
            size_t seed = 0;
            boost::hash_combine(seed, name_n_month.name);
            boost::hash_combine(seed, name_n_month.month);
            boost::hash_combine(seed, name_n_month.year);
            return seed;
        }
    };
}

using HoursByNameNMonth = std::unordered_map<NameNMonth, int>;

// Later this can be turned into a thread-safe singleton to not pass a "zoo" of
// parameters to the functions
struct Config {
    std::string blacklist;
    char separator;
    std::vector<std::string> input_files;
};

namespace {
    const std::array<std::string, 12> MONTH_BY_NUMBER = { "January", "February", "March", "April", "May", "June", "July",
                                                          "August", "September", "October", "November", "December" };

    // We can use std::pair<T, bool> alternatively or std::optional<T> in C++17
    const std::string POISON_PILL = "poison pill";

    Config parse_command_line(int ac, char **av);

    using ColIndices = std::tuple<int, int, int>;
    ColIndices parse_header(std::ifstream &ifs, char sep);

    void do_consumers_work(ThreadSafeQueue<std::string> &thread_safe_queue,
                           char sep,
                           int name_idx,
                           int date_idx,
                           int hours_idx,
                           HoursByNameNMonth &hours_by_name_n_month);
}

std::ostream &operator<<(std::ostream &os, const NameNMonth &name_n_month)
{
    os << name_n_month.name << ';' << MONTH_BY_NUMBER[name_n_month.month-1] << ' ' << name_n_month.year;
    return os;
}

int main(int argc, char **argv)
{
    Config config = parse_command_line(argc, argv);

    const std::string &blacklist = config.blacklist;
    std::set<std::string> banned_lines;
    if (!blacklist.empty()) {
        std::ifstream ifs(blacklist);
        if (!ifs) throw std::runtime_error("Couldn't open " + blacklist + " for reading");

        std::string line;
        while (getline(ifs, line)) {
            banned_lines.insert(line);
        }
    }

    char sep = config.separator;

    // TODO: Add multiple files support
    const std::string &input_file = config.input_files[0];

    std::ifstream ifs(input_file);
    if (!ifs) throw std::runtime_error("Couldn't open " + input_file + " for reading");

    int name_idx;
    int date_idx;
    int hours_idx;
    std::tie(name_idx, date_idx, hours_idx) = parse_header(ifs, sep);

    HoursByNameNMonth hours_by_name_n_month;
    ThreadSafeQueue<std::string> thread_safe_queue;

    auto start = get_current_time_fenced();

    // TODO: Add a thread pool
    std::thread t(do_consumers_work, std::ref(thread_safe_queue), sep, name_idx, date_idx, hours_idx, std::ref(hours_by_name_n_month));

    std::string line;
    while (std::getline(ifs, line)) {
        if (banned_lines.count(line)) {
            std::cerr << "Banned line found warning: " << line << '\n';
        } else {
            thread_safe_queue.push(line);
        }
    }
    thread_safe_queue.push(POISON_PILL);

    t.join();

    auto finish = get_current_time_fenced();
    auto total_time = finish - start;

    for (const auto &pair : hours_by_name_n_month) {
        std::cout << pair.first << ';' << pair.second << '\n';
    }

    std::cout << "Total time: " << to_us(total_time) << std::endl;

    return 0;
}

namespace {
    Config parse_command_line(int ac, char **av)
    {
        namespace po = boost::program_options;

        po::options_description generic("Generic options");
        // TODO: Add log file option
        generic.add_options()
                ("help,h", "produce help message")
                ("blacklist,b", po::value<std::string>()->default_value(""), "list of banned lines")
                ("separator,s", po::value<char>()->default_value(';'), "separator")
                ;

        po::options_description hidden("Hidden options");
        hidden.add_options()
                ("input-file", po::value<std::vector<std::string> >(), "input file")
                ;

        po::options_description cmdline_options;
        cmdline_options.add(generic).add(hidden);

        po::options_description visible("Allowed options");
        visible.add(generic);

        po::positional_options_description p;
        p.add("input-file", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(ac, av)
                          .options(cmdline_options).positional(p).run(), vm);
        vm.notify();

        if (vm.count("help")) {
            std::cout << visible << '\n';
            exit(0);
        }

        // Input file must be specified
        if (!vm.count("input-file")) {
            throw std::runtime_error("No input files");
        }

        return { vm["blacklist"].as<std::string>(),
                    vm["separator"].as<char>(),
                vm["input-file"].as<std::vector<std::string> >() };
    }

    ColIndices parse_header(std::ifstream &ifs, char sep)
    {
        std::string header;
        std::getline(ifs, header);

        std::stringstream ss(header);
        std::string col;
        int idx = 0;
        // TODO: Check if any of the indices are left uninitialized
        int name_idx = 0;
        int date_idx = 0;
        int hours_idx = 0;
        while (std::getline(ss, col, sep)) {
            if (col == "Name") {
                name_idx = idx;
            } else if (col == "date") {
                date_idx = idx;
            } else if (col == "logged hours") {
                hours_idx = idx;
            }
            ++idx;
        }

        return std::make_tuple(name_idx, date_idx, hours_idx);
    }

    void do_consumers_work(ThreadSafeQueue<std::string> &thread_safe_queue,
                           char sep,
                           int name_idx,
                           int date_idx,
                           int hours_idx,
                           HoursByNameNMonth &hours_by_name_n_month)
    {
        std::string name;
        std::string date_str;
        std::string hours;

        std::string line;
        while ((line = thread_safe_queue.wait_and_pop()) != POISON_PILL) {
            std::stringstream ss(line);

            int i = 0;
            for (; i < name_idx; ++i) {
                ss.ignore(std::numeric_limits<std::streamsize>::max(), sep);
            }
            std::getline(ss, name, sep);
            ++i;
            for (; i < date_idx; ++i) {
                ss.ignore(std::numeric_limits<std::streamsize>::max(), sep);
            }
            std::getline(ss, date_str, sep);
            ++i;
            for (; i < hours_idx; ++i) {
                ss.ignore(std::numeric_limits<std::streamsize>::max(), sep);
            }
            std::getline(ss, hours, sep);
            ++i;

            boost::gregorian::date date = boost::gregorian::from_simple_string(date_str);
            // Assuming the year is implicitly cast to int
            NameNMonth name_n_month = { name, date.month().as_number(), date.year() };
            hours_by_name_n_month[name_n_month] += std::stoi(hours);
        }
    }
}
