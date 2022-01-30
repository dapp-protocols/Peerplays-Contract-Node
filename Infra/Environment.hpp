#pragma once

#include <set>
#include <typeinfo>

class Environment {
    thread_local static std::set<std::pair<std::size_t, void*>> local_environment;
};
