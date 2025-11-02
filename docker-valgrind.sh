#!/bin/bash
set -e

cd "$(dirname "$0")"

IMAGE_NAME="redisbloom-valgrind"
DOCKERFILE="Dockerfile.valgrind"

echo "=== Building Docker image: $IMAGE_NAME ==="
docker build -f "$DOCKERFILE" -t "$IMAGE_NAME" .

echo ""
echo "=== Image built successfully! ==="
echo ""
echo "Usage examples:"
echo ""
echo "1. Run all tests with Valgrind:"
echo "   docker run --rm $IMAGE_NAME"
echo ""
echo "2. Run specific test:"
echo "   docker run --rm -e TEST=test_cuckoo:testCuckoo.test_num_deletes $IMAGE_NAME"
echo ""
echo "3. Run interactive shell:"
echo "   docker run --rm -it $IMAGE_NAME bash"
echo ""
echo "4. Run with specific test file:"
echo "   docker run --rm -e TESTFILE=test_cuckoo.py $IMAGE_NAME"
echo ""

# Check if user passed arguments
if [ "$#" -gt 0 ]; then
    echo "Running with arguments: $@"
    docker run --rm "$IMAGE_NAME" "$@"
else
    docker run --rm "$IMAGE_NAME"
fi
