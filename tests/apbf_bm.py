import redis
import time

def detect(list_a, list_b):
	return len(set(list_a).intersection(list_b)) / float(len(list_a))

def insert(series, from_n, to_n):
    for i in range(from_n, to_n):
        redis_pipe.execute_command('apbf.add', series, i)
    redis_pipe.execute()

def query(series, pos_from, pos_to, neg_to):
    pos_count = 0.0
    for i in range(int(pos_from), int(pos_to)):
        redis_pipe.execute_command('apbf.exists', series, i)
        if i % 100 is 99:
            responses = redis_pipe.execute()
            for response in responses:
                pos_count +=response[0]
    print ("True Positive " + str(round(pos_count / int(pos_to - pos_from), 3)))

    neg_count = 0.0
    for i in range(0, int(neg_to)):
        redis_pipe.execute_command('apbf.exists', series, i)
        if i % 100 is 99:
            responses = redis_pipe.execute()
            for response in responses:
                neg_count +=response[0]
    print ("False Negative " + str(round(neg_count / neg_to, 3)))

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

start_time = time.time()

tests = 50000
redis.execute_command('apbf.reserve apbf 2', tests)
series = 'apbf'
insert(series, 0, tests * 2)
query(series, tests * 1.1, tests * 2, tests * 0.9)
insert(series, tests * 2, tests * 3)
query(series, tests * 2.1, tests * 3, tests * 1.9)
insert(series, tests * 3, tests * 4)
query(series, tests * 3.1, tests * 4, tests * 2.9)
insert(series, tests * 4, tests * 5)
query(series, tests * 4.1, tests * 5, tests * 3.9)
insert(series, tests * 5, tests * 6)
query(series, tests * 5.1, tests * 6, tests * 4.9)