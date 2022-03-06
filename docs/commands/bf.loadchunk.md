Restores a filter previously saved using `SCANDUMP`. See the `SCANDUMP` command
for example usage.

This command overwrites any bloom filter stored under `key`. Make sure that
the bloom filter is not be changed between invocations.

### Parameters

* **key**: Name of the key to restore
* **iter**: Iterator value associated with `data` (returned by `SCANDUMP`)
* **data**: Current data chunk (returned by `SCANDUMP`)

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

See BF.SCANDUMP for an example.