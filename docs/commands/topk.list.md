Return full list of items in Top K list.

### Parameters

* **key**: Name of sketch where item is counted.
* **WITHCOUNT**: Count of each element is returned.  

@return

k (or less) items in Top K list.

@array-reply of @simple-string-reply - the names of items in the TopK list.
If `WITHCOUNT` is requested, @array-reply of @simple-string-reply and 
@integer-reply pairs of the names of items in the TopK list and their count.

@examples

```
TOPK.LIST topk
1) foo
2) 42
3) bar
```

```
TOPK.LIST topk WITHCOUNT
1) foo
2) (integer) 12
3) 42
4) (integer) 7
5) bar
6) (integer) 2
```