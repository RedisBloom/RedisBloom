Begins an incremental save of the cuckoo filter.

This command is useful for large cuckoo filters that cannot fit into the `DUMP` and `RESTORE` model.

The first time this command is called, the value of `iter` should be 0. 

This command returns successive `(iter, data)` pairs until `(0, NULL)` indicates completion.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to save.
</details>

<details open><summary><code>iterator</code></summary>

Iterator value; either 0 or the iterator from a previous invocation of this command
</details>

## Return value

Either

- @array-reply of @integer-reply (_Iterator_) and @binary-reply (_Data_). 

  The Iterator is passed as input to the next invocation of `CF.SCANDUMP`. If _Iterator_ is 0, then it means iteration has completed.

  The iterator-data pair should also be passed to `CF.LOADCHUNK` when restoring the filter.

- @error-reply on error (invalid arguments, key not found, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> CF.RESERVE cf 8
OK
redis> CF.ADD cf item1
(integer) 1
redis> CF.SCANDUMP cf 0
1) (integer) 1
2) "\x01\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x02\x00\x14\x00\x01\x008\x9a\xe0\xd8\xc3\x7f\x00\x00"
redis> CF.SCANDUMP cf 1
1) (integer) 9
2) "\x00\x00\x00\x00\a\x00\x00\x00"
redis> CF.SCANDUMP cf 9
1) (integer) 0
2) (nil)
redis> DEL bf
(integer) 1
redis> CF.LOADCHUNK cf 1 "\x01\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x02\x00\x14\x00\x01\x008\x9a\xe0\xd8\xc3\x7f\x00\x00"
OK
redis> CF.LOADCHUNK cf 9 "\x00\x00\x00\x00\a\x00\x00\x00"
OK
redis> CF.EXISTS cf item1
(integer) 1
{{< / highlight >}}

Python code:
{{< highlight bash >}}
chunks = []
iter = 0
while True:
    iter, data = CF.SCANDUMP(key, iter)
    if iter == 0:
        break
    else:
        chunks.append([iter, data])

# Load it back
for chunk in chunks:
    iter, data = chunk
    CF.LOADCHUNK(key, iter, data)
{{< / highlight >}}
