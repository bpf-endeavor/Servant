import os
import sys


def report_latency_measures(f):
    arr = []
    count = 0
    for line in f:
        line.strip()
        val, c = tuple(map(lambda x: int(x), line.split(',')))
        arr.extend([val] * c)
        count += c

    percentiles = [0.1, 0.5, 0.9, 0.99, 0.999]
    for p in percentiles:
        index = int(count * p)
        print('@{}: {:.2f} us'.format(p, arr[index] / 1000))



target_dir = os.path.dirname(sys.argv[0])
target_dir = os.path.abspath(target_dir)

for exp in sorted(os.listdir(target_dir)):
    path = os.path.join(target_dir, exp)
    if not os.path.isdir(path):
        continue
    subdirs = os.listdir(path)
    print('\n\n')
    print('Experiment:', exp)
    for sub in subdirs:
        path2 = os.path.join(path, sub)
        hist_path = os.path.join(path2, 'histogram.csv')
        print('Attempt:', sub)
        print('-' * 40)
        with open(hist_path, 'r') as f:
            report_latency_measures(f)

