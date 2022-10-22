Retrieve, for each input reverse rank, an estimation of the value (floating-point) with that reverse rank.

Multiple estimations can be retrieved in a signle call.

## Required arguments

<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

<details open><summary><code>revrank</code></summary>

Reverse rank, for which the value should be determined.

0 is the reverse rank of the value of the largest observation.
  
_n_-1 is the reverse rank of the value of the smallest observation,  where _n_ denotes the number of observations added to the sketch.
</details>

## Return value

@array-reply - an array of floating-points populated with value_1, value_2, ..., value_N:

- Return an accurate result when `revrank` is 0 (the value of the largest observation)
- Return an accurate result when `revrank` is _n_-1 (the value of the smallest observation), where _n_ denotes the number of observations added to the sketch.
- Return 'inf' when `revrank` is equal to _n_ or larger than _n_

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t COMPRESSION 1000
OK
redis> TDIGEST.ADD t 1 2 2 3 3 3 4 4 4 4 5 5 5 5 5
OK
redis> TDIGEST.BYREVRANK t 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
 1) "5"
 2) "5"
 3) "5"
 4) "5"
 5) "5"
 6) "4"
 7) "4"
 8) "4"
 9) "4"
10) "3"
11) "3"
12) "3"
13) "2"
14) "2"
15) "1"
16) "-inf"
{{< / highlight >}}
