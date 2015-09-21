#!/usr/bin/env python

import sys

def load_file(filename):
    fh = open(filename, "r")
    out = list()
    while True:
        line = fh.readline()
        if line.startswith("#"):
            continue;
        if not line:
            break
        words = line.split()

        if words[4].isdigit():
            words[4] = words[5]

        name = words[4].strip()
        out.append({'name': name,
                    'procid': int(words[0]),
                    'threadid': int(words[1][:-1]),
                    'start': int(words[2]),
                    'length': int(words[3]),
                    'end': int(words[2]) + int(words[3])})
    fh.close()
    return out

filename = "schedule.dat"
if len(sys.argv) > 1:
    filename = sys.argv[1]

tasks = load_file(filename)

procids = [task['procid'] for task in tasks]
procids = set(procids)

min_time = dict()
for procid in procids:
    min_time[procid] = min([task['start'] for task in tasks if task['procid'] == procid])

for task in tasks:
    task['start'] -= min_time[task['procid']]
    task['end'] -= min_time[task['procid']]

tasum = dict()
for task in tasks:
    if task['name'] in tasum:
        (num, length) = tasum[task['name']]
        tasum[task['name']] = (num + 1, length + task['length']/1000000.0)
    else:
        tasum[task['name']] = (1, task['length']/1000000.0)

threadids = set()
for task in tasks:
    threadids.add(task['threadid'])
num_threads = len(threadids)

total = sum([task['length'] for task in tasks]) / 1000000.0
end_time = max([task['end'] for task in tasks]) / 1000000.0

full_time = end_time * num_threads * len(procids)
idle_time = full_time - total

s = sorted(tasum, key=lambda x: tasum[x])

def pf(name, num, time):
    print("{:16s}".format(name), "{0:10d}".format(num), "{0:10.2f}".format(time), "{0:6.2f} %".format(time/total*100), "{0:6.2f} %".format(time/full_time*100))

def pf2(name, time):
    print("{:16s}".format(name), "----------", "{0:10.2f}".format(time), "--------", "{0:6.2f} %".format(time/full_time*100))

print('end_time   ', "{0:10.2f}".format(end_time))
print('parallelism', "{0:10.2f}".format(total/end_time))
print('efficiency ', "{0:10.2f} %".format(total/full_time*100))
print('num threads:', num_threads)
print('total_time:', total)
print()
for i in s:
    (num, length) = tasum[i]
    pf(i, num, length)
pf2('idle', idle_time)
