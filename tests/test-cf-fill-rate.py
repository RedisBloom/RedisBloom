import requests
import redis
import time

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

start_time = time.time()

filters = [['1b5i', '2b5i', '4b5i', '8b5i'], ['1b10i', '2b10i', '4b10i', '8b10i'],
           ['1b25i', '2b25i', '4b25i', '8b25i'], ['1b50i', '2b50i', '4b50i', '8b50i'],
           ['1b100i', '2b100i', '4b100i', '8b100i'], ['1b500i', '2b500i', '4b500i', '8b500i']]
buckets = [1, 2, 4, 8]
iterations = [5, 10, 25, 50, 100, 500]

q = 1024 * 128
for i in range(len(iterations)):
  for j in range(len(buckets)):
    redis.execute_command('CF.RESERVE ' + filters[i][j] + ' ' + str(q) +
                          ' BUCKETSIZE ' + str(buckets[j]) +
                          ' MAXITERATIONS ' + str(iterations[i]))

print 'add'
for i in range(len(filters)):
  for j in range(len(filters[i])):
    for x in xrange(q * 20):
      redis_pipe.execute_command('cf.add ' + filters[i][j] + ' ' + str(x))
    redis_pipe.execute()
    print redis.execute_command('cf.debug ' + filters[i][j]) + '\n'


print("--- %s seconds to add ---" % (time.time() - start_time))
start_time = time.time()

print 'check'
size = 100
for i in range(len(filters)):
  '''
  count = 0
  for x in xrange(size * q):
    res = redis.execute_command('cf.exists ', filters[i], str(x + size * q))
    #print res
    if res == 1:
      count = count + 1
  print filters[i] + ' error rate is ' + str(round(float(count) / (size * q),4) * 100) + '%'
  print redis.execute_command('cf.debug ' + filters[i]) + '\n'
  '''
 

print("--- %s seconds to check ---" % (time.time() - start_time))
start_time = time.time()
'''
print redis.execute_command('cf.debug cf')

redis.execute_command('cf.compact cf')
print("--- %s seconds to compact ---" % (time.time() - start_time))
start_time = time.time()

print redis.execute_command('cf.debug cf')
'''