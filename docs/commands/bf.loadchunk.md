Restores a filter previously saved using `BF.SCANDUMP`. 

See the `BF.SCANDUMP` command for example usage.

This command overwrites any bloom filter stored under `key`. Make sure that the bloom filter is not be changed between invocations.

### Parameters

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to restore.
</details>

<details open><summary><code>iter</code></summary>

Iterator value associated with `data` (returned by `BF.SCANDUMP`)
</details>

<details open><summary><code>data</code></summary>

Current data chunk (returned by `BF.SCANDUMP`)
</details>

## Return value

Either

- @simple-string-reply - `OK` if executed correctly
- @error-reply on error (invalid arguments, wrong key type, wrong data, etc.)

## Examples

See `BF.SCANDUMP` for an example.
