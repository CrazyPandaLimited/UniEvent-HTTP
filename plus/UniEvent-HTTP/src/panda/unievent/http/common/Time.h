#pragma once

#include <panda/time.h>

namespace panda { namespace unievent { namespace http {

const char month_snames[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const char day_snames[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

// adapted from apr_rfc822_date, https://apr.apache.org 
string rfc822_date(const time::datetime& t) {
    size_t length = sizeof("Mon, 20 Jan 2020 20:20:20 GMT") - 1;
    string result(length);
    result.length(length);
    char *date_str = result.buf();

    const char* s = &day_snames[t.wday][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ',';
    *date_str++ = ' ';
    *date_str++ = t.mday / 10 + '0';
    *date_str++ = t.mday % 10 + '0';
    *date_str++ = ' ';
    s = &month_snames[t.mon][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';

    *date_str++ = t.year / 1000 + '0';
    *date_str++ = t.year % 1000 / 100 + '0';
    *date_str++ = t.year % 100 / 10 + '0';
    *date_str++ = t.year % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = t.hour / 10 + '0';
    *date_str++ = t.hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = t.min / 10 + '0';
    *date_str++ = t.min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = t.sec / 10 + '0';
    *date_str++ = t.sec % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = 'G';
    *date_str++ = 'M';
    *date_str++ = 'T';

    return result;
}

}}} // namespace panda::unievent::http
