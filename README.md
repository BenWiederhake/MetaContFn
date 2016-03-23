# What is this?

Enumerates all metastability-containing functions


## Metasta-what?

Let's say you have a chip that computes some function, given `i` input
bits, and producing `j` output bits.  And your function clearly defines
(among other things):
```
00110101001 -> 001101010010100101
00110111001 -> 001001110010110101
      ^           ^  ^      ^
```
See that?  A single bitflip in the input causes three bitflips in the
output!

Now let's say there's a fault somewhere else in the system (of course
it's somewhere else; our code/hardware is *never* faulty!), and
precisely that input pin doesn't quite read a logical 0, nor a logical
1.  The voltage is … somewhere in between.  It "hangs in the air", so
to speak.  In other words: it is metastable.  Thus, the following might
happen:

```
00110101001 -> 001101010010100101
001101M1001 -> 001M01M100101M0101
00110111001 -> 001001110010110101
      ^           ^  ^      ^
```

Oh no!  The metastability is *spreading*!

But worry not, metastability-containing functions are to the rescue!
They have the superpower of changing at most one output bit whenever an
input bit changes.  This is in a way [the opposite of a hash
function](https://en.wikipedia.org/wiki/Avalanche_effect).  Also,
getting the number of `M`s in the output to 0 is [impossible in all
interesting
cases.](https://www.computer.org/csdl/trans/tc/1981/02/06312173.pdf)


## How does this work?  Also, notation?

So, the above is *not* a metastability-containing function.  Here's a
positive example instead:
```
0000 -> 00000000
0001 -> 10000000
0010 -> 10000000
0011 -> 11000000
0100 -> 10000000
0101 -> 10100000
0110 -> 10010000
0111 -> 10000000
1000 -> 10000000
1001 -> 10001000
1010 -> 10000100
1011 -> 10000000
1100 -> 10000010
1101 -> 10000000
1110 -> 10000000
1111 -> 10000001
```

Alternatively, we could just list the hex values in order:
```
 0, 80, 80, c0, 80, a0, 90, 80, 80, 88, 84, 80, 82, 80, 80, 81
```
However, now we lost the information about how many output bits there
are.  Is it really only 8 or are there just zero bits in front of each
output?
```
fn(B^4 -> B^8)[ 0, 80, 80, c0, 80, a0, 90, 80, 80, 88, 84, 80, 82, 80, 80, 81]
```
There.  Now it's obvious that it takes 4 input bits, and has exactly 8
output bits, and it doesn't need 16 lines to write it down.
(Verifyability suffers a bit, but you'll learn to read hex quickly.)


## So how do you enumerate them?

Well, there's only a finite number of binary functions, and the
question of being metastability-containing can be answered in finite
time.  If a juicy, tasty metastability-containing function is found,
output it.  At a very high level, that's essentially all this code
does.

#### But doesn't take this a long time?

Not if you do it right.  [Below](#statistics) you can see that it truly
is pretty fast.

Let's say we consider the abstract space of all functions that take
`#in` input bits and `#out` bits.  Then there will be
`(2^#out)^(2^#in)` functions.  For `#in=#out=4`, this amounts to `2^64`
functions, which I've been told is a lot.

So, the naive approach to just enumerate *everything* obviously doesn't
work.

#### But how *does* it work?

Let's say you're counting up, from 0 to
[18446744073709551616](https://www.wolframalpha.com/input/?i=2^64), and
want to find "nice looking numbers", whatever that means:
```
00000000000000000000
00000000000000000001
00000000000000000002
00000000000000000003
00000000000000000004
00000000000000000005
```
Wait a second! No matter what comes next, the very fact that it starts
with `0000000000` makes it un-nice!  So, we can instead just continue
counting from the next number that doesn't start with that ugly
`00000000000`.
```
00000000001000000000
00000000001000000001
00000000001000000002
```
Did we just count to 1000000000 very quickly?  In a way, yes.  We were
able to make a very general statement about the search space and could
prune a lot of it.

And that's essentially what each `analyzer` in the code does:  read the
current function left-to-right, and yell as early as possible when
something is wrong.  That allows us to make "big steps".

#### Metastability-containing

The analyzer with the name `metastability-containing` does [exactly
what it says on the
tin](http://tvtropes.org/pmwiki/pmwiki.php/Main/ExactlyWhatItSaysOnTheTin):
it reads the function, left to right, and yells as soon as the
function can't possibly be metastability-containing anymore.

Let's take the initial example:
```
fn(B^4 -> B^8)[ 0, 7, …
```
"Stop!", the analyzer yells, "What are you thinking!  We have
`f(0b0000)=0b00000000` and `f(0b0001)=0b00000111`, what the hell!  No way
in hell this function is metastability-containing.  The *second place*
already contradicts my goal!  No matter what you present me in future,
it should differ from this function in the *second place or earlier*!"

After being scolded like this, you increase the second place (and of
course reset all places after it to 0), and try again:

```
fn(B^4 -> B^8)[ 0, 8, …
```
And this time, the analyzer `metastability-containing` just nods,
smiles, listens eagerly, and humbly mumbles "Proceed." while waiting
for your next move.

This brings the functions space down to something that looks like:
```
(#out+1)^(2^#in)
```

#### Input relevance

Let's say we have a constant function.  Or a function that ignores part
of it's input.  "Argh!  Do away with this abolishment!" you might
exclaim, and so does this `analyzer`.  That's the whole point.

The only downside:  it doesn't play well with the idea of "yelling
early".  Instead, it waits until the last moment, only informing you
that this specific function isn't too great.

This may have room for improvement. I don't know. Have fun, in case you
want to do this. However, there are [other potential
optimizations](#missing-optimizations) which may be much more
effective.

It doesn't reduce the function space enough to have an impact on my
above estimate of the size.

#### Output order

First, notice that it's not really interesting when two output pins
compute the same function.  (Which can't happen anyway when the overall
function is metastability-containing *and* the output-pin functions are
non-constant.)

However, the actual *order* of the output pins is irrelevant.  We can
view them as unordered.  Thus we enforce an "arbitrary" order upon them.
Specifically, the function for the most significant output-pin must be
the first to switch to '1'.  Note that if multiple output pins
simultaneously switch to 1 for the "first time" (still reading from
left to right), then the function can't possibly be
metastability-containing.  Thus, we only have to remember the input at
which an output pin first became 1.  If this happens before (or
simultaneously with) an "earlier" output pin had its first 1,
then we know that the given function is just a permutation of another
function we already considered (and didn't exclude).  Also, since every
output pin has to be 1 at some point in time, we can cancel early.  For
example, let's say we haven't yet looked at the last 5 input patterns,
but know that there are 7 output pins which haven't seen their first 1
yet.  Obviously, no matter what those last 5 patterns are, this is not
going to work.

Together with `metastabiliy-containing`, this further reduces the
function space to something like:
```
(#out+1)^(2^#in) / (#out!)
~= (#out+1)^(2^#in - #out)
```

This has the nice accidental behavior of converging to 0 as `#out`
approaches `2^#in`, which makes a lot of sense.  For each pin, there's
only `2^#in` possible functions.  If `#out > 2^#in`, then by pigeonhole
principle some output pins are identical.

#### Output pins depending only on one pin

TOWRITE

Note that this will change how many relevant functions there are, and
may actually reduce the cardinality of certain spaces to 0.

NOTIMPLEMENTED

#### Input order

I still percieve a lot of redundancy and symmetry in the results.  What
is this exactly?  My best guess is "permutations of input bits"

NOTIMPLEMENTED

## Statistics

Here's a raw table.  I've run it only once (ugh) on my machine (erm)
which is pretty noisy (oh god sorry) so if you run this on your
machine, you might end up with different timing.  However, the algorithm
itself is deterministic, so the number of found functions will stay the
same.

<!--

Raw data (csv):

#out \ #in,2,3,4,5
2,3§<.01s,195§<.01s,131667§0.3s,>75M§>300s
3,0§0s,55§<.01s,124086§0.4s,
4,0§0s,2§<.01s,45415§0.2s,
5,0§0s,0§0s,8764§<0.1s,>53M§>300s
6,0§0s,0§0s,1052§<0.1s,
7,0§0s,0§0s,72§<0.01s,
8,0§0s,0§0s,2§2<0.01s,
9,0§0s,0§0s,0§0s,
10,0§0s,0§0s,0§0s,
11,0§0s,0§0s,0§0s,>11M§>120s
12,0§0s,0§0s,0§0s,>9.2M§>120s
13,0§0s,0§0s,0§0s,500312§9s
14,0§0s,0§0s,0§0s,15640§0.4s
15,0§0s,0§0s,0§0s,272§<0.1s
16,0§0s,0§0s,0§0s,2§<0.01s


steps,2,3,4,5
2,13,828,551K,>380M
3,0,1073,1.6M,
4,0,307,1.6M,
5,0,0,708K,>1.3B
6,0,0,164K,
7,0,0,20K,
8,0,0,1007,
9,0,0,0,
10,0,0,0,
11,0,0,0,>1.3B
12,0,0,0,>1.6B
13,0,0,0,130M
14,0,0,0,6.6M
15,0,0,0,217K
16,0,0,0,3818



Process with:

sed -re 's%<%\&lt;%g' | \
sed -re 's%>%\&gt;%g' | \
sed -re 's%§([^,]*)(,|$)%; <code>\1</code>\2%g' | \
sed -re 's%,%</td><td align="right">%g' | \
sed -re 's%^%<tr><th align="right">%g' | \
sed -re 's%$%</td></tr>%g' | \
awk 'BEGIN {print "<table>"} NR==1 {gsub("td","th");print} NR!=1 {print} END {print "</table>"}'

Note that <th></td> is parsed as <th></th>, so no need to clean up.

-->

<table>
<tr><th align="right">#out \ #in</th><th align="right">2</th><th align="right">3</th><th align="right">4</th><th align="right">5</th></tr>
<tr><th align="right">2</td><td align="right">3; <code>&lt;.01s</code></td><td align="right">195; <code>&lt;.01s</code></td><td align="right">131667; <code>0.3s</code></td><td align="right">&gt;75M; <code>&gt;300s</code></td></tr>
<tr><th align="right">3</td><td align="right">0; <code>0s</code></td><td align="right">55; <code>&lt;.01s</code></td><td align="right">124086; <code>0.4s</code></td><td align="right"></td></tr>
<tr><th align="right">4</td><td align="right">0; <code>0s</code></td><td align="right">2; <code>&lt;.01s</code></td><td align="right">45415; <code>0.2s</code></td><td align="right"></td></tr>
<tr><th align="right">5</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">8764; <code>&lt;0.1s</code></td><td align="right">&gt;53M; <code>&gt;300s</code></td></tr>
<tr><th align="right">6</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">1052; <code>&lt;0.1s</code></td><td align="right"></td></tr>
<tr><th align="right">7</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">72; <code>&lt;0.01s</code></td><td align="right"></td></tr>
<tr><th align="right">8</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">2; <code>2&lt;0.01s</code></td><td align="right"></td></tr>
<tr><th align="right">9</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right"></td></tr>
<tr><th align="right">10</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right"></td></tr>
<tr><th align="right">11</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">&gt;11M; <code>&gt;120s</code></td></tr>
<tr><th align="right">12</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">&gt;9.2M; <code>&gt;120s</code></td></tr>
<tr><th align="right">13</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">500312; <code>9s</code></td></tr>
<tr><th align="right">14</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">15640; <code>0.4s</code></td></tr>
<tr><th align="right">15</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">272; <code>&lt;0.1s</code></td></tr>
<tr><th align="right">16</td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">0; <code>0s</code></td><td align="right">2; <code>&lt;0.01s</code></td></tr>
</table>

What does this table mean?  For example, let's look at the entry
`#in=4, #out=7`, which says "72; <0.01s".  This means that it found 72
relevant functions in less than 0.01 seconds.  Note that several of the
entries for `#in=5` weren't allowed to finish and were aborted instead,
because I'm impatient.  Note that in these cases, it looked to me like
most of the function space wasn't even explored, so the given numbes
may or may not correlate with the numbers if it were allowed to finish.

Also, here's a table of how many steps each computation took:

<table>
<tr><th align="right">steps</th><th align="right">2</th><th align="right">3</th><th align="right">4</th><th align="right">5</th></tr>
<tr><th align="right">2</td><td align="right">13</td><td align="right">828</td><td align="right">551K</td><td align="right">&gt;380M</td></tr>
<tr><th align="right">3</td><td align="right">0</td><td align="right">1073</td><td align="right">1.6M</td><td align="right"></td></tr>
<tr><th align="right">4</td><td align="right">0</td><td align="right">307</td><td align="right">1.6M</td><td align="right"></td></tr>
<tr><th align="right">5</td><td align="right">0</td><td align="right">0</td><td align="right">708K</td><td align="right">&gt;1.3B</td></tr>
<tr><th align="right">6</td><td align="right">0</td><td align="right">0</td><td align="right">164K</td><td align="right"></td></tr>
<tr><th align="right">7</td><td align="right">0</td><td align="right">0</td><td align="right">20K</td><td align="right"></td></tr>
<tr><th align="right">8</td><td align="right">0</td><td align="right">0</td><td align="right">1007</td><td align="right"></td></tr>
<tr><th align="right">9</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right"></td></tr>
<tr><th align="right">10</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right"></td></tr>
<tr><th align="right">11</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">&gt;1.3B</td></tr>
<tr><th align="right">12</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">&gt;1.6B</td></tr>
<tr><th align="right">13</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">130M</td></tr>
<tr><th align="right">14</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">6.6M</td></tr>
<tr><th align="right">15</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">217K</td></tr>
<tr><th align="right">16</td><td align="right">0</td><td align="right">0</td><td align="right">0</td><td align="right">3818</td></tr>
</table>

One can nicely see here that with increasing `#out`, the [pigeonhole
principle I mentioned](#output-order) does its magic.

## HACKING

#### This document

Write the rest.

Spend more computation time on the ["table"](#statistics) above.

#### Missing optimizations

TOWRITE

[missing analyzer](#output-pins-depending-only-on-one-pin)

See all `TODO`s in the code, and do the respective benchmarking.


#### Missing features

Enable `DEBUG_PRINT` and use these numbers to better understand current
behavior of the `analyzer`s.  Maybe don't run certain analyzers at all,
if `last_change` is already low enough?

#### Understanding the starting patterns

Here's what the nearly-first relevant function looks like for each `#out`:
```
#out=2 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0]
#out=3 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 4, 0]
#out=4 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 4, 8, 0]
#out=5 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 4, 0, 8,10, 0]
#out=6 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 4, 0, 0, 8, 0,10,20, 0]
#out=7 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 4, 8, 0, 0,10, 0,20,40, 0]
#out=8 => ... 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 4, 0, 0, 8,10, 0, 0,20, 0,40,80, 0]
#out=12=> ... 1, 2, 0, 4, 0, 0, 8, 0,10,20, 0,40, 0, 0,80,XX, 0, 0,XX, 0,XX,XX, 0]
              ^  ^     ^        ^     ^  ^     ^        ^  ^        ^     ^  ^- ..111110
              |  |     |        |     |  |     |        |  |        |     *---- ..111101
              |  |     |        |     |  |     |        |  |        *---------- ..111011
              |  |     |        |     |  |     |        |  *------------------- ..111000
              |  |     |        |     |  |     |        *---------------------- ..110111
              |  |     |        |     |  |     *------------------------------- ..110100
              |  |     |        |     |  *------------------------------------- ..110010
              |  |     |        |     *---------------------------------------- ..110001
              |  |     |        *---------------------------------------------- ..101111
              |  |     *------------------------------------------------------- ..101100
              |  *------------------------------------------------------------- ..101010
              *---------------------------------------------------------------- ..101001
```

The pattern seems to look like this:  starting with `..111110`, count
down until you find a pattern that has Hamming distance 2 to each other
pattern reserved so far.  Repeat until done.  If this pattern holds
true, then this should be the *a* (not necessarily the first)
interesting function with `#out=16`:
```
f(111..11110) = 8000
f(111..11101) = 4000
f(111..11011) = 2000
f(111..11000) = 1000
f(111..10111) =  800
f(111..10100) =  400
f(111..10010) =  200
f(111..10001) =  100
f(111..01111) =   80
f(111..01100) =   40
f(111..01010) =   20
f(111..01001) =   10
f(111..00110) =    8
f(111..00101) =    4
f(111..00011) =    2
f(111..00000) =    1
f(otherwise)  =    0
```
Let's try to build a `fn(B^5->B^16)` from that.  Canonicalize the above
to the form the program expects by inverting the least significant
output pin:
```
fn(B^5 -> B^16)
[   0,    1,    1,    3,    1,    5,    9,    1,
    1,   11,   21,    1,   41,    1,    1,   81,
    1,  101,  201,    1,  401,    1,    1,  801,
 1001,    1,    1, 2001,    1, 4001, 8001,    1]
```
Verifying this by hand yields:  Yep, that's a metastability-containing
function.

Additionally, the program finds this other function:
```
fn(B^5 -> B^16)
[   0,    1,    2,    0,    4,    0,    0,    8,
   10,    0,    0,   20,    0,   40,   80,    0,
  100,    0,    0,  200,    0,  400,  800,    0,
    0, 1000, 2000,    0, 4000,    0,    0, 8000]
```

Not sure how to upper bound this appropriately, but it quickly leads to
[the previous result](#output-order) `#out <= 2^#in`.  Of each
consecutive group of two (starting at the end, so one has `...xyz0` and
`...xyz1`), at most one can make it on the list.  This leads to the
more interesting result `#out <= 2^(#in - 1)`.  Note that this bound
seems to be tight at least for `#in <= 5`, as proven by example
(previous construction and [statistics section](#statistics)).

The difference to each *actually* first function seems to only depend
on whether the all-zeroes input-pattern ends up on the list.
