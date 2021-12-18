#!/bin/bash

for module in $(tac ./modules.order) ; do
	rmmod $(echo "$module" | sed -E 's/^.*\/([^/]+?)\.ko$/\1/');
done;
for module in $(cat ./modules.order) ; do
	insmod "$module";
done;

