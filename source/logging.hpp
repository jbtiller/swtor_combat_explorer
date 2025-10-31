#pragma once

#include <boost/log/trivial.hpp>

#define BLT(lev) BOOST_LOG_TRIVIAL(lev)
#define BLT_LINE(lev, line) BOOST_LOG_TRIVIAL(lev) << "Line " << line << ": "

auto set_log_filter() -> void;
