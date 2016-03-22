/* MetaContFn -- enumerates all meta-containing functions
 * Copyright (C) Ben Wiederhake 2016
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Dependencies:
 *   Boost Iostreams 1.58 (other versions are probably fine, too)
 * Compile with (uses gcc):
 *   make
 *   If you don't have gcc, you may need to find an alternative to __builtin_ctz
 * Run as:
 *   ./mcf [<num_inputs> [<num_outputs>]]
 * Faster version (less checks, worse debuggablity):
 *   make mcf_fast && ./mcf_fast [<num_inputs> [<num_outputs>]]
 * Where:
 * - <num_inputs> is the number of binary inputs.  Defaults to 3.
 * - <num_outputs> is the number of binary outputs.  Defaults to 3.
 */

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#include <boost/io/ios_state.hpp>


/* ----- Things that will be everywhere ----- */

typedef unsigned int myint;
/* The program will take up to O(MAX_BITS**MAX_BITS) time,
 * so I don't think you're going to need more than that 20.  */
#define MAX_BITS 20
static_assert(sizeof(myint) * 8 >= MAX_BITS, "Bad MAX_BITS size chosen!");

myint pin2mask(const myint pin) {
    assert(pin <= MAX_BITS);
    return static_cast<myint>(1) << pin;
}

class function;

struct bit_address {
    /* What's the lowest input-pattern that upset this analyzer?
     * (Or f.end_input if not upset.) */
    myint input_pattern;

    /* For the given input, what's the most significant pin that upset this
     * analyzer?
     * (Or undefined if not upset.) */
    myint bit;

    // Convenience: upset
    bit_address(myint input_pattern, myint bit) :
            input_pattern(input_pattern), bit(bit) {
    }

    // Convenience: not upset
    bit_address(const function& f);
    /*Must come after the definition of class function.  Sigh. */

    // Collapse default operator= and hand-written std::min overload
    void assign_min(const bit_address& other) {
        if (other.input_pattern < input_pattern) {
            input_pattern = other.input_pattern;
            bit = other.bit;
        } else if (other.input_pattern == input_pattern) {
            /* Note that other.bit is not defined if
             * 'other.input_pattern == f.end_input', which we can't check
             * right now.  However, in that case it doesn't matter what
             * ends up in this->bit, so don't care. */
            bit = std::min(bit, other.bit);
        }
    }
};

/* Glorified std::vector<myint>. Also, glorified BigNum. */
class function {
public:
    typedef std::vector<myint> image_t;
    const myint num_inputs;
    const myint num_outputs;
    const myint end_input;
    const myint end_output;
    image_t image;

    function(const myint num_inputs, const myint num_outputs) :
            num_inputs(num_inputs), num_outputs(num_outputs),
            end_input(pin2mask(num_inputs)), end_output(pin2mask(num_outputs)),
            image(end_input) {
        assert(num_inputs > 0);
        assert(num_outputs > 0);
    }

    /* "Count up".  Note that we're treating 'image' as a very large number:
     * 'image[0]' is the most significant place, and
     * 'image[end_input - 1]' is the least significant place.
     * Thus, if 'at' is 'end_input - 1', we take the smallest possible step,
     * and if 'at' is 0, we take a large step (the smallest that increases
     * 'image[0]').
     * The return value is the most significant place that changed. Observe
     * that this is either the value of 'at', or a more significant place,
     * i.e., numerically lower index.
     * If that isn't possible, return end_input, which is an invalid place
     * (and also greater than 'at'). */
    myint advance(const bit_address at) {
        /* TODO/benchmark: pass by value or pass by reference?
         * Since it's 64 bits in total, it *should* fit into a register, so
         * I'll start with pass-by-value. */
        assert(at.input_pattern < end_input);
        // Reset "digits" at "less significant places":
        for (myint i = at.input_pattern + 1; i < end_input; ++i) {
            image[i] = 0;
        }

        /* TODO/benchmark: unroll the first iteration of this to get rid of
         * 'increment' which may or may not interfere with pipelining. */

        myint increment = pin2mask(at.bit);
        // Increment image[at], with carry:
        for (myint i = at.input_pattern; i >= 1; --i) {
            /* ↑ Consider only functions that map 0 to 0.
             * Thus, never change image[0]. */
            /* FIXME: This assumes that pin2mask(MAX_BITS)+pin2mask(MAX_BITS)
             * doesn't overflow.  Uhh.  See static_assert below this class. */
            image[i] += increment;
            increment = 1;
            if (image[i] < end_output) {
                // Valid!
                return i;
            } else {
                // Wrap-around of this digit.
                image[i] = 0;
            }
        }
        /* Wrap-around of full "number"!
         * (Ignoring image[0] of course; see above.) */
        return end_input;
    }
};
static_assert(sizeof(myint) * 8 >= 1 + MAX_BITS,
        "Fix function.advance implementation to handle overflow gracefully.");

bit_address::bit_address(const function& f) :
        input_pattern(f.end_input)
#ifndef MCF_ALLOW_UNINITIALIZED
        , bit(0)
#endif
{
}


/* ----- Utility class & functions ----- */

myint parse_arg(char *arg) {
    const unsigned long raw_val = std::stoul(arg, nullptr, 0);
    if (raw_val > MAX_BITS) {
        throw std::out_of_range(""); // message is ignored anyway
    }
    return static_cast<unsigned int>(raw_val);
}

std::ostream& operator<<(std::ostream& out, const function& f) {
    out << "fn(B^" << f.num_inputs << " -> B^" << f.num_outputs << ")";

    if (f.image.size() == 0) {
        // Uhh.
        out << "[]";
    } else if (f.image.size() == 1) {
        // Uhhhhh.
        out << "[" << f.image[0] << "]";
    } else {
        // Thanks.

        /* Restore flags, width, fill.  Reliably. */
        std::setfill('d');
        boost::io::ios_flags_saver butler_flags(out);
        boost::io::ios_width_saver butler_width(out);
        boost::io::ios_fill_saver butler_fill(out);

        /* Always output full hexa code, including zeros. */
        const myint out_w = (f.num_outputs + 3) / 4;
        out << std::hex;

        out << "[" << std::setw(out_w) << f.image[0];
        for (myint i = 1; i < f.image.size(); ++i) {
            // Yuk, formatting with iostream.
            out << std::setw(0) << ", " << std::setw(out_w) << f.image[i];
        }
        out << std::setw(0) << "]";
    }

    return out;
}

std::ostream& operator<<(std::ostream& out, const bit_address& at) {
    /* Must behave like one element for 'out'.  Sigh. */
    std::ostringstream buf;
    buf << at.input_pattern << "." << std::setw(2) << std::setfill('0') << at.bit;
    return out << buf.str();
}


/* ----- Central superclass / interface ----- */
/* Note that each analyzer shall have the ability to retain state,
 * so I prefer this abstract class over functors.  Also, I get away
 * without having to write a template, which is nice. */

class analyzer {
public:
    constexpr analyzer() = default;

    virtual ~analyzer() = default;

    /* Gets the most significant place that changed since the last invocation;
     * or 0 if there was no last invocation.  (Which fits well because then you
     * can treat that as the same case.)
     * Returns either the most significant place that has to be increased,
     * before this analyzer is satisfied -- or 'f.end_input' if satisfied. */
    virtual bit_address analyze(const function& f, const myint first_changed) = 0;

    virtual const std::string& get_name() const = 0;
};


/* ----- Useful analyzers ----- */
/* Each of these represents a criterion a function needs to fulfill.
 * TODO/benchmark: Combine/inline all this into a single loop over all inputs?
 * However, for maintenance/development, the current structure is better. */

/* Check if the function is metastability-containing.  Duh. */
class metastability_containing: public analyzer {
public:
    // Stateless, modulo vtable entries
    constexpr metastability_containing() = default;

    virtual ~metastability_containing() = default;

    virtual bit_address analyze(const function& f, const myint first_changed) {
        // 'first_changed==0' is rare enough (once) to need no extra filtering.
        for (myint i = first_changed; i < f.end_input; ++i) {
            const myint output = f.image[i];
            myint max_tz_plus_one = 0;
            for (myint j = f.num_inputs; j > 0; --j) {
                const myint in_pin = j - 1;
                // Affected bits if in-pin is 'M':
                const myint change = output ^ f.image[i & ~pin2mask(in_pin)];
                if (is_pot_or_zero(change)) {
                    // It's good.
                    continue;
                }
                /* Not containing!  More than one output changes!  In order to
                 * fix this, *at least* the least significant offending output
                 * pin must change.  However, we want to look at all input pins
                 * and choose the most significant pin of all least significant
                 * offending pins.
                 * In case you're trying to get rid of __builtin_ctz, don't
                 * worry:  it will never be called with 0. */
                max_tz_plus_one = std::max(max_tz_plus_one,
                        myint(__builtin_ctz(change) + 1));
            }
            if (max_tz_plus_one) {
                return bit_address(i, max_tz_plus_one - 1);
            }
        }
        // Fine!
        return bit_address(f);
    }

    virtual const std::string& get_name() const {
        static const std::string name = "is_msc";
        return name;
    }

private:
    /* Is 'v' a power of two, or zero? */
    static bool is_pot_or_zero(const myint v) {
        // Based on:
        // https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
        return (v & (v - 1)) == 0;
    }
};

/* Check that each input pin is relevant.  An input pin is relevant *iff* there
 * are two inputs x, y only differing on the state of that input pin,
 * and f(x) != f(y). */
class input_relevance: public analyzer {
public:
    input_relevance(const function& f) :
            first_relevant(f.num_inputs, f.end_input) {
    }

    virtual ~input_relevance() = default;

    virtual bit_address analyze(const function& f, const myint first_changed) {
        assert(first_relevant.size() == f.num_inputs);

        // Partially unwind state
        for (myint i = 0; i < first_relevant.size() && relevant_inputs > 0;
                ++i) {
            assert(first_relevant[i] <= f.end_input);
            if (first_relevant[i] != f.end_input
                    && first_relevant[i] >= first_changed) {
                assert(relevant_inputs > 0);
                --relevant_inputs;
                first_relevant[i] = f.end_input;
            }
        }
        if (relevant_inputs == f.num_inputs) {
            return bit_address(f);
        }

        // Wind state forward
        for (myint i = first_changed; i < f.end_input; ++i) {
            const myint output = f.image[i];
            for (myint in_pin = 0; in_pin < f.num_inputs; ++in_pin) {
                assert(first_relevant[in_pin] != i);
                if (first_relevant[in_pin] < i) {
                    continue;
                }
                assert(first_relevant[in_pin] == f.end_input);
                if (!(i & pin2mask(in_pin))) {
                    continue;
                }
                const myint opposite_input = i & ~pin2mask(in_pin);
                // f.image[opposite_input] destroys all kinds of locality :/
                // TODO/benchmark: Hilbert walk the image????
                // This would require A LOT changes.
                // TODO/benchmark: merge classes with metastability_containing?
                if (output != f.image[opposite_input]) {
                    // Relevant!
                    first_relevant[in_pin] = i;
                    if (++relevant_inputs == f.num_inputs) {
                        return bit_address(f);
                    }
                }
            }
        }

        /* There's an irrelevant input!  Try again.
         * However, the property is already fulfilled when
         * f.image[f.end_input - 1] != 0, so we can't say much. */
        assert(relevant_inputs < f.num_inputs);
        assert(f.end_input > 0); // already in f's constructor
        return bit_address(f.end_input - 1, 0); // smallest increment
    }

    virtual const std::string& get_name() const {
        static const std::string name = "in_rel";
        return name;
    }

private:
    // On which input-pattern was the i-th input-pin first relevant?
    std::vector<myint> first_relevant;
    // How many inputs are known to be relevant?
    myint relevant_inputs = 0;
};

/* Check that the output pins are relevant, pairwise independent and ordered
 * (and thus strictly ordered).
 * Yes, that's *three* birds with one stone.  I'm sorry.  But as you will see,
 * all three properties are actually kind of the same.
 *
 * (1) Check that each output pin is relevant.
 *     An output pin is relevant *iff* there are inputs x, y such that the
 *     output pin has different states in f(x), f(y).  This eliminates functions
 *     with constant output pins.
 *
 * (2) Two output pins a, b are independent *iff* there are inputs x, y such
 *     that the output pins have different states when representing f(x), f(y).
 *     This eliminates functions with redundant output pins.  Note that due to
 *     f(0) == 0 (see constructor of class 'function') it is already impossible
 *     that x is always the exact opposite of y (and thus redundant in a
 *     different way).
 *     Yeah, this property is also implied by "metastability-containing and no
 *     constant pins", but it can be detected here trivially and makes the
 *     following easier to implement:
 *
 * (3) Consider two output pins a, b.  Let ONE(a) and ONE(b) each be the first
 *     input-pattern that cause a and b respectively to be in the "on" state.
 *     Note that ONE(a) != ONE(b), because otherwise it can't possibly be
 *     metastability-containing anymore.  Define the order of output bits to be
 *     the *reverse* order of ONE(a), ONE(b).  Only allow functions where the
 *     output pins are ordered according to this definition.
 *     This eliminates the inherent combinatorial explosion, and leaves only
 *     semantically distinct functions.
 *
 * Note that ordering with inequality implies independence,
 * so we actually get (2) for free. */
class output_ordered: public analyzer {
public:
    output_ordered(const function& f) {
        assert(f.num_outputs > 0);
        /* Must be guaranteed by print_remaining already.  Note: this is
         * necessary to guarantee the first loop invariant in 'analyze()'
         * in its first invocation.
         * To see it in action, start the program with #out / 2 > 2^#in */
        assert(can_fit(f.num_outputs, f.end_input));
        first_ones.reserve(f.num_outputs);
    }

    virtual ~output_ordered() = default;

    virtual bit_address analyze(const function& f, const myint first_changed) {
        assert(first_ones.size() <= f.num_outputs);

        // Partially unwind state
        while (!first_ones.empty()) {
            assert(first_ones.back() < f.end_input);
            if (first_ones.back() >= first_changed) {
                first_ones.pop_back();
            } else {
                break;
            }
        }
        if (first_ones.size() == f.num_outputs) {
            if (DEBUG_ORD) {
                std::cout << "ord: Incomplete unwind" << std::endl;
            }
            return bit_address(f);
        }

        // Wind state forward
        for (myint i = first_changed; i < f.end_input; ++i) {
            /* Loop invariant:  it must still be (theoretically) possible to fit
             * all remaining first_ones in the runway, according to can_fit.
             * Second invariant:  not all first-zeros have been seen already. */
            assert(can_fit(f.num_outputs - first_ones.size(), f.end_input - i));
            assert(first_ones.size() < f.num_outputs);
            const myint output = f.image[i];
            const myint out_pin = first_ones.size();
            if (output & (pin2mask(out_pin) - 1)) {
                /* A naughty pin was set.  The next output that doesn't have a
                 * naughty output pin necessarily has 'out_pin' set. */
                return bit_address(i, out_pin);
            }
            if (output & pin2mask(out_pin)) {
                assert(first_ones.empty() || first_ones.back() < i);
                /* Great!  This can't make things worse.  (And if, then another
                 * analyzer is complaining.) */
                first_ones.push_back(i);
                assert(first_ones.size() <= f.num_outputs);
                if (first_ones.size() == f.num_outputs) {
                    /* Whee! Finished! */
                    return bit_address(f);
                }
                continue;
            }
            /* Not a '1'?  Hmm.  We might have run out of runway. */
            assert(f.num_outputs - first_ones.size() > 0);
            if (!can_fit(f.num_outputs - first_ones.size(), f.end_input - (i + 1))) {
                /* Then the next output that has enough runway necessarily has
                 * 'out_pin' set. */
                return bit_address(i, out_pin);
            }
        }

        /* Not possible to reach this. */
        std::cout << "FAIL" << std::endl;
        std::cerr << "FAIL" << std::endl;
        assert(false);
        // Sigh.
        return bit_address(0, 0);
    }

    virtual const std::string& get_name() const {
        static const std::string name = "out_ord";
        return name;
    }

    /* Must be public, as the constructor would like to have that property
     * already.  And *that* can only be guaranteed if print_remaining checks
     * it. */
    static bool can_fit(const size_t ones, const size_t runway) {
        /* Two consecutive input patterns ending in ..0 and ..1 can't introduce
         * two (or more) new first-ones together (read: in summation).
         *
         * Proof: First of all, a single input pattern can't introduce two or
         * more by itself, because that's an obvious violation of
         * metastability-containment.  So the only way to achieve that is by
         * "distributing it", i.e., each input pattern introduces exactly one
         * first-one.  However, the second pattern is adjacent to the first
         * pattern, so it must also contain the one introduced by the first.
         * Furthermore, the second pattern must be also adjacent to another
         * input pattern (as the ...0 pattern can't have been all zeros, because
         * by construction f(0)=0, and the ...0 pattern introduces a one).
         * This earlier pattern can't possibly contain either 1 because it
         * appeared first in the outputs to the ..0 and ..1 patterns,
         * respectively.  Thus the ..1 pattern and the other input pattern
         * differ in only one bit, but their outputs in (at least) two bits.
         * Violation to metastability-containment!
         *
         * So, given 'runway', we can fit at most round_up(runway / 2)
         * first-ones.  As we can see in README.md#statistics, this bound seems
         * to be tight; at least for #out <= 16. */
        const size_t max_fit = (runway + 1) / 2;
        return ones <= max_fit;
    }

private:
    static const bool DEBUG_ORD = false;

    /* For each output pin, on which input-pattern did we first see it
     * getting activated?  Note that this will always be an ordered,
     * strictly increasing sequence. */
    std::vector<myint> first_ones;
};


/* ----- Combining it all ----- */

const static bool DEBUG_PRINT = false;

const static size_t DEBUG_PRINT_STEP = 5000000;

/* Print all (remaining) functions with the desired properties to std::cout.
 * Note that the 'properties' vector will not be changed, but its elements.
 * Also prints some statistics to std::cerr. */
void print_remaining(function& f, std::vector<analyzer*>& properties) {
    boost::io::ios_width_saver butler_width(std::cerr);
    std::cerr << "Searching for function with " << properties.size()
            << " properties:";
    std::cerr << std::endl;
    for (analyzer* a : properties) {
        std::cerr << a->get_name();
        if (DEBUG_PRINT) {
            std::cerr << '\t';
        } else {
            std::cerr << std::endl;
        }
    }
    if (DEBUG_PRINT) {
        std::cerr << std::endl;
    }
    size_t steps = 0;
    myint display_watchdog = 0;
    myint fns = 0;
    if (output_ordered::can_fit(f.num_outputs, f.end_input)) {
        myint last_change = 0;
        do {
            if (DEBUG_PRINT) {
                std::cerr << "#? " << f << std::endl;
            }
            ++display_watchdog;
            ++steps;
            bit_address next_change(f);

            for (analyzer* a : properties) {
                const bit_address proposed = a->analyze(f, last_change);
                if (DEBUG_PRINT) {
                    std::cerr << proposed << '\t';
                }
                next_change.assign_min(proposed);
            }
            if (DEBUG_PRINT) {
                std::cerr << std::endl;
            }
            if (next_change.input_pattern == f.end_input) {
                // Yay!
                std::cout << "=> " << f << std::endl;
                ++fns;
                next_change.input_pattern = f.end_input - 1;
                next_change.bit = 0;
            } else if (display_watchdog >= DEBUG_PRINT_STEP) {
                std::cerr << "#_ " << f << std::endl;
                std::cerr << "#_ " << fns << " fns in " << steps << " steps."
                        << std::endl;
                display_watchdog -= DEBUG_PRINT_STEP;
            }
            last_change = f.advance(next_change);
        } while (last_change < f.end_input);
    } else {
        std::cerr << "Impossibly many output pins."
                "  Pruning whole search right away." << std::endl;
    }
    std::cerr << std::setw(0) << "Done searching.  Found "
            << fns << " fns in " << steps << " steps." << std::endl;
}


/* ----- Calling it ----- */

int main(int argc, char **argv) {
    myint num_inputs;
    myint num_outputs;
    try {
        num_inputs = (argc > 1) ? parse_arg(argv[1]) : 3;
        num_outputs = (argc > 2) ? parse_arg(argv[2]) : 3;
    } catch (const std::invalid_argument& ia) {
        std::cerr << "Arguments are non-numeric." << std::endl;
        std::cerr << "Usage: " << argv[0] << " [<num_inputs> [<num_outputs>]]"
                << std::endl;
        return 1;
    } catch (const std::out_of_range& ia) {
        std::cerr << "Arguments are too big; only [0, " << MAX_BITS
                << "] is supported!" << std::endl;
        std::cerr << "Usage: " << argv[0] << " [<num_inputs> [<num_outputs>]]"
                << std::endl;
        return 1;
    }

    std::cerr << "n_in = " << num_inputs << ", n_out = " << num_outputs
            << std::endl;

    function f = function(num_inputs, num_outputs);

    /* HERE BE DRAGONS!  The analyzers are not really as independent as they
     * may seem.  For instance, 'output_ordered' may sometimes (and
     * inconsistently) enforce metastability-containment.  Thus, if you remove
     * 'metastability_containing' from the list but leave 'output_ordered', you
     * may be surprised by some/all functions being skipped. */
    metastability_containing p_msc;
    input_relevance p_ir(f);
    output_ordered p_ord(f);

    std::vector<analyzer*> properties;
    properties.push_back(&p_ord);
    properties.push_back(&p_msc);
    properties.push_back(&p_ir);

    print_remaining(f, properties);

    return 0;
}
