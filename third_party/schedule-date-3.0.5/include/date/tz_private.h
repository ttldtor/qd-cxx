#ifndef TZ_PRIVATE_H
#define TZ_PRIVATE_H

// The MIT License (MIT)
//
// Copyright (c) 2015, 2016 Howard Hinnant
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Our apologies.  When the previous paragraph was written, lowercase had not yet
// been invented (that would involve another several millennia of evolution).
// We did not mean to shout.

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
#include "tz.h"
#else
#include "date.h"
#include <vector>
#endif

namespace org_ttldtor_qdcxx_date_305_v1
{

namespace detail
{

#if !USE_OS_TZDB

enum class tz {utc, local, standard};

//forward declare to avoid warnings in gcc 6.2
class MonthDayTime;
std::istream& operator>>(std::istream& is, MonthDayTime& x);
std::ostream& operator<<(std::ostream& os, const MonthDayTime& x);


class MonthDayTime
{
private:
    struct pair
    {
#if defined(_MSC_VER) && (_MSC_VER < 1900)
        pair() : month_day_(org_ttldtor_qdcxx_date_305_v1::jan / 1), weekday_(0U) {}

        pair(const org_ttldtor_qdcxx_date_305_v1::month_day& month_day, const org_ttldtor_qdcxx_date_305_v1::weekday& weekday)
            : month_day_(month_day), weekday_(weekday) {}
#endif

        org_ttldtor_qdcxx_date_305_v1::month_day month_day_;
        org_ttldtor_qdcxx_date_305_v1::weekday   weekday_;
    };

    enum Type {month_day, month_last_dow, lteq, gteq};

    Type                         type_{month_day};

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
    union U
#else
    struct U
#endif
    {
        org_ttldtor_qdcxx_date_305_v1::month_day          month_day_;
        org_ttldtor_qdcxx_date_305_v1::month_weekday_last month_weekday_last_;
        pair                     month_day_weekday_;

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
        U() : month_day_{org_ttldtor_qdcxx_date_305_v1::jan/1} {}
#else
        U() :
            month_day_(org_ttldtor_qdcxx_date_305_v1::jan/1),
            month_weekday_last_(org_ttldtor_qdcxx_date_305_v1::month(0U), org_ttldtor_qdcxx_date_305_v1::weekday_last(org_ttldtor_qdcxx_date_305_v1::weekday(0U)))
        {}

#endif // !defined(_MSC_VER) || (_MSC_VER >= 1900)

        U& operator=(const org_ttldtor_qdcxx_date_305_v1::month_day& x);
        U& operator=(const org_ttldtor_qdcxx_date_305_v1::month_weekday_last& x);
        U& operator=(const pair& x);
    } u;

    std::chrono::hours           h_{0};
    std::chrono::minutes         m_{0};
    std::chrono::seconds         s_{0};
    tz                           zone_{tz::local};

public:
    MonthDayTime() = default;
    MonthDayTime(local_seconds tp, tz timezone);
    MonthDayTime(const org_ttldtor_qdcxx_date_305_v1::month_day& md, tz timezone);

    org_ttldtor_qdcxx_date_305_v1::day day() const;
    org_ttldtor_qdcxx_date_305_v1::month month() const;
    tz zone() const {return zone_;}

    void canonicalize(org_ttldtor_qdcxx_date_305_v1::year y);

    sys_seconds
       to_sys(org_ttldtor_qdcxx_date_305_v1::year y, std::chrono::seconds offset, std::chrono::seconds save) const;
    sys_days to_sys_days(org_ttldtor_qdcxx_date_305_v1::year y) const;

    sys_seconds to_time_point(org_ttldtor_qdcxx_date_305_v1::year y) const;
    int compare(org_ttldtor_qdcxx_date_305_v1::year y, const MonthDayTime& x, org_ttldtor_qdcxx_date_305_v1::year yx,
                std::chrono::seconds offset, std::chrono::minutes prev_save) const;

    friend std::istream& operator>>(std::istream& is, MonthDayTime& x);
    friend std::ostream& operator<<(std::ostream& os, const MonthDayTime& x);
};

// A Rule specifies one or more set of datetimes without using an offset.
// Multiple dates are specified with multiple years.  The years in effect
// go from starting_year_ to ending_year_, inclusive.  starting_year_ <=
// ending_year_. save_ is in effect for times from the specified time
// onward, including the specified time. When the specified time is
// local, it uses the save_ from the chronologically previous Rule, or if
// there is none, 0.

//forward declare to avoid warnings in gcc 6.2
class Rule;
bool operator==(const Rule& x, const Rule& y);
bool operator<(const Rule& x, const Rule& y);
bool operator==(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y);
bool operator<(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y);
bool operator==(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y);
bool operator<(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y);
bool operator==(const Rule& x, const std::string& y);
bool operator<(const Rule& x, const std::string& y);
bool operator==(const std::string& x, const Rule& y);
bool operator<(const std::string& x, const Rule& y);
std::ostream& operator<<(std::ostream& os, const Rule& r);

class Rule
{
private:
    std::string          name_;
    org_ttldtor_qdcxx_date_305_v1::year           starting_year_{0};
    org_ttldtor_qdcxx_date_305_v1::year           ending_year_{0};
    MonthDayTime         starting_at_;
    std::chrono::minutes save_{0};
    std::string          abbrev_;

public:
    Rule() = default;
    explicit Rule(const std::string& s);
    Rule(const Rule& r, org_ttldtor_qdcxx_date_305_v1::year starting_year, org_ttldtor_qdcxx_date_305_v1::year ending_year);

    const std::string& name() const {return name_;}
    const std::string& abbrev() const {return abbrev_;}

    const MonthDayTime&         mdt()           const {return starting_at_;}
    const org_ttldtor_qdcxx_date_305_v1::year&           starting_year() const {return starting_year_;}
    const org_ttldtor_qdcxx_date_305_v1::year&           ending_year()   const {return ending_year_;}
    const std::chrono::minutes& save()          const {return save_;}

    static void split_overlaps(std::vector<Rule>& rules);

    friend bool operator==(const Rule& x, const Rule& y);
    friend bool operator<(const Rule& x, const Rule& y);
    friend bool operator==(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y);
    friend bool operator<(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y);
    friend bool operator==(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y);
    friend bool operator<(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y);
    friend bool operator==(const Rule& x, const std::string& y);
    friend bool operator<(const Rule& x, const std::string& y);
    friend bool operator==(const std::string& x, const Rule& y);
    friend bool operator<(const std::string& x, const Rule& y);

    friend std::ostream& operator<<(std::ostream& os, const Rule& r);

private:
    org_ttldtor_qdcxx_date_305_v1::day day() const;
    org_ttldtor_qdcxx_date_305_v1::month month() const;
    static void split_overlaps(std::vector<Rule>& rules, std::size_t i, std::size_t& e);
    static bool overlaps(const Rule& x, const Rule& y);
    static void split(std::vector<Rule>& rules, std::size_t i, std::size_t k,
                      std::size_t& e);
};

inline bool operator!=(const Rule& x, const Rule& y) {return !(x == y);}
inline bool operator> (const Rule& x, const Rule& y) {return   y < x;}
inline bool operator<=(const Rule& x, const Rule& y) {return !(y < x);}
inline bool operator>=(const Rule& x, const Rule& y) {return !(x < y);}

inline bool operator!=(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y) {return !(x == y);}
inline bool operator> (const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y) {return   y < x;}
inline bool operator<=(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y) {return !(y < x);}
inline bool operator>=(const Rule& x, const org_ttldtor_qdcxx_date_305_v1::year& y) {return !(x < y);}

inline bool operator!=(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y) {return !(x == y);}
inline bool operator> (const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y) {return   y < x;}
inline bool operator<=(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y) {return !(y < x);}
inline bool operator>=(const org_ttldtor_qdcxx_date_305_v1::year& x, const Rule& y) {return !(x < y);}

inline bool operator!=(const Rule& x, const std::string& y) {return !(x == y);}
inline bool operator> (const Rule& x, const std::string& y) {return   y < x;}
inline bool operator<=(const Rule& x, const std::string& y) {return !(y < x);}
inline bool operator>=(const Rule& x, const std::string& y) {return !(x < y);}

inline bool operator!=(const std::string& x, const Rule& y) {return !(x == y);}
inline bool operator> (const std::string& x, const Rule& y) {return   y < x;}
inline bool operator<=(const std::string& x, const Rule& y) {return !(y < x);}
inline bool operator>=(const std::string& x, const Rule& y) {return !(x < y);}

struct zonelet
{
    enum tag {has_rule, has_save, is_empty};

    std::chrono::seconds gmtoff_;
    tag tag_ = has_rule;

#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
    union U
#else
    struct U
#endif
    {
        std::string          rule_;
        std::chrono::minutes save_;

        ~U() {}
        U() {}
        U(const U&) {}
        U& operator=(const U&) = delete;
    } u;

    std::string                        format_;
    org_ttldtor_qdcxx_date_305_v1::year                         until_year_{0};
    MonthDayTime                       until_date_;
    sys_seconds                        until_utc_;
    local_seconds                      until_std_;
    local_seconds                      until_loc_;
    std::chrono::minutes               initial_save_{0};
    std::string                        initial_abbrev_;
    std::pair<const Rule*, org_ttldtor_qdcxx_date_305_v1::year> first_rule_{nullptr, org_ttldtor_qdcxx_date_305_v1::year::min()};
    std::pair<const Rule*, org_ttldtor_qdcxx_date_305_v1::year> last_rule_{nullptr, org_ttldtor_qdcxx_date_305_v1::year::max()};

    ~zonelet();
    zonelet();
    zonelet(const zonelet& i);
    zonelet& operator=(const zonelet&) = delete;
};

#else  // USE_OS_TZDB

struct ttinfo
{
    std::int32_t  tt_gmtoff;
    unsigned char tt_isdst;
    unsigned char tt_abbrind;
    unsigned char pad[2];
};

static_assert(sizeof(ttinfo) == 8, "");

struct expanded_ttinfo
{
    std::chrono::seconds offset;
    std::string          abbrev;
    bool                 is_dst;
};

struct transition
{
    sys_seconds            timepoint;
    const expanded_ttinfo* info;

    transition(sys_seconds tp, const expanded_ttinfo* i = nullptr)
        : timepoint(tp)
        , info(i)
        {}

    friend
    std::ostream&
    operator<<(std::ostream& os, const transition& t)
    {
        org_ttldtor_qdcxx_date_305_v1::operator<<(os, t.timepoint) << "Z ";
        if (t.info->offset >= std::chrono::seconds{0})
            os << '+';
        os << make_time(t.info->offset);
        if (t.info->is_dst > 0)
            os << " daylight ";
        else
            os << " standard ";
        os << t.info->abbrev;
        return os;
    }
};

#endif  // USE_OS_TZDB

}  // namespace detail

}  // namespace org_ttldtor_qdcxx_date_305_v1

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#include "tz.h"
#endif

#endif  // TZ_PRIVATE_H
