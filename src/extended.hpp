// An x87 extended-precision number (64-bit mantissa) for the helpers that the
// reference computes on the x87 stack, where the intermediate results keep 64
// bits (IRR). Only finite numbers; the four operations round to nearest even
// like the x87 unit in its default mode. Internal to the VBRUN helpers.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace vbrun {

class Extended {
public:
    Extended() = default;

    static Extended from_double(const double value) {
        Extended result;
        if (value == 0.0) {
            return result;
        }
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        result.negative_ = (bits >> 63) != 0U;
        const int biased = static_cast<int>((bits >> 52) & 0x7FFU);
        std::uint64_t fraction = bits & 0xFFFFFFFFFFFFFULL;
        int exponent = 0;
        if (biased == 0) {  // a denormal
            exponent = -1022;
            while ((fraction & (1ULL << 52)) == 0U) {
                fraction <<= 1;
                --exponent;
            }
        } else {
            fraction |= 1ULL << 52;
            exponent = biased - 1023;
        }
        result.mantissa_ = fraction << 11;
        result.exponent_ = exponent;
        return result;
    }

    // Rounded to a Double, to nearest even.
    double to_double() const {
        if (mantissa_ == 0U) {
            return 0.0;
        }
        std::uint64_t kept = mantissa_ >> 11;
        const std::uint64_t low = mantissa_ & 0x7FFU;
        int exponent = exponent_;
        if (low > 0x400U || (low == 0x400U && (kept & 1U) != 0U)) {
            ++kept;
            if (kept == (1ULL << 53)) {
                kept >>= 1;
                ++exponent;
            }
        }
        const double magnitude =
            std::ldexp(static_cast<double>(kept), exponent - 52);
        return negative_ ? -magnitude : magnitude;
    }

    bool is_zero() const { return mantissa_ == 0U; }

    bool is_negative() const { return negative_ && mantissa_ != 0U; }

    static Extended from_int(const std::int64_t value) {
        Extended result;
        if (value == 0) {
            return result;
        }
        result.negative_ = value < 0;
        std::uint64_t magnitude = result.negative_
                                      ? 0ULL - static_cast<std::uint64_t>(value)
                                      : static_cast<std::uint64_t>(value);
        int exponent = 63;
        while ((magnitude >> 63) == 0U) {
            magnitude <<= 1;
            --exponent;
        }
        result.mantissa_ = magnitude;
        result.exponent_ = exponent;
        return result;
    }

    // The nearest integer (halves to even), for numbers below 2^62.
    std::int64_t round_to_int() const {
        if (mantissa_ == 0U || exponent_ < -1) {
            return 0;
        }
        // value = mantissa_ * 2^(exponent_ - 63)
        const int shift = 63 - exponent_;
        std::uint64_t whole = 0;
        if (shift <= 0) {
            whole = mantissa_ << (-shift);
        } else if (shift >= 64) {
            whole = 0;  // below one half (exponent_ == -1 is handled below)
        } else {
            whole = mantissa_ >> shift;
            const std::uint64_t rest = mantissa_ & ((1ULL << shift) - 1U);
            const std::uint64_t half = 1ULL << (shift - 1);
            if (rest > half || (rest == half && (whole & 1U) != 0U)) {
                ++whole;
            }
        }
        if (exponent_ == -1) {
            // between 0.5 and 1: above one half rounds to 1, exactly one half
            // rounds to 0
            whole = mantissa_ > (1ULL << 63) ? 1U : 0U;
        }
        const auto signed_whole = static_cast<std::int64_t>(whole);
        return negative_ ? -signed_whole : signed_whole;
    }

    // This number times 2^count.
    Extended scaled(const int count) const {
        Extended result = *this;
        if (result.mantissa_ != 0U) {
            result.exponent_ += count;
        }
        return result;
    }

    Extended negated() const {
        Extended result = *this;
        if (result.mantissa_ != 0U) {
            result.negative_ = !result.negative_;
        }
        return result;
    }

    Extended absolute() const {
        Extended result = *this;
        result.negative_ = false;
        return result;
    }

    // -1, 0 or 1.
    friend int compare(const Extended& a, const Extended& b) {
        if (a.mantissa_ == 0U && b.mantissa_ == 0U) {
            return 0;
        }
        if (a.mantissa_ == 0U) {
            return b.negative_ ? 1 : -1;
        }
        if (b.mantissa_ == 0U) {
            return a.negative_ ? -1 : 1;
        }
        if (a.negative_ != b.negative_) {
            return a.negative_ ? -1 : 1;
        }
        int order = 0;
        if (a.exponent_ != b.exponent_) {
            order = a.exponent_ < b.exponent_ ? -1 : 1;
        } else if (a.mantissa_ != b.mantissa_) {
            order = a.mantissa_ < b.mantissa_ ? -1 : 1;
        }
        return a.negative_ ? -order : order;
    }

    friend Extended operator+(const Extended& a, const Extended& b) {
        if (a.mantissa_ == 0U) {
            return b;
        }
        if (b.mantissa_ == 0U) {
            return a;
        }
        // x is the one of larger magnitude.
        const bool a_larger =
            a.exponent_ > b.exponent_ ||
            (a.exponent_ == b.exponent_ && a.mantissa_ >= b.mantissa_);
        const Extended& x = a_larger ? a : b;
        const Extended& y = a_larger ? b : a;
        const int shift = x.exponent_ - y.exponent_;
        Wide big{x.mantissa_, 0U};
        Wide lesser{y.mantissa_, 0U};
        bool sticky = false;
        shift_right(lesser, shift, sticky);
        int exponent = x.exponent_;
        Wide sum;
        if (x.negative_ == y.negative_) {
            sum = add(big, lesser);
            if (sum.high < big.high ||
                (sum.high == big.high && sum.low < big.low)) {
                // a carry out of the top bit
                const bool lost = (sum.low & 1U) != 0U;
                sum.low = (sum.low >> 1) | (sum.high << 63);
                sum.high = (sum.high >> 1) | (1ULL << 63);
                sticky = sticky || lost;
                ++exponent;
            }
        } else {
            sum = subtract(big, lesser);
            if (sticky) {
                // the bits shifted out make the difference a little smaller
                sum = subtract(sum, Wide{0U, 1U});
            }
        }
        return finish(x.negative_, sum, exponent, sticky);
    }

    friend Extended operator-(const Extended& a, const Extended& b) {
        return a + b.negated();
    }

    friend Extended operator*(const Extended& a, const Extended& b) {
        if (a.mantissa_ == 0U || b.mantissa_ == 0U) {
            return Extended();
        }
        const Wide product = multiply(a.mantissa_, b.mantissa_);
        return finish(a.negative_ != b.negative_, product,
                      a.exponent_ + b.exponent_ + 1, false);
    }

    friend Extended operator/(const Extended& a, const Extended& b) {
        if (a.mantissa_ == 0U) {
            return Extended();
        }
        // The quotient of the mantissas is in (0.5, 2): its integer bit and
        // 126 more bits, one at a time.
        std::uint64_t remainder = a.mantissa_;
        Wide quotient{0U, 0U};
        if (remainder >= b.mantissa_) {
            remainder -= b.mantissa_;
            quotient.low = 1U;
        }
        for (int step = 0; step < 126; ++step) {
            const bool carry = (remainder >> 63) != 0U;
            remainder <<= 1;
            std::uint64_t bit = 0;
            if (carry || remainder >= b.mantissa_) {
                remainder -= b.mantissa_;
                bit = 1U;
            }
            quotient.high = (quotient.high << 1) | (quotient.low >> 63);
            quotient.low = (quotient.low << 1) | bit;
        }
        return finish(a.negative_ != b.negative_, quotient,
                      a.exponent_ - b.exponent_ + 1, remainder != 0U);
    }

private:
    struct Wide {
        std::uint64_t high;
        std::uint64_t low;
    };

    static Wide add(const Wide& a, const Wide& b) {
        Wide result{a.high + b.high, a.low + b.low};
        if (result.low < a.low) {
            ++result.high;
        }
        return result;
    }

    static Wide subtract(const Wide& a, const Wide& b) {
        Wide result{a.high - b.high, a.low - b.low};
        if (a.low < b.low) {
            --result.high;
        }
        return result;
    }

    static Wide multiply(const std::uint64_t a, const std::uint64_t b) {
        const std::uint64_t a_low = a & 0xFFFFFFFFU;
        const std::uint64_t a_high = a >> 32;
        const std::uint64_t b_low = b & 0xFFFFFFFFU;
        const std::uint64_t b_high = b >> 32;
        const std::uint64_t ll = a_low * b_low;
        const std::uint64_t lh = a_low * b_high;
        const std::uint64_t hl = a_high * b_low;
        const std::uint64_t hh = a_high * b_high;
        const std::uint64_t middle =
            (ll >> 32) + (lh & 0xFFFFFFFFU) + (hl & 0xFFFFFFFFU);
        Wide result;
        result.low = (ll & 0xFFFFFFFFU) | (middle << 32);
        result.high = hh + (lh >> 32) + (hl >> 32) + (middle >> 32);
        return result;
    }

    // Shifts right; the bits that fall out set `sticky`.
    static void shift_right(Wide& value, const int count, bool& sticky) {
        if (count <= 0) {
            return;
        }
        if (count >= 128) {
            sticky = sticky || value.high != 0U || value.low != 0U;
            value = Wide{0U, 0U};
            return;
        }
        if (count >= 64) {
            const int rest = count - 64;
            const std::uint64_t lost_low = value.low;
            std::uint64_t lost_part = 0;
            if (rest > 0) {
                lost_part = value.high & ((1ULL << rest) - 1U);
            }
            sticky = sticky || lost_low != 0U || lost_part != 0U;
            value.low = rest == 0 ? value.high : value.high >> rest;
            value.high = 0U;
            return;
        }
        const std::uint64_t lost = value.low & ((1ULL << count) - 1U);
        sticky = sticky || lost != 0U;
        value.low = (value.low >> count) | (value.high << (64 - count));
        value.high >>= count;
    }

    // Builds the number from a 128-bit magnitude: the value is
    // value * 2^(exponent - 127); `sticky` says that more, smaller bits
    // followed.
    static Extended finish(const bool negative, Wide value, int exponent,
                           bool sticky) {
        if (value.high == 0U && value.low == 0U) {
            return Extended();
        }
        while ((value.high >> 63) == 0U) {
            value.high = (value.high << 1) | (value.low >> 63);
            value.low <<= 1;
            --exponent;
        }
        Extended result;
        result.negative_ = negative;
        std::uint64_t mantissa = value.high;
        const std::uint64_t half = 1ULL << 63;
        const bool up = value.low > half ||
                        (value.low == half && (sticky || (mantissa & 1U)));
        if (up) {
            ++mantissa;
            if (mantissa == 0U) {
                mantissa = half;
                ++exponent;
            }
        }
        result.mantissa_ = mantissa;
        result.exponent_ = exponent;
        return result;
    }

    bool negative_ = false;
    int exponent_ = 0;  // the number is mantissa_ * 2^(exponent_ - 63)
    std::uint64_t mantissa_ = 0;
};

}  // namespace vbrun
