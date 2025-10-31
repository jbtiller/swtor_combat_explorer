#include <cstdlib>
#include <map>
#include <string>

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include "logging.hpp"

auto set_log_filter() -> void { // 
    if (const char* bl_level = std::getenv("BL_LEVEL")) {
        std::map<std::string,boost::log::trivial::severity_level> sevs {
            {"trace",   boost::log::trivial::trace},
            {"debug",   boost::log::trivial::debug},
            {"info",    boost::log::trivial::info},
            {"warning", boost::log::trivial::warning},
            {"error",   boost::log::trivial::error},
            {"fatal",   boost::log::trivial::fatal},
        };
        if (auto levit = sevs.find(bl_level); levit != sevs.end()) {
            auto lev = levit->second;
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= lev
            );
        }
    }
}
