#!/usr/bin/env python3

# Copyright (c) 2021 Arm Limited
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from   pathlib import Path
import os
import sys

def best_match(p, paths):
    idx = 0
    while True:
        idx += 1
        if idx >= len(p.parts):
            return None
        try:
            paths = list(filter(
                lambda q: q.parts[-idx] == p.parts[-idx] if idx < len(q.parts) else False,
                paths
            ))
        except IndexError:
            return None
        if len(paths) == 1:
            return paths[0]

def restructure(mbed, old_cordio, new_cordio):
    mbed_files = list(Path(mbed).glob('**/*.c')) + list(Path(mbed).glob('**/*.h'))
    old_files = list(Path(old_cordio).glob('**/*.c')) + list(Path(old_cordio).glob('**/*.h'))
    new_files = list(Path(new_cordio).glob('**/*.c')) + list(Path(new_cordio).glob('**/*.h'))

    dir_move = dict()
    unmoved = []

    # Pass 1: In NEW CORDIO, move files that were moved between OLD CORDIO and MBED, and delete files that were in OLD
    # CORDIO but weren't included in MBED.
    total = len(new_files)
    count = 0
    for newf in new_files:
        oldf = best_match(newf, old_files)
        mbedf = best_match(newf, mbed_files)
        if not oldf:
            # File was added to Cordio
            unmoved.append(newf)
        elif not mbedf:
            # File was removed by ARM
            print('rm -f \"{}\"'.format(newf.relative_to(new_cordio)))
        else:
            # File was moved by ARM
            newf_rel = newf.relative_to(new_cordio)
            mbedf_rel = mbedf.relative_to(mbed)
            target = '/'.join(mbedf_rel.parts[:-1])
            dir_move[newf_rel.parts[:-1]] = target
            print(f'mkdir -p \"{target}\"')
            print(f'mv -f \"{newf_rel}\" \"{mbedf_rel}\"')
        count += 1
        print('\rPass 1: {}/{} files ({}%)'.format(count, total, 100*count//total), file=sys.stderr, end='')
    print('', file=sys.stderr)

    # Pass 2: In NEW CORDIO, move files that were added to NEW CORDIO to where their siblings were moved where possible.
    total = len(unmoved)
    count = 0
    unmovable = 0
    for newf in unmoved:
        newf_rel = newf.relative_to(new_cordio)
        target_dir = dir_move.get(newf_rel.parts[:-1])
        if target_dir:
            print(f'mv -f \"{newf_rel}\" \"{target_dir}\"')
        else:
            unmovable += 1
        count += 1
        print('\rPass 2: {}/{} files ({}%)'.format(count, total, 100*count//total), file=sys.stderr, end='')
    print(f'\nDone', file=sys.stderr)

if __name__ == '__main__':
    if len(sys.argv) != 4 or not all([os.path.isdir(p) for p in sys.argv[1:]]):
        print(f'Usage: {sys.argv[0]} <mbed OS> <old cordio> <new cordio>')
    restructure(
        os.path.realpath(sys.argv[1]),
        os.path.realpath(sys.argv[2]),
        os.path.realpath(sys.argv[3])
    )
