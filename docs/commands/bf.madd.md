Adds one or more items to the Bloom Filter and creates the filter if it does not exist yet.
This command operates identically to `BF.ADD` except that it allows multiple inputs and returns
multiple values.

### Parameters

* **key**: The name of the filter
* **item**: One or more items to add

@return

@array-reply of @integer-reply - for each item which is either "1" or "0" depending
on whether the corresponding input element was newly added to the filter or may
have previously existed.

@examples

```
redis> BF.MADD bf item1 item2
1) (integer) 0
2) (integer) 1
```