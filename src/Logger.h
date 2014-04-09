#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

#define HS_TIME boost::posix_time::to_simple_string(boost::posix_time::ptime(boost::posix_time::second_clock::local_time()).time_of_day()).substr(0,5)

#define HS_INFO HS_INFO_NOSPACE << " "
#define HS_ERROR HS_ERROR_NOSPACE << " "
#define HS_WARNING std::cout << "[" << HS_TIME << "][Warning] "

#define HS_INFO_NOSPACE std::cout << "[" << HS_TIME << "]"
#define HS_ERROR_NOSPACE std::cerr <<  "[" << HS_TIME << "][Error]"


#endif /* LOGGER_H_ */
