Return information about a Bloom filter.

### Parameters

* **key**: key name for an existing Bloom filter.

Optional parameters:
* `CAPACITY` Number of unique items that can be stored in this Bloom filter before scaling would be required (including already added items)
* `SIZE` Memory size: number of bytes allocated for this Bloom filter
* `FILTERS` Number of sub-filters
* `ITEMS` Number of items that were added to this Bloom filter and detected as unique (items that caused at least one bit to be set in at least one sub-filter)
* `EXPANSION` Expansion rate

When no optional parameter is specified: return all information fields.

@return

@array-reply with information about the Bloom filter.

Error when `key` does not exist.

Error when `key` is of a type other than Bloom filter.

@examples

```sql
redis> BF.ADD bf1 observation1
(integer) 1
redis> BF.INFO bf1
 1) Capacity
 2) (integer) 100
 3) Size
 4) (integer) 240
 5) Number of filters
 6) (integer) 1
 7) Number of items inserted
 8) (integer) 1
 9) Expansion rate
10) (integer) 2
redis> BF.INFO bf1 CAPACITY
1) (integer) 100
```
