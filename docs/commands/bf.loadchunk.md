Restores a Bloom filter previously saved using `BF.SCANDUMP`.

See the `BF.SCANDUMP` command for example usage.

<note><b>Notes</b>

- This command overwrites the Bloom filter stored under `key`. 
- Make sure that the Bloom filter is not changed between invocations.

</note>

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to restore.
</details>

<details open><summary><code>iterator</code></summary>

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
