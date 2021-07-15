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

import math
import sys

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} separator <separated_tag_file', file=sys.stderr)
        exit(1)

    separator = sys.argv[1]
    old_tags = dict()
    new_tags = dict()
    tags = old_tags
    lines = 0

    for line in sys.stdin:
        line = line[:-1]
        if line == separator:
            if tags == old_tags:
                tags = new_tags
            else:
                break
        elems = line.split(' ')
        tags[elems[0]] = elems[1:]
        lines += 1

    total = sum((len(old_tags), len(new_tags)))
    print(
        '{} lines/{} tags ({} OLD and {} NEW) read'.format(
            lines,
            total,
            len(old_tags),
            len(new_tags)
        ),
        file=sys.stderr
    )

    added = set()
    deleted = set()
    moved = dict()
    unchanged = 0

    for tag, filename in new_tags.items():
        if tag not in old_tags:
            added.add(tag)
        elif old_tags[tag] != new_tags[tag]:
            moved[tag] = (old_tags[tag],new_tags[tag])
        else:
            unchanged += 1

    print('{} tags added'.format(len(added)))
    for tag in added:
        print(f' * {tag}')

    print('{} tags deleted'.format(len(deleted)))
    for tag in deleted:
        print(f' * {tag}')

    print('{} tags moved'.format(len(moved)))
    for tag, (fro, to) in moved.items():
        print(f' * {tag} (from \'{fro[0]}\' to \'{to[0]}\')')

    print(f'{unchanged} tags unchanged')

    width = len('unchanged')

    print('+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+', file=sys.stderr)
    print(
        ('| {0: <{width}} | {1: <{width}} | {2: <{width}} | {3: <{width}} |').format(
            'Added',
            'Deleted',
            'Moved',
            'Unchanged',
            width=width
        )
        , file=sys.stderr
    )
    print('+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+', file=sys.stderr)
    print(
        ('| {0: <{width}} | {1: <{width}} | {2: <{width}} | {3: <{width}} |').format(
            len(added),
            len(deleted),
            len(moved),
            unchanged,
            width=width
        )
    , file=sys.stderr)
    print('+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+-' + ('-'*width) + '-+', file=sys.stderr)