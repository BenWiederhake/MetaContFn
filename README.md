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

Did we just count to 1000000000 very quickly?  In a way, yes.  We were able to make a very general statement about the search space and could prune a lot of it.

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

#### Output order

TOWRITE

First, notice that it's not really interesting when two output pins
compute the same function.  (Which can't happen anyway when the overall
function is metastability-containing *and* the output-pin functions are
non-constant.)

However, the actual *order* of the output pins is irrelevant. We can
view them as unordered. Thus we enforce an "arbitrary" order upon them.
Specifically, the function for the most significant output-pin must be
the first to switch to '1'.  Note that if multiple output pins
simultaneously switch to 1 for the "first time" (still reading from
left to right), then the function can't possibly be
metastability-containing.  Thus, we only have to remember the inout at
which an output pin first became 1.

FIXME: This is a new insight not reflected in the code.  Hooray?

#### Output pins depending only on one pin

TOWRITE

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
2,3@<.01s,195@<.01s,131667@0.5s,>6.3M@>36s
3,0@<.01s,55@<.01s,124086@0.6s,>9.2M@>61s
4,,2@<.01s,45415@0.6s,>5.7M@>64s
5,,0@<.01s,8764@1.4s,>1.9M@>68s
6,,,1052@3.8s,>134K@>56s
7,,,72@9.1s,>12K@>86s
8,,,2@20.6s,>1174@>200s
9,,,0@38s,>=0@>300s
10,,,,>=0@>300s

Process with:

sed -re 's%<%\&lt;%g' | \
sed -re 's%>%\&gt;%g' | \
sed -re 's%@([^,]*)(,|$)%; <code>\1</code>\2%g' | \
sed -re 's%,%</td><td align="right">%g' | \
sed -re 's%^%<tr><th align="right">%g' | \
sed -re 's%$%</td></tr>%g' | \
awk 'BEGIN {print "<table>"} NR==1 {gsub("td","th");print} NR!=1 {print} END {print "</table>"}'

Note that <th></td> is parsed as <th></th>, so no need to clean up.

-->

<table>
<tr><th align="right">#out \ #in</th><th align="right">2</th><th align="right">3</th><th align="right">4</th><th align="right">5</th></tr>
<tr><th align="right">2</td><td align="right">3; <code>&lt;.01s</code></td><td align="right">195; <code>&lt;.01s</code></td><td align="right">131667; <code>0.5s</code></td><td align="right">&gt;6.3M; <code>&gt;36s</code></td></tr>
<tr><th align="right">3</td><td align="right">0; <code>&lt;.01s</code></td><td align="right">55; <code>&lt;.01s</code></td><td align="right">124086; <code>0.6s</code></td><td align="right">&gt;9.2M; <code>&gt;61s</code></td></tr>
<tr><th align="right">4</td><td align="right"></td><td align="right">2; <code>&lt;.01s</code></td><td align="right">45415; <code>0.6s</code></td><td align="right">&gt;5.7M; <code>&gt;64s</code></td></tr>
<tr><th align="right">5</td><td align="right"></td><td align="right">0; <code>&lt;.01s</code></td><td align="right">8764; <code>1.4s</code></td><td align="right">&gt;1.9M; <code>&gt;68s</code></td></tr>
<tr><th align="right">6</td><td align="right"></td><td align="right"></td><td align="right">1052; <code>3.8s</code></td><td align="right">&gt;134K; <code>&gt;56s</code></td></tr>
<tr><th align="right">7</td><td align="right"></td><td align="right"></td><td align="right">72; <code>9.1s</code></td><td align="right">&gt;12K; <code>&gt;86s</code></td></tr>
<tr><th align="right">8</td><td align="right"></td><td align="right"></td><td align="right">2; <code>20.6s</code></td><td align="right">&gt;1174; <code>&gt;200s</code></td></tr>
<tr><th align="right">9</td><td align="right"></td><td align="right"></td><td align="right">0; <code>38s</code></td><td align="right">&gt;=0; <code>&gt;300s</code></td></tr>
<tr><th align="right">10</td><td align="right"></td><td align="right"></td><td align="right"></td><td align="right">&gt;=0; <code>&gt;300s</code></td></tr>
</table>


What does this table mean?  For example, let's look at the entry
`#in=4, #out=7`, which says "72; 9.1s".  This means that it found 72
relevant functions in 9.1 seconds.  Note that none of the entries for
`#in=5` were allowed to finesh.  Instead, computation was aborted,
because I'm impatient.  Note that in these cases, it looked to me like
most of the function space wasn't even explored, so the given numbes
may or may not correlate with the numbers if it were allowed to finish.

## HACKING

#### This document

Write the rest.

Spend more computation time on the ["table"](#statistics) above.

#### Missing optimizations

TOWRITE

[improved bound!](#output-order)

[missing analyzer](#output-pins-depending-only-on-one-pin)

#### Missing features

See all `TODO`s in the code, and do the respective benchmarking.

Enable `DEBUG_PRINT` and use these numbers to better understand current
behavior of the `analyzer`s.  Maybe don't run certain analyzers at all,
if `last_change` is already low enough?

Here's what the first relevant function looks like for each `#out`:
```
#out=2 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0]
#out=3 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 2, 1, 0]
#out=4 => ... 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 4, 0, 2, 0, 0, 1]
#out=5 => ... 0, 0, 0, 0, 0, 0, 0,10, 8, 0, 0, 4, 0, 2, 1, 0]
#out=6 => ... 0, 0, 0, 0, 0,20,10, 0, 0, 8, 4, 0, 2, 0, 0, 1]
#out=7 => ... 0, 0, 0,40, 0,20,10, 0, 0, 8, 4, 0, 2, 0, 0, 1]
```
What can we learn from this pattern?  Why don't I understand it?  (I'm
sure it's not because I'm tired and already falling asleep.)
