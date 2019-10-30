import requests
import redis
import time

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

start_time = time.time()

filters = ['1f1b', '2f1b', '4f1b', '8f1b', '2f2b', '4f2b', '8f2b', '2f4b', '4f4b', '8f4b']

q = 1024 * 4
redis.execute_command('CF.RESERVE 1f1b ' + str(q) + ' BUCKETSIZE 1 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 2f1b ' + str(q / 2) + ' BUCKETSIZE 1 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 4f1b ' + str(q / 4) + ' BUCKETSIZE 1 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 8f1b ' + str(q / 8) + ' BUCKETSIZE 1 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 2f2b ' + str(q / 2) + ' BUCKETSIZE 2 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 4f2b ' + str(q / 4) + ' BUCKETSIZE 2 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 8f2b ' + str(q / 8) + ' BUCKETSIZE 2 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 2f4b ' + str(q / 2) + ' BUCKETSIZE 4 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 4f4b ' + str(q / 4) + ' BUCKETSIZE 4 MAXITERATIONS 50')
redis.execute_command('CF.RESERVE 8f4b ' + str(q / 8) + ' BUCKETSIZE 4 MAXITERATIONS 50')

print 'add'
for i in range(len(filters)):
  for x in xrange(q):
    redis_pipe.execute_command('cf.add ' + filters[i] + ' ' + str(x))
  redis_pipe.execute()

print("--- %s seconds to add ---" % (time.time() - start_time))
start_time = time.time()

print 'check'
size = 100
for i in range(len(filters)):
  count = 0
  for x in xrange(size * q):
    res = redis.execute_command('cf.exists ', filters[i], str(x + size * q))
    #print res
    if res == 1:
      count = count + 1
  print filters[i] + ' error rate is ' + str(round(float(count) / (size * q),4) * 100) + '%'
  print redis.execute_command('cf.debug ' + filters[i]) + '\n'
 

print("--- %s seconds to check ---" % (time.time() - start_time))
start_time = time.time()
'''
print redis.execute_command('cf.debug cf')

redis.execute_command('cf.compact cf')
print("--- %s seconds to compact ---" % (time.time() - start_time))
start_time = time.time()

print redis.execute_command('cf.debug cf')
'''