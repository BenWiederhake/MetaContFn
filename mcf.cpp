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
 * Where:
 * - <num_inputs> is the number of binary inputs.  Defaults to 4.
 * - <num_outputs> is the number of binary outputs.  Defaults to 7.
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

/* There's no standard abstraction for mappings with the key set:
 *     { (a, b) \in {0, ..., n - 1}^2 \mid a < b }
 * Proposal: the following should be valid code:
 *     std::triangle<std::vector<mytype>> t(num_entries, mytype(42));
 *     // Two indices for handling as a triangle:
 *     t.at(0, 1) = mytype(1337); // 'at' and 'op[]' bounded by 'size2d()'
 *     // one index for handling as a collection:
 *     t[42] = mytype(23); // 'at' and 'op[]' bounded by 'size()'
 *     // Not shown: begin(), end(), etc.
 *     std::cout << t[0] << std::endl; // outputs "mytype(1337)"
 * Thus, here's an incomplete (but hopefully not broken) implementation: */
class triangle_myint {
public:
    triangle_myint(const myint num, const myint init) :
            n(num), store(tr(num), init) {
    }

    /* Disable copy / move */
    triangle_myint(const triangle_myint&) = delete;
    triangle_myint(triangle_myint&&) = delete;
    triangle_myint& operator=(const triangle_myint&) = delete;
    triangle_myint&& operator=(triangle_myint&&) = delete;

    myint& at(const myint a, const myint b) {
        assert(a < b);
        assert(b < n);
        const myint idx = tr(b) + a;
        assert(idx < store.size());
        return store.at(idx);
    }
    // ↑ No need for const version needed here, thankfully.

    myint size2d() const {
        return n;
    }

    myint& operator[](const myint idx) {
        assert(idx < store.size());
        return store.at(idx);
    }
    // ↑ No need for const version needed here, thankfully.

    myint size() const {
        return static_cast<myint>(store.size());
    }

private:
    /* 'n' could be computed on-the-fly, but ... eh. */
    const myint n;
    std::vector<myint> store;

    /* Compute the nth triangle number.
     * In other words: compute the size of the key set (see class comment). */
    static myint tr(const myint n) {
        return (n * (n - 1)) / 2;
    }
};

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
 *     that x is always the opposite of y (and thus redundant in a different
 *     way).
 *     Yeah, this property is also implied by "metastability-containing and no
 *     constant pins", but it can be detected here trivially and makes the
 *     following easier to implement:
 *
 * (3) Consider two output pins a, b.  Let ONES(a) and ONES(b) each be the set
 *     of inputs that cause a and b (respectively, not simultaneously) to be in
 *     the "on" state.  Define the order of output bits to be the lexicographical
 *     order of ONES(a), ONES(b).  Only allow functions where the output pins are
 *     ordered according to this definition.
 *     This eliminates the inherent combinatorial explosion, and leaves only
 *     truly different functions.
 *     In fact, we *reverse* this order, so that only output pin 0 can
 *     potentially be the constant zero function.
 *
 * Note that ordering with inequality implies independence. */
class output_ordered: public analyzer {
public:
    output_ordered(const function& f) :
            first_one(f.end_input), first_difference(f.num_outputs, f.end_input) {
        assert(f.num_outputs > 0);
    }

    virtual ~output_ordered() = default;

    virtual myint analyze(const function& f, const myint first_changed) {
        assert(differences <= first_difference.size());
        assert(first_difference[0] <= f.end_input);

        // Partially unwind state
        for (myint i = 0; i < first_difference.size() && differences > 0; ++i) {
            assert(first_difference[i] <= f.end_input);
            if (first_difference[i] != f.end_input
                    && first_difference[i] >= first_changed) {
                assert(differences > 0);
                --differences;
                first_difference[i] = f.end_input;
            }
        }
        assert(first_one <= f.end_input);
        if (first_one != f.end_input && first_one >= first_changed) {
            first_one = f.end_input;
        }
        if (differences == first_difference.size()
                && first_one != f.end_input) {
            if (DEBUG_ORD) {
                std::cout << "ord: Incomplete unwind" << std::endl;
            }
            return f.end_input;
            /* If !first_one, we don't know yet whether pin 0 is the constant
             * zero function, that's why there's no more cases here. */
        }

        // Wind state forward
        for (myint i = first_changed; i < f.end_input; ++i) {
            // TODO/benchmark/design: Pull apart into *two* loops?
            const myint output = f.image[i];
            // TODO/benchmark: inline pin2mask(0)?
            if (first_one == f.end_input && (output & pin2mask(0))) {
                first_one = i;
                if (differences == first_difference.size()) {
                    if (DEBUG_ORD) {
                        std::cout << "ord: final 'first_one' (diff was at "<<first_difference[0]<<")" << std::endl;
                    }
                    // Whee! Success!
                    return f.end_input;
                }
            }
            if (differences == first_difference.size()) {
                continue;
            }
            for (myint out_pin_i = 0; out_pin_i < f.num_outputs; ++out_pin_i) {
                // TODO/benchmark: Cache '!!(output & pin2mask(out_pin_i))' ?
                for (myint out_pin_j = out_pin_i + 1; out_pin_j < f.num_outputs;
                        ++out_pin_j) {
                    if (!!(output & pin2mask(out_pin_i))
                            == !!(output & pin2mask(out_pin_j))) {
                        // Same state => skip
                        continue;
                    }
                    myint& diff = first_difference.at(out_pin_i, out_pin_j);
                    if (diff != f.end_input) {
                        // First difference lies in the past => skip
                        continue;
                    }
                    if (output & pin2mask(out_pin_i)) {
                        // Smaller pin has triggered first => violated order!
                        if (DEBUG_ORD) {
                            std::cout << "ord: inval diff" << std::endl;
                        }
                        assert(!(output & pin2mask(out_pin_j)));
                        return i;
                    }
                    assert(output & pin2mask(out_pin_j));
                    // Correct difference.  Log and continue.
                    diff = i;
                    ++differences;
                    if (differences == first_difference.size()) {
                        if (first_one != f.end_input) {
                            if (DEBUG_ORD) {
                                std::cout << "ord: final diff" << std::endl;
                            }
                            /* Whee! Success! */
                            return f.end_input;
                        } else {
                            /* TODO/benchmark: What is quicker?
                             * - Letting it run normally, maybe exploiting
                             *   loop unrolling?
                             * - Trying a 'break 2;' like currently? */
                            out_pin_i = out_pin_j = f.num_outputs;
                        }
                    }
                }
            }
        }

        // Handle non-fulfillment
        assert(
                (differences < first_difference.size())
                        || (first_one == f.end_input));
        if (differences < first_difference.size()) {
            /* In each step / place / input-pattern-index-thing, the number
             * of resolvable differences is at most:
             *     number of zero-pins * number of one-pins
             * ... which is at most num_outputs^2 / 2: */
            const myint half_outputs = f.num_outputs / 2;
            const myint max_diff_per_step = half_outputs
                    * (f.num_outputs - half_outputs);
            assert(max_diff_per_step > 0);
            /* Divide, round up */
            const myint min_steps_needed = (first_difference.size()
                    - differences + max_diff_per_step - 1)
                    / max_diff_per_step;
            /* In case you didn't notice: 'min_steps_needed' is never
             * greater than 2.  Ugh.
             * TODO/optimize: This doesn't seem to be tight yet. */
            assert(min_steps_needed > 0);
            if (DEBUG_ORD) {
                std::cout << "ord: missing " << (first_difference.size()
                        - differences) << " diffs:";
            }
            if (DEBUG_ORD) {
                for (myint i = 0; i < first_difference.size(); ++i) {
                    std::cout << " " << first_difference[i];
                }
                std::cout << std::endl;
            }
            return f.end_input - min_steps_needed;
        }
        assert(first_one == f.end_input);
        /* So all are different, but the zeroth pin is the constant zero
         * function?  Dangit.  That *might* change with only touching the
         * last place of the function image, so: */
        if (DEBUG_ORD) {
            std::cout << "ord: missing first_one" << std::endl;
        }
        return f.end_input - 1;
    }

    virtual const std::string& get_name() const {
        static const std::string name = "out_ord";
        return name;
    }

private:
    static const bool DEBUG_ORD = false;

    /* Only the first output pin can possibly be the constant zero function,
     * so keep track of it separately. */
    myint first_one;

    /* On which input-pattern did we see a valid difference between i and j?
     * => std::triangle<std::vector<myint>> pretty please? */
    triangle_myint first_difference;
    // How many valid differences are known?
    myint differences = 0;
};


/* ----- Combining it all ----- */

const static bool DEBUG_PRINT = false;

/* Print all (remaining) functions with the desired properties to std::cout.
 * Note that the 'properties' vector will not be changed, but its elements.
 * Also prints some statistics to std::cerr. */
void print_remaining(function& f, std::vector<analyzer*>& properties) {
    boost::io::ios_width_saver butler_width(std::cerr);
    myint last_change = 0;
    std::cerr << "Searching for function with " << properties.size()
            << " properties:" << std::endl;
    if (DEBUG_PRINT) {
        std::cerr << std::setw(10);
        for (analyzer* a : properties) {
            std::cerr << a->get_name() << '\t';
        }
        std::cerr << std::endl;
    }
    myint counter = 0;
    do {
        if (DEBUG_PRINT) {
            std::cerr << "#? " << f << std::endl;
        }
        ++counter;
        const myint tell = last_change;
        last_change = f.end_input;
        if (DEBUG_PRINT) {
            std::cerr << std::setw(10);
        }
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
            counter = 0;
            last_change = f.end_input - 1;
        } else if (counter > 1000000) {
            std::cerr << "#_ " << f << std::endl;
            counter = 0;
        }
    } while ((last_change = f.advance(last_change)) < f.end_input);
    std::cerr << std::setw(0) << "Done searching." << std::endl;
}


/* ----- Calling it ----- */

int main(int argc, char **argv) {
    myint num_inputs;
    myint num_outputs;
    try {
        num_inputs = (argc > 1) ? parse_arg(argv[1]) : 4;
        num_outputs = (argc > 2) ? parse_arg(argv[2]) : 7;
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
