Retrieve, for each floating-point input value, the estimated rank of the value (the number of observations in the sketch that are smaller than the value + half the number of observations that are equal to the value).

Multiple ranks can be retrieved in a signle call.

## Required arguments
<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

<details open><summary><code>value</code></summary>
is input value for which the rank should be estimated.

## Return value

@array-reply - an array of integers populated with rank_1, rank_2, ..., rank_V:
  
- -1 - when `value` is smaller than the value of the smallest observation.
- The number of observations - when `value` is larger than the value of the largest observation.
- Otherwise: an estimation of the number of (observations smaller than `value` + half the observations equal to `value`).
  
0 is the rank of the value of the smallest observation.

_n_-1 is the rank of the value of the largest observation; _n_ denotes the number of observations added to the sketch.
  
All values are -2 if the sketch is empty.  

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE s COMPRESSION 1000
OK
redis> TDIGEST.ADD s 10 20 30 40 50 60
OK
redis> TDIGEST.RANK s 0 10 20 30 40 50 60 70
1) (integer) -1
2) (integer) 1
3) (integer) 2
4) (integer) 3
5) (integer) 4
6) (integer) 5
7) (integer) 6
8) (integer) 6
redis> TDIGEST.REVRANK s 0 10 20 30 40 50 60 70
1) (integer) 6
2) (integer) 5
3) (integer) 4
4) (integer) 3
5) (integer) 2
6) (integer) 1
7) (integer) 0
8) (integer) -1  
{{< / highlight >}}
  
{{< highlight bash >}}
redis> TDIGEST.CREATE s COMPRESSION 1000
OK
redis> TDIGEST.ADD s 10 10 10 10 20 20
OK
redis> TDIGEST.RANK s 10 20
1) (integer) 2
2) (integer) 5
redis> TDIGEST.REVRANK s 10 20
1) (integer) 4
2) (integer) 1
{{< / highlight >}}
