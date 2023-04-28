Returns the cardinality of a Bloom filter - number of items that were added to a Bloom filter and detected as unique (items that caused at least one bit to be set in at least one sub-filter)

(since RedisBloom 2.4.4)

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter.

</details>

## Return value
 
Either

- @integer-reply - the number of items that were added to this Bloom filter and detected as unique (items that caused at least one bit to be set in at least one sub-filter), or 0 when `key` does not exist.
- @error-reply on error (invalid arguments, wrong key type, etc.)

Note: when `key` exists - return the same value as `BF.INFO key ITEMS`.

## Examples

{{< highlight bash >}}
redis> BF.ADD bf1 item_foo
(integer) 1
redis> BF.CARD bf1
(integer) 1
redis> BF.CARD bf_new
(integer) 0
{{< / highlight >}}
