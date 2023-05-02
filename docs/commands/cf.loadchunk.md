Restores a cuckoo filter previously saved using `CF.SCANDUMP`.

See the `CF.SCANDUMP` command for example usage.

<note><b>Notes</b>

- This command overwrites the cuckoo filter stored under `key`.
- Make sure that the cuckoo filter is not changed between invocations.

</note>

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to restore.
</details>

<details open><summary><code>iterator</code></summary>

Iterator value associated with `data` (returned by `CF.SCANDUMP`)
</details>

<details open><summary><code>data</code></summary>

Current data chunk (returned by `CF.SCANDUMP`)
</details>

## Return value

Returns one of these replies:

- @simple-string-reply - `OK` if executed correctly
- @error-reply on error (invalid arguments, wrong key type, wrong data, etc.)

## Examples

See `CF.SCANDUMP` for an example.
