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
 * Compile with:
 *   make
 * Run as:
 *   ./mcf [<num_inputs> [<num_outputs>]]
 * Faster version (less checks, worse debuggablity):
 *   make mcf_fast && ./mcf_fast [<num_inputs> [<num_outputs>]]
 * Where:
 * - <num_inputs> is the number of binary inputs.  Defaults to 4.
 * - <num_outputs> is the number of binary outputs.  Defaults to 7.
 *
 * Here's some fun statistics, gathered by running it once on my machine:
 *
 * in  out fns      time
 * -----------------------
 * 2   2   3        <.01s
 * 2   3   0        <.01s
 * -----------------------
 * 3   2   195      <.01s
 * 3   3   55       <.01s
 * 3   4   2        <.01s
 * 3   5   0        <.01s
 * -----------------------
 * 4   2   131667   0.5s
 * 4   3   124086   0.6s
 * 4   4   45415    0.6s
 * 4   5   8764     1.4s
 * 4   6   1052     3.8s
 * 4   7   72       9.1s
 * 4   8   2        20.6s
 * 4   9   0        38s
 * -----------------------
 * 5   2   >6.3M    >36s
 * 5   3   >9.2M    >61s
 * 5   4   >5.7M    >64s
 * 5   5   >1.9M    >68s
 * 5   6   >134K    >56s
 * 5   7   >12K     >86s
 * 5   8   >1174*   >200s
 * 5   9   >=0*     >300s
 * 5   10  >=0*     >300s
 *
 * *: most of the function space wasn't even explored due to the limited time.
 */

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/io/ios_state.hpp>


/* ----- Things that will be everywhere ----- */

typedef unsigned int myint;
/* The program will take up to O(MAX_BITS**MAX_BITS) time,
 * so I don't think you're going to need more than that 20.  */
#define MAX_BITS 20
static_assert(sizeof(myint) * 8 >= MAX_BITS,
        "Bad MAX_BITS size chosen!");

myint pin2mask(const myint pin) {
    assert(pin <= MAX_BITS);
    return static_cast<myint>(1) << pin;
}

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
    myint advance(const myint at) {
        assert(at < end_input);
        // Reset "digits" at "less significant places":
        for (myint i = at + 1; i < end_input; ++i) {
            image[i] = 0;
        }
        // Increment image[at], with carry:
        for (myint i = at; i >= 1; --i) {
            /* Consider only functions that map 0 to 0.
             * Thus, never change image[0]. */
            if (++image[i] < end_output) {
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


/* ----- Central superclass / interface ----- */
/* Note that each analyzer shall have the ability to retain state,
 * so I prefer this over functors.  Also, I get away without having
 * to write a template, which is nice. */

class analyzer {
public:
    constexpr analyzer() = default;

    virtual ~analyzer() = default;

    /* Gets the most significant place that changed since the last invocation;
     * or 0 if there was no last invocation.  (Which fits well because then you
     * can treat that as the same case.)
     * Returns either the most significant place that has to be increased,
     * before this analyzer is satisfied -- or 'f.end_input' if satisfied. */
    virtual myint analyze(const function& f, const myint first_changed) = 0;

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

    virtual myint analyze(const function& f, const myint first_changed) {
        // 'first_changed==0' is rare enough (once) to need no extra filtering.
        for (myint i = first_changed; i < f.end_input; ++i) {
            const myint output = f.image[i];
            for (myint in_pin = 0; in_pin < f.num_inputs; ++in_pin) {
                // Affected bits if in-pin is 'M':
                const myint change = output ^ f.image[i & ~pin2mask(in_pin)];
                if (is_pot_or_zero(change)) {
                    // It's good.
                    continue;
                }
                // Not containing! More than one output changes!
                return i;
            }
        }
        // Fine!
        return f.end_input;
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

    virtual myint analyze(const function& f, const myint first_changed) {
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
            return f.end_input;
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
                        return f.end_input;
                    }
                }
            }
        }

        /* There's an irrelevant input!  Try again.
         * However, the property is already fulfilled when
         * f.image[f.end_input - 1] != 0, so we can't say much. */
        assert(relevant_inputs < f.num_inputs);
        assert(f.end_input > 0); // already in f's constructor
        return f.end_input - 1; // smallest increment
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
    output_ordered(const function& f) :
            first_ones(f.num_outputs, f.end_input) {
        assert(f.num_outputs > 0);
    }

    virtual ~output_ordered() = default;

    virtual myint analyze(const function& f, const myint first_changed) {
        assert(first_ones.size() == f.num_outputs);
        assert(count_ones <= f.num_outputs);

        // Partially unwind state
        for (myint i = 0; i < first_ones.size() && count_ones > 0; ++i) {
            assert(first_ones[i] <= f.end_input);
            if (first_ones[i] != f.end_input
                    && first_ones[i] >= first_changed) {
                assert(count_ones > 0);
                --count_ones;
                first_ones[i] = f.end_input;
            }
        }
        if (count_ones == f.num_outputs) {
            if (DEBUG_ORD) {
                std::cout << "ord: Incomplete unwind" << std::endl;
            }
            return f.end_input;
        }

        // Wind state forward
        for (myint i = first_changed; i < f.end_input; ++i) {
            {
                const myint missing_ones = (f.num_outputs - count_ones);
                if (i + missing_ones > f.end_input) {
                    if (DEBUG_ORD) {
                        std::cout << "ord: missing many ones" << std::endl;
                    }
                    return f.end_input - missing_ones;
                }
            }
            const myint output = f.image[i];
            for (myint out_pin = 0; out_pin < f.num_outputs; ++out_pin) {
                if (!(output & pin2mask(out_pin))) {
                    continue;
                }
                if (first_ones[out_pin] < i) {
                    continue;
                }
                assert(first_ones[out_pin] == f.end_input);
                /* => This definitely is the first one! */

                if (count_ones + 1 < out_pin) {
                    /* This is just a generalization of the upcoming check. */
                    if (DEBUG_ORD) {
                        std::cout << "ord: Too few ones for this first one"
                                << std::endl;
                    }
                    return i;
                }
                /* Did all "previous" out-pins already have their first one? */
                for (myint out_pin_p = 0; out_pin_p < out_pin; ++out_pin_p) {
                    if (first_ones[out_pin_p] >= i) {
                        /* Nope. */
                        if (DEBUG_ORD) {
                            std::cout << "ord: order violated" << std::endl;
                        }
                        return i;
                    }
                }

                /* Still here? Then it's a "valid" one. */
                first_ones[out_pin] = i;
                ++count_ones;
                if (count_ones == f.num_outputs) {
                    if (DEBUG_ORD) {
                        std::cout << "ord: Enough ones" << std::endl;
                    }
                    return f.end_input;
                }
            }
        }

        // Handle non-fulfillment
        assert(f.num_outputs - 1 == count_ones);
        if (DEBUG_ORD) {
            std::cout << "ord: missing final one" << std::endl;
        }
        return f.end_input - 1;
    }

    virtual const std::string& get_name() const {
        static const std::string name = "out_ord";
        return name;
    }

private:
    static const bool DEBUG_ORD = false;

    /* For each output pin, on which input-pattern did we first see it
     * getting activated?  */
    std::vector<myint> first_ones;
    myint count_ones = 0;
};


/* ----- Combining it all ----- */

const static bool DEBUG_PRINT = false;

const static myint DEBUG_PRINT_STEP = 5000000;

/* Print all (remaining) functions with the desired properties to std::cout.
 * Note that the 'properties' vector will not be changed, but its elements.
 * Also prints some statistics to std::cerr. */
void print_remaining(function& f, std::vector<analyzer*>& properties) {
    boost::io::ios_width_saver butler_width(std::cerr);
    myint last_change = 0;
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
    myint steps = 0;
    myint counter = 0; // FIXME: rename 'display_watchdog'
    myint fns = 0;
    do {
        if (DEBUG_PRINT) {
            std::cerr << "#? " << f << std::endl;
        }
        ++counter;
        ++steps;
        const myint tell = last_change;
        last_change = f.end_input;
        for (analyzer* a : properties) {
            const myint proposed = a->analyze(f, tell);
            if (DEBUG_PRINT) {
                std::cerr << proposed << '\t';
            }
            last_change = std::min(last_change, proposed);
        }
        if (DEBUG_PRINT) {
            std::cerr << std::endl;
        }
        if (last_change == f.end_input) {
            // Yay!
            std::cout << "=> " << f << std::endl;
            ++fns;
            last_change = f.end_input - 1;
        } else if (counter >= DEBUG_PRINT_STEP) {
            std::cerr << "#_ " << f << std::endl;
            std::cerr << "#_ " << fns << " fns in " << steps << " steps."
                    << std::endl;
            counter -= DEBUG_PRINT_STEP;
        }
    } while ((last_change = f.advance(last_change)) < f.end_input);
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
