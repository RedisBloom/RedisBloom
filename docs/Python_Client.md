# redisbloom-py 
A Python Client Library for RedisBloom.

### Overview
This library allows python developers an easy access to RedisBloom. The package extends [redis-py](https://github.com/andymccurdy/redis-py)'s interface.  

### Installation
``` 
$ pip install redisbloom
```

### Usage example
```sql
# Using Bloom Filter
from redisbloom import Client
rb = Client()
rb.bfCreate('bloom', 0.01, 1000)
rb.bfAdd('bloom', 'foo')        # returns 1
rb.bfAdd('bloom', 'foo')        # returns 0
rb.bfExists('bloom', 'foo')     # returns 1
rb.bfExists('bloom', 'noexist') # returns 0

# Using Cuckoo Filter
from redisbloom import Client
rb = Client()
rb.cfCreate('cuckoo', 1000)
rb.cfAdd('cuckoo', 'filter')        # returns 1
rb.cfAddNX('cuckoo', 'filter')      # returns 0
rb.cfExists('cuckoo', 'filter')     # returns 1
rb.cfExists('cuckoo', 'noexist')    # returns 0

# Using Cuckoo Filter
from redisbloom import Client
rb = Client()
rb.cmsInitByDim('dim', 1000, 5)
rb.cmsIncrBy('dim', ['foo'], [5])
rb.cmsIncrBy('dim', ['foo', 'bar'], [5, 15])
rb.cmsQuery('dim', 'foo', 'bar')    # returns [10, 15]

# Using Cuckoo Filter
from redisbloom import Client
rb = Client()
rb.topkReserve('topk', 3, 20, 3, 0.9)
rb.topkAdd('topk', 'A', 'B', 'C', 'D', 'E', 'A', 'A', 'B', 
                   'C', 'G', 'D', 'B', 'D', 'A', 'E', 'E')
rb.topkQuery('topk', 'A', 'B', 'C', 'D')    # returns [1, 1, 0, 1]
rb.topkCount('topk', 'A', 'B', 'C', 'D')    # returns [4, 3, 2, 3]
rb.topkList('topk')                         # returns ['D', 'A', 'B']
```

### License
[BSD 3-Clause](https://github.com/RedisBloom/redisbloom-py/blob/master/LICENSE)