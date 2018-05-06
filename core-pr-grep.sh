#!/bin/bash
shopt -s expand_aliases

# This script makes the assumption that you are running it from the bitcoin repo and that
# the directory structure is such that bitcoin and bitcoin-abc are in the same directory.
# Example:
# /projects
#   /bitcoin
#   /bitcoin-abc

# This script scans the bitcoin-abc repo for commit messages relating to PRs.
# It then scans the bitcoin repo for merge commits with those same PR numbers and tags
# that merge commit.  This should enable quicker scanning for missed backports.

# You can ignore failures related to duplicate tags, as some backports are split into multiple diffs
# that end up trying to tag the same commit again.

# Note: If you plan on running this script multiple times, consider cleaning up the old tags
# before you do so:
# git tag | grep 'abc-backported' | xargs git tag -d

alias abc_log="git --git-dir ../bitcoin-abc/.git log"

prnums="[0-9]\{4\}[0-9]*"
alias prgrep="grep -io 'pr[ #]*$prnums\|bitcoin/pull/$prnums' | grep -io '$prnums'"

prs=$(abc_log | prgrep)
for pr in $prs
do
  maybegithash=$(git --no-pager log --merges --grep "[ #\/]$pr" --pretty=oneline | awk '{print $1}')
  for githash in $maybegithash
  do
    # TODO replace the shorthash with the ABC commit hash
    # For now, the shorthash is used because tags cannot be duplicated.
    git tag "abc-backported-${githash:0:8}" $githash
  done
done
