#!/bin/zsh

for i in {1..10}
do
   python3.11 gen.py
   ./test.sh &> result$i.txt
done
