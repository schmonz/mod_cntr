#!/bin/sh

set -e

echo "$(uname -s)-$(uname -m)-$(uname -r)" | tr '[A-Z]' '[a-z]'
