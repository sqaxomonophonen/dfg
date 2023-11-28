#!/usr/bin/env bash
cd $(dirname $0)
wc -l $(git ls-files '*.c' '*.h')
