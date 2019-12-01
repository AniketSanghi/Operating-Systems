#!/bin/bash

make serial_hash parallel_hash

./serial_hash $1 $2 1 > ./serial/serial.output

sort thread-1.out > ./serial/check.output

rm "thread-1.out"

./parallel_hash $1 $2 $3 > ./parallel/parallel.output

if [ -f "./parallel/check.output.temp" ]
then
    rm "./parallel/check.output.temp"
fi

for i in `seq 1 $3`
do
    cat "thread-$i.out" >> ./parallel/check.output.temp
    rm "thread-$i.out"
done

sort ./parallel/check.output.temp > ./parallel/check.output
rm ./parallel/check.output.temp

echo "Checking Output Diff"

# cat ./parallel/parallel.output | awk '{print $1 $2 $3 $4}' > ./parallel/parallel.o.output
# cat ./serial/serial.output | awk '{print $1 $2 $3 $4' > ./serial/serial.o.output
# diff ./parallel/parallel.o.output ./serial/serial.o.output

echo "Checking Thread Diff"

diff ./parallel/check.output ./serial/check.output

echo "Checking Diff Completed"
