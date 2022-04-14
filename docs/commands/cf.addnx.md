Adds an item to a cuckoo filter if the item did not exist previously.
See documentation on `CF.ADD` for more information on this command.

This command is equivalent to a `CF.CHECK` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists in order to
use the available capacity more efficiently. However, deleting
elements can introduce **false negative** error rate!

Note that this command is slower than `CF.ADD` because it first checks whether the
item exists.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

@return

@integer-reply - where "1" means the item has been added to the filter, and "0" mean, the item already existed.

@examples

```
redis> CF.ADDNX cf item1
(integer) 0
redis> CF.ADDNX cf item_new
(integer) 1
```