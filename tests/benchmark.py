import requests
import redis
import time

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

def create_topk(ctx, k, width, depth):
    ctx.execute_command('topk.reserve', 'bm_topk', k, k * width, depth, 0.7)

def detect(list_a, list_b):
	return len(set(list_a).intersection(list_b)) / float(len(list_a))

def insertSS(book):
	print "\nUsing sorted set with pipeline"
#for line in page:
	for line in book.iter_lines():
		if line is not '' and line is not ' ':
			for word in line.split():
				redis_pipe.zincrby('bm_text', 1, word)

	responses = redis_pipe.execute()
	for response in responses:
		pass

# test Top-K
def insertTopK(book):
	print("K Width(*k) Depth Memory Accuracy Time")
	k_list = [10, 50, 100, 1000]
	for k in k_list:
		real_results = redis.zrevrange('bm_text', 0, k - 1)
		for width in [5, 10, 15]:
			for depth in [5, 10, 15]:
				redis.execute_command('DEL', 'bm_topk')
				create_topk(redis, k, width, depth)
				start_time = time.time()

				for line in book.iter_lines():
					if line is not '' and line is not ' ':
						a = line.split()
						redis_pipe.execute_command('topk.add', 'bm_topk', *a)

				responses = redis_pipe.execute()
				for response in responses:
					pass

				leaderboard = redis.execute_command('topk.list', 'bm_topk')
				print(str(k) + " " + str(width) + " " + str(depth) + " " +
						str(redis.memory_usage('bm_topk')) + " " +
						str(detect(real_results, leaderboard) * 100) + " " +
						str(time.time() - start_time))

start_time = time.time()
print "Downloading data"
war_and_piece = requests.get('http://www.gutenberg.org/files/2600/2600-0.txt')

'''
crime_and_punishment = requests.get('http://www.gutenberg.org/files/2554/2554-0.txt')
dracula = requests.get('http://www.gutenberg.org/ebooks/345.txt.utf-8')
'''

print("--- %s seconds ---" % (time.time() - start_time))
start_time = time.time()

'''
insertSS(dracula)
insertSS(crime_and_punishment)
'''
insertSS(war_and_piece)
insertSS(war_and_piece)
insertSS(war_and_piece)

real_results = redis.zrevrange('bm_text', 0, 49)
print("--- %s seconds ---" % (time.time() - start_time))
print('Memory used %s'% redis.memory_usage('bm_text'))
print('This is an accurate list for comparison')
print(redis.zcount('bm_text', '-inf', '+inf'))

'''
insertTopK(dracula)
insertTopK(crime_and_punishment)
'''
insertTopK(war_and_piece)
insertTopK(war_and_piece)
insertTopK(war_and_piece)
