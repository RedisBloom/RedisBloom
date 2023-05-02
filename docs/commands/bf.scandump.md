Begins an incremental save of the Bloom filter.

This command is useful for large Bloom filters that cannot fit into the `DUMP` and `RESTORE` model.

The first time this command is called, the value of `iter` should be 0. 

This command returns successive `(iter, data)` pairs until `(0, NULL)` to indicate completion.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to save.
</details>

<details open><summary><code>iterator</code></summary>

Iterator value; either 0 or the iterator from a previous invocation of this command
</details>

## Return value

Returns one of these replies:

- @array-reply of @integer-reply (_Iterator_) and @binary-reply (_Data_). 

  The Iterator is passed as input to the next invocation of `BF.SCANDUMP`. If _Iterator_ is 0, then it means iteration has completed.

  The iterator-data pair should also be passed to `BF.LOADCHUNK` when restoring the filter.

- @error-reply on error (invalid arguments, key not found, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> BF.RESERVE bf 0.1 10
OK
redis> BF.ADD bf item1
1) (integer) 1
redis> BF.SCANDUMP bf 0
1) (integer) 1
2) "\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x02\x00\x00\x00\b\x00\x00\x00\x00\x00\x00\x00@\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x9a\x99\x99\x99\x99\x99\xa9?J\xf7\xd4\x9e\xde\xf0\x18@\x05\x00\x00\x00\n\x00\x00\x00\x00\x00\x00\x00\x00"
redis> BF.SCANDUMP bf 1
1) (integer) 9
2) "\x01\b\x00\x80\x00\x04 \x00"
redis> BF.SCANDUMP bf 9
1) (integer) 0
2) ""
redis> DEL bf
(integer) 1
redis> BF.LOADCHUNK bf 1 "\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x02\x00\x00\x00\b\x00\x00\x00\x00\x00\x00\x00@\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x9a\x99\x99\x99\x99\x99\xa9?J\xf7\xd4\x9e\xde\xf0\x18@\x05\x00\x00\x00\n\x00\x00\x00\x00\x00\x00\x00\x00"
OK
redis> BF.LOADCHUNK bf 9 "\x01\b\x00\x80\x00\x04 \x00"
OK
redis> BF.EXISTS bf item1
(integer) 1
{{< / highlight >}}

Python code:
{{< highlight bash >}}
chunks = []
iter = 0
while True:
    iter, data = BF.SCANDUMP(key, iter)
    if iter == 0:
        break
    else:
        chunks.append([iter, data])

# Load it back
for chunk in chunks:
    iter, data = chunk
    BF.LOADCHUNK(key, iter, data)
{{< / highlight >}}
