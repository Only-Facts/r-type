#ifndef CLI_HPP
#define CLI_HPP

#include <cstddef>
#include <iostream>
#include <ostream>

inline void error(const char* msg);
inline void warn(const char* msg);
inline void info(const char* msg);
inline void debug(const char* msg);
 
std::size_t print_usage(std::ostream& os = std::cout);
std::size_t bad_usage(std::ostream& os = std::cerr);

#endif // !CLI_HPP
