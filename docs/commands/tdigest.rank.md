Retrieve, for each floating-point input value, the estimated rank of the value (the number of observations in the sketch that are smaller than the value + half the number of observations that are equal to the value).

Multiple ranks can be retrieved in a signle call.

## Required arguments
<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

<details open><summary><code>value</code></summary>
is input value for which the rank should be estimated.

## Return value

@array-reply - an array of results populated with rank_1, rank_2, ..., rank_N:
  
- -1 - when value is smaller than the value of the smallest observation
- The number of observations - when value is larger than the value of the largest observation
- Otherwise: an estimation of the number of (observations smaller than a given value + half the observations equal to the given value).
  
The rank of the smallest observation, when there is a single smallest observation, is 0.

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.ADD t 10 20 30 40 50
OK
redis> TDIGEST.RANK t -5 10 40 30 100
1) (integer) -1
2) (integer) 1
3) (integer) 4
4) (integer) 3
5) (integer) 5
{{< / highlight >}}
  
  
redis> TDIGEST.CREATE s
OK
redis> TDIGEST.ADD s 10 10 10 10 20
OK
redis> TDIGEST.RANK s 10
1) (integer) 2
