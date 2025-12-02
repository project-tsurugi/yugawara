#include "compare_value.h"

#include <cmath>

#include <decimal.hh>

#include <takatori/value/dispatch.h>

#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

namespace tvalue = ::takatori::value;

using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    [[nodiscard]] compare_result compare(tvalue::data const& left, tvalue::data const& right) const {
        return tvalue::dispatch(*this, left, right);
    }

    compare_result operator()(tvalue::data const&, tvalue::data const&) const {
        return compare_result::undefined;
    }

    compare_result operator()(tvalue::boolean const& left, tvalue::data const& right) const {
        if (right.kind() != tvalue::boolean::tag) {
            return compare_result::undefined;
        }
        auto v_left = left.get();
        auto v_right = unsafe_downcast<tvalue::boolean const&>(right).get();
        return compare_values(v_left, v_right);
    }

    compare_result operator()(tvalue::int4 const& left, tvalue::data const& right) const {
        return compare_exact_number(left.get(), right);
    }

    compare_result operator()(tvalue::int8 const& left, tvalue::data const& right) const {
        return compare_exact_number(left.get(), right);
    }

    compare_result operator()(tvalue::decimal const& left, tvalue::data const& right) const {
        ::decimal::Decimal v_left {
            unsafe_downcast<tvalue::decimal const&>(left).get()
        };
        return compare_exact_number(v_left, right);
    }

    compare_result operator()(tvalue::float4 const& left, tvalue::data const& right) const {
        return compare_approx_number(left.get(), right);
    }

    compare_result operator()(tvalue::float8 const& left, tvalue::data const& right) const {
        return compare_approx_number(left.get(), right);
    }

    compare_result operator()(tvalue::character const& left, tvalue::data const& right) const {
        if (right.kind() == tvalue::character::tag) {
            auto v_right = unsafe_downcast<tvalue::character>(right).get();
            return compare_values(left.get(), v_right);
        }
        return compare_result::undefined;
    }

    compare_result operator()(tvalue::octet const& left, tvalue::data const& right) const {
        if (right.kind() == tvalue::octet::tag) {
            auto v_right = unsafe_downcast<tvalue::octet>(right).get();
            return compare_values(left.get(), v_right);
        }
        return compare_result::undefined;
    }

    compare_result operator()(tvalue::date const& left, tvalue::data const& right) const {
        if (right.kind() == tvalue::date::tag) {
            auto v_right = unsafe_downcast<tvalue::date>(right).get();
            return compare_values(left.get().days_since_epoch(), v_right.days_since_epoch());
        }
        return compare_result::undefined;
    }

    compare_result operator()(tvalue::time_of_day const& left, tvalue::data const& right) const {
        if (right.kind() == tvalue::time_of_day::tag) {
            auto v_left = left.get();
            auto v_right = unsafe_downcast<tvalue::time_of_day>(right).get();

            if (auto r = compare_values(v_left.second_of_day(), v_right.second_of_day()); r != compare_result::equal) {
                return r;
            }
            return compare_values(v_left.subsecond(), v_right.subsecond());
        }
        return compare_result::undefined;
    }

    compare_result operator()(tvalue::time_point const& left, tvalue::data const& right) const {
        if (right.kind() == tvalue::time_point::tag) {
            auto v_left = left.get();
            auto v_right = unsafe_downcast<tvalue::time_point>(right).get();

            if (auto r = compare_values(v_left.seconds_since_epoch(), v_right.seconds_since_epoch());
                    r != compare_result::equal) {
                return r;
            }
            return compare_values(v_left.subsecond(), v_right.subsecond());
        }
        return compare_result::undefined;
    }

private:
    template<class T>
    static compare_result compare_generic(T const& left, tvalue::data const& right) {
        using left_type = T;
        if (right.kind() != left_type::tag) {
            return compare_result::undefined;
        }
        auto&& v_right = unsafe_downcast<left_type>(right).get();
        return compare_values(left.get(), v_right);
    }

    template<class T>
    static compare_result compare_exact_number(T const& v_left, tvalue::data const& right) {
        if (right.kind() == tvalue::int4::tag) {
            auto v_right = unsafe_downcast<tvalue::int4>(right).get();
            return compare_values(v_left, v_right);
        }
        if (right.kind() == tvalue::int8::tag) {
            auto v_right = unsafe_downcast<tvalue::int8>(right).get();
            return compare_values(v_left, v_right);
        }
        if (right.kind() == tvalue::decimal::tag) {;
            ::decimal::Decimal v_right {
                    unsafe_downcast<tvalue::decimal>(right).get()
            };
            return compare_values(v_left, v_right);
        }

        return compare_result::undefined;
    }

    template<class T, class U>
    static compare_result compare_values(T const& left, U const& right) {
        if (left == right) {
            return compare_result::equal;
        }
        if (left < right) {
            return compare_result::less;
        }
        return compare_result::greater;
    }

    template<class T, class U>
    static compare_result compare_approx_values(T left, U right) {
        // check comparability
        auto c_left = std::fpclassify(left);
        auto c_right = std::fpclassify(right);
        if (c_left == FP_NAN || c_right == FP_NAN) {
            return compare_result::undefined;
        }
        if (c_left == FP_INFINITE && c_right == FP_INFINITE && std::signbit(left) == std::signbit(right)) {
            return compare_result::undefined;
        }
        if (left == right) {
            return compare_result::equal;
        }
        if (left < right) {
            return compare_result::less;
        }
        return compare_result::greater;
    }

    static compare_result compare_approx_number(double v_left, tvalue::data const& right) {
        if (right.kind() == tvalue::float4::tag) {
            auto v_right = unsafe_downcast<tvalue::float4>(right).get();
            return compare_approx_values(v_left, v_right);
        }
        if (right.kind() == tvalue::float8::tag) {
            auto v_right = unsafe_downcast<tvalue::float8>(right).get();
            return compare_approx_values(v_left, v_right);
        }
        return compare_result::undefined;
    }
};

} // namespace

compare_result compare(tvalue::data const& left, tvalue::data const& right) {
    engine e {};
    return e.compare(left, right);
}

} // namespace yugawara::analyzer::details
