#!/bin/sh

echo "Running legacy-printer-app test..."

# Check if binary exists
if [ ! -f "./legacy-printer-app" ]; then
    echo "legacy-printer-app binary not found"
    exit 1
fi

./legacy-printer-app --help > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "legacy-printer-app failed to run"
    exit 1
fi

echo "legacy-printer-app test passed"
exit 0
