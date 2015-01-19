#!/bin/sh

# install pre-commit hook for doing clean commits
if test ! \( -x .git/hooks/pre-commit -a -L .git/hooks/pre-commit \);
then
	rm -f .git/hooks/pre-commit
	ln -s ../../common/hooks/pre-commit.hook .git/hooks/pre-commit
fi

cmake .
