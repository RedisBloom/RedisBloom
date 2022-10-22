Retrieve, for each input value, the estimated rank of value (the number of observations in the sketch that are smaller than the value + half the number of observations that are equal to the value).

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

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.ADD t 10 20 30 40 50
OK
redis>TDIGEST.RANK t -5 40 30 100
1) (integer) -1
2) (integer) 4
3) (integer) 3
4) (integer) 5
{{< / highlight >}}
